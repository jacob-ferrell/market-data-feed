#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include "constants.h"
#include "messages.h"
#include "ticker_registry.h"
#include "order_book.h"
#include "../tools/timestamp_utils.h"


flat_hash_map live_orders = flat_hash_map{};

anomalies anomaly_counts = anomalies{};

uint32_t last_trade_prices[NUM_INSTRUMENTS];

price_list_t price_list;

book_t book;

books_t books;

ring_buffers_t ring_buffers;

book_t& ticker_book(const char* ticker) {
    return books[ticker_index(ticker)];
}

bool apply_delta_to_level(
    price_list_t& list,
    int64_t delta,
    uint32_t price
) {

    for (int i = 0; i < MAX_LEVELS_PER_SIDE; i++) {

        price_level& cur_level = list[i];

        if (cur_level.price == price) {

            assert(
                delta >= 0 
                   || std::abs(delta) <= (int64_t)cur_level.quantity
                   && "applying delta would result in underflow"
            );

            cur_level.quantity += delta;
            return true;
        }

    }

    return false;

}

void insert_level(
    int i,
    const price_level& level, 
    price_list_t& list
) {
    price_level tmp = list[i];

    for (int j = i + 1; j < MAX_LEVELS_PER_SIDE; j++) {
        price_level tmp2 = list[j];
        list[j] = tmp;
        tmp = tmp2;
    }

    list[i] = level;
    
}

template<typename Cmp>
bool add_level(
    const price_level& level, 
    price_list_t& list,
    Cmp cmp
) {

    for (int i = 0; i < MAX_LEVELS_PER_SIDE; i++) {

        price_level& cur = list[i];

        if (level.price == cur.price) {
            cur.quantity += level.quantity;
            return false;
        }

        if (cmp(level.price, cur.price) || cur.price == 0) {

            insert_level(
                i,
                level,
                list
            ); 

            return true;

        }

    }

    assert(!"add_bid: unreachable");

}

bool process_add(const add_order_msg& order, price_list_t& list) {

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

    price_level level = {
        order.quantity,
        order.price,
    };

    std::function<bool(std::uint32_t, std::uint32_t)> cmp;

    switch (order.side) {
        case BID: cmp = std::greater<uint32_t>(); break;
        case ASK: cmp = std::less<uint32_t>(); break;
        default: assert(!"Invalid side");
    }

    return add_level(level, list, cmp);
}

bool process_cancel(const cancel_order_msg& order, book_t& book) {

    live_order* existing_order = live_orders.get(order.order_id);

    if (!existing_order) {
        anomaly_counts.cancel++;
        return false;
    }

    int64_t quantity_delta = -((int64_t)existing_order->quantity);

    bool delta_applied = apply_delta_to_level(
        book[existing_order->side],
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

bool process_modify(const modify_order_msg& order, book_t& book) {

    live_order* existing_order = live_orders.get(order.order_id);

    if (!existing_order) {
        anomaly_counts.modify++;
        return false;
    }


    int64_t quantity_delta = order.quantity - existing_order->quantity;

    bool delta_applied = apply_delta_to_level(
        book[existing_order->side],
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

bool process_trade(const trade_msg& order, book_t& book, uint32_t& last_trade_price) {

    live_order* bid_order = live_orders.get(order.bid_order_id);

    live_order* ask_order = live_orders.get(order.ask_order_id);

    if (!bid_order || !ask_order) {
        anomaly_counts.trade++;
        return false;
    }

    assert(bid_order->price == ask_order->price);

    int64_t quantity_delta = -((int64_t)std::min(bid_order->quantity, ask_order->quantity));

    bool bid_delta_applied = apply_delta_to_level(
        book[BID],
        quantity_delta,
        bid_order->price
    );

    if (!bid_delta_applied) {
        anomaly_counts.trade++;
        return false;
    }

    bool ask_delta_applied = apply_delta_to_level(
        book[ASK],
        quantity_delta,
        ask_order->price
    );

    assert(ask_delta_applied);

    uint32_t price = bid_order->price;

    live_orders.apply_trade(*bid_order, *ask_order, quantity_delta);

    last_trade_price = price;

    return true;

}

snapshot get_book_snapshot(const book_t& book, const int& ticker_idx) {
    price_level best_bid = book[BID][0];
    price_level best_ask = book[ASK][0];

    snapshot out = {
        .timestamp = get_timestamp_ms(),
        .best_bid_quantity = best_bid.quantity,
        .best_ask_quantity = best_ask.quantity,
        .best_bid_price = best_bid.price,
        .best_ask_price = best_ask.price,
        .last_trade_price = last_trade_prices[ticker_idx],
        .instrument_idx = (uint32_t) ticker_idx
    };

    return out;

}

bool publish_snapshot(const book_t& book, const int& ticker_idx) {
    snapshot ss = get_book_snapshot(book, ticker_idx);    

    return ring_buffers[ticker_idx].try_push(ss);

}
 
bool process_message(const message& msg) {

    book_t* book = nullptr;
    int ticker_idx;
    bool success;

    switch (msg.type) {
        case ADD: {
            ticker_idx = ticker_index(msg.add.ticker);
            book = &books[ticker_idx];
            success = process_add(msg.add, (*book)[msg.add.side]);
            break;
        }

        case CANCEL: {
            ticker_idx = ticker_index(msg.cancel.ticker);
            book = &books[ticker_idx];
            success = process_cancel(msg.cancel, *book);
            break;
        }
        case MODIFY: {
            ticker_idx = ticker_index(msg.modify.ticker);
            book = &books[ticker_idx];
            success = process_modify(msg.modify, *book);
            break;
        }
        case TRADE: {
            ticker_idx = ticker_index(msg.trade.ticker);
            book = &books[ticker_idx];
            success = process_trade(msg.trade, *book, last_trade_prices[ticker_idx]);
            break;
        }

        default: assert(!"process_message unreachable");

    }

    publish_snapshot(*book, ticker_idx);    
    
    return success;

}

