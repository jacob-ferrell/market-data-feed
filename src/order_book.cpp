
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include "messages.h"

typedef struct {
    std::uint32_t price;
    std::uint64_t quantity;
} price_list_line;

static const std::uint8_t MAX_PRICES_PER_SIDE = 8;

static const char* TICKER = "AAPL";

static std::array<std::array<price_list_line, MAX_PRICES_PER_SIDE>, 2> price_list;

int index_side(Side side) {
    return static_cast<int>(side);
}

// [4, 3, 1, 0]
// [0, 0, 0, 0]

void insert_line(
    int i,
    const price_list_line& line_to_insert, 
    std::array<price_list_line, MAX_PRICES_PER_SIDE>& list
) {
    price_list_line tmp = list[i];

    for (int j = i + 1; j < MAX_PRICES_PER_SIDE; j++) {
        price_list_line tmp2 = list[j];
        list[j] = tmp;
        tmp = tmp2;
    }

    list[i] = line_to_insert;
    
}

bool add_ask(
    const price_list_line& line, 
    std::array<price_list_line, MAX_PRICES_PER_SIDE>& list
) {
    std::uint32_t prev_price = list[0].price;

    for (int i = 0; i < MAX_PRICES_PER_SIDE; i++) {
        price_list_line* cur = &list[i];

        if (line.price == cur->price) {
            cur->quantity += line.quantity;
            return false;
        }

        if (line.price > prev_price) {

            insert_line(
                i,
                line,
                list
            ); 

            return true;

        }

        prev_price = cur->price;

    }

    assert(!"add_bid: unreachable");

}

bool add_order(
    const price_list_line& line, 
    std::array<price_list_line, MAX_PRICES_PER_SIDE>& list,
    std::function<bool(std::uint32_t, std::uint32_t)> cmp
) {
    std::uint32_t prev_price = list[0].price;

    for (int i = 0; i < MAX_PRICES_PER_SIDE; i++) {
        price_list_line* cur = &list[i];

        if (line.price == cur->price) {
            cur->quantity += line.quantity;
            return false;
        }

        if (cmp(line.price, prev_price)) {

            insert_line(
                i,
                line,
                list
            ); 

            return true;

        }

        prev_price = cur->price;

    }

    assert(!"add_bid: unreachable");

}

bool process_add(message* msg) {
    add_order_msg order = msg->add;

    if (!strcmp(order.ticker, TICKER)) {
        return false;    
    }

    price_list_line line = {
        order.price,
        order.quantity,
    };

    switch (order.side) {
        case BID: { return add_order(line, price_list[0], std::greater<uint32_t>()); }
        case ASK: { return add_order(line, price_list[1], std::less<uint32_t>()); }
    }

    assert(!"process_add: unreachable");

}

bool process_cancel(message* msg) {
if (!strcmp(msg->cancel.ticker, TICKER)) {
        return false;    
    }
}

bool process_modify(message* msg) {
if (!strcmp(msg->modify.ticker, TICKER)) {
        return false;    
    }
}

bool process_trade(message* msg) {
if (!strcmp(msg->trade.ticker, TICKER)) {
        return false;    
    }
}

bool process_message(message* msg) {
    char* ticker;

    switch (msg->type) {
        case ADD: ticker = msg->add.ticker; break;
        case CANCEL: ticker = msg->cancel.ticker; break;
        case MODIFY: ticker = msg->modify.ticker; break;
        case TRADE: ticker = msg->trade.ticker; break;
    }

    if (!strcmp(ticker, TICKER)) {
        return false;    
    }

    
    
}




 
