#pragma once

#include <juce_core/juce_core.h>
#include <glib.h>
#include "org-bluez-Adapter1.h"
#include "org-bluez-Device1.h"
#include "org-bluez-GattCharacteristic1.h"
#include "juce_bluetooth_log.h"

using DeviceProxy = std::unique_ptr<OrgBluezDevice1, decltype(&g_object_unref)>;
using CharacteristicProxy = std::unique_ptr<OrgBluezGattCharacteristic1, decltype(&g_object_unref)>;

namespace genki::bluez_utils {

inline auto get_address_string(const char* addr) -> juce::String
{
    return juce::MACAddress(addr).toString();
}

inline auto get_native_address_string(const juce::String& str) -> juce::String
{
    return juce::MACAddress(str.replace("-:", "")).toString().replace("-", ":").toUpperCase();
}

inline auto get_device_address(OrgBluezDevice1* device)
{
    return get_address_string(org_bluez_device1_get_address(device));
}

inline auto get_device_object_path_from_address(const OrgBluezAdapter1* adapter, juce::StringRef device_address) -> juce::String
{
    // Make sure device address in the format "XX:XX:XX:XX:XX:XX"
    const auto native_addr_str = get_native_address_string(device_address);
    const char* adapter_path = g_dbus_proxy_get_object_path(G_DBUS_PROXY(adapter));

    return juce::String(g_strdup_printf("%s/dev_%s", adapter_path, g_strdelimit(g_strdup(native_addr_str.getCharPointer()), ":", '_')));
}

inline auto get_device_from_object_path(juce::StringRef device_path) -> DeviceProxy
{
    GError* error = nullptr;

    OrgBluezDevice1* device = org_bluez_device1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"org.bluez",
			device_path.text,
            nullptr,
            &error
    );

    if (error != nullptr)
    {
        LOG(fmt::format("Bluetooth - D-Bus error: {}", error->message));
        return DeviceProxy(nullptr, g_object_unref);
    }

    return DeviceProxy(device, g_object_unref);
}

inline auto get_device_for_address(const OrgBluezAdapter1* adapter, juce::StringRef device_address) -> DeviceProxy
{
    // Device address in the format "XX:XX:XX:XX:XX:XX"
    const auto native_addr_str = get_native_address_string(device_address);
    const auto object_path = get_device_object_path_from_address(adapter, native_addr_str);

    return get_device_from_object_path(object_path);
}

} // namespace genki::gattlib_utils
