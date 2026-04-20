#include "flat_hash_map.h"
#include <sys/types.h>
#include <cstdint>
#include "messages.h"

void flat_hash_map::apply_trade(live_order& bid, live_order& ask) {
    int64_t delta = -((int64_t)std::min(bid.quantity, ask.quantity));
    apply_trade(bid, ask, delta);
}

void flat_hash_map::apply_trade(live_order& bid, live_order& ask, int64_t delta) {
    bid.quantity += delta;
    ask.quantity += delta;

    if (bid.quantity == 0) remove(bid.order_id);
    if (ask.quantity == 0) remove(ask.order_id);
}

bool flat_hash_map::exists(const live_order& order) {
    return order.order_id != 0;
}

bool flat_hash_map::insert(const live_order& order) {

    live_order& existing_order = get_slot(order.order_id);

    if (exists(existing_order)) return false;

    existing_order.order_id = order.order_id;
    existing_order.side = order.side;
    existing_order.price = order.price;
    existing_order.quantity = order.quantity;

    size++;

    return true;

}

bool flat_hash_map::update(const uint64_t& order_id, const uint32_t& quantity) {

    live_order& order = get_slot(order_id);

    if (!exists(order)) return false;

    order.quantity = quantity;

    return true;

}

bool flat_hash_map::remove(const uint64_t& order_id) {

    live_order& order = get_slot(order_id);

    if (!exists(order)) return false;

    order = live_order{};

    size--;

    return true;

}

live_order& flat_hash_map::get_slot(const std::uint64_t& order_id) {

    std::uint64_t hashed = hash(order_id);

    while (buckets[hashed].order_id != order_id && buckets[hashed].order_id != 0) {
        hashed = hash(hashed + 1);
    }

    return buckets[hashed];

}


live_order* flat_hash_map::get(const std::uint64_t& order_id) {

    live_order& order = get_slot(order_id);

    if (exists(order)) return &order;

    return nullptr;

}

std::uint64_t flat_hash_map::hash(const uint64_t& order_id) {
    return order_id & (capacity - 1);
}


