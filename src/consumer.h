#pragma once

#include <atomic>
#include "constants.h"
#include "snapshot.h"
#include "spsc_ring_buffer.h"

void consumer_fun(const std::atomic<bool>& running);
