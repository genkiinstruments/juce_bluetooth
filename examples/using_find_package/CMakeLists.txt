cmake_minimum_required(VERSION 3.17)

project(juce_ble_example_using_find_package VERSION 1.0.0)

include (../../cmake/CPM.cmake)

# juce_bluetooth depends on JUCE and these libraries and they need to be present
CPMAddPackage("gh:microsoft/GSL#v4.0.0")
CPMAddPackage("gh:fmtlib/fmt#10.1.1")
CPMAddPackage("gh:ericniebler/range-v3#0.12.0")

find_package(juce_bluetooth CONFIG REQUIRED)

juce_add_console_app(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.cpp)

target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_SILENCE_XCODE_15_LINKER_WARNING
        )

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        genki::bluetooth

        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        )
