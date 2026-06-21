
#include "consumer.h"
#include "messages.h"
#include "parser.h"
#include "order_book.h"
#include "ticker_registry.h"
#include "../tools/timestamp_utils.h"
#include "reporter.h"
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
    std::atomic<uint64_t> total_messages{0};

    long add_count = 0;
    long cancel_count = 0;
    long modify_count = 0;
    long trade_count = 0;

    message msg;

    auto start = std::chrono::steady_clock::now();

    printf("RSS after init: %ld MB\n", rss_kb() / 1024);

    std::atomic<bool> running{true};

    std::thread consumer(consumer_fun, std::ref(running));

    std::thread reporter(reporter_fun, std::ref(running), std::ref(total_messages));

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
        total_messages.fetch_add(1, std::memory_order_relaxed);
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

    reporter.join();

    printf("RSS after replay: %ld MB\n", rss_kb() / 1024);

    const uint64_t final_total = total_messages.load();
 
    log_totals(
        final_total, 
        add_count, 
        cancel_count, 
        modify_count, 
        trade_count, 
        start, 
        end
    );

    log_throughput(final_total, start, end);

    log_anomalies(anomaly_counts);

    fclose(fp);

    return 0;
}

