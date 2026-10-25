#ifndef RDEPARTURES_HAIKU_TYPES_H
#define RDEPARTURES_HAIKU_TYPES_H

#include <cstdint>

using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using int32 = std::int32_t;
using int64 = std::int64_t;
using bigtime_t = std::int64_t;
using status_t = std::int32_t;

constexpr status_t B_OK = 0;

#endif
