#pragma once
#include "MoonCore/Types.hpp"
#include "MoonCore/Utility/Format.hpp"

namespace MC
{
    struct Version
    {
        u16 Major = 0;
        u16 Minor = 0;
        u32 Patch = 0;
    };
}

MOON_FMT_FORMATTER(MC::Version, "{}.{}.{}", MOON_FMT_FORMATTER_MEMBER(Major), MOON_FMT_FORMATTER_MEMBER(Minor), MOON_FMT_FORMATTER_MEMBER(Patch));