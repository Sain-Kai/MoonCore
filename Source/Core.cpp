#include "Core.hpp"

#include "CommandLine.hpp"
#include "Filesystem/Filesystem.hpp"
#include "Logging/Logger.hpp"
#include "Memory/MemoryTracker.hpp"
#include <Shutdown.hpp>

void SetupPlatform() noexcept;

extern "C" LIBRARY_EXPORT i32 InitializeCore(i32 argc, char7** argv)
{
	SetupPlatform();

    MC::Clock::Time::Initialize();
    MC::Logging::Logger::InitializeLogger();
    MC::Memory::MemoryTracker::Initialize();
    MC::CommandLine::Initialize(argc, argv);
    MC::Filesystem::Filesystem::Initialize();

	MC::String buffer;
	MC::Formatter::FormatTo(buffer, TEXT("{}"),
		MC::Filesystem::Filesystem::GetKnownFolder(MC::Filesystem::KnownFolder::Temp));
	MC::Filesystem::DirectoryStorage::StoreDirectory("Temp", buffer);

	MC::Formatter::FormatTo(buffer, TEXT("{}"),
		MC::Filesystem::Filesystem::GetKnownFolder(MC::Filesystem::KnownFolder::AppData));
	MC::Filesystem::DirectoryStorage::StoreDirectory("AppData", buffer);

	MC::Formatter::FormatTo(buffer, TEXT("{}"),
		MC::Filesystem::Filesystem::GetKnownFolder(MC::Filesystem::KnownFolder::UserHome));
	MC::Filesystem::DirectoryStorage::StoreDirectory("UserHome", buffer);

	MC::Formatter::FormatTo(buffer, TEXT("{}"),
		MC::Filesystem::Filesystem::GetKnownFolder(MC::Filesystem::KnownFolder::Documents));
	MC::Filesystem::DirectoryStorage::StoreDirectory("Documents", buffer);

	MC::Formatter::FormatTo(buffer, TEXT("{}"),
		MC::Filesystem::Filesystem::GetKnownFolder(MC::Filesystem::KnownFolder::ExecutableDir));
	MC::Filesystem::DirectoryStorage::StoreDirectory("Executable", buffer);

	MC::Formatter::FormatTo(buffer, TEXT("{}"),
		MC::Filesystem::Filesystem::GetKnownFolder(MC::Filesystem::KnownFolder::WorkingDir));
	MC::Filesystem::DirectoryStorage::StoreDirectory("Working", buffer);

    return 0;
}

extern "C" LIBRARY_EXPORT i32 ShutdownCore()
{
    MC::Memory::MemoryTracker::Shutdown();
    MC::Logging::Logger::ShutdownLogger();
    MC::Clock::Time::Shutdown();
	return 0;
}

#if defined(PLATFORM_WINDOWS)
#undef TEXT
#include <Windows.h>

inline BOOL WINAPI TerminationHandler(DWORD signal) noexcept
{
    switch (signal)
    {
        case CTRL_CLOSE_EVENT:
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
			MC::Details::RequestShutdown(0);
			return TRUE;
        default: return FALSE;
    }
}

inline void SetupPlatform() noexcept
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	SetConsoleCtrlHandler(TerminationHandler, TRUE);
}
#endif