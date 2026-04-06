# C++ Learning Project Plan: Low-Latency Market Data Feed Processor

## Context

Jacob is a Java developer transitioning into performance-critical C++ systems engineering. This plan provides a realistic project with production-grade requirements, structured as if assigned to a junior C++ developer at a trading firm or systems company. The goal is not to produce a tutorial project — it is to simulate the demands of real performance engineering work, forcing you to confront memory, concurrency, and latency in ways Java never requires.

---

## The Project: Real-Time Market Data Feed Processor

**What it is:** A single-node C++ application that ingests a high-throughput stream of simulated market data (price updates, trade events), maintains an in-memory order book per instrument, and exposes a lock-free snapshot interface consumed by downstream "strategy" threads — all within strict latency bounds.

This is a canonical domain for C++ performance work. Every major exchange, HFT firm, and market data vendor has a version of this system in C++.

---

## Functional Requirements

1. **Ingestion:** Read a stream of binary-encoded market data messages from a UDP socket (or a file replay at full speed). Messages represent Level 2 order book updates: add order, modify order, cancel order, trade executed.

2. **Order Book Maintenance:** For each of N instruments (start with 10, scale to 1000), maintain a price-level order book:
   - Bid side and ask side, sorted by price
   - Each price level has an aggregate quantity
   - Support for add/cancel/modify operations

3. **Snapshot Publication:** After each update, publish a "top of book" snapshot (best bid, best ask, last trade price, timestamp) into a per-instrument ring buffer readable by consumer threads without locking.

4. **Consumer Threads:** Two simulated strategy threads that poll the ring buffers and log when the spread (ask - bid) crosses a threshold. These threads must not block the feed handler.

5. **Statistics Reporter:** A low-priority thread that prints throughput and latency percentiles (p50, p95, p99, p999) every second without impacting the hot path.

---

## Non-Functional Requirements (The Hard Part)

These are the requirements that will force you to learn C++ deeply:

### Latency
- Per-message processing latency (ingestion → snapshot published): **< 1 microsecond** at p99
- No dynamic memory allocation (`new`/`malloc`) on the hot path after initialization
- No system calls on the hot path (no logging, no I/O, no `clock_gettime` via syscall — use RDTSC or `std::chrono` with care)

### Throughput
- Must sustain **1,000,000 messages/second** without message loss
- Replay file can be used to simulate this; real UDP multicast is a stretch goal

### Memory
- Total RSS (resident set size) must remain **under 512 MB** for 1000 instruments
- Memory layout of the order book must be **cache-friendly** — no pointer-chased linked lists
- Pre-allocate all working memory at startup using an arena/pool allocator

### CPU
- Feed handler thread must be **pinned to a dedicated CPU core** (via `pthread_setaffinity_np`)
- No false sharing between the feed handler and consumer threads — shared structures must be **cache-line padded** (64 bytes)
- Compiler optimization flags: `-O3 -march=native`; understand what each does

### Concurrency
- Feed handler and consumer threads must communicate **without mutexes** on the hot path
- Use a **single-producer, single-consumer (SPSC) lock-free ring buffer** you write yourself
- No `std::mutex` or `std::condition_variable` on the critical path; they are permitted only in the stats reporter

### Correctness Under Pressure
- The system must handle **out-of-order messages** (sequence gaps) gracefully — define a recovery policy
- Must detect and report **sequence number gaps** without crashing

---

## What You Will Be Forced to Learn

### Phase 1 — C++ Fundamentals (Before Writing Any of This)

You cannot skip this. Java's GC and object model will actively mislead you.

- **Memory model**: stack vs heap, object lifetime, RAII (`std::unique_ptr`, `std::shared_ptr`, destructors)
- **Move semantics**: `std::move`, rvalue references — why they exist and what Java's equivalent is (nothing)
- **The object layout**: sizeof, alignof, padding, struct packing — Java hides all of this
- **Undefined behavior**: why reading uninitialized memory, signed overflow, and data races are not runtime exceptions in C++ — they are silent corruption
- **Build system basics**: CMake, compile flags, linking
- Resources: *A Tour of C++* (Stroustrup), cppreference.com

### Phase 2 — Memory Without a GC

This is the largest mental shift from Java.

- **Custom allocators**: write a simple arena/bump allocator; understand why `new`/`delete` is too slow for the hot path (it calls into `malloc`, which can lock, which has unpredictable latency)
- **Placement new**: construct objects in pre-allocated memory
- **`std::pmr`** (polymorphic memory resources): the standard library's allocator interface
- **Memory pools**: a fixed-size object pool for order book entries
- **Valgrind + AddressSanitizer**: you will have memory bugs; learn to find them
- Key insight: **latency spikes from the allocator are the #1 source of p99 blowout** in C++ systems

### Phase 3 — Cache and CPU Awareness

Java developers have no intuition for this. It will feel like magic until it clicks.

- **CPU cache hierarchy**: L1 (~4 cycles), L2 (~12 cycles), L3 (~40 cycles), RAM (~200 cycles). A cache miss to RAM is 50x slower than an L1 hit.
- **False sharing**: two threads writing different variables that happen to share a cache line. The fix: `alignas(64)`.
- **Data-oriented design**: store arrays of values, not arrays of objects with pointers. `struct PriceLevel { int64_t price; int64_t qty; }` in a flat array beats `vector<PriceLevel*>` badly.
- **Branch prediction**: keep hot-path conditionals predictable; use `[[likely]]`/`[[unlikely]]`
- **`perf stat` and `perf record`**: measure cache misses and branch mispredictions
- **`cachegrind`**: simulate cache behavior
- Key insight: **your order book's memory layout will determine your latency more than your algorithm**

### Phase 4 — Lock-Free Concurrency

This is where most of the difficulty lives.

- **C++ memory model**: `std::atomic`, `memory_order_relaxed`, `memory_order_acquire`, `memory_order_release`, `memory_order_seq_cst`. These are not optional — using `seq_cst` everywhere is correct but defeats the purpose.
- **The SPSC ring buffer**: write this yourself. It is ~50 lines of C++ and will teach you more about the memory model than any book. A reader and a writer share head/tail indices; no lock needed if only one thread reads and one writes.
- **Why mutexes are banned on the hot path**: a `std::mutex::lock()` call can cause a context switch, which costs ~1-10 microseconds — your entire latency budget
- **`std::atomic<uint64_t>` with `load(acquire)` / `store(release)`**: the correct pattern for the ring buffer
- **ThreadSanitizer (TSan)**: will catch data races; run it during development
- Resource: *C++ Concurrency in Action* (Williams) — read chapters on the memory model carefully

### Phase 5 — Measurement and Profiling

You cannot optimize what you cannot measure.

- **RDTSC**: read the CPU timestamp counter directly for nanosecond-precision latency measurement on the hot path. Understand its caveats (CPU frequency scaling, out-of-order execution, core migration).
- **Latency percentiles**: p50/p95/p99/p999 are not averages. p999 is what your most latency-sensitive user experiences. Use `HdrHistogram` or write a simple histogram.
- **`perf`**: CPU profiling, cache miss counting, branch prediction stats
- **`strace`**: verify there are no unexpected syscalls on the hot path
- **`numactl` and `taskset`**: pin processes to cores and NUMA nodes
- **Flamegraphs**: visualize where CPU time is spent
- Key insight: **measure first, optimize second. Intuition about C++ performance is often wrong.**

### Phase 6 — Stretch Goals (After the Core Works)

- Replace simulated UDP with real `SO_RCVBUF`-tuned UDP socket; experiment with `SO_BUSY_POLL` to avoid interrupt-driven receive
- NUMA-aware memory allocation (`libnuma`)
- Huge pages (`mmap` with `MAP_HUGETLB`) to reduce TLB pressure
- Kernel bypass networking (DPDK or af_xdp) — read about it, understand why it exists
- Compare your ring buffer against `boost::lockfree::spsc_queue`

---

## Suggested Milestones

| Milestone | Deliverable |
|-----------|-------------|
| M1 | Replay a binary message file, parse messages, print stats. No order book yet. |
| M2 | In-memory order book for 1 instrument, single-threaded, all allocation on startup. |
| M3 | Scale to 1000 instruments. Measure RSS. Enforce < 512 MB. |
| M4 | Add SPSC ring buffer. Feed handler writes, one consumer thread reads. No locks. |
| M5 | Add RDTSC latency measurement. Produce p99 latency report. Hit < 1 µs p99. |
| M6 | Pin threads to cores. Measure the improvement with and without pinning. |
| M7 | Add stats reporter thread. Ensure it does not perturb hot path latency. |
| M8 | Run under TSan (no races), ASan (no memory errors), and `perf` (no unexpected hotspots). |

---

## Verification Criteria

- **Correctness**: order book state after replaying a known sequence matches expected output (write a golden-file test)
- **Latency**: RDTSC measurements show p99 < 1 µs under sustained 1M msg/s load
- **Memory**: `/proc/self/status` VmRSS < 512 MB at peak load
- **No dynamic allocation on hot path**: override global `operator new` to assert in release mode, or use a custom allocator that tracks all allocations
- **No data races**: TSan clean under full load
- **No memory errors**: ASan clean on all test inputs

---

## Recommended Resources

| Topic | Resource |
|-------|----------|
| Modern C++ | *A Tour of C++* — Stroustrup |
| Concurrency | *C++ Concurrency in Action* — Williams |
| Performance | *Computer Systems: A Programmer's Perspective* — Bryant/O'Hallaron (chapters on memory hierarchy) |
| Low latency patterns | CppCon talks: "Want fast C++?" (Andrei Alexandrescu), "Lock-free programming" (Herb Sutter) — both on YouTube |
| Memory model | cppreference.com/memory_order — read it multiple times |
| Profiling | `man perf`, Brendan Gregg's blog (brendangregg.com) |
