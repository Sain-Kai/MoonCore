#pragma once
#include "Async/Details/WaitStrategy.inl"

namespace MC::Async::Details
{
    MC::Memory::Scope<WaitStrategy> WaitStrategy::s_WaitStrategy = nullptr;
}
