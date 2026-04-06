# Implementation Guide: Low-Latency Market Data Feed Processor

This document provides step-by-step guidance for working through the project plan. It assumes you understand the concepts described in PLAN.md and focuses on the practical "how do I actually start and progress through this?" questions.

---

## Before Writing Any Code

### Set Up Your Environment

Install the tools you'll need throughout the project before you write a single line:

- **Compiler:** Use `g++` (GCC 12+) or `clang++` (Clang 15+). Both are fine; Clang's error messages are generally clearer for beginners.
- **Build system:** CMake 3.20+. Every milestone will build through CMake.
- **Sanitizers:** These ship with GCC and Clang — no separate install needed, but verify they work early.
- **Profiling:** Install `perf` (usually the `linux-perf` or `perf` package for your distro), `valgrind`, and `hotspot` (a GUI flamegraph viewer for perf data).
- **Optional but useful:** `htop` with tree view to verify thread pinning, `numactl` for later milestones.

### Project Directory Structure

Decide on your layout before M1. A structure that scales well through all milestones:

```
market-data-feed/
  CMakeLists.txt          # top-level build
  src/                    # all .cpp and .h files
  tests/                  # correctness tests
  tools/                  # message generator, replay scripts
  data/                   # generated binary message files
  docs/                   # notes, latency reports
```

Keep the build directory out of the source tree (`cmake -B build`). You'll have multiple build configurations by M5 (debug, asan, tsan, release), so name them accordingly: `build-debug`, `build-asan`, `build-release`.

### CMake Configuration Strategy

Set up your CMakeLists.txt with multiple targets from the start:
- A main executable target
- A test executable target
- Separate CMake presets or manual configurations for debug, AddressSanitizer, ThreadSanitizer, and release builds

Get comfortable switching between them. You should be running ASan and TSan builds regularly, not just at the end.

---

## Milestone-by-Milestone Guidance

### M1 — Binary Message Parsing and Replay

**Goal:** Read a file of binary-encoded messages and parse them correctly.

**First decision: define your message format.** You need to decide the binary layout of your messages before you can write anything. Design it on paper first. Think about: what fields does each message type need? What are their types and sizes? How will you distinguish message types? Fixed-length messages are much easier to handle than variable-length ones — favor fixed-length for this project.

#### Understanding the Message Types

The four message types map directly to things participants do in a market:

**Add Order** — a participant wants to buy or sell. They specify which instrument, which side (bid = buy, ask = sell), at what price, for how much quantity, and a unique order ID so the order can be modified or cancelled later.

**Cancel Order** — a participant withdraws a previously placed order. The order ID is the primary key; everything else about the order can be looked up. Carrying the instrument ID in the cancel message as well lets you route it to the correct order book without a global lookup.

**Modify Order** — a participant changes the quantity on an existing order. Price changes are typically modeled as a cancel followed by a new add, so modify only needs to carry the order ID, new quantity, and instrument ID.

**Trade Executed** — two orders matched. This is a notification, not an instruction. It carries the instrument ID, the price traded, the quantity traded, and a timestamp. Whether it references the specific order IDs involved depends on the feed; for this project that's optional.

#### Fields Every Message Shares

- **Message type** — a single byte or small integer that tells the parser what kind of message follows. Without this, the parser can't interpret anything.
- **Sequence number** — a monotonically increasing integer assigned by the feed. This is how you detect gaps: if you receive sequence 1001 after 999, you know 1000 was dropped. The plan requires gap detection, so every message needs this.
- **Timestamp** — when the exchange generated the message. This is distinct from when you received it; both are useful for different things.

#### How Binary Formats Work

In a binary protocol, each field occupies a fixed number of bytes at a fixed offset within the message. There are no delimiters, tags, or labels — just bytes at known positions. The receiver knows "bytes 0–1 are the message type, bytes 2–9 are the sequence number" and so on. The parser doesn't scan or search; it casts or copies bytes directly into struct fields. This is why binary formats parse orders of magnitude faster than text formats like JSON or FIX.

With fixed-length messages, every message of a given type is the same size. The parser reads N bytes, checks the type field, and knows exactly what it has. Variable-length messages require knowing how many bytes to read before you know what you have — avoid them for this project.

#### A Real Reference: NASDAQ ITCH 5.0

You don't need to invent this from scratch conceptually. NASDAQ publishes their ITCH 5.0 binary protocol specification publicly — it's freely available as a PDF on NASDAQ's developer site. It defines the exact message types, field names, sizes, and byte offsets used by a real order book feed. Reading it will give you a concrete mental model of what a binary market data specification looks like in practice.

You are not implementing ITCH — you're designing your own simplified format — but seeing a real specification resolves a lot of uncertainty about what fields matter and why. Pay particular attention to how they distinguish message types, what fields each message carries, and how they encode prices (they use integers scaled by a fixed factor, not floating point — this is deliberate and worth understanding before you decide your own price representation).

#### Decisions to Make When Designing Your Format

Once you understand what each message type means, the decisions for your own format are:

- **Integer type for each field:** Price, quantity, order ID, sequence number, timestamp each have a natural size. Think about the maximum value each needs to hold.
- **Price encoding:** Floating point is problematic for exact equality comparisons and has rounding behavior that makes order book logic error-prone. Most real systems use a scaled integer — price expressed in "ticks" or integer millionths of a dollar. Decide your scale factor before you write anything that touches price.
- **Instrument ID:** A small integer (2 or 4 bytes) is much faster to route on than a variable-length string. Plan to map instrument names to integer IDs at startup and use only the integer ID inside messages.
- **Total message size:** Calculate the byte count for each message type. Smaller messages mean more fit in a cache line, which affects throughput at scale.

**Second decision: write the message generator first.** Before you can test your parser, you need test data. Write a separate small tool (in `tools/`) that generates a binary file full of synthetic messages. This tool can use whatever C++ you're comfortable with — it is not on the hot path, so `new`, `printf`, and `std::vector` are all fine here.

**Third: write the parser.** Reading binary data from a file in C++ means opening the file, reading raw bytes into a buffer, and casting or copying those bytes into your struct types. The key issue to understand here is struct alignment and padding — the in-memory layout of a struct may not match your wire format if you aren't careful. Read about `#pragma pack` and `__attribute__((packed))` and understand the tradeoffs before deciding whether to use them.

**Validation at M1:** Print a count of each message type parsed, a total message count, and a simple throughput number (messages per second). Verify the counts match what your generator produced.

---

### M2 — Single-Instrument Order Book

**Goal:** Maintain a correct in-memory order book for one instrument with no dynamic allocation after startup.

**Start with correctness, then worry about allocation.** Get the order book logic right first in whatever way feels natural. Then replace all dynamic allocation with pre-allocated structures.

**The key design question for the order book:** How do you store price levels? This is the most important data structure decision in the project. Your options are essentially a sorted flat array, a hash map, or a tree. Each has different performance characteristics.6: Given the cache-friendliness requirement in the plan, a flat array sorted by price is worth serious consideration over pointer-based structures. Think through the tradeoffs: what operations need to be fast? Add, cancel, and modify all need to find a price level quickly. How many distinct price levels do you realistically expect?

**Pre-allocation strategy:** Decide upfront the maximum number of price levels per side per instrument. Allocate all of it at startup. This means you need to make conservative estimates and commit to them. Overestimate rather than underestimate — you can tune later.

**Validation at M2:** Write a known sequence of add/modify/cancel messages by hand (in a test file), replay them, and verify the final order book state matches what you'd compute by hand. This is your first golden-file test — keep it and expand it.

---

### M3 — Scale to 1000 Instruments

**Goal:** Handle 1000 instruments within the 512 MB RSS budget.

**Do the math first.** Before writing code, calculate your expected memory usage: number of instruments × (price levels per side × 2 sides × bytes per price level). If this exceeds 512 MB, you need to reduce your per-instrument allocation. This arithmetic exercise often reveals surprising constraints and forces you to think carefully about your data layout.

**How to measure RSS:** Read `/proc/self/status` and look for `VmRSS`. Do this programmatically from within your application so you can print it at startup and at peak load. You should see the number stabilize after initialization and not grow during the replay — if it grows, you have unintended dynamic allocation happening.

**Validation at M3:** Run the replay with 1000 instruments under full load and confirm RSS stays under 512 MB. Any growth in RSS during replay is a bug.

---

### M4 — SPSC Ring Buffer and Consumer Thread

**Goal:** Feed handler writes snapshots into a ring buffer; a consumer thread reads them without locks.

**Write the ring buffer before integrating it.** Test it in isolation with a simple single-threaded test (writer writes N items, reader reads them back, verify correctness) before putting it in the feed handler path. Once you're confident it's correct in isolation, integrate it.

**The critical concept for the ring buffer:** The writer owns the tail index; the reader owns the head index. Each thread only writes to its own index and reads the other's. This is what makes the lock unnecessary — there is no shared mutable state beyond the indices and the buffer itself, and the access pattern ensures only one thread ever writes each slot.

**Memory ordering in the ring buffer:** This is where you must use `std::atomic` correctly. The key question for each load/store of the head and tail: is this a synchronization point? The store to the tail (after writing a slot) needs to be visible to the reader; the store to the head (after consuming a slot) needs to be visible to the writer. Use `memory_order_release` on the publish store and `memory_order_acquire` on the consuming load. The intermediate reads of the buffer data can use `memory_order_relaxed`. If in doubt, use `memory_order_seq_cst` to get correctness first, then profile and tighten the ordering.

**Run TSan as soon as you have multiple threads.** Add a TSan build target and run it after M4. TSan will catch any accidental shared state you missed. Do not proceed to M5 without a clean TSan run.

**Validation at M4:** The consumer thread should observe every snapshot the feed handler publishes. Add counters: published count vs. consumed count. Under sustained load they should match (accounting for the ring buffer's capacity — if the consumer is slower than the producer, you will see drops; that's expected and should be reported, not silently ignored).

---

### M5 — RDTSC Latency Measurement

**Goal:** Measure per-message latency at nanosecond precision and produce percentile reports.

**What to measure:** Latency is the time from "message bytes received" to "snapshot written to ring buffer." Record a timestamp at the start of processing and at the end, compute the difference, and store it. Do not log or print latency on the hot path — store raw samples in a pre-allocated array and compute statistics offline (in the stats reporter thread or at shutdown).

**RDTSC vs. `std::chrono`:** `std::chrono::high_resolution_clock` often calls `clock_gettime`, which on most Linux systems is a vDSO call (not a full syscall, but still slower than RDTSC). For the hot path, RDTSC is the right tool. Understand its caveats: it measures CPU cycles, not wall time; you need to convert using the CPU frequency; it can be reordered by out-of-order execution (use `__asm__ volatile("" ::: "memory")` or `_mm_lfence()`/`_mm_mfence()` as barriers if needed); and it can vary across cores (pin your thread to avoid this, which you're doing in M6 anyway).

**How to compute percentiles:** Collect raw latency samples in a pre-allocated circular buffer. At reporting time (once per second in the stats thread), sort a copy and index into it for p50/p95/p99/p999. Alternatively, use an HDR histogram structure, which is purpose-built for this and avoids the sort. HDR Histogram (the C or C++ port) is a single-header library and worth using.

**The < 1 µs p99 target:** This will not happen automatically. If you miss it initially, the usual causes are: dynamic allocation on the hot path, a cache miss in the order book lookup, or incorrect memory ordering (using `seq_cst` unnecessarily). Profile first, optimize second.

**Validation at M5:** Print a latency report showing p50, p95, p99, and p999 at shutdown. Under simulated 1M msg/s load, p99 should be under 1000 nanoseconds.

---

### M6 — Thread Pinning

**Goal:** Pin the feed handler to a dedicated core and measure the latency improvement.

**How to pin threads:** Use `pthread_setaffinity_np` with a `cpu_set_t`. Call it at the start of the thread's run function. You need to decide which core to pin to — avoid core 0 (often used by the kernel for interrupts) and pick an isolated core if possible.

**Isolating a core (optional but instructive):** You can add `isolcpus=N` to your kernel boot parameters to prevent the scheduler from running normal processes on core N. This reduces jitter significantly. It's not required for the project but worth understanding.

**How to measure the improvement:** Run M5's latency measurement with and without the pinning call (make it configurable via a command-line flag or compile-time constant). The difference in p99 latency should be measurable — pinning reduces variance from scheduler preemption.

**False sharing check:** By M6, verify your shared data structures are cache-line padded. Any struct that is written by one thread and read by another should be aligned to 64 bytes. The symptom of false sharing is high p99 with no obvious cause — `perf stat` will show elevated cache coherence traffic.

---

### M7 — Statistics Reporter Thread

**Goal:** Add a low-priority reporting thread that does not perturb the hot path.

**The core design constraint:** The stats reporter must never cause the feed handler to block or stall. This means the stats reporter can only read data that the feed handler has already made available via atomic operations or lock-free structures. It must not take any lock that the feed handler also takes.

**What the stats reporter reads:** Throughput counters (messages processed per second) and the latency histogram. Both should be updated by the feed handler using atomic operations (`std::atomic<uint64_t>` for counters; the latency histogram can use a double-buffering approach where the feed handler writes to one buffer while the reporter reads from the other).

**Thread priority:** Use `pthread_setschedparam` or `nice` to give the stats reporter a lower scheduling priority than the feed handler. This ensures the OS preempts it rather than the feed handler under contention.

**Validation at M7:** Measure latency p99 with and without the stats reporter thread running. The difference should be negligible (< 100 ns added to p99). If it isn't, you have a contention problem between the reporter and the hot path.

---

### M8 — Final Validation Pass

**Goal:** Clean bill of health from sanitizers and profiler.

Run your full test suite under each of these configurations and fix everything they find:

1. **AddressSanitizer:** Catches memory errors — use-after-free, buffer overflows, reads of uninitialized memory. Run your full replay test under ASan. Fix every error — there are no benign ASan errors.

2. **ThreadSanitizer:** Catches data races. Run under TSan with the multi-threaded configuration (all threads active). A TSan data race report almost always indicates a real bug, not a false positive. Fix them.

3. **`perf stat`:** Run `perf stat ./market-data-feed <replay-file>` and examine `cache-misses`, `branch-misses`, and `context-switches`. High cache miss counts indicate layout problems; high context switches on the feed handler thread indicate something is causing preemption.

4. **`strace`:** Run `strace -c ./market-data-feed <replay-file>` and verify there are no syscalls being made during the steady-state processing loop. Unexpected syscalls (especially `brk` or `mmap`, which indicate dynamic allocation) need to be eliminated.

---

## Cross-Cutting Concerns

### How to Think About Java vs. C++ Memory

The single biggest mental shift: in Java, `new` is cheap and the GC handles cleanup. In C++, `new` calls `malloc`, which acquires a lock internally, may call into the kernel to get more memory, and has unpredictable latency. This is why the plan bans dynamic allocation on the hot path. Your hot path must operate entirely within memory you allocated at startup.

When you catch yourself writing `std::vector::push_back` or `std::map::insert` in the feed handler loop, stop — those allocate. You need fixed-capacity, pre-allocated alternatives everywhere on the hot path.

### Build Configuration Discipline

Keep four build configurations active throughout development:

| Config | Flags | When to Use |
|--------|-------|-------------|
| Debug | `-O0 -g` | Step-through debugging |
| ASan | `-O1 -g -fsanitize=address` | Memory error checking |
| TSan | `-O1 -g -fsanitize=thread` | Race condition checking |
| Release | `-O3 -march=native -DNDEBUG` | Performance measurement |

Never measure latency in a debug or sanitizer build — the overhead is enormous. Never ship a change without running ASan and TSan on it.

### Testing Strategy

You need two categories of tests:

1. **Correctness tests:** Golden-file tests that replay a known sequence and verify the order book state. These run in the debug build and should catch any logic errors in add/cancel/modify handling. Write these early (M2) and keep them passing through every milestone.

2. **Performance tests:** The latency and throughput measurements from M5 onward. These run in the release build only, under full simulated load. They are not pass/fail in the CI sense — they are measurements you record and track over time.

### When You Get Stuck

- **Correctness bugs:** Add temporary logging (it's fine in the debug build), use a debugger (`gdb` or `lldb`), and reduce your replay file to the minimal sequence that reproduces the bug.
- **Latency budget misses:** Profile before changing code. Use `perf record` + `perf report` or a flamegraph to find where time is actually going. Intuition about performance is often wrong.
- **Sanitizer errors:** Read the full stack trace. ASan and TSan errors include the allocation and access sites, which almost always point directly to the problem.
- **Concurrency bugs that TSan doesn't catch:** These usually involve incorrect memory order assumptions. Draw a happens-before diagram: for the consumer to safely read a slot, what must have happened first, and what `std::atomic` operations enforce that ordering?

---

## Recommended Learning Order

Work through Phase 1 of the plan (C++ fundamentals) before starting M1. You need to understand RAII and object lifetime before you write any structs that manage resources. The rest of the phases align naturally with the milestones: Phase 2 (allocators) before M2, Phase 3 (cache) as you design the order book for M2/M3, Phase 4 (lock-free) before M4, and Phase 5 (profiling) before M5.

Do not read ahead to Phase 4 and try to write the ring buffer before you have a working order book. The milestones are ordered the way they are because each one reveals the constraint that motivates the next.
