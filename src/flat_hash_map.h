#pragma once

#include <array>
#include <cstdint>
#include "messages.h"

typedef struct {
    std::uint64_t order_id;
    std::uint32_t price;
    std::uint32_t quantity;
    Side side;
} live_order;

class flat_hash_map {

    // must be power of 2 for hash function to work properly
    static const uint64_t MAX_SIZE = 16777216;

    uint64_t size = 0;

    uint64_t capacity = MAX_SIZE;

    std::array<live_order, MAX_SIZE> buckets{};

public:
    flat_hash_map() = default;

    flat_hash_map(const flat_hash_map&) = delete;
    flat_hash_map(flat_hash_map&&) = delete;
    flat_hash_map operator=(const flat_hash_map&) = delete;
    flat_hash_map operator=(flat_hash_map&&) = delete;

    bool insert(const live_order& order);
    bool update(const uint64_t& order_id, const uint32_t& quantity);
    bool remove(const uint64_t& order_id);
    live_order* get(const uint64_t& order_id);

    void clear() {
        buckets.fill(live_order{});
        size = 0;
    };

    void apply_trade(live_order& bid, live_order& ask);
    void apply_trade(live_order& bid, live_order& ask, int64_t delta);

private:
    uint64_t hash(const uint64_t& hash);
    bool exists(const live_order& order);
    live_order& get_slot(const uint64_t& order_id);
};
