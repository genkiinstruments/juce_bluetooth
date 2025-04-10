#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <range/v3/view/facade.hpp>

namespace genki {

class ValueTreeRange : public ranges::view_facade<ValueTreeRange>
{
    friend ranges::range_access;

    juce::ValueTree               cur;
    [[nodiscard]] juce::ValueTree read() const { return cur; }

    [[nodiscard]] bool equal(ranges::default_sentinel_t) const { return !cur.isValid(); }
    void               next() { cur = cur.getSibling(1); }

public:
    explicit ValueTreeRange() = default;
    explicit ValueTreeRange(const juce::ValueTree& vt) : cur(vt.getChild(0))
    {
        assert(vt.isValid());
    }
};

inline std::string_view to_string_view(const juce::String& s)
{
    return {s.getCharPointer(), static_cast<size_t>(s.length())};
}

inline auto property(const juce::Identifier& id)
{
    return [id](const juce::ValueTree& vt)
    { return vt.getProperty(id); };
}

template<typename T>
inline auto property_as(const juce::Identifier& id)
{
    return [id](const juce::ValueTree& vt)
    { return static_cast<T>(vt.getProperty(id)); };
}

} // namespace genki