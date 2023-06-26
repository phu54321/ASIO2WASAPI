cmake_minimum_required(VERSION 3.22)
project(ASIO2WASAPI2)

set(CMAKE_CXX_STANDARD 14)

add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        VC_EXTRALEAN
)

#set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Debug mode")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Build-Debug)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    message("Release mode")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Build-Release)
else ()
    message(FATAL_ERROR "Error: Only Debug/Release mode supported")
endif ()


# Main driver
add_library(ASIO2WASAPI2 SHARED
        Source/Driver/ASIO2WASAPI2.cpp
        Source/Driver/dllmain.cpp
        Source/Driver/logger.cpp
        Source/Driver/register.cpp
        Source/Driver/TrayOpener.cpp
        Source/Driver/COMBaseClasses.cpp
        Source/Driver/ASIO2WASAPI2.rc
        Source/Driver/ASIO2WASAPI2.def
        )
target_include_directories(ASIO2WASAPI2 PRIVATE Source/ASIOSDK/common)
target_link_libraries(ASIO2WASAPI2 Avrt.lib winmm.lib)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # 64-bit mode
    message("x64 target")
    set_target_properties(ASIO2WASAPI2
            PROPERTIES
            OUTPUT_NAME "ASIO2WASAPI264"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/x64"
            )

else ()
    # 32-bit mode - add installer target
    add_executable(Install WIN32
            Source/Install/Install.cpp
            Source/Install/Install.rc
            )

    add_executable(Uninstall WIN32
            Source/Uninstall/Uninstall.cpp
            Source/Uninstall/Uninstall.rc
            )

    set_target_properties(Install Uninstall
            PROPERTIES
            LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")

endif ()
