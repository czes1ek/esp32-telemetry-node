#pragma once

#include <cstddef>

#include "reading.h"
#include "ring_buffer.h"

namespace core {

constexpr size_t kReadingBufferCapacity = 64;

using ReadingBuffer = RingBuffer<Reading, kReadingBufferCapacity>;

}
