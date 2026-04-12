
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include "order_book.h"
#include "flat_hash_map.h"
#include "messages.h"

static const uint8_t MAX_PRICES_PER_SIDE = 8;

static const char* TICKER = "AAPL";

static flat_hash_map live_orders = flat_hash_map{};

static anomalies anomaly_counts = anomalies{};

static std::array<std::array<price_list_line, MAX_PRICES_PER_SIDE>, 2> price_list;

bool apply_delta_to_price_list_line(
    uint8_t side,
    int64_t delta,
    uint32_t price
) {

    uint8_t price_list_idx;

    if (side == BID) {
        price_list_idx = 0;
    } else {
        price_list_idx = 1;
    }

    for (int i = 0; i < MAX_PRICES_PER_SIDE; i++) {

        price_list_line* cur_line = &price_list[price_list_idx][i];

        if (cur_line->price == price) {

            assert(
                delta >= 0 
                   || std::abs(delta) < (int64_t)cur_line->quantity
                   && "applying delta would result in underflow"
            );

            cur_line->quantity += delta;
            return true;
        }

    }

    return false;

}

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

bool add_line(
    const price_list_line& line, 
    std::array<price_list_line, MAX_PRICES_PER_SIDE>& list,
    std::function<bool(std::uint32_t, std::uint32_t)> cmp
) {

    for (int i = 0; i < MAX_PRICES_PER_SIDE; i++) {
        price_list_line* cur = &list[i];

        if (line.price == cur->price) {
            cur->quantity += line.quantity;
            return false;
        }

        if (cmp(line.price, cur->price) || cur->price == 0) {

            insert_line(
                i,
                line,
                list
            ); 

            return true;

        }

    }

    assert(!"add_bid: unreachable");

}

bool process_add(const add_order_msg& order) {

    if (strcmp(order.ticker, TICKER)) {
        return false;    
    }

    live_order lo = {
        order.order_id,
        order.price,
        order.quantity,
        order.side
    };

    if (!live_orders.insert(lo)) {
        anomaly_counts.add++;
        return false;
    }

    price_list_line line = {
        order.price,
        order.quantity,
    };

    switch (order.side) {
        case BID: { return add_line(line, price_list[0], std::greater<uint32_t>()); }
        case ASK: { return add_line(line, price_list[1], std::less<uint32_t>()); }
    }

    assert(!"process_add: unreachable");

}

bool process_cancel(const cancel_order_msg& order) {

    if (strcmp(order.ticker, TICKER)) {
            return false;    
    }

    live_order* existing_order = live_orders.get(order.order_id);

    if (!existing_order) {
        anomaly_counts.cancel++;
        return false;
    }

    int64_t quantity_delta = -((int64_t)existing_order->quantity);

    bool delta_applied = apply_delta_to_price_list_line(
        existing_order->side, 
        quantity_delta, 
        existing_order->price
    );

    if (delta_applied) {
        live_orders.remove(existing_order->order_id);
    } else {
        anomaly_counts.cancel++;
    }

    return delta_applied;
     
}

bool process_modify(const modify_order_msg& order) {

    if (strcmp(order.ticker, TICKER)) {
        return false;
    }

    live_order* existing_order = live_orders.get(order.order_id);

    if (!existing_order) {
        anomaly_counts.modify++;
        return false;
    }

    int64_t quantity_delta = order.quantity - existing_order->quantity;

    bool delta_applied = apply_delta_to_price_list_line(
        existing_order->side, 
        quantity_delta, 
        existing_order->price
    );

    if (delta_applied) {
        existing_order->quantity = order.quantity;
    } else {
        anomaly_counts.modify++;
    }

    return delta_applied;

}

bool process_trade(const trade_msg& order) {

    if (strcmp(order.ticker, TICKER)) {
        return false;    
    }

    int64_t quantity_delta = -((int64_t)order.quantity);

    bool bid_delta_applied = apply_delta_to_price_list_line(
        BID,
        quantity_delta,
        order.price
    );

    if (!bid_delta_applied) {
        anomaly_counts.trade++;
        return false;
    }

    bool ask_delta_applied = apply_delta_to_price_list_line(
        ASK,
        quantity_delta,
        order.price
    );

    if (!ask_delta_applied) {
        anomaly_counts.trade++;
    }

    return ask_delta_applied;

}

bool process_message(const message* msg) {

    switch (msg->type) {
        case ADD: return process_add(msg->add);
        case CANCEL: return process_cancel(msg->cancel);
        case MODIFY: return process_modify(msg->modify);
        case TRADE: return process_trade(msg->trade);
    }

    assert(!"process_message unreachable");
    
}




 
