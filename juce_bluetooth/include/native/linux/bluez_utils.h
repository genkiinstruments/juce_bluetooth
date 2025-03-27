#pragma once

#include <juce_core/juce_core.h>

namespace genki::bluez_utils {

inline auto get_address_string(const char* addr) -> juce::String
{
    return juce::MACAddress(addr).toString();
}

inline auto get_native_address_string(const juce::String& str) -> juce::String
{
    return juce::MACAddress(str.replace("-:", "")).toString().replace("-", ":").toUpperCase();
}

} // namespace genki::gattlib_utils
