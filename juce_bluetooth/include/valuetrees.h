#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace genki {

//======================================================================================================================
inline juce::ValueTree getAncestor(const juce::ValueTree& vt, const juce::Identifier& ancestorType)
{
    const auto& parent = vt.getParent();

    return !parent.isValid() ? juce::ValueTree{} : (parent.hasType(ancestorType) ? parent : getAncestor(vt.getParent(), ancestorType));
}

inline juce::ValueTree findChildWithProperty(const juce::ValueTree& vt, const juce::Identifier& id, const juce::var& value)
{
    for (const auto& ch: vt)
    {
        if (ch.hasProperty(id) && ch.getProperty(id) == value)
            return ch;

        if (const auto& res = findChildWithProperty(ch, id, value); res.isValid())
            return res;
    }

    return juce::ValueTree();
}

//======================================================================================================================
struct ValueTreeListener : juce::ValueTree::Listener
{
    explicit ValueTreeListener(juce::ValueTree vt)
        : valueTrees({std::move(vt)})
    {
        for (auto& v: valueTrees) v.addListener(this);
    }

    explicit ValueTreeListener(std::initializer_list<juce::ValueTree> vts)
        : valueTrees(vts)
    {
        for (auto& vt: valueTrees) vt.addListener(this);
    }

    void listen(const juce::ValueTree& vt, bool shouldListen = true)
    {
        if (shouldListen)
            valueTrees.emplace_back(vt);
        else
            valueTrees.erase(std::find(valueTrees.begin(), valueTrees.end(), vt));

        // ValueTrees might have been moved (if the vector has grown), but the listeners are bound to the ValueTree
        // object itself, not the underlying data. Will need to register as a listener again.
        for (auto& v: valueTrees)
            v.addListener(this);
    }

    std::function<void(juce::ValueTree&, const juce::Identifier&)> property_changed{};
    std::function<void(juce::ValueTree&, juce::ValueTree&)>        child_added{};
    std::function<void(juce::ValueTree&, juce::ValueTree&, int)>   child_removed{};

    void valueTreePropertyChanged(juce::ValueTree& vt, const juce::Identifier& id) override
    {
        if (property_changed) property_changed(vt, id);
    }
    void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override
    {
        if (child_added) child_added(parent, child);
    }
    void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int idx) override
    {
        if (child_removed) child_removed(parent, child, idx);
    }

    std::vector<juce::ValueTree> valueTrees;
};

} // namespace genki
