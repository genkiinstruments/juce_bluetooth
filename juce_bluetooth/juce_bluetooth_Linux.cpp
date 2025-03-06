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
            auto device = parent;
            jassert(device.hasType(ID::BLUETOOTH_DEVICE));

            if (auto it = connections.find(device.getProperty(ID::address).toString()); it != connections.end())
            {
                gattlib_connection_t* connection = it->second.first;

                int services_count = 0; // gattlib_discover_primary will update this...
                gattlib_primary_service_t* services = nullptr; // gattlib_discover_primary will allocate this :shiver:

                const auto ret = gattlib_discover_primary(connection, &services, &services_count);
                jassert(ret == GATTLIB_SUCCESS);

        	    for (int i = 0; i < services_count; i++) {
                    std::array<char, MAX_LEN_UUID_STR + 1> uuid_str{};

                    const auto uuid_str_ret = gattlib_uuid_to_string(&services[i].uuid, uuid_str.data(), uuid_str.size());
                    jassert(uuid_str_ret == GATTLIB_SUCCESS);

                    // TODO: attr_handle_start/attr_handle_end
                    device.appendChild({ID::SERVICE, {{ID::uuid, juce::String(uuid_str.data())}}, {}}, nullptr);
                }

                free(services); // WATCH OUT

                genki::message(device, ID::SERVICES_DISCOVERED);
            }
        }
        if (child.hasType(ID::DISCOVER_CHARACTERISTICS))
        {
            const auto device = getAncestor(child, ID::BLUETOOTH_DEVICE);
            jassert(device.isValid());

            auto service = parent;
            jassert(service.hasType(ID::SERVICE));

            if (auto it = connections.find(device.getProperty(ID::address).toString()); it != connections.end())
            {
                gattlib_connection_t* connection = it->second.first;

                int characteristics_count = 0; // gattlib_discover_char will update this...
                gattlib_characteristic_t* characteristics = nullptr; // gattlib_discover_char will allocate this :shiver:

                const auto ret = gattlib_discover_char(connection, &characteristics, &characteristics_count);
                jassert(ret == GATTLIB_SUCCESS);

                for (int i = 0; i < characteristics_count; i++) {
                    std::array<char, MAX_LEN_UUID_STR + 1> uuid_str{};

                    const auto uuid_str_ret = gattlib_uuid_to_string(&characteristics[i].uuid, uuid_str.data(), uuid_str.size());
                    jassert(uuid_str_ret == GATTLIB_SUCCESS);

                    // TODO: properties/handle/value_handle
                    service.appendChild({ID::CHARACTERISTIC, {{ID::uuid, juce::String(uuid_str.data())}}, {}}, nullptr);
                }

                free(characteristics); // WATCH OUT
            }
        }
        else if (child.hasType(ID::ENABLE_NOTIFICATIONS))
        {
            jassert(parent.hasType(ID::CHARACTERISTIC));

            jassertfalse;
        }
        else if (child.hasType(ID::ENABLE_INDICATIONS))
        {
            jassert(parent.hasType(ID::CHARACTERISTIC));

            jassertfalse;
        }
        else if (child.hasType(ID::SCAN))
        {
            if (child.getProperty(ID::should_start))
            {
                LOG("Bluetooth - Starting scan...");

                // TODO: One callback per device is fired, need to use bluez through D-Bus directly to get RSSI updates during discovery
                const auto discovered_device_cb = [](gattlib_adapter_t*, const char* addr, const char* name, void* user_data) {
                    jassert(user_data != nullptr);
                    reinterpret_cast<Impl*>(user_data)->deviceDiscovered(addr ? addr : "", name ? name : "");
                };

                // TODO: Filters
                const auto ret = gattlib_adapter_scan_enable_with_filter_non_blocking(
                        adapter,
                        nullptr,// uuid_list
                        0,      // rssi_threshold
                        0,      // enabled_filters
                        discovered_device_cb,
                        0, // timeout, 0 means indefinite scan
                        reinterpret_cast<void*>(this) // :shrug:
                );

                jassert(ret == GATTLIB_SUCCESS);
            }
            else
            {
                LOG("Bluetooth - Stopping scan...");

                const auto ret = gattlib_adapter_scan_disable(adapter);
                jassert(ret == GATTLIB_SUCCESS);
            }
        }
        else if (child.hasType(ID::BLUETOOTH_DEVICE))
        {
            LOG(fmt::format("Bluetooth - Device added: {} ({}), {}",
                            child.getProperty(ID::name).toString(),
                            child.getProperty(ID::address).toString(),
                            child.getProperty(ID::is_connected) ? "connected" : "not connected"));
        }
    }

    void connect(const juce::ValueTree& device, const BleDevice::Callbacks& callbacks)
    {
        const auto on_device_connect = [](gattlib_adapter_t*, const char* addr, gattlib_connection_t* connection, int error, void* user_data)
        {
            reinterpret_cast<Impl*>(user_data)->deviceConnected(addr, connection, error);
        };

        const auto addr = device.getProperty(ID::address).toString();
        connections.insert({addr, {nullptr, callbacks}});

        const auto ret = gattlib_connect(
            adapter,
            addr.getCharPointer(),
            GATTLIB_CONNECTION_OPTIONS_NONE,
            on_device_connect,
            this
        );

        jassert(ret == GATTLIB_SUCCESS);
    }

    void deviceConnected(std::string_view addr, gattlib_connection_t* connection, int error)
    {
        if (error != GATTLIB_SUCCESS)
        {
            LOG(fmt::format("Bluetooth - Failed to connect to device: {}, error: {}", addr.data(), error));
            connections.erase(juce::String(addr.data()));

            return;
        }

        const auto addr_str = juce::String(addr.data());

        connections.at(addr_str).first = connection;

        const auto handler = [](gattlib_connection_t* conn, void* user_data)
        {
            reinterpret_cast<Impl*>(user_data)->deviceDisconnected(conn);
        };

        const auto ret = gattlib_register_on_disconnect(connection, handler, this);
        jassert(ret == GATTLIB_SUCCESS);

        if (auto ch = valueTree.getChildWithProperty(ID::address, addr_str); ch.isValid())
        {
            ch.setProperty(ID::is_connected, true, nullptr);
        }
    }

    void deviceDisconnected(gattlib_connection_t* connection)
    {
        const auto it = std::find_if(connections.begin(), connections.end(), [&](const auto& p) { return p.second.first == connection; });
        if (it != connections.end())
        {
            const auto& addr = it->first;
            connections.erase(it);

            if (auto ch = valueTree.getChildWithProperty(ID::address, addr); ch.isValid())
                valueTree.removeChild(ch, nullptr);
        }
    }

    void deviceDiscovered(std::string_view addr, std::string_view name)
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
    std::map<juce::String, std::pair<gattlib_connection_t*, genki::BleDevice::Callbacks>> connections;

    gattlib_adapter_t* adapter = nullptr;
};

BleAdapter::Impl::Impl(ValueTree vt) : valueTree(std::move(vt))
{
    const auto ret = gattlib_adapter_open(nullptr, &adapter);

    if (ret != GATTLIB_SUCCESS || adapter == nullptr)
    {
        LOG(fmt::format("Bluetooth - Failed to open default adapter: {}", ret));
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

BleDevice BleAdapter::connect(const ValueTree& device, const BleDevice::Callbacks& callbacks) const
{
    impl->connect(device, callbacks);

    return BleDevice(device);
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
