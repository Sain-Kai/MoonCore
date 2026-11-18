#pragma once
#include "MoonCore/Platform/Base/PlatformMacros.hpp"

// Bitwise operations
#define MOON_BIT(x) static_cast<u64>(1 << (x))
#define MOON_IS_POWER_OF_2(x) x && !((x) & ((x) - 1))

// Attributes
#define MOON_DEPRECATED           [[deprecated]]
#define MOON_DEPRECATED_MSG(text) [[deprecated(text)]]

#define MOON_NODISCARD           [[nodiscard]]
#define MOON_NODISCARD_MSG(text) [[nodiscard(text)]]

#define MOON_MAYBE_UNUSED [[maybe_unused]]
#define MOON_NORETURN     [[noreturn]]

#define MOON_GB(bytes) static_cast<usize>((bytes) * MOON_BIT(30))
#define MOON_MB(bytes) static_cast<usize>((bytes) * MOON_BIT(20))
#define MOON_KB(bytes) static_cast<usize>((bytes) * MOON_BIT(10))