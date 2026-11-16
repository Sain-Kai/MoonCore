workspace "MoonCore"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "MoonCore"
    
    require "premake5"

    group "Dependencies"
        include "Tests"
    group "Tests"
        include "Tests/DynamicBitset"
    group ""