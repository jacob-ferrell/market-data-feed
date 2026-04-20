#pragma once

#include <cstdint>

typedef enum { ADD, CANCEL, MODIFY, TRADE } MessageType;

typedef enum { BID, ASK, NUM_SIDES /* not a side */ } Side;

typedef struct {
    std::uint64_t order_id;
    std::uint32_t price;
    std::uint32_t quantity;
    Side side;
} live_order;


typedef struct {
    std::uint64_t order_id;
    std::uint64_t sequence_num;
    char ticker[8];
    std::uint64_t timestamp;
    std::uint32_t price;
    std::uint32_t quantity;
    Side side;
} add_order_msg;

static_assert(sizeof(add_order_msg) == 48, "add_order_msg size mismatch");

typedef struct {
    std::uint64_t order_id;
    std::uint64_t sequence_num;
    std::uint64_t timestamp;
    char ticker[8];
} cancel_order_msg;

static_assert(sizeof(cancel_order_msg) == 32, "cancel_order_msg size mismatch");

typedef struct {
    std::uint64_t order_id;
    std::uint64_t sequence_num;
    std::uint64_t timestamp;
    char ticker[8];
    std::uint32_t quantity;
} modify_order_msg;

static_assert(sizeof(modify_order_msg) == 40, "modify_order_msg size mismatch");

typedef struct {
    std::uint64_t sequence_num;
    std::uint64_t timestamp;
    std::uint64_t bid_order_id;
    std::uint64_t ask_order_id;
    char ticker[8];
} trade_msg;

static_assert(sizeof(trade_msg) == 40, "trade_msg size mismatch");

typedef struct {
    std::uint8_t type;

    union {
        add_order_msg add;
        cancel_order_msg cancel;
        modify_order_msg modify;
        trade_msg trade;
    };

} message;
