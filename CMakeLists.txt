cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
set(module_name juce_bluetooth)

project(${module_name} VERSION 0.1.0)
include(cmake/fetch_from_git.cmake)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    fetch_from_git(JUCE https://github.com/juce-framework/JUCE.git)
endif()

juce_add_module(${module_name})

target_include_directories(${module_name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(genki::bluetooth ALIAS ${module_name})

option(JUCE_BLUETOOTH_BUILD_EXAMPLES OFF "Enable juce_bluetooth example builds")

if (JUCE_BLUETOOTH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()