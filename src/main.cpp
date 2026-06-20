
#include "constants.h"
#include "messages.h"
#include "parser.h"
#include "order_book.h"
#include "snapshot.h"
#include "ticker_registry.h"
#include "spsc_ring_buffer.h"
#include "../tools/timestamp_utils.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

static int SPREAD_THRESHOLD = 500000;

static int CORE_ID = 1;

long rss_kb() {

    FILE* f = fopen("/proc/self/status", "r");

    if (!f) return -1;

    char line[128];

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            long kb;
            sscanf(line + 6, "%ld", &kb);
            fclose(f);
            return kb;
        }
    }

    fclose(f);

    return -1;

}

void consumer_fun(const std::atomic<bool>& running) {
    while (running) {

        for (int i = 0; i < NUM_INSTRUMENTS; i++) {

            ring_buffer& buffer = ring_buffers[i];

            snapshot ss;

            if (!buffer.try_pop(ss)) continue;

            int64_t spread = (int64_t) ss.best_ask_price - (int64_t) ss.best_bid_price;
        /*
            if (std::abs(spread) > SPREAD_THRESHOLD) {
                printf("spread alert: instrument=%u, ask=%u, bid=%u, spread=%ld", ss.instrument_idx, ss.best_ask_price, ss.best_bid_price, spread);
            }
        */
        }

    }
}

void log_throughput(
    long total_messages,
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end
) {
 
    double seconds = std::chrono::duration<double>(end - start).count();

    double throughput = total_messages / seconds;

    std::printf("Time:   %.3f seconds\n", seconds);
    std::printf("Throughput: %.0f msg/sec\n", throughput);
}

void log_totals(
    long total_messages,
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

void log_latency() {
    std::sort(latencies.begin(), latencies.end());
    size_t count = latencies.size();

    uint64_t p50 = latencies[count * 0.50];
    uint64_t p95 = latencies[count * 0.95];
    uint64_t p99 = latencies[count * 0.99];
    uint64_t p999 = latencies[count * 0.999];

    std::printf("p50=%lu, p95=%lu, p99=%lu, p999=%lu\n", p50, p95, p99, p999);
}

int main (int argc, char *argv[]) {
    // Open data binary
    FILE* fp = fopen("data/data.bin", "rb");

    if (!fp) {
        perror("fopen");
        return 1;
    }

    initialize_tickers();

    // get total file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    long total_messages = 0;

    long add_count = 0;
    long cancel_count = 0;
    long modify_count = 0;
    long trade_count = 0;

    message msg;

    auto start = std::chrono::steady_clock::now();

    printf("RSS after init: %ld MB\n", rss_kb() / 1024);

    std::atomic<bool> running{true};

    std::thread consumer(consumer_fun, std::ref(running));

    calibrate_cycles_per_ns();

    bool pin_thread = false;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--pin") == 0) pin_thread = true;
    }

    if (pin_thread) {
        // a bitmask representing which CPU cores a thread is allowed to run on
        cpu_set_t cpuset;

        // zero out the bitmask — no cores selected yet
        CPU_ZERO(&cpuset);

        // set the bit for core_id — allow the thread to run on that core only
        CPU_SET(CORE_ID, &cpuset);

        // apply the affinity mask to the current thread
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

    while (parse_next_message(fp, &msg)) {
        total_messages++;
        process_message(msg);
        switch (msg.type) {
            case ADD: {add_count++; break;}
            case CANCEL: {cancel_count++; break;}
            case MODIFY: {modify_count++; break;}
            case TRADE: {trade_count++; break;}
        }
    }

    auto end = std::chrono::steady_clock::now();

    running.store(false);

    consumer.join();

    printf("RSS after replay: %ld MB\n", rss_kb() / 1024);
 
    log_totals(
        total_messages, 
        add_count, 
        cancel_count, 
        modify_count, 
        trade_count, 
        start, 
        end
    );

    log_throughput(total_messages, start, end);

    log_anomalies(anomaly_counts);

    log_latency();

    fclose(fp);

    return 0;
}

