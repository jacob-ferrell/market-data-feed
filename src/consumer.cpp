#include "consumer.h"

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
