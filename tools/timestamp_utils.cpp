#include "./timestamp_utils.h"
#include <chrono>


uint64_t get_timestamp_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
