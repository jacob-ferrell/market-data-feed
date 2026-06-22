#include "reporter.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread> 
#include <immintrin.h> 

void log_latency(latency_buffer& buffer) {

    uint32_t end = std::min(
        (uint32_t) buffer.samples.size(), 
        buffer.count.load(
        std::memory_order_relaxed)
    );

    std::sort(buffer.samples.begin(), buffer.samples.begin() + end);

    uint64_t p50 = buffer.samples[end * 0.50];
    uint64_t p95 = buffer.samples[end * 0.95];
    uint64_t p99 = buffer.samples[end * 0.99];
    uint64_t p999 = buffer.samples[end * 0.999];

    std::printf("p50=%lu, p95=%lu, p99=%lu, p999=%lu\n", p50, p95, p99, p999);
}


//swaps buffers and returns the now inactive buffer
//that is ready to be read
bool swap_active_buffer() {
    bool prev_active = active_buffer.load(std::memory_order_relaxed);
    active_buffer.store(!prev_active, std::memory_order_release);

    latency_buffer& prev_active_buffer = double_buffer[prev_active];

    // spin when seq is odd
    while (prev_active_buffer.seq.load(std::memory_order_acquire) & 1) {
        _mm_pause();
    }

    log_latency(prev_active_buffer);

    prev_active_buffer.count.store(0, std::memory_order_release);
    return prev_active;
}

void reporter_fun(const std::atomic<bool>& running, const std::atomic<uint64_t>& total_messages) {

    // struct that holds the scheduling priority value
    struct sched_param param;

    // set the priority to 0 — required for SCHED_IDLE, which ignores priority anyway
    param.sched_priority = 0;

    // apply the scheduling policy to the current thread:
    // pthread_self() gets the current thread's handle
    // SCHED_IDLE is the lowest possible scheduling class — only runs when nothing else needs CPU
    // &param passes the priority struct
    pthread_setschedparam(pthread_self(), SCHED_IDLE, &param);

    uint64_t prev_total = 0;
    uint32_t elapsed_seconds = 0;

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        elapsed_seconds++;
        std::printf("T+%us - T+%us:\n", elapsed_seconds - 1, elapsed_seconds);
        swap_active_buffer();

        uint64_t total = total_messages.load(std::memory_order_relaxed); 
        uint64_t cur_total = total - prev_total;    
        prev_total = total;
        std::printf("Throughput: %lu\n", cur_total);
    }
}

void log_throughput(
    uint64_t total_messages,
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end
) {
 
    double seconds = std::chrono::duration<double>(end - start).count();

    double throughput = total_messages / seconds;

    std::printf("Time:   %.3f seconds\n", seconds);
    std::printf("Throughput: %.0f msg/sec\n", throughput);
}

void log_totals(
    uint64_t total_messages,
    long add_count,
    long cancel_count,
    long modify_count,
    long trade_count,
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end
) {

    std::printf("Total messages: %lu\n", total_messages);
    std::printf("Add:    %lu\n", add_count);
    std::printf("Cancel: %lu\n", cancel_count);
    std::printf("Modify: %lu\n", modify_count);
    std::printf("Trade:  %lu\n", trade_count);
}

void log_anomalies(
    const anomalies& counts
) {
    std::printf("\nAnomalies:\n");
    std::printf("  Add:    %lu\n", anomaly_counts.add);
    std::printf("  Cancel: %lu\n", anomaly_counts.cancel);
    std::printf("  Modify: %lu\n", anomaly_counts.modify);
    std::printf("  Trade:  %lu\n", anomaly_counts.trade);
}

