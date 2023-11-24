juce_add_console_app(juce_ble_example_hrs)
target_sources(juce_ble_example_hrs PRIVATE main.cpp)

target_compile_definitions(juce_ble_example_hrs
        PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
	JUCE_USE_CURL=0
	JUCE_WEB_BROWSER=0
        )

target_link_libraries(juce_ble_example_hrs
        PRIVATE
        genki::bluetooth

        PUBLIC
        GSL
        fmt
        range-v3

        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        )
