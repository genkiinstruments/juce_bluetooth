#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_LINUX

#include "juce_bluetooth.h"

#include <glib.h>

#include "org-bluez-Adapter1.h"
#include "org-bluez-Device1.h"
#include "org-bluez-GattCharacteristic1.h"
#include "org-bluez-GattDescriptor1.h"
#include "org-bluez-GattService1.h"

#include <gsl/span>
#include <range/v3/view.hpp>

#include "format.h"
#include "ranges.h"
#include "native/linux/bluez_utils.h"

using namespace juce;

#ifndef GENKI_BLUETOOTH_LOG_ENABLED
#define GENKI_BLUETOOTH_LOG_ENABLED 1
#endif

#define LOG(text) JUCE_BLOCK_WITH_FORCED_SEMICOLON(if (GENKI_BLUETOOTH_LOG_ENABLED) { DBG(text); })

namespace genki {
//======================================================================================================================
struct LambdaTimer : public juce::Timer
{
    LambdaTimer(std::function<void()> cb, int intervalMs) : callback(std::move(cb)) { startTimer(intervalMs); }

    ~LambdaTimer() override { stopTimer(); }

    void timerCallback() override { if (callback) callback(); }

    std::function<void()> callback;
};

struct BleAdapter::Impl : private juce::ValueTree::Listener {
    explicit Impl(ValueTree);
    ~Impl() override;

    //==================================================================================================================
    void connect(const juce::ValueTree& device, const BleDevice::Callbacks& callbacks)
    {
        // const auto on_device_connect = [](gattlib_adapter_t*, const char* addr, gattlib_connection_t* connection, int error, void* user_data)
        // {
        //     reinterpret_cast<Impl*>(user_data)->deviceConnected(addr, connection, error);
        // };

        // const auto addr = device.getProperty(ID::address).toString();
        // {
        //     const juce::ScopedLock lock(connectionsLock);
        //     connections.insert({addr, {nullptr, callbacks}});
        // }

        // const auto native_addr = gattlib_utils::get_native_address_string(addr);

        // [[maybe_unused]] const auto ret = gattlib_connect(
        //     adapter,
        //     native_addr.getCharPointer(),
        //     GATTLIB_CONNECTION_OPTIONS_NONE,
        //     on_device_connect,
        //     this
        // );

        // jassert(ret == GATTLIB_SUCCESS);
    }

    void disconnect(const BleDevice& device)
    {
        // const auto addr = device.state.getProperty(ID::address).toString();

        // const juce::ScopedLock lock(connectionsLock);

        // if (const auto it = connections.find(addr); it != connections.end())
        // {
        //     LOG(fmt::format("Bluetooth - Disconnect device: {}", addr));

        //     [[maybe_unused]] const auto ret = gattlib_disconnect(it->second.first, false);
        //     jassert(ret == GATTLIB_SUCCESS);
        // }
    }

    void writeCharacteristic(const BleDevice& device, const juce::Uuid& charactUuid, gsl::span<const gsl::byte> data, bool withResponse)
    {
        // const auto addr = device.state.getProperty(ID::address).toString();

        // const juce::ScopedLock lock(connectionsLock);

        // if (const auto it = connections.find(addr); it != connections.end())
        // {
        //     const auto connection = it->second.first;

        //     uuid_t uuid = genki::gattlib_utils::from_juce_uuid(charactUuid);

        //     LOG("Bluetooth - INNER WRITE");

        //     // TODO: Can we get a characteristic_written callback?
        //     const auto write_func = withResponse ? &gattlib_write_char_by_uuid : &gattlib_write_without_response_char_by_uuid;

        //     [[maybe_unused]] const auto ret = write_func(
        //         connection,
        //         &uuid,
        //         data.data(),
        //         data.size()
        //     );

        //     jassert(ret == GATTLIB_SUCCESS);

        //     // Imitate the async-ness of the API
        //     juce::MessageManager::callAsync([this, charactUuid, success = ret == GATTLIB_SUCCESS] {
        //         if (auto h = handlers.find(charactUuid); h != handlers.end())
        //             h->second->characteristicWritten(charactUuid, success);
        //     });
        // }
    }

    //==================================================================================================================
    // void deviceConnected(std::string_view addr, gattlib_connection_t* connection, int error)
    // {
    //     if (error != GATTLIB_SUCCESS)
    //     {
    //         LOG(fmt::format("Bluetooth - Failed to connect to device: {}, error: {}", addr.data(), error));

    //         const juce::ScopedLock lock(connectionsLock);
    //         connections.erase(juce::String(addr.data()));

    //         return;
    //     }

    //     LOG(fmt::format("Bluetooth - Device connected: {}", addr.data()));

    //     const auto addr_str = gattlib_utils::get_address_string(addr.data());

    //     {
    //         const juce::ScopedLock lock(connectionsLock);
    //         connections.at(addr_str).first = connection;
    //     }

    //     const auto handler = [](gattlib_connection_t* conn, void* user_data)
    //     {
    //         reinterpret_cast<Impl*>(user_data)->deviceDisconnected(conn);
    //     };

    //     [[maybe_unused]] const auto ret = gattlib_register_on_disconnect(connection, handler, this);
    //     jassert(ret == GATTLIB_SUCCESS);

    //     juce::MessageManager::callAsync([=]
    //         {
    //             if (auto ch = valueTree.getChildWithProperty(ID::address, addr_str); ch.isValid())
    //             {
    //                 ch.setProperty(ID::is_connected, true, nullptr);
    //                 ch.setProperty(ID::max_pdu_size, 20, nullptr); // TODO: How to know?
    //             }
    //         });
    // }

    // void deviceDisconnected(gattlib_connection_t* connection)
    // {
    //     const juce::ScopedLock lock(connectionsLock);

    //     const auto it = std::find_if(connections.begin(), connections.end(), [&](const auto& p) { return p.second.first == connection; });

    //     if (it != connections.end())
    //     {
    //         const auto& addr = it->first;

    //         for (const auto& srv : valueTree.getChildWithProperty(ID::address, addr))
    //             if (srv.hasType(ID::SERVICE))
    //                 for (const auto& ch : srv)
    //                     if (ch.hasType(ID::CHARACTERISTIC))
    //                         handlers.erase(juce::Uuid(ch.getProperty(ID::uuid).toString()));

    //         connections.erase(it);

    //         juce::MessageManager::callAsync([=]
    //             {
    //                 if (auto ch = valueTree.getChildWithProperty(ID::address, addr); ch.isValid())
    //                     valueTree.removeChild(ch, nullptr);
    //             });
    //     }
    // }

    void deviceDiscovered(std::string_view addr, std::string_view name, bool is_connected = false)
    {
        const auto addr_str     = bluez_utils::get_address_string(addr.data());
        const auto name_str     = juce::String(name.data());
        // const auto rssi         = [&] {
        //     int16_t rssi_i16 = 0;
        //     gattlib_get_rssi_from_mac(adapter, addr.data(), &rssi_i16);
        //     return static_cast<int>(rssi_i16);
        // }();
        const auto rssi = 0;

        const auto now = (int) juce::Time::getMillisecondCounter();

        juce::MessageManager::callAsync([=]
            {
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
            });
    }

     void characteristicValueChanged(const juce::Uuid& uuid, gsl::span<const gsl::byte> value)
     {
         juce::MessageManager::callAsync([this, uuid, data = std::vector{value.begin(), value.end()}]
             {
                 LOG(fmt::format("Bluetooth - DATA: {}", gsl::as_bytes(gsl::make_span(data))));

                 // if (auto it = handlers.find(uuid); it != handlers.end())
                 // {
                 //     LOG("FOUND");
                 //     it->second->valueChanged(uuid, gsl::as_bytes(gsl::make_span(data)));
                 // }
             });
     }

     void characteristicWritten(const juce::Uuid& uuid, bool success)
     {
         juce::MessageManager::callAsync([this, uuid, success]
             {
                 // if (auto it = handlers.find(uuid); it != handlers.end())
                 //    it->second->characteristicWritten(uuid, success);
             });
     }

     // void notificationStateChanged(const gattlib_connection_t* connection, uint16_t handle, bool is_notifying)
     // {
     //     const juce::ScopedLock lock(connectionsLock);

     //     const auto it = std::find_if(connections.begin(), connections.end(), [&](const auto& p) { return p.second.first == connection; });

     //     if (it != connections.end())
     //     {
     //         const auto& addr = it->first;

     //         for (const auto& srv : valueTree.getChildWithProperty(ID::address, addr))
     //             if (srv.hasType(ID::SERVICE))
     //                 for (const auto& ch : srv)
     //                     if (ch.hasType(ID::CHARACTERISTIC) && static_cast<int>(ch.getProperty(ID::handle)) == handle)
     //                         juce::MessageManager::callAsync([=]
     //                             {
     //                                 genki::message(ch, {ID::NOTIFICATIONS_ARE_ENABLED, {}});
     //                             });
     //     }
     // }

     //==================================================================================================================
     void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override
     {
        if (child.hasType(ID::DISCOVER_SERVICES))
        {
            LOG("Bluetooth - DISCOVER SERVICES");

            auto device = parent;
            jassert(device.hasType(ID::BLUETOOTH_DEVICE));

            // const juce::ScopedLock lock(connectionsLock);

            // if (auto it = connections.find(device.getProperty(ID::address).toString()); it != connections.end())
            // {
            //     gattlib_connection_t* connection = it->second.first;

            //     int services_count = 0; // gattlib_discover_primary will update this...
            //     gattlib_primary_service_t* services = nullptr; // gattlib_discover_primary will allocate this :shiver:

            //     [[maybe_unused]] const auto ret = gattlib_discover_primary(connection, &services, &services_count);
            //     jassert(ret == GATTLIB_SUCCESS);

           	//     for (int i = 0; i < services_count; i++) {
            //         const auto uuid_str = genki::gattlib_utils::get_uuid_string(services[i].uuid);

            //         LOG(fmt::format("Bluetooth - SERVICE: {}", uuid_str));

            //         device.appendChild({ID::SERVICE, {
            //             {ID::uuid, uuid_str},
            //             {ID::handle_start, services[i].attr_handle_start},
            //             {ID::handle_end, services[i].attr_handle_end},
            //         }, {}}, nullptr);
            //     }

            //     free(services); // WATCH OUT

            //     genki::message(device, ID::SERVICES_DISCOVERED);
            // }
        }
        else if (child.hasType(ID::DISCOVER_CHARACTERISTICS))
        {
            const auto device = getAncestor(child, ID::BLUETOOTH_DEVICE);
            jassert(device.isValid());

            auto service = parent;
            jassert(service.hasType(ID::SERVICE));

            // const juce::ScopedLock lock(connectionsLock);

            // if (auto it = connections.find(device.getProperty(ID::address).toString()); it != connections.end())
            // {
            //     gattlib_connection_t* connection = it->second.first;

            //     int characteristics_count = 0; // gattlib_discover_char will update this...
            //     gattlib_characteristic_t* characteristics = nullptr; // gattlib_discover_char will allocate this :shiver:

            //     [[maybe_unused]] const auto ret = gattlib_discover_char(connection, &characteristics, &characteristics_count);
            //     jassert(ret == GATTLIB_SUCCESS);

            //     const int handle_start = service.getProperty(ID::handle_start);
            //     const int handle_end = service.getProperty(ID::handle_end);

            //     for (int i = 0; i < characteristics_count; i++) {
            //         const auto& [handle, properties, value_handle, uuid] = characteristics[i];

            //         if (handle >= handle_start && handle <= handle_end)
            //         {
            //             const auto uuid_str = genki::gattlib_utils::get_uuid_string(characteristics[i].uuid);

            //             LOG(fmt::format("Bluetooth - CHARACTERISTIC: {}", uuid_str));

            //             service.appendChild({ID::CHARACTERISTIC, {
            //                 {ID::uuid, uuid_str},
            //                 {ID::properties, properties}, // TODO
            //                 {ID::handle, handle},
            //                 {ID::value_handle, value_handle},
            //             }, {}}, nullptr);
            //         }
            //     }

            //     free(characteristics); // WATCH OUT
            // }
        }
        else if (child.hasType(ID::ENABLE_NOTIFICATIONS) || child.hasType(ID::ENABLE_INDICATIONS))
        {
            jassert(parent.hasType(ID::CHARACTERISTIC));

            const auto& device = getAncestor(child, ID::BLUETOOTH_DEVICE);

            // const juce::ScopedLock lock(connectionsLock);

            // if (auto it = connections.find(device.getProperty(ID::address).toString()); it != connections.end())
            // {
            //     auto& [connection, callbacks] = it->second;

            //     const auto data_handler = [](const uuid_t* uuid, const uint8_t* data, size_t data_length, void* user_data)
            //     {
            //         const auto uuid_str = genki::gattlib_utils::get_uuid_string(*uuid);

            //         reinterpret_cast<Impl*>(user_data)->characteristicValueChanged(juce::Uuid(uuid_str), gsl::as_bytes(gsl::span(data, data_length)));
            //     };

            //     const auto state_change_handler = [](gattlib_connection_t* conn, uint16_t handle, bool is_notifying, void* user_data)
            //     {
            //         reinterpret_cast<Impl*>(user_data)->notificationStateChanged(conn, handle, is_notifying);
            //     };

            //     gattlib_register_notification_state_change(connection, state_change_handler, this);

            //     {
            //         const auto register_func = child.hasType(ID::ENABLE_NOTIFICATIONS)
            //                                     ? &gattlib_register_notification
            //                                     : &gattlib_register_indication;

            //         [[maybe_unused]] const auto ret = register_func(connection, data_handler, this);
            //         jassert(ret == GATTLIB_SUCCESS);
            //     }

            //     {
            //         const auto juce_uuid = juce::Uuid(parent.getProperty(ID::uuid).toString());
            //         const auto uuid = genki::gattlib_utils::from_juce_uuid(juce_uuid);

            //         handlers.insert_or_assign(juce_uuid, &callbacks);

            //         LOG(fmt::format("Bluetooth - Enable notifications for UUID: {}", juce_uuid.toDashedString()));

            //         [[maybe_unused]] const auto ret = gattlib_notification_start(connection, &uuid);
            //         jassert(ret == GATTLIB_SUCCESS);
            //     }
            // }
        }
        else if (child.hasType(ID::SCAN))
        {
            if (child.getProperty(ID::should_start))
            {
                LOG("Bluetooth - Starting scan...");
                using namespace ranges;

                const auto rng       = ValueTreeRange(child);
                const auto uuid_strs = rng | views::transform(property_as<String>(ID::uuid));

                LOG(fmt::format("Bluetooth - Starting scan for services:\n{}",
                        uuid_strs
                        | views::transform(to_string_view)
                        | views::join('\n')
                        | to<std::string>())
                );

                // const auto uuids = uuid_strs | views::transform(genki::gattlib_utils::from_juce_uuid) | to<std::vector>();

                // // Put in in a form that the gattlib C API can work with
                // auto uuid_ps = uuids | views::transform([](const uuid_t& uuid) { return const_cast<uuid_t*>(&uuid); }) | to<std::vector>();
                // uuid_ps.push_back(nullptr);

                // const auto discovered_device_cb = [](gattlib_adapter_t*, const char* addr, const char* name, void* user_data) {
                //                     jassert(user_data != nullptr);
                //                     reinterpret_cast<Impl*>(user_data)->deviceDiscovered(addr ? addr : "", name ? name : "");
                //                 };

                // // TODO: Filter by services...
                // connectedDevicePoll = std::make_unique<LambdaTimer>(
                //                         [this]
                //                         {
                //                             const auto connected_device_cb = [](gattlib_adapter_t*, const char* addr, const char* name, void* user_data)
                //                             {
                //                                 jassert(user_data != nullptr);
                //                                 reinterpret_cast<Impl*>(user_data)->deviceDiscovered(addr ? addr : "", name ? name : "", true);
                //                             };

                //                             gattlib_adapter_probe_connected_devices(adapter, connected_device_cb, this);
                //                         }, 500);

                // [[maybe_unused]] const auto ret = gattlib_adapter_scan_enable_with_filter_non_blocking(
                //         adapter,
                //         uuid_ps.size() > 1 ? uuid_ps.data() : nullptr,
                //         0, // rssi_threshold
                //         uuid_ps.size() > 1 ? GATTLIB_DISCOVER_FILTER_USE_UUID : 0,
                //         discovered_device_cb,
                //         0, // timeout, 0 means indefinite scan
                //         reinterpret_cast<void*>(this) // :shrug:
                // );

                // jassert(ret == GATTLIB_SUCCESS);
            }
            else
            {
                LOG("Bluetooth - Stopping scan...");

                // [[maybe_unused]] const auto ret = gattlib_adapter_scan_disable(adapter);
                // jassert(ret == GATTLIB_SUCCESS);
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

    //==================================================================================================================
    juce::ValueTree valueTree;

    // juce::CriticalSection connectionsLock;
    // std::map<juce::String, std::pair<gattlib_connection_t*, genki::BleDevice::Callbacks>> connections;
    // std::map<juce::Uuid, genki::BleDevice::Callbacks*> handlers;

    OrgBluezAdapter1* bluezAdapter = nullptr;

    std::unique_ptr<LambdaTimer> connectedDevicePoll;
};

//======================================================================================================================
BleAdapter::Impl::Impl(ValueTree vt) : valueTree(std::move(vt))
{
    const auto on_finish = [](GObject* source_object, GAsyncResult* res, gpointer user_data)
    {
        auto* p = reinterpret_cast<BleAdapter::Impl*>(user_data);
        auto& vt = p->valueTree;

        GError* error = nullptr;
        p->bluezAdapter = ORG_BLUEZ_ADAPTER1(source_object);

        if (!org_bluez_adapter1_proxy_new_for_bus_finish(res, &error))
        {
            LOG(fmt::format("Bluetooth - Error opening default adapter: {}\n", error->message));
            vt.setProperty(ID::status, static_cast<int>(AdapterStatus::Disabled), nullptr);

            g_error_free(error);
        }
        else
        {
            org_bluez_adapter1_set_powered(p->bluezAdapter, true);

            const juce::String name(org_bluez_adapter1_get_name(p->bluezAdapter));
            LOG(fmt::format("Bluetooth - Opened adapter: {}", name));

            vt.setProperty(ID::name, name, nullptr);
            vt.setProperty(ID::status, static_cast<int>(AdapterStatus::PoweredOn), nullptr);
        }
    };

    org_bluez_adapter1_proxy_new_for_bus(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        "org.bluez",
        "/org/bluez/hci0",
        nullptr,
        on_finish,
        this
    );

    valueTree.addListener(this);
}

BleAdapter::Impl::~Impl()
{
    if (bluezAdapter != nullptr)
    {
        g_object_unref(bluezAdapter);
    }
}

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

void BleAdapter::disconnect(const BleDevice& device)
{
    impl->disconnect(device);
}

size_t BleAdapter::getMaximumValueLength(const BleDevice&)
{
    jassertfalse;
    return 0;
}

//======================================================================================================================
void BleDevice::write(BleAdapter& adapter, const Uuid& uuid, gsl::span<const gsl::byte> data, bool withResponse)
{
    LOG(fmt::format("WRITE {} {}", uuid.toDashedString(), data));

    adapter.impl->writeCharacteristic(*this, uuid, data, withResponse);
}

}// namespace genki

#endif// JUCE_LINUX
