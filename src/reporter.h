#pragma once
#include "order_book.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

static const uint32_t LATENCIES_CAPACITY = 1000000;

static const uint32_t DOUBLE_BUFFER_SIZE = 2;

using latencies_t = std::array<uint32_t, LATENCIES_CAPACITY>;

struct latency_buffer {
    std::array<uint32_t, LATENCIES_CAPACITY> samples{};
    std::atomic<std::uint32_t> count;
    std::atomic<std::uint32_t> seq;
};

inline latency_buffer double_buffer[DOUBLE_BUFFER_SIZE];

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
