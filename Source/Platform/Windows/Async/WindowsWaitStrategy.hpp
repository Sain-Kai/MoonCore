#pragma once
#include "Async/Details/WaitStrategy.inl"

namespace MC::Windows::Async
{
    class WindowsWaitStrategy final : public MC::Async::Details::WaitStrategy
    {
    public:
        bool WaitImpl(volatile void* waitable, void* comparable, usize size, u32 timeout) noexcept  override;
        void NotifyImpl(void* waitable) noexcept override;
        void NotifyAllImpl(void* waitable) noexcept override;
    };
}