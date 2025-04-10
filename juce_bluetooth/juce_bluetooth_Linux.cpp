#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_LINUX

#include "juce_bluetooth.h"
#include "juce_bluetooth_log.h"

#include <glib.h>

#include "org-bluez-Adapter1.h"
#include "org-bluez-Device1.h"
#include "org-bluez-GattCharacteristic1.h"
#include "org-bluez-GattDescriptor1.h"
#include "org-bluez-GattService1.h"

#include <gsl/span>
#include <range/v3/view.hpp>

#include "format.h"
#include "native/linux/bluez_utils.h"
#include "ranges.h"

using namespace juce;

namespace ID {
const juce::Identifier dbus_object_path("dbus_object_path");
}

namespace genki {
//======================================================================================================================
struct LambdaTimer : public juce::Timer
{
    LambdaTimer(std::function<void()> cb, int intervalMs) : callback(std::move(cb)) { startTimer(intervalMs); }

    ~LambdaTimer() override { stopTimer(); }

    void timerCallback() override
    {
        if (callback) callback();
    }

    std::function<void()> callback;
};

struct BleAdapter::Impl : private juce::ValueTree::Listener
{
    explicit Impl(ValueTree);
    ~Impl() override;

    //==================================================================================================================
    void connect(const juce::ValueTree& deviceState, const BleDevice::Callbacks& callbacks)
    {
        const juce::String address = deviceState.getProperty(ID::address);

        const auto& [it, was_inserted] = connections.insert({address,
                                                             std::make_pair(bluez_utils::get_device_for_address(bluezAdapter, address), callbacks)});

        if (!was_inserted)
        {
            LOG(fmt::format("Bluetooth - Device already in list of connections: {}", address));
            return;
        }

        const auto on_device_connected = [](GObject* source_object, GAsyncResult* res, gpointer user_data)
        {
            auto* p = reinterpret_cast<BleAdapter::Impl*>(user_data);

            GError*          err = nullptr;
            OrgBluezDevice1* dev = ORG_BLUEZ_DEVICE1(source_object);

            if (!org_bluez_device1_call_connect_finish(dev, res, &err))
            {
                LOG(fmt::format("Bluetooth - Error connecting device: {}\n", err->message));

                p->deviceConnected(dev, false);

                g_error_free(err);
            }
        };

        OrgBluezDevice1* device = it->second.first.get();

        org_bluez_device1_call_connect(
                device,
                nullptr, // cancelable
                on_device_connected,
                this);
    }

    void disconnect(const BleDevice& device)
    {
        const auto addr = device.state.getProperty(ID::address).toString();

        clearCharacteristicCacheForDevice(addr);

        if (const auto it = connections.find(addr); it != connections.end())
        {
            LOG(fmt::format("Bluetooth - Disconnect device: {}", addr));

            const auto on_device_disconnected = [](GObject* source_object, GAsyncResult* res, gpointer)
            {
                GError*          err = nullptr;
                OrgBluezDevice1* dev = ORG_BLUEZ_DEVICE1(source_object);

                if (!org_bluez_device1_call_connect_finish(dev, res, &err))
                {
                    LOG(fmt::format("Bluetooth - Error disconnecting device: {}\n", err->message));

                    g_error_free(err);
                }
            };

            OrgBluezDevice1* dev = it->second.first.get();

            org_bluez_device1_call_disconnect(
                    dev,
                    nullptr, // cancelable
                    on_device_disconnected,
                    this);
        }
    }

    void writeCharacteristic(const BleDevice& device, const juce::Uuid& charactUuid, gsl::span<const gsl::byte> data, bool withResponse)
    {
        const auto addr = device.state.getProperty(ID::address).toString();

        if (const auto& vt = findChildWithProperty(device.state, ID::uuid, charactUuid.toDashedString()); vt.isValid())
        {
            const juce::String characteristic_object_path = vt.getProperty(ID::dbus_object_path);
            GDBusObject*       obj                        = g_dbus_object_manager_get_object(dbusObjectManager, characteristic_object_path.getCharPointer());

            if (obj != nullptr)
            {
                GDBusInterface* interface = g_dbus_object_get_interface(obj, "org.bluez.GattCharacteristic1");

                if (interface != nullptr)
                {
                    const auto on_write_complete = [](GObject* source_object, GAsyncResult* res, gpointer user_data)
                    {
                        GError* err = nullptr;

                        OrgBluezGattCharacteristic1* charact = ORG_BLUEZ_GATT_CHARACTERISTIC1(source_object);
                        auto*                        p       = reinterpret_cast<BleAdapter::Impl*>(user_data);

                        const auto& cc = p->characteristicCache;

                        const char* object_path = g_dbus_proxy_get_object_path(G_DBUS_PROXY(charact));

                        const auto it = std::find_if(cc.begin(), cc.end(), [&](const auto& ent)
                                                     { return ent.dbusObjectPath.compare(object_path) == 0; });

                        bool success = org_bluez_gatt_characteristic1_call_start_notify_finish(charact, res, &err);

                        if (!success)
                        {
                            const auto uuid = juce::Uuid(juce::String(org_bluez_gatt_characteristic1_get_uuid(charact)));
                            LOG(fmt::format("Bluetooth - Error writing characteristic: {} - {}\n", uuid.toDashedString(), err->message));

                            g_error_free(err);
                        }

                        if (it != cc.end())
                            it->callbacks->characteristicWritten(it->uuid, success);

                        g_object_unref(charact);
                    };

                    GError* error = nullptr;

                    OrgBluezGattCharacteristic1* char_proxy = org_bluez_gatt_characteristic1_proxy_new_sync(
                            g_dbus_proxy_get_connection(G_DBUS_PROXY(interface)),
                            G_DBUS_PROXY_FLAGS_NONE,
                            "org.bluez",
                            g_dbus_proxy_get_object_path(G_DBUS_PROXY(interface)),
                            nullptr,
                            &error);

                    if (error != nullptr)
                    {
                        LOG(fmt::format("Bluetooth - Failed to get D-Bus proxy for characteristic: {}", error->message));
                        g_error_free(error);
                    }

                    GVariant* arg_value = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, data.data(), data.size(), sizeof(gsl::byte));

                    GVariantBuilder builder{};
                    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
                    g_variant_builder_add(&builder, "{sv}", "type", g_variant_new_string(withResponse ? "request" : "command"));
                    GVariant* arg_options = g_variant_builder_end(&builder);

                    org_bluez_gatt_characteristic1_call_write_value(
                            char_proxy,
                            arg_value,
                            arg_options,
                            nullptr, // cancelable
                            on_write_complete,
                            this);
                }
            }
        }
    }

    //==================================================================================================================
    void deviceConnected(OrgBluezDevice1* device, bool success)
    {
        const juce::String address = bluez_utils::get_device_address(device);

        if (!success)
        {
            LOG(fmt::format("Bluetooth - Failed to connect to device: {}", address));

            deviceDisconnected(device);
            return;
        }

        LOG(fmt::format("Bluetooth - Device connected: {}", address));

        if (auto ch = valueTree.getChildWithProperty(ID::address, address); ch.isValid())
        {
            ch.removeProperty(ID::is_connected, nullptr);
            ch.setProperty(ID::is_connected, true, nullptr);
            ch.setProperty(ID::max_pdu_size, 20, nullptr); // TODO: How to know?
        }
    }

    void deviceDisconnected(OrgBluezDevice1* device)
    {
        const auto addr_str = bluez_utils::get_device_address(device);

        LOG(fmt::format("Bluetooth - Device disconnected: {}", addr_str));

        if (const auto conn = connections.find(addr_str); conn != connections.end())
        {
            connections.erase(conn);

            clearCharacteristicCacheForDevice(addr_str);
        }
    }

    void clearCharacteristicCacheForDevice(const juce::String& address)
    {
        if (auto dev = valueTree.getChildWithProperty(ID::address, address); dev.isValid())
        {
            for (const auto& srv: dev)
            {
                if (srv.hasType(ID::SERVICE))
                {
                    for (const auto& ch: srv)
                    {
                        if (ch.hasType(ID::CHARACTERISTIC))
                        {
                            const juce::String& object_path = ch.getProperty(ID::dbus_object_path);

                            const auto it = std::find_if(characteristicCache.begin(), characteristicCache.end(), [&](const auto& ent)
                                                         { return ent.dbusObjectPath.compare(object_path) == 0; });

                            if (it != characteristicCache.end())
                                characteristicCache.erase(it);
                        }
                    }
                }
            }

            valueTree.removeChild(dev, nullptr);
        }
    }
    void deviceDiscovered(std::string_view addr, std::string_view name, int16_t rssi, [[maybe_unused]] bool is_connected)
    {
        const auto addr_str = bluez_utils::get_address_string(addr.data());
        const auto name_str = juce::String(name.data());

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
            valueTree.appendChild({ID::BLUETOOTH_DEVICE, {{ID::name, name_str}, {ID::address, addr_str}, {ID::rssi, rssi}, {ID::is_connected, false}, {ID::last_seen, now}}}, nullptr);
        }
    }

    void characteristicValueChanged(std::string_view object_path, gsl::span<const gsl::byte> data)
    {
        const auto it = std::find_if(characteristicCache.begin(), characteristicCache.end(), [&](const auto& ent)
                                     { return ent.dbusObjectPath.compare(object_path.data()) == 0; });

        if (it != characteristicCache.end())
        {
            [[maybe_unused]] const auto& [_, uuid, callback, obj_path] = *it;
            callback->valueChanged(uuid, data);
        }
    }

    void characteristicWritten(const juce::Uuid&, bool)
    {
        // TODO
    }

    //==================================================================================================================
    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override
    {
        if (child.hasType(ID::DISCOVER_SERVICES))
        {
            auto deviceState = parent;
            jassert(deviceState.hasType(ID::BLUETOOTH_DEVICE));

            if (auto it = connections.find(deviceState.getProperty(ID::address).toString()); it != connections.end())
            {
                const auto& address = it->first;

                const juce::String device_path = bluez_utils::get_device_object_path_from_address(bluezAdapter, address);

                GList* objects = g_dbus_object_manager_get_objects(dbusObjectManager);

                for (GList* l = objects; l != NULL; l = l->next)
                {
                    GDBusObject*       object = G_DBUS_OBJECT(l->data);
                    const juce::String object_path(g_dbus_object_get_object_path(object));

                    if (object_path.startsWith(device_path))
                    {
                        GDBusInterface* interface = g_dbus_object_get_interface(object, "org.bluez.GattService1");

                        if (interface != nullptr)
                        {
                            GVariant* uuid_variant = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(interface), "UUID");

                            if (uuid_variant != nullptr)
                            {
                                const juce::Uuid uuid(juce::String(g_variant_get_string(uuid_variant, nullptr)));

                                deviceState.appendChild({ID::SERVICE, {
                                                                              {ID::uuid, uuid.toDashedString()},
                                                                              {ID::dbus_object_path, object_path},
                                                                      },
                                                         {}},
                                                        nullptr);

                                g_variant_unref(uuid_variant);
                            }

                            g_object_unref(interface);
                        }
                    }
                }

                g_list_free_full(objects, g_object_unref);

                genki::message(deviceState, ID::SERVICES_DISCOVERED);
            }
        }
        else if (child.hasType(ID::DISCOVER_CHARACTERISTICS))
        {
            const auto device = getAncestor(child, ID::BLUETOOTH_DEVICE);
            jassert(device.isValid());

            auto service = parent;
            jassert(service.hasType(ID::SERVICE));

            GList*             objects      = g_dbus_object_manager_get_objects(dbusObjectManager);
            const juce::String service_path = service.getProperty(ID::dbus_object_path);

            for (GList* l = objects; l != NULL; l = l->next)
            {
                GDBusObject*       object = G_DBUS_OBJECT(l->data);
                const juce::String object_path(g_dbus_object_get_object_path(object));

                if (object_path.startsWith(service_path))
                {
                    GDBusInterface* interface = g_dbus_object_get_interface(object, "org.bluez.GattCharacteristic1");

                    if (interface != nullptr)
                    {
                        GVariant* uuid_variant = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(interface), "UUID");

                        if (uuid_variant != nullptr)
                        {
                            const juce::Uuid uuid(juce::String(g_variant_get_string(uuid_variant, nullptr)));

                            service.appendChild({ID::CHARACTERISTIC, {
                                                                             {ID::uuid, uuid.toDashedString()},
                                                                             {ID::dbus_object_path, object_path},
                                                                     },
                                                 {}},
                                                nullptr);

                            g_variant_unref(uuid_variant);
                        }

                        g_object_unref(interface);
                    }
                }
            }

            g_list_free_full(objects, g_object_unref);
        }
        else if (child.hasType(ID::ENABLE_NOTIFICATIONS) || child.hasType(ID::ENABLE_INDICATIONS))
        {
            jassert(parent.hasType(ID::CHARACTERISTIC));
            auto characteristic = parent;

            const auto device = getAncestor(characteristic, ID::BLUETOOTH_DEVICE);
            jassert(device.isValid());

            const juce::String characteristic_object_path = characteristic.getProperty(ID::dbus_object_path);
            GDBusObject*       obj                        = g_dbus_object_manager_get_object(dbusObjectManager, characteristic_object_path.getCharPointer());

            if (obj != nullptr)
            {
                GDBusInterface* interface = g_dbus_object_get_interface(obj, "org.bluez.GattCharacteristic1");

                if (interface != nullptr)
                {
                    GError* error = nullptr;

                    OrgBluezGattCharacteristic1* char_proxy = org_bluez_gatt_characteristic1_proxy_new_sync(
                            g_dbus_proxy_get_connection(G_DBUS_PROXY(interface)),
                            G_DBUS_PROXY_FLAGS_NONE,
                            "org.bluez",
                            g_dbus_proxy_get_object_path(G_DBUS_PROXY(interface)),
                            nullptr,
                            &error);

                    const auto on_notify_ready = [](GObject* source_object, GAsyncResult* res, gpointer user_data)
                    {
                        GError* err = nullptr;

                        OrgBluezGattCharacteristic1* charact = ORG_BLUEZ_GATT_CHARACTERISTIC1(source_object);
                        auto*                        p       = reinterpret_cast<BleAdapter::Impl*>(user_data);

                        const auto uuid = juce::Uuid(juce::String(org_bluez_gatt_characteristic1_get_uuid(charact)));

                        if (!org_bluez_gatt_characteristic1_call_start_notify_finish(charact, res, &err))
                        {
                            LOG(fmt::format("Bluetooth - Error enabling notifications for characteristic: {} - {}\n", uuid.toDashedString(), err->message));

                            g_error_free(err);
                            return;
                        }

                        const char* object_path = g_dbus_proxy_get_object_path(G_DBUS_PROXY(charact));

                        if (const auto& ch = findChildWithProperty(p->valueTree, ID::dbus_object_path, object_path); ch.isValid())
                        {
                            const auto& dev = getAncestor(ch, ID::BLUETOOTH_DEVICE);

                            if (const auto it = p->connections.find(dev.getProperty(ID::address)); it != p->connections.end())
                            {
                                const auto& callbacks = it->second.second;

                                p->characteristicCache.emplace(charact, uuid, callbacks);
                            }

                            genki::message(ch, {ID::NOTIFICATIONS_ARE_ENABLED, {}});
                        }
                    };

                    org_bluez_gatt_characteristic1_call_start_notify(
                            char_proxy,
                            nullptr, // cancelable
                            on_notify_ready,
                            this);
                }
            }
        }
        else if (child.hasType(ID::SCAN))
        {
            if (child.getProperty(ID::should_start))
            {
                const auto rng       = ValueTreeRange(child);
                const auto uuid_strs = rng | ranges::views::transform(property_as<String>(ID::uuid));

                if (ranges::distance(uuid_strs) > 0)
                {
                    LOG(fmt::format("Bluetooth - Starting scan for services:\n{}",
                                    uuid_strs | ranges::views::transform(to_string_view) | ranges::views::join('\n') | ranges::to<std::string>()));

                    GVariantBuilder props_builder{};
                    g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));

                    GVariantBuilder uuid_builder{};
                    g_variant_builder_init(&uuid_builder, G_VARIANT_TYPE("as"));

                    for (const auto& uuid: uuid_strs)
                        g_variant_builder_add(&uuid_builder, "s", uuid.getCharPointer());

                    g_variant_builder_add(&props_builder, "{sv}", "UUIDs", g_variant_builder_end(&uuid_builder));
                    g_variant_builder_add(&props_builder, "{sv}", "Transport", g_variant_new_string("le")); // Only LE devices

                    GVariant* properties = g_variant_builder_end(&props_builder);

                    GError* error = nullptr;
                    if (!org_bluez_adapter1_call_set_discovery_filter_sync(
                                bluezAdapter,
                                properties,
                                nullptr, // cancellable
                                &error))
                    {
                        LOG(fmt::format("Bluetooth - Failed to set discovery filter: {}", error->message));
                    }
                }
                else
                {
                    LOG("Bluetooth - Starting scan...");
                }

                // TODO: Filter by UUID
                connectedDevicePoll = std::make_unique<LambdaTimer>([this]
                                                                    { probeConnectedDevices(); }, 500);

                GError* error = nullptr;
                if (!org_bluez_adapter1_call_start_discovery_sync(bluezAdapter, nullptr, &error))
                {
                    LOG(fmt::format("Bluetooth - Failed to start discovery: {}", error->message));
                    g_error_free(error);
                }
            }
            else
            {
                LOG("Bluetooth - Stopping scan...");

                GError* error = nullptr;
                if (!org_bluez_adapter1_call_stop_discovery_sync(bluezAdapter, nullptr, &error))
                {
                    LOG(fmt::format("Bluetooth - Failed to stop scan: {}", error->message));
                    g_error_free(error);
                }
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
        const char*           object_path = g_dbus_object_get_object_path(object);
        const GDBusInterface* interface   = g_dbus_object_manager_get_interface(dbusObjectManager, object_path, "org.bluez.Device1");

        if (interface != nullptr)
        {
            const DeviceProxy device = bluez_utils::get_device_from_object_path(object_path);

            const char* addr         = org_bluez_device1_get_address(device.get());
            const char* name         = org_bluez_device1_get_name(device.get());
            const auto  rssi         = static_cast<int16_t>(org_bluez_device1_get_rssi(device.get()));
            const bool  is_connected = org_bluez_device1_get_connected(device.get());

            deviceDiscovered(addr ? addr : "", name ? name : "", rssi, is_connected);
        }
    }

    void dbusInterfaceProxyPropertiesChanged(GDBusProxy* interface_proxy, GVariant* changed_properties, const gchar* const*)
    {
        const char*        proxy_object_path = g_dbus_proxy_get_object_path(interface_proxy);
        const juce::String interface_name(g_dbus_proxy_get_interface_name(interface_proxy));

        if (interface_name == "org.bluez.Device1")
        {
            if (g_variant_n_children(changed_properties) > 0)
            {
                DeviceProxy device = bluez_utils::get_device_from_object_path(proxy_object_path);

                GVariantIter* iter = nullptr;
                g_variant_get(changed_properties, "a{sv}", &iter);

                const gchar* key   = nullptr;
                GVariant*    value = nullptr;

                while (g_variant_iter_loop(iter, "{&sv}", &key, &value))
                {
                    if (strcmp(key, "Connected") == 0)
                    {
                        const bool is_connected = g_variant_get_boolean(value);
                        LOG(fmt::format("Bluetooth - Device state changed: {}", is_connected ? "Connected" : "Disconnected"));

                        if (!is_connected)
                            deviceDisconnected(device.get());
                    }
                    else if (strcmp(key, "ServicesResolved") == 0)
                    {
                        if (g_variant_get_boolean(value))
                            deviceConnected(device.get(), true);
                    }
                    else if (strcmp(key, "RSSI") == 0)
                    {
                        const auto rssi = static_cast<int16_t>(g_variant_get_int16(value));

                        const char* addr         = org_bluez_device1_get_address(device.get());
                        const char* name         = org_bluez_device1_get_name(device.get());
                        const bool  is_connected = org_bluez_device1_get_connected(device.get());

                        deviceDiscovered(addr ? addr : "", name ? name : "", rssi, is_connected);
                    }
                }

                g_variant_iter_free(iter);
            }
        }
        else if (interface_name == "org.bluez.GattCharacteristic1")
        {
            if (g_variant_n_children(changed_properties) > 0)
            {
                GVariantIter* iter = nullptr;
                g_variant_get(changed_properties, "a{sv}", &iter);

                const gchar* key   = nullptr;
                GVariant*    value = nullptr;

                while (g_variant_iter_loop(iter, "{&sv}", &key, &value))
                {
                    if (strcmp(key, "Value") == 0)
                    {
                        const uint8_t* data     = nullptr;
                        gsize          data_len = 0;
                        data                    = static_cast<const uint8_t*>(g_variant_get_fixed_array(value, &data_len, sizeof(uint8_t)));

                        characteristicValueChanged(
                                std::string_view(proxy_object_path),
                                gsl::as_bytes(gsl::span(data, static_cast<size_t>(data_len))));
                    }
                }

                g_variant_iter_free(iter);
            }
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
                &error);

        if (error != NULL)
        {
            LOG(fmt::format("Bluetooth - Error creating D-Bus bject manager: {}", error->message));
            g_error_free(error);
        }
        else
        {
            // Note: Need to be explicit to please the C
            using ObjectAddedFn                 = void (*)(GDBusObjectManager*, GDBusObject*, gpointer);
            const ObjectAddedFn on_object_added = [](auto* manager, auto* object, auto user_data)
            {
                auto* p = reinterpret_cast<BleAdapter::Impl*>(user_data);
                jassert(manager == p->dbusObjectManager);

                p->dbusObjectAdded(object);
            };

            g_signal_connect(G_DBUS_OBJECT_MANAGER(dbusObjectManager), "object-added", G_CALLBACK(on_object_added), this);

            using PropertiesChangedFn                                       = void (*)(GDBusObjectManager*, GDBusObjectProxy*, GDBusProxy*, GVariant*, const gchar* const*, gpointer);
            const PropertiesChangedFn on_interface_proxy_properties_changed = [](auto* manager, auto*, auto* interface_proxy, auto* changed_properties, auto invalidated_properties, auto user_data)
            {
                auto* p = reinterpret_cast<BleAdapter::Impl*>(user_data);
                jassert(manager == p->dbusObjectManager);

                p->dbusInterfaceProxyPropertiesChanged(interface_proxy, changed_properties, invalidated_properties);
            };

            g_signal_connect(G_DBUS_OBJECT_MANAGER(dbusObjectManager), "interface-proxy-properties-changed", G_CALLBACK(on_interface_proxy_properties_changed), this);
        }
    }

    void probeConnectedDevices()
    {
        GList* objects = nullptr;

        objects = g_dbus_object_manager_get_objects(dbusObjectManager);

        for (GList* l = objects; l != nullptr; l = l->next)
        {
            GDBusObject* object = G_DBUS_OBJECT(l->data);

            const char* object_path = g_dbus_object_get_object_path(object);

            if (!strstr(object_path, "/dev_"))
                continue;


            GDBusInterface* interface = g_dbus_object_get_interface(object, "org.bluez.Device1");
            if (interface == nullptr)
                continue;

            GDBusProxy* proxy = G_DBUS_PROXY(interface);

            GVariant* connected_variant = g_dbus_proxy_get_cached_property(proxy, "Connected");
            if (connected_variant != nullptr)
            {
                const bool is_connected = g_variant_get_boolean(connected_variant);
                g_variant_unref(connected_variant);

                if (is_connected)
                {
                    GVariant* name_variant    = g_dbus_proxy_get_cached_property(proxy, "Name");
                    GVariant* address_variant = g_dbus_proxy_get_cached_property(proxy, "Address");
                    GVariant* rssi_variant    = g_dbus_proxy_get_cached_property(proxy, "RSSI");

                    if (address_variant)
                    {
                        const char*   addr = g_variant_get_string(address_variant, nullptr);
                        const char*   name = name_variant ? g_variant_get_string(name_variant, nullptr) : "";
                        const int16_t rssi = rssi_variant ? g_variant_get_int16(rssi_variant) : 0;

                        deviceDiscovered(addr ? addr : "", name ? name : "", rssi, is_connected);

                        if (name_variant)
                            g_variant_unref(name_variant);

                        g_variant_unref(address_variant);
                    }
                }
            }

            g_object_unref(interface);
        }

        g_list_free_full(objects, g_object_unref);
    }

    //==================================================================================================================
    juce::ValueTree valueTree;

    std::map<juce::String, std::pair<DeviceProxy, BleDevice::Callbacks>> connections;

    struct CharacteristicCacheEntry
    {
        CharacteristicCacheEntry(OrgBluezGattCharacteristic1* charact, juce::Uuid u, const genki::BleDevice::Callbacks& cbs)
            : characteristicProxy(charact, g_object_unref),
              uuid(std::move(u)),
              callbacks(&cbs),
              dbusObjectPath(g_dbus_proxy_get_object_path(G_DBUS_PROXY(characteristicProxy.get())))
        {
        }

        CharacteristicProxy                characteristicProxy;
        const juce::Uuid                   uuid;
        const genki::BleDevice::Callbacks* callbacks;
        const juce::String                 dbusObjectPath;

        bool operator<(const CharacteristicCacheEntry& rhs) const
        {
            return dbusObjectPath.compare(rhs.dbusObjectPath) < 0;
        }
    };

    std::set<CharacteristicCacheEntry> characteristicCache;

    OrgBluezAdapter1*   bluezAdapter      = nullptr;
    GDBusObjectManager* dbusObjectManager = nullptr;

    std::unique_ptr<LambdaTimer> connectedDevicePoll;
};

//======================================================================================================================
BleAdapter::Impl::Impl(ValueTree vt) : valueTree(std::move(vt))
{
    const auto on_adapter_ready = [](GObject* source_object, GAsyncResult* res, gpointer user_data)
    {
        auto* p   = reinterpret_cast<BleAdapter::Impl*>(user_data);
        auto& pvt = p->valueTree;

        GError* error   = nullptr;
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
            this);

    valueTree.addListener(this);
}

BleAdapter::Impl::~Impl()
{
    if (bluezAdapter != nullptr)
    {
        if (bluezAdapter != nullptr)
            g_object_unref(bluezAdapter);

        if (dbusObjectManager != nullptr)
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
    adapter.impl->writeCharacteristic(*this, uuid, data, withResponse);
}

} // namespace genki

#endif // JUCE_LINUX
