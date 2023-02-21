#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace genki {

inline void message(juce::ValueTree receiver, const juce::ValueTree& message)
{
    receiver.appendChild(message, nullptr);
    receiver.removeChild(message, nullptr);
}

inline void message(juce::ValueTree receiver, const juce::Identifier& id)
{
    message(std::move(receiver), {id, {}});
}

} // namespace genki
