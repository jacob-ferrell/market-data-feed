#pragma once
#include "order_book.h"
#include <atomic>
#include <chrono>
#include <cstdint>

static const uint32_t LATENCIES_CAPACITY = 1000000;

static const uint32_t DOUBLE_BUFFER_SIZE = 2;

using latencies_t = std::array<uint32_t, LATENCIES_CAPACITY>;

inline uint32_t latencies_size = 0;

extern latencies_t latencies;

inline std::array<latencies_t, DOUBLE_BUFFER_SIZE> double_buffer;

inline std::atomic<bool> active_buffer{false};

inline std::array<uint32_t, DOUBLE_BUFFER_SIZE> double_buffer_counts;

void reporter_fun(const std::atomic<bool>& running, const std::atomic<uint64_t>& total_messages);

void log_throughput(
    uint64_t total_messages,
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end
);

void log_totals(
    uint64_t total_messages,
    long add_count,
    long cancel_count,
    long modify_count,
    long trade_count,
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end
);

void log_anomalies(
    const anomalies& counts
);

void log_latency();
