juce_add_console_app(scan)
target_sources(scan PRIVATE main.cpp)

target_compile_definitions(scan
        PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        )

fetch_from_git(gsl https://github.com/microsoft/GSL.git GIT_TAG main)
fetch_from_git(fmt https://github.com/fmtlib/fmt.git)
fetch_from_git(range-v3 https://github.com/ericniebler/range-v3.git)

target_link_libraries(scan
        PRIVATE
        genki::bluetooth

        PUBLIC
        GSL
        fmt
        range-v3

        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        )