#pragma once
#include "Core.hpp"
#include "Clock/Time.hpp"

namespace MC::Clock
{
	class WindowsTimeSource final : public TimeSource
	{
	public:
		WindowsTimeSource() noexcept = default;

	public:
	    u64 GetTick() const noexcept override;
		u64 GetFrequency() const noexcept override;
		Date ConvertTickToDate(u64 tick) const noexcept override;
	    Date GetDate() const noexcept override;
	};
}