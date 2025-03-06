#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_LINUX

#include "juce_bluetooth.h"

#include <gattlib.h>

#include "format.h"

using namespace juce;

#ifndef GENKI_BLUETOOTH_LOG_ENABLED
#define GENKI_BLUETOOTH_LOG_ENABLED 1
#endif

#define LOG(text) JUCE_BLOCK_WITH_FORCED_SEMICOLON(if (GENKI_BLUETOOTH_LOG_ENABLED) DBG(text);)

namespace genki {

struct BleAdapter::Impl : private ValueTree::Listener {
    explicit Impl(ValueTree);
    ~Impl() override;

    //==================================================================================================================
    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override
    {
        if (child.hasType(ID::DISCOVER_SERVICES))
        {
            const auto& device = parent;
            jassert(device.hasType(ID::BLUETOOTH_DEVICE));

            jassertfalse;
        }
        if (child.hasType(ID::DISCOVER_CHARACTERISTICS))
        {
            const auto device = getAncestor(child, ID::BLUETOOTH_DEVICE);
            jassert(device.isValid());

            const auto& service = parent;
            jassert(service.hasType(ID::SERVICE));

            jassertfalse;
        }
        else if (child.hasType(ID::ENABLE_NOTIFICATIONS) || child.hasType(ID::ENABLE_INDICATIONS))
        {
            jassert(parent.hasType(ID::CHARACTERISTIC));

            jassertfalse;
        }
        else if (child.hasType(ID::SCAN))
        {
            if (child.getProperty(ID::should_start))
            {
                LOG("Bluetooth - Starting scan...");

                const auto discovered_device_cb = [](gattlib_adapter_t*, const char* addr, const char* name, void*) {
                    const std::string_view name_str = name ? name : "Unknown";
                    fmt::print("discovered_device_cb {} {}\n", addr, name_str);

                    // reinterpret_cast<Impl*>(user_data)->addPeripheral(addr, name);
                };

                // TODO: Filters
                const auto ret = gattlib_adapter_scan_enable_with_filter_non_blocking(
                        adapter,
                        nullptr,// uuid_list
                        0,      // rssi_threshold
                        0,      // enabled_filters
                        discovered_device_cb,
                        0,
                        reinterpret_cast<void*>(this));

                jassert(ret == GATTLIB_SUCCESS);
            }
            else
            {
                LOG("Bluetooth - Stopping scan...");
                jassertfalse;
            }
        }
        else if (child.hasType(ID::BLUETOOTH_DEVICE))
        {
            LOG(fmt::format("Bluetooth: Device added: {} ({}), {}",
                            child.getProperty(ID::name).toString(),
                            child.getProperty(ID::address).toString(),
                            child.getProperty(ID::is_connected) ? "connected" : "not connected"));
        }
    }

    void addPeripheral(std::string_view addr, std::string_view name)
    {
        const auto addr_str     = juce::String(addr.data());
        const auto name_str     = juce::String(name.data());
        const bool is_connected = false;// TODO: How to know?
        const auto rssi         = [&] {
            int16_t rssi_i16 = 0;
            gattlib_get_rssi_from_mac(adapter, addr.data(), &rssi_i16);
            return static_cast<int>(rssi_i16);
        }();

        const auto now = (int) juce::Time::getMillisecondCounter();

        if (auto ch = valueTree.getChildWithProperty(ID::address, addr_str); ch.isValid())
        {
            ch.setProperty(ID::rssi, rssi, nullptr);
            if (name_str.isNotEmpty())
                ch.setProperty(ID::name, name_str, nullptr);

            ch.setProperty(ID::last_seen, now, nullptr);
        }
        else
        {
            valueTree.appendChild({ID::BLUETOOTH_DEVICE, {{ID::name, name_str}, {ID::address, addr_str}, {ID::rssi, rssi}, {ID::is_connected, is_connected}, {ID::last_seen, now}}}, nullptr);
        }
    }

    //==================================================================================================================
    juce::ValueTree valueTree;

    juce::CriticalSection peripheralsLock;
    std::map<juce::String, std::pair<gattlib_connection_t*, genki::BleDevice::Callbacks>> peripherals;

    gattlib_adapter_t* adapter = nullptr;
};

BleAdapter::Impl::Impl(ValueTree vt) : valueTree(std::move(vt))
{
    const auto ret = gattlib_adapter_open(nullptr, &adapter);

    fmt::print("RET: {}\n", ret);

    if (ret != GATTLIB_SUCCESS || adapter == nullptr)
    {
        LOG(fmt::format("Failed to open default adapter - Error: {}", ret));
        valueTree.setProperty(ID::status, static_cast<int>(AdapterStatus::Disabled), nullptr);
    }
    else
    {
        valueTree.setProperty(ID::status, static_cast<int>(AdapterStatus::PoweredOn), nullptr);
        valueTree.setProperty(ID::name, gattlib_adapter_get_name(adapter), nullptr);
    }

    valueTree.addListener(this);
}

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

}// namespace genki

#endif// JUCE_LINUX
