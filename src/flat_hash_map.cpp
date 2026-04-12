#include "flat_hash_map.h"
#include <sys/types.h>
#include <cstdint>

bool flat_hash_map::insert(const live_order& order) {

    live_order* existing_order = get(order.order_id);

    if (existing_order->order_id != 0) return false;

    existing_order->order_id = order.order_id;
    existing_order->side = order.side;
    existing_order->price = order.price;
    existing_order->quantity = order.quantity;

    size++;

    return true;

}

bool flat_hash_map::update(const uint64_t order_id, const uint32_t quantity) {

    live_order* order = get(order_id);

    if (!order) return false;

    order->quantity = quantity;

    return true;

}

bool flat_hash_map::remove(const uint64_t order_id) {

    live_order* order = get(order_id);

    if (!order) return false;

    *order = live_order{};

    size--;

    return true;


}

live_order* flat_hash_map::get(const std::uint64_t& order_id) {

    std::uint64_t hashed = hash(order_id);

    while (buckets[hashed].order_id != order_id && buckets[hashed].order_id != 0) {
        hashed = hash(hashed + 1);
    }

    if (buckets[hashed].order_id == 0) {
        return nullptr;
    }

    return &buckets[hashed];

}

std::uint64_t flat_hash_map::hash(const std::uint64_t& order_id) {
    return order_id & (capacity - 1);
}

bool remove(std::uint64_t order_id);


