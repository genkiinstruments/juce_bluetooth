juce_add_console_app(scan)
target_sources(scan PRIVATE main.cpp)

target_compile_definitions(scan
        PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        )

target_link_libraries(scan
        PRIVATE
        genki::bluetooth

        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        )