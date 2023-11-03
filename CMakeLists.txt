cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
set(module_name juce_bluetooth)

project(${module_name} VERSION 1.0.0)
include(cmake/CPM.cmake)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    CPMAddPackage("gh:juce-framework/JUCE#7.0.7")
endif()

CPMAddPackage("gh:microsoft/GSL#v4.0.0")
CPMAddPackage("gh:fmtlib/fmt#10.1.1")
CPMAddPackage("gh:ericniebler/range-v3#0.12.0")

juce_add_module(${module_name})

target_include_directories(${module_name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(genki::bluetooth ALIAS ${module_name})

option(JUCE_BLUETOOTH_BUILD_EXAMPLES OFF "Enable juce_bluetooth example builds")

if (JUCE_BLUETOOTH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()