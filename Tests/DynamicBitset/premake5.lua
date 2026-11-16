project "DynamicBitset"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"
    exceptionhandling "off"
    characterset "Unicode"
 
    targetdir ("%{wks.location}/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}")
    objdir    ("%{wks.location}/bin-int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}")
 
    files { "**.hpp", "**.cpp" }
 
    uses  { "Core", "GoogleTest" }
    links { "Core", "GoogleTest" }
 
    filter "options:link=shared"
        staticruntime "off"
        postbuildcommands { "{COPYFILE} \"%{wks.location}/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Core/Core.dll\" \"%{cfg.targetdir}\"" }
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