#include "spsc_ring_buffer.h"
#include <atomic>
#include <cstdint>
#include "messages.h"

// called by producer, owns tail 
bool ring_buffer::try_push(const snapshot& snapshot) {
    uint64_t t = get_tail_for_producer();
    uint64_t h = get_head_for_producer();

    if (t - h >= CAPACITY) return false;

    buffer[t % CAPACITY] = snapshot;

    tail.store(t + 1, std::memory_order_release);

    return true;

}

// called by consumer, owns head
bool ring_buffer::try_pop(snapshot& snapshot) {
    uint64_t h = get_head_for_consumer();
    uint64_t t = get_tail_for_consumer();

    if (h == t) return false;

    snapshot = buffer[h % CAPACITY];

    head.store(h + 1, std::memory_order_release);

    return true;

}

uint64_t ring_buffer::get_head_for_consumer() {
    return head.load(std::memory_order_relaxed);
}

uint64_t ring_buffer::get_tail_for_consumer() {
    return tail.load(std::memory_order_acquire);
}

uint64_t ring_buffer:: get_head_for_producer() {
    return head.load(std::memory_order_acquire);
}

uint64_t ring_buffer::get_tail_for_producer() {
    return tail.load(std::memory_order_relaxed);
}

