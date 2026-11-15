#pragma once
#include "Async/WaitStrategy.hpp"

namespace MC::Async
{
    MC::Memory::Scope<WaitStrategy> WaitStrategy::s_WaitStrategy = nullptr;
}
