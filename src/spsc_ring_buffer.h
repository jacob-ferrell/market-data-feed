#pragma once

#include "order_book.h"
#include "snapshot.h"
#include <array>
#include <atomic>
#include <cstdint>

class ring_buffer {

    static const int CAPACITY = 1024;

    std::array<snapshot, CAPACITY> buffer{};

    // owned by the consumer, this is used with reading or 'popping' snapshots
    alignas(64) std::atomic<uint64_t> head{0};

    // owned by the producer. this is used for writing or 'pushing' snapshots
    alignas(64) std::atomic<uint64_t> tail{0};

public:

    bool try_push(const snapshot& snapshot);

    bool try_pop(snapshot& snapshot);

private:

    uint64_t get_head_for_consumer();

    uint64_t get_head_for_producer();

    uint64_t get_tail_for_consumer();

    uint64_t get_tail_for_producer();

 };

using ring_buffers_t = std::array<ring_buffer, NUM_INSTRUMENTS>;

extern ring_buffers_t ring_buffers;


