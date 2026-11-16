#include "WindowsWaitStrategy.hpp"

#include "Memory/Pointers/Reference.hpp"
#include "Platform/Windows/WindowsInclude.hpp"

namespace MC::Windows::Async
{
    bool WindowsWaitStrategy::WaitImpl(volatile void* waitable, void* comparable, usize size, u32 timeout) noexcept
    {
        return WaitOnAddress(waitable, comparable, size, timeout) != FALSE;
    }

    void WindowsWaitStrategy::NotifyImpl(void* waitable) noexcept
    {
        WakeByAddressSingle(waitable);
    }

    void WindowsWaitStrategy::NotifyAllImpl(void* waitable) noexcept
    {
        WakeByAddressAll(waitable);
    }
}

namespace MC::Async::Details
{
    void WaitStrategy::Initialize() noexcept
    {
        s_WaitStrategy = Memory::MakeScope<Windows::Async::WindowsWaitStrategy>();
    }
}