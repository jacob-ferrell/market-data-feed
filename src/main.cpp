
#include "messages.h"
#include "parser.h"
#include <chrono>
#include <cstdio>

void log_metrics(long file_size, std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end) {
   
}

int main (int argc, char *argv[]) {
    // Open data binary
    FILE* fp = fopen("data/data.bin", "rb");

    if (!fp) {
        perror("fopen");
        return 1;
    }

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

    while (parse_next_message(fp, &msg)) {
        total_messages++;
        switch (msg.type) {
            case ADD: {add_count++; break;}
            case CANCEL: {cancel_count++; break;}
            case MODIFY: {modify_count++; break;}
            case TRADE: {trade_count++; break;}
        }
    }

    auto end = std::chrono::steady_clock::now();
 
    double seconds = std::chrono::duration<double>(end - start).count();

    double throughput = total_messages / seconds;

    std::printf("Total messages: %lu\n", total_messages);
    std::printf("Add:    %lu\n", add_count);
    std::printf("Cancel: %lu\n", cancel_count);
    std::printf("Modify: %lu\n", modify_count);
    std::printf("Trade:  %lu\n", trade_count);
    std::printf("Time:   %.3f seconds\n", seconds);
    std::printf("Throughput: %.0f msg/sec\n", throughput);

    fclose(fp);

    return 0;
}

