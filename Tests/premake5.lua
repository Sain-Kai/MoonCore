project "GoogleTest"
    kind "StaticLib"
    language "C++"
    cppdialect "C++23"
    exceptionhandling "off"

    targetdir ("%{wks.location}/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}")
    objdir    ("%{wks.location}/bin-int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}")

    files
    {
        "%{wks.location}/Dependencies/googletest/googletest/src/gtest-all.cc",
        "%{wks.location}/Dependencies/googletest/googletest/src/gtest_main.cc",
    }

    includedirs
    {
        "%{wks.location}/Dependencies/googletest/googletest/include",
        "%{wks.location}/Dependencies/googletest/googletest"
    }

    defines { "GTEST_HAS_EXCEPTIONS=0" }

    filter "options:link=shared"
        staticruntime "off"
    filter "options:link=static"
        staticruntime "on"
    filter "configurations:Debug"
        symbols "on"
        runtime "Debug"
    filter "configurations:Release"
        optimize "on"
        runtime "Release"
    filter "action:vs*"
        buildoptions { "/utf-8" }
    filter {}

    usage "PUBLIC"
        includedirs { "%{wks.location}/Dependencies/googletest/googletest/include" }
        includedirs { "%{wks.location}/Tests/Shared" }
        defines { "GTEST_HAS_EXCEPTIONS=0" }