#include "./timestamp_utils.h"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>

uint64_t now_ns() {
    uint64_t elapsed_cycles = rdtsc();
    return start_wall_ns + calc_rdtsc_latency_in_ns(start_cycles, elapsed_cycles);

}

uint64_t get_timestamp_ms() {
    assert(start_wall_ns > 0 && start_cycles > 0);
    return now_ns() / 1000000;
}

uint64_t calc_rdtsc_latency_in_ns(uint64_t start, uint64_t end) {
    assert(cycles_per_ns > 0);
    return (end - start) / cycles_per_ns;
}

void calibrate_cycles_per_ns() {
    // timespec holds a time split into whole seconds and nanoseconds
    struct timespec ts;

    // read the current wall clock time into ts
    clock_gettime(CLOCK_MONOTONIC, &ts);

    // convert to a single nansecond value for easier arithmetic
    uint64_t wall_start = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    start_wall_ns = wall_start;

    // record the CPU cycle counter at the same moment
    uint64_t rdtsc_start = rdtsc();

    start_cycles = rdtsc_start;

    // spin until 10ms of wall clock time has elapsed
    do {
    clock_gettime(CLOCK_MONOTONIC, &ts);
    } while ((ts.tv_sec * 1000000000ULL + ts.tv_nsec) - wall_start < 10000000ULL);

    // record the cycle counter again after the interval
    uint64_t rdtsc_end = rdtsc();

    // record the final wall clock time
    uint64_t wall_end = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    // cycles elapsed divided by nanoseconds elapsed = cycles per nanosecond
    cycles_per_ns = (double)(rdtsc_end - rdtsc_start) / (double)(wall_end - wall_start);

  }
