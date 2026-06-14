
#include <cstdint>

inline double cycles_per_ns = 0.0;
 
inline uint64_t start_wall_ns = 0;

inline uint64_t start_cycles = 0;

inline uint64_t rdtsc() {
      uint32_t lo, hi;
      __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
      return ((uint64_t)hi << 32) | lo;
}

uint64_t get_timestamp_ms();

void calibrate_cycles_per_ns();

uint64_t calc_rdtsc_latency_in_ns(uint64_t start, uint64_t end);
