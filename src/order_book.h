#pragma once

#include <cstdint>
#include "messages.h"

typedef struct {
    std::uint32_t price;
    std::uint64_t quantity;
} price_list_line;

typedef struct {
    std::uint64_t add;
    std::uint64_t modify;
    std::uint64_t cancel;
    std::uint64_t trade;
} anomalies;

typedef struct {
    std::uint64_t order_id;
    std::uint32_t price;
    std::uint32_t quantity;
    Side side;
} live_order;


bool process_message(const message* msg);

