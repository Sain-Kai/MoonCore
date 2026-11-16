#include "Async/Thread.hpp"
#include "Async/ThreadPool.hpp"

namespace MC::Async
{
    thread_local CurrentThread::ThreadData CurrentThread::t_Data;

    u64 Details::ThreadBase::ThreadId() const noexcept
    {
        return m_ThreadData ? m_ThreadData->ThreadId : INVALID_THREAD;
    }

    bool Details::ThreadBase::RequestStop() const noexcept
    {
        return m_ThreadData && !m_ThreadData->StopRequested.exchange(true);
    }

    Thread::~Thread() noexcept
    {
        if (Joinable()) Join();
    }

#if !defined(PLATFORM_WINDOWS)
    void CurrentThread::Yield() noexcept {}
    void CurrentThread::Sleep(Clock::Timestep duration) noexcept {}
    void CurrentThread::SleepUntil(Clock::Timepoint timepoint) noexcept {}
    void CurrentThread::SleepUntil(Clock::Timepoint timepoint) noexcept {}
    u64 CurrentThread::ID() noexcept { return static_cast<u64>(GetCurrentThreadId()); }

    Memory::Reference<ThreadBase> Details::ThreadBase::Create(
        const ThreadSpecification& specification, Callables::Callable<void()>&& function) noexcept
    {
        return nullptr;
    }
#endif
}