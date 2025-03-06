cmake_minimum_required(VERSION 3.17)

project(juce_ble_example_discover VERSION 1.0.0)

juce_add_console_app(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.cpp)

target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        )

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        genki::bluetooth

        PUBLIC
        GSL
        fmt
        range-v3

        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        )
