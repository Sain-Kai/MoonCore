#include "Utility/Debug.hpp"
#include "Logging/Logger.hpp"
#include "Shutdown.hpp"

using namespace MC::Logging;

namespace MC::Utility::Debugging
{
    static TerminationHandler s_Handler = nullptr;
    static TerminationHandler s_CoreTerminationHandler = nullptr;

    void SetTerminationHandler(const TerminationHandler& handler) noexcept
    {
        s_Handler = handler;
    }

    TerminationHandler GetTerminationHandler() noexcept
    {
        return s_Handler;
    }

    void Details::TerminateRuntime(const char7* message, const SourceData& source) noexcept
    {
        Logger::Fatal(Logger::Module::Client, FormatString(message, source));
        if (s_Handler) s_Handler();
        if (s_CoreTerminationHandler) s_CoreTerminationHandler();
        abort();
    }
}

namespace MC::Details
{
    void SetCoreTerminationHandler(const Utility::Debugging::TerminationHandler& handler) noexcept
    {
        MC::Utility::Debugging::s_CoreTerminationHandler = handler;
    }

    Utility::Debugging::TerminationHandler GetCoreTerminationHandler() noexcept
    {
        return Utility::Debugging::s_CoreTerminationHandler;
    }
}