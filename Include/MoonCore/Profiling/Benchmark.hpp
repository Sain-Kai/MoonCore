#pragma once
#include "MoonCore/Clock/Time.hpp"
#include "MoonCore/Logging/Logger.hpp"

namespace MC::Profiling
{
    // TODO: Add future-like benchmark result structure for profiler output

    template<Concept::TimeUnit Unit, usize OutputPrecision = 4>
    class Benchmark
    {
    public:
        Benchmark(const MC::StringView& benchmarkName) noexcept
            : m_StartTime(Clock::Time::Now()), m_BenchmarkName(benchmarkName) 
        {}

        ~Benchmark() noexcept
        {
            Clock::Timestep duration = Clock::Time::Now() - m_StartTime;
            Logging::Logger::Debug(Logging::Logger::Module::Profiling, Logging::Logger::Severity::Info, TEXT("Benchmark '{0}' ended lasting {1:.{2}f}{3}"), 
                m_BenchmarkName, Unit::AsUnit(duration), OutputPrecision, Unit::UnitName);
        }

    private:
        Clock::Timepoint m_StartTime;
        MC::StringView m_BenchmarkName;
    };
}