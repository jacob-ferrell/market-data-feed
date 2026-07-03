# Low-Latency Market Data Feed Processor

A single-node C++ application that ingests a high-throughput stream of simulated market data, maintains an in-memory order book per instrument, and exposes a lock-free snapshot interface consumed by downstream strategy threads — all within strict latency bounds.

This is a learning project designed to simulate the demands of real performance engineering work at a trading firm: memory layout, lock-free concurrency, nanosecond-precision measurement, and CPU-aware design.

---

## Requirements

### Functional
- Parse a stream of binary-encoded market data messages (add, cancel, modify, trade)
- Maintain a price-level order book (bid + ask) for up to 1000 instruments
- Publish best-bid/best-ask/last-trade snapshots into per-instrument lock-free ring buffers after each update
- Consumer threads poll ring buffers and log when the spread crosses a threshold
- A stats reporter thread prints throughput and latency percentiles every second

### Non-Functional
| Requirement | Target |
|---|---|
| p99 processing latency | < 1 µs |
| Throughput | ≥ 1,000,000 msg/sec |
| Peak RSS (1000 instruments) | < 512 MB |
| Hot path allocation | None after startup |
| Hot path synchronization | No mutexes |
| Data races | TSan clean |
| Memory errors | ASan clean |

---

## Build

```bash
# Release (performance measurement)
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# ThreadSanitizer
cmake -B build-tsan -DCMAKE_BUILD_TYPE=TSan
cmake --build build-tsan

# AddressSanitizer
cmake -B build-asan -DCMAKE_BUILD_TYPE=ASan
cmake --build build-asan
```

## Run

```bash
# Generate test data (30M messages, 1000 instruments)
./build-release/tools/generate_messages

# Run without thread pinning
./build-release/src/market_data_feed

# Run with feed handler pinned to core 1
./build-release/src/market_data_feed --pin
```

---

## Milestones

### M1 — Binary Message Parsing and Replay

**Goal:** Read a file of binary-encoded messages, parse them, and print summary stats.

The message format is fixed-length binary with four types: `ADD_ORDER`, `CANCEL_ORDER`, `MODIFY_ORDER`, `TRADE`. Each message carries a type byte, sequence number, instrument ID, and type-specific fields. Fixed-length messages mean the parser never scans — it reads N bytes and casts directly into a struct.

Prices are encoded as scaled integers (no floating point) to avoid equality comparison issues and rounding errors in order book logic.

A message generator in `tools/generate_messages.cpp` produces synthetic binary replay files at configurable volume and instrument count.

**Validation:** Message type counts printed at shutdown match generator output exactly.

---

### M2 — Single-Instrument Order Book

**Goal:** Maintain a correct in-memory order book for one instrument with no dynamic allocation after startup.

The order book stores price levels as a flat sorted array per side (bid/ask), with a fixed capacity of 50 levels per side pre-allocated at startup. Insertion maintains sort order in-place. No heap allocation occurs during message processing.

Live orders are tracked in a custom open-addressing flat hash map (`flat_hash_map`) backed by a fixed-size array, also pre-allocated at startup. This avoids `std::unordered_map` which allocates per-entry.

**Validation:** A known message sequence replayed against the order book produces the expected book state (golden-file test in `tests/`).

---

### M3 — Scale to 1000 Instruments

**Goal:** Handle 1000 instruments within the 512 MB RSS budget.

Memory layout per instrument: 2 sides × 50 levels × 12 bytes per level = 1.2 KB per book. At 1000 instruments the order book array is ~1.2 MB. The bulk of RSS comes from the live order hash map and the per-instrument SPSC ring buffers.

RSS is measured at startup and at shutdown by reading `/proc/self/status`.

**Result:** 432 MB RSS after replaying 30M messages across 1000 instruments — within the 512 MB budget.

---

### M4 — SPSC Ring Buffer and Consumer Thread

**Goal:** Feed handler writes snapshots into a lock-free ring buffer; a consumer thread reads without locks.

A hand-written single-producer single-consumer ring buffer (`spsc_ring_buffer`) uses a 1024-slot `std::array<snapshot>` with separate atomic head and tail indices. The writer owns `tail`; the reader owns `head`. Each thread only writes its own index and reads the other's — no lock needed.

`head` and `tail` are `alignas(64)` to prevent false sharing between the feed handler and consumer threads. Publish uses `memory_order_release`; consume uses `memory_order_acquire`.

There is one ring buffer per instrument. A consumer thread polls all 1000 buffers and logs when `ask_price - bid_price` crosses a configurable spread threshold.

**Validation:** Consumer-observed snapshot count matches feed handler publish count under full load.

---

### M5 — RDTSC Latency Measurement

**Goal:** Measure per-message processing latency at nanosecond precision and produce percentile reports.

Latency is measured from the start of `process_message` to the point the snapshot is written to the ring buffer. RDTSC is read at both points, with `lfence` barriers before each read to prevent out-of-order execution from skewing the measurement.

```cpp
__asm__ volatile("lfence" ::: "memory");
uint64_t start = rdtsc();
// ... process message, publish snapshot ...
__asm__ volatile("lfence" ::: "memory");
uint64_t end = rdtsc();
```

Raw samples are stored in a pre-allocated 1M-entry array — no allocation, no I/O on the hot path. Percentiles are computed by sorting the sample array at reporting time in the stats thread.

**Results (30M messages, release build, per-second averages excluding final partial second):**

| p50 | p95 | p99 | p999 | Throughput |
|---|---|---|---|---|
| 106 ns | 265 ns | 330 ns | 568 ns | ~4.8M msg/sec |

p99 is well under the 1 µs target.

---

### M6 — Thread Pinning

**Goal:** Pin the feed handler to a dedicated core and measure the latency improvement.

Thread pinning is opt-in via a `--pin` command-line flag. When enabled, `pthread_setaffinity_np` restricts the feed handler to core 1 at startup.

```cpp
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(CORE_ID, &cpuset);
pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
```

Pinning eliminates scheduler-driven core migrations, reducing latency variance. On a lightly loaded machine without core isolation (`isolcpus`), the difference is negligible — the scheduler rarely migrates threads in the absence of competing load. The meaningful gains appear under contention or with `isolcpus` in kernel boot parameters.

| Configuration | p50 | p95 | p99 | p999 | Throughput |
|---|---|---|---|---|---|
| No pin (run 1) | 107 ns | 264 ns | 327 ns | 563 ns | 4.8M msg/sec |
| No pin (run 2) | 106 ns | 267 ns | 333 ns | 574 ns | 4.7M msg/sec |
| `--pin` (run 1) | 107 ns | 269 ns | 333 ns | 580 ns | 4.7M msg/sec |
| `--pin` (run 2) | 112 ns | 266 ns | 334 ns | 571 ns | 4.3M msg/sec |

---

### M7 — Statistics Reporter Thread

**Goal:** Report throughput and latency percentiles every second without perturbing the hot path.

The reporter thread wakes once per second, swaps a double buffer, and computes percentiles from the previous second's samples. The feed handler writes into the active buffer; the reporter reads from the inactive one.

The reporter is given the lowest possible scheduling priority via `SCHED_IDLE` so the OS always preempts it in favor of the feed handler under contention:

```cpp
struct sched_param param = { .sched_priority = 0 };
pthread_setschedparam(pthread_self(), SCHED_IDLE, &param);
```

**Double-buffer synchronization (seqlock):** A race exists where the feed handler may have loaded the active buffer index just before the reporter flips it, and could still be mid-write to the old buffer when the reporter begins reading it. A per-buffer seqlock resolves this without ever blocking the feed handler:

- Feed handler: `seq.fetch_add(1, relaxed)` before writing (seq → odd), `seq.fetch_add(1, release)` after (seq → even). The count increment is inside the seqlock bracket so the reporter's acquire sees it.
- Reporter: after flipping, spins on the old buffer's seq with `memory_order_acquire` until even before reading. `_mm_pause()` is used in the spin body.

```cpp
while (prev_active_buffer.seq.load(std::memory_order_acquire) & 1) {
    _mm_pause();
}
```

The spin duration is nanoseconds — a single sample write — against a one-second sleep interval, so the inaccuracy in per-second windows is negligible.

**Output (example):**
```
T+0s - T+1s:
p50=131, p95=328, p99=447, p999=663
Throughput: 3941872
```

---

### M8 — Sanitizer and Profiler Validation

**ThreadSanitizer:** Clean after the seqlock fix. The original double-buffer implementation had a data race on `samples[]` between the feed handler and the reporter thread. The seqlock's release/acquire pairing on `seq` establishes the necessary happens-before relationship.

**AddressSanitizer:** Clean. No heap allocation occurs on the hot path after initialization, so there are no use-after-free or buffer overflow opportunities in the message processing loop.

**`perf stat` (release build, --pin, 30M messages):**

```
instructions:           57,511,679,991   (1.2 IPC)
cpu-cycles:             46,138,960,093   (3.5 GHz effective)
branch-misses:             153,601,683   (1.0% miss rate)
stalled-cycles-frontend:  23,546,330,835  (51% frontend idle)
```

Branch prediction is healthy at 1.0% miss rate. The 51% frontend stall rate reflects the varied per-message-type code paths (switch dispatch, hash map pointer chasing, multiple book handlers) — expected for this workload, and consistent with the IPC staying at 1.2 rather than collapsing.

---

## Architecture

```
                    ┌─────────────────────────────┐
  data.bin ──────▶  │     Feed Handler Thread      │
                    │  parse → order book → RDTSC  │
                    └──────────┬──────────┬────────┘
                               │          │
                    snapshots  │          │  latency samples
                    (SPSC RB)  │          │  (double buffer + seqlock)
                               ▼          ▼
                    ┌──────────────┐  ┌──────────────────┐
                    │   Consumer   │  │  Reporter Thread  │
                    │   Thread     │  │  (SCHED_IDLE)     │
                    │ spread alert │  │  p50/p95/p99/p999 │
                    └──────────────┘  └──────────────────┘
```

## Key Design Decisions

| Decision | Choice | Reason |
|---|---|---|
| Order book storage | Flat sorted array | Cache-friendly; no pointer chasing |
| Live order index | Open-addressing hash map | No per-entry allocation |
| Feed→consumer IPC | SPSC ring buffer | Lock-free; single writer/reader invariant |
| Latency measurement | RDTSC + lfence | No syscall; nanosecond resolution |
| Reporter IPC | Double buffer + seqlock | Reporter never blocks feed handler |
| Reporter scheduling | SCHED_IDLE | OS always preempts reporter before feed handler |
| Thread pinning | `pthread_setaffinity_np` | Eliminates migration jitter; opt-in via --pin |
