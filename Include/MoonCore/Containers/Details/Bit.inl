#pragma once
#include "MoonCore/Core.hpp"

namespace MC::Details
{
#if defined(COMPILER_MSVC) 
    constexpr u64 PopCount(u64 x) noexcept { return __popcnt64(x); }
#endif
}