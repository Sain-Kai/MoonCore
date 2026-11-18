#pragma once
#include "MoonCore/Core.hpp"
#include "MoonCore/Callables/Callable.hpp"
#include "MoonCore/Containers/Array.hpp"
#include "MoonCore/Containers/String.hpp"

namespace MC
{
    class CommandLine
    {
    public:
        static MC::StringView Get() noexcept;
        static const Array<String>& GetArguments() noexcept;
        static MC::StringView GetProgramPath() noexcept;
        static bool Initialize(i32 argumentCount, char7** const arguments) noexcept;
        static void SetShutdownRequestHandler(const Callables::Callable<void(i32 exitCode)>& handler) noexcept;

    private:
        static Array<MC::String> s_Arguments;
        static MC::String s_CommandLine;
        static MC::String s_ProgramPath;
    };
}