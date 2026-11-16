#pragma once
#include "Core.hpp"

namespace MC::Details
{
    void RequestShutdown(i32 exitCode) noexcept;
    void SetCoreTerminationHandler(const Utility::Debugging::TerminationHandler& handler) noexcept;
    Utility::Debugging::TerminationHandler GetCoreTerminationHandler() noexcept;
}