#include "CommandLine.hpp"
#include "Shutdown.hpp"

static MC::Callables::Callable<void(i32)> s_ShutdownRequestHandler = nullptr;

void MC::Details::RequestShutdown(i32 exitCode) noexcept
{
    if (s_ShutdownRequestHandler) s_ShutdownRequestHandler(exitCode);
}

namespace MC
{
    Array<MC::String> CommandLine::s_Arguments = {};
    String CommandLine::s_CommandLine = TEXT("");
    String CommandLine::s_ProgramPath = TEXT("");

    StringView CommandLine::Get() noexcept
    {
        return s_CommandLine;
    }

    const Array<String>& CommandLine::GetArguments() noexcept
    {
        return s_Arguments;
    }

    MC::StringView CommandLine::GetProgramPath() noexcept
    {
        return s_ProgramPath;
    }

    bool CommandLine::Initialize(i32 argumentCount, char7** const arguments) noexcept
    {
        if (argumentCount <= 0) return false;

        s_ProgramPath = AsChar8(arguments[0]);
        s_Arguments.Reserve(argumentCount);

        for (i32 i = 1; i < argumentCount; ++i)
        {
            s_CommandLine += AsChar8(arguments[i]);
            s_Arguments.EmplaceBack(AsChar8(arguments[i]));
        }

        return true;
    }

    void CommandLine::SetShutdownRequestHandler(const Callables::Callable<void(i32 exitCode)>& handler) noexcept
    {
        s_ShutdownRequestHandler = handler;
    }
}
