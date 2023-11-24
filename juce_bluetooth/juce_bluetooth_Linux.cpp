#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_LINUX

#include "juce_bluetooth.h"

using namespace juce;

namespace genki {

struct BleAdapter::Impl : private ValueTree::Listener
{
    explicit Impl(ValueTree);
    ~Impl() override;

    juce::ValueTree valueTree;
};

BleAdapter::Impl::Impl(ValueTree vt) : valueTree(std::move(vt)) { jassertfalse; }

BleAdapter::Impl::~Impl() { jassertfalse; }

//======================================================================================================================
BleAdapter::BleAdapter(ValueTree::Listener& l)
{
    state.addListener(&l);
    impl = std::make_unique<Impl>(state);
}

BleAdapter::BleAdapter() : impl(std::make_unique<Impl>(state)) { startTimer(500); }

BleAdapter::~BleAdapter() = default;

BleDevice BleAdapter::connect(const ValueTree&, const BleDevice::Callbacks&) const
{
    jassertfalse;
    return BleDevice{};
}

void BleAdapter::disconnect(const BleDevice&)
{
    jassertfalse;
}

size_t BleAdapter::getMaximumValueLength(const BleDevice&)
{
    jassertfalse;
    return 0;
}

//======================================================================================================================
void BleDevice::write(BleAdapter&, const Uuid&, gsl::span<const gsl::byte>, bool)
{
    jassertfalse;
}

} // namespace genki

#endif // JUCE_LINUX
