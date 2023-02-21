cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
set(module_name juce_bluetooth)

project(${module_name} VERSION 0.1.0)
include(cmake/fetch_from_git.cmake)

option(JUCE_SOURCE_DIR "JUCE source directory, if external" "")

if (DEFINED JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/juce)
else()
    include (cmake/fetch_from_git.cmake)
    fetch_from_git(JUCE https://github.com/juce-framework/JUCE.git)
endif()

fetch_from_git(gsl https://github.com/microsoft/GSL.git GIT_TAG main)
fetch_from_git(fmt https://github.com/fmtlib/fmt.git)
fetch_from_git(range-v3 https://github.com/ericniebler/range-v3.git)

juce_add_module(${module_name})

target_include_directories(${module_name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${module_name}
        INTERFACE
        GSL
        fmt
        range-v3
#        $<$<CXX_COMPILER_ID:MSVC>:WindowsApp.lib>
#        $<$<CXX_COMPILER_ID:MSVC>:cppwinrt>
        )

add_library(genki::bluetooth ALIAS ${module_name})

add_subdirectory(examples)