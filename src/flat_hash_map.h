#pragma once

#include <array>
#include <cstdint>
#include "messages.h"
#include "order_book.h"

class flat_hash_map {

    static const uint64_t MAX_SIZE = 16000000;

    uint64_t size = 0;

    uint64_t capacity = MAX_SIZE;

    std::array<live_order, MAX_SIZE> buckets{};

public:
    flat_hash_map() = default;

    flat_hash_map(const flat_hash_map&) = delete;
    flat_hash_map(flat_hash_map&&) = delete;
    flat_hash_map operator=(const flat_hash_map&) = delete;
    flat_hash_map operator=(flat_hash_map&&) = delete;

    bool insert(const live_order& live_order);
    bool update(const uint64_t order_id, const uint32_t quantity);
    bool remove(uint64_t order_id);
    live_order* get(const uint64_t& order_id);

private:
    uint64_t hash(const uint64_t& hash);

};
