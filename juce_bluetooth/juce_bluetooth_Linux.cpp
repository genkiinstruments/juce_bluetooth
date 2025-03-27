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
    void connect(const juce::ValueTree& deviceState, const BleDevice::Callbacks& callbacks)
    {
        const juce::String addr_str = deviceState.getProperty(ID::address);

        // Device address in the format "XX:XX:XX:XX:XX:XX";
        const auto native_addr_str = bluez_utils::get_native_address_string(addr_str);
        const char* adapter_path = g_dbus_proxy_get_object_path(G_DBUS_PROXY(bluezAdapter));

        char* device_path = g_strdup_printf("%s/dev_%s", adapter_path, g_strdelimit(g_strdup(native_addr_str.getCharPointer()), ":", '_'));

        GError* error = nullptr;
        OrgBluezDevice1* device = org_bluez_device1_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE,
				"org.bluez",
				device_path,
                nullptr,
                &error
        );

        if (error != nullptr)
        {
            LOG(fmt::format("Bluetooth - D-Bus error: {}", error->message));
            return;
        }

        connections.insert({addr_str, {nullptr, callbacks}});

        const auto on_device_connected = [](GObject* source_object, GAsyncResult* res, gpointer user_data)
        {
            auto* p = reinterpret_cast<BleAdapter::Impl*>(user_data);

            GError* err = nullptr;
            OrgBluezDevice1* dev = ORG_BLUEZ_DEVICE1(source_object);

            if (!org_bluez_device1_call_connect_finish(dev, res, &err))
            {
                LOG(fmt::format("Bluetooth - Error connecting device: {}\n", err->message));
                g_error_free(err);
            }

            p->deviceConnected(dev, err == nullptr);
        };

        org_bluez_device1_call_connect(
            device,
            nullptr, // cancelable
            on_device_connected,
            this
        );
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
    void deviceConnected(OrgBluezDevice1* device, bool success)
    {
        const juce::String address = bluez_utils::get_address_string(org_bluez_device1_get_address(device));

        if (!success)
        {
            LOG(fmt::format("Bluetooth - Failed to connect to device: {}", address));

            connections.erase(address);
            return;
        }

        LOG(fmt::format("Bluetooth - Device connected: {}", address));

        connections.at(address).first = device;

        if (auto ch = valueTree.getChildWithProperty(ID::address, address); ch.isValid())
        {
            ch.setProperty(ID::is_connected, true, nullptr);
            ch.setProperty(ID::max_pdu_size, 20, nullptr); // TODO: How to know?
        }
    }

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

    void deviceDiscovered(std::string_view addr, std::string_view name, int16_t rssi, bool is_connected)
    {
        const auto addr_str     = bluez_utils::get_address_string(addr.data());
        const auto name_str     = juce::String(name.data());

        const auto now = (int) juce::Time::getMillisecondCounter();

        juce::MessageManager::callAsync([this, addr_str, name_str, now, rssi, is_connected]
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
                    valueTree.appendChild({ID::BLUETOOTH_DEVICE, {
                        {ID::name, name_str},
                        {ID::address, addr_str},
                        {ID::rssi, rssi},
                        {ID::is_connected, is_connected},
                        {ID::last_seen, now}
                    }}, nullptr);
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

                if (distance(uuid_strs) > 0)
                {
                    GVariantBuilder props_builder{};
                    g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));

                    GVariantBuilder uuid_builder{};
                    g_variant_builder_init(&uuid_builder, G_VARIANT_TYPE("as"));

                    for (const auto& uuid : uuid_strs)
                        g_variant_builder_add(&uuid_builder, "s", uuid.getCharPointer());

                    g_variant_builder_add(&props_builder, "{sv}", "UUIDs", g_variant_builder_end(&uuid_builder));
                    g_variant_builder_add(&props_builder, "{sv}", "Transport", g_variant_new_string("le"));  // Only LE devices

                    GVariant* properties = g_variant_builder_end(&props_builder);

                    GError* error = nullptr;
                    if (!org_bluez_adapter1_call_set_discovery_filter_sync(
                        bluezAdapter,
                        properties,
                        nullptr, // cancellable
                        &error
                    ))
                    {
                        LOG(fmt::format("Bluetooth - Failed to set discovery filter: {}", error->message));
                    }
                }

                {
                    GError* error = nullptr;
                    if (!org_bluez_adapter1_call_start_discovery_sync(bluezAdapter, nullptr, &error))
                    {
                        LOG(fmt::format("Bluetooth - Failed to start discovery: {}", error->message));
                        g_error_free(error);
                    }
                }
            }
            else
            {
                LOG("Bluetooth - Stopping scan...");

                GError* error = nullptr;
                if (!org_bluez_adapter1_call_stop_discovery_sync(bluezAdapter, nullptr, &error))
                    LOG(fmt::format("Bluetooth - Failed to stop scan: {}", error->message));
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

    void dbusObjectAdded(GDBusObject* object)
    {
        const char* object_path = g_dbus_object_get_object_path(object);
        const GDBusInterface* interface = g_dbus_object_manager_get_interface(dbusObjectManager, object_path, "org.bluez.Device1");

        if (interface != nullptr)
        {
            GError* error = nullptr;
            OrgBluezDevice1* device = org_bluez_device1_proxy_new_for_bus_sync(
                G_BUS_TYPE_SYSTEM,
                G_DBUS_PROXY_FLAGS_NONE,
                "org.bluez",
                object_path,
                nullptr,
                &error
            );

            if (error != nullptr)
            {
                LOG(fmt::format("Bluetooth - D-Bus error: {}", error->message));
                g_error_free(error);
                return;
            }

            const char* addr = org_bluez_device1_get_address(device);
            const char* name = org_bluez_device1_get_name(device);
            const auto rssi = static_cast<int16_t>(org_bluez_device1_get_rssi(device));
            const bool is_connected = org_bluez_device1_get_connected(device);

            deviceDiscovered(addr ? addr : "", name ? name : "", rssi, is_connected);
        }
    }

    void dbusInterfaceProxyPropertiesChanged(GDBusProxy* interface_proxy, GVariant* changed_properties, const gchar* const*)
    {
        const char* proxy_object_path = g_dbus_proxy_get_object_path(interface_proxy);
        const juce::String interface_name(g_dbus_proxy_get_interface_name(interface_proxy));

        if (interface_name == "org.bluez.Device1")
        {
    		GError* error = nullptr;

    		OrgBluezDevice1* device = org_bluez_device1_proxy_new_for_bus_sync(
    				G_BUS_TYPE_SYSTEM,
    				G_DBUS_PROXY_FLAGS_NONE,
    				"org.bluez",
    				proxy_object_path,
                    nullptr,
                    &error
            );

            if (error != nullptr)
            {
                LOG(fmt::format("Bluetooth - D-Bus error: {}", error->message));
                return;
            }

            GVariantDict dict = {};
      		g_variant_dict_init(&dict, changed_properties);

            if (GVariant* rssi_v = g_variant_dict_lookup_value(&dict, "RSSI", G_VARIANT_TYPE_INT16); rssi_v != nullptr)
            {
                const auto rssi = static_cast<int16_t>(g_variant_get_int16(rssi_v));

                const char* addr = org_bluez_device1_get_address(device);
                const char* name = org_bluez_device1_get_name(device);
                const bool is_connected = org_bluez_device1_get_connected(device);

                deviceDiscovered(addr ? addr : "", name ? name : "", rssi, is_connected);

                g_variant_unref(rssi_v);
            }

            g_variant_dict_clear(&dict);
        }
    }

    void initializeObjectManager()
    {
        GError* error = nullptr;

        dbusObjectManager = g_dbus_object_manager_client_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM,
            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
            "org.bluez",
            "/",
            nullptr, // get_proxy_type_func
            nullptr, // get_proxy_type_user_data
            nullptr, // get_proxy_type_destroy_notify
            nullptr, // cancellable
            &error
        );

        if (error != NULL)
        {
            LOG(fmt::format("Bluetooth - Error creating D-Bus bject manager: {}", error->message));
            g_error_free(error);
        }
        else
        {
            // Note: Need to be explicit to please the C
            using ObjectAddedFn = void (*)(GDBusObjectManager*, GDBusObject*, gpointer);
            const ObjectAddedFn on_object_added = [](auto* manager, auto* object, auto user_data)
            {
                auto* p = reinterpret_cast<BleAdapter::Impl*>(user_data);
                jassert(manager == p->dbusObjectManager);

                p->dbusObjectAdded(object);
            };

            g_signal_connect(G_DBUS_OBJECT_MANAGER(dbusObjectManager), "object-added", G_CALLBACK(on_object_added), this);

            using PropertiesChangedFn = void (*)(GDBusObjectManager*, GDBusObjectProxy*, GDBusProxy*, GVariant*, const gchar* const*, gpointer);
            const PropertiesChangedFn on_interface_proxy_properties_changed = [](auto* manager, auto*, auto* interface_proxy, auto* changed_properties, auto invalidated_properties, auto user_data)
            {
                auto* p = reinterpret_cast<BleAdapter::Impl*>(user_data);
                jassert(manager == p->dbusObjectManager);

                p->dbusInterfaceProxyPropertiesChanged(interface_proxy, changed_properties, invalidated_properties);
            };

            g_signal_connect(G_DBUS_OBJECT_MANAGER(dbusObjectManager), "interface-proxy-properties-changed", G_CALLBACK(on_interface_proxy_properties_changed), this);
        }
    }

    //==================================================================================================================
    juce::ValueTree valueTree;

    // juce::CriticalSection connectionsLock;
    // std::map<juce::String, std::pair<gattlib_connection_t*, genki::BleDevice::Callbacks>> connections;
    // std::map<juce::Uuid, genki::BleDevice::Callbacks*> handlers;

    std::map<juce::String, std::pair<OrgBluezDevice1*, genki::BleDevice::Callbacks>> connections;

    OrgBluezAdapter1* bluezAdapter = nullptr;
    GDBusObjectManager* dbusObjectManager = nullptr;

    std::unique_ptr<LambdaTimer> connectedDevicePoll;
};

//======================================================================================================================
BleAdapter::Impl::Impl(ValueTree vt) : valueTree(std::move(vt))
{
    const auto on_adapter_ready = [](GObject* source_object, GAsyncResult* res, gpointer user_data)
    {
        auto* p = reinterpret_cast<BleAdapter::Impl*>(user_data);
        auto& pvt = p->valueTree;

        GError* error = nullptr;
        p->bluezAdapter = ORG_BLUEZ_ADAPTER1(source_object);

        if (!org_bluez_adapter1_proxy_new_for_bus_finish(res, &error))
        {
            LOG(fmt::format("Bluetooth - Error opening default adapter: {}\n", error->message));
            pvt.setProperty(ID::status, static_cast<int>(AdapterStatus::Disabled), nullptr);

            g_error_free(error);
        }
        else
        {
            org_bluez_adapter1_set_powered(p->bluezAdapter, true);

            const juce::String name(org_bluez_adapter1_get_name(p->bluezAdapter));
            LOG(fmt::format("Bluetooth - Opened adapter: {}", name));

            pvt.setProperty(ID::name, name, nullptr);
            pvt.setProperty(ID::status, static_cast<int>(AdapterStatus::PoweredOn), nullptr);

            p->initializeObjectManager();
        }
    };

    org_bluez_adapter1_proxy_new_for_bus(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        "org.bluez",
        "/org/bluez/hci0",
        nullptr, // cancellable
        on_adapter_ready,
        this
    );

    valueTree.addListener(this);
}

BleAdapter::Impl::~Impl()
{
    if (bluezAdapter != nullptr)
    {
        g_object_unref(bluezAdapter);
        g_object_unref(dbusObjectManager);
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
