#pragma once

#include <cstdint>

typedef struct {
    uint64_t timestamp;
    uint64_t best_bid_quantity;
    uint64_t best_ask_quantity;
    uint32_t best_bid_price;
    uint32_t best_ask_price;
    std::uint32_t last_trade_price;
    std::uint32_t instrument_idx;
} snapshot;

static_assert(sizeof(snapshot) == 40, "snapshot size mismatch");

