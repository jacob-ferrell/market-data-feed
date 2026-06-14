#pragma once

#include "messages.h"
#include "flat_hash_map.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include "constants.h"
#include "spsc_ring_buffer.h"
#include "ticker_registry.h"
#include "snapshot.h"

static const uint32_t LATENCIES_CAPACITY = 1000000;

using latencies_t = std::array<uint32_t, LATENCIES_CAPACITY>;

inline uint32_t latencies_size = 0;

extern latencies_t latencies;

typedef struct {
    std::uint64_t quantity;
    std::uint32_t price;
} price_level;

using price_list_t = std::array<price_level, MAX_LEVELS_PER_SIDE>;

extern price_list_t price_list;

extern uint32_t last_trade_prices[NUM_INSTRUMENTS];

using book_t = std::array<price_list_t, NUM_SIDES>;

extern book_t book;

using books_t = std::array<book_t, NUM_INSTRUMENTS>;

extern books_t books;

typedef struct {
    std::uint64_t add;
    std::uint64_t modify;
    std::uint64_t cancel;
    std::uint64_t trade;
} anomalies;

extern anomalies anomaly_counts;

extern flat_hash_map live_orders;

book_t& ticker_book(const char* instrument);

int ticker_index(const char* ticker);

bool process_message(const message& msg);

snapshot get_instrument_snapshot(const char* ticker);

