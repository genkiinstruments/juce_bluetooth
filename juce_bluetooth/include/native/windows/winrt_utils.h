#pragma once

#include <juce_core/juce_core.h>

#include <winrt/base.h>
#include <winrt/windows.devices.bluetooth.genericattributeprofile.h>
#include <winrt/windows.devices.enumeration.h>
#include <winrt/windows.devices.radios.h>
#include <winrt/windows.foundation.collections.h>

using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Devices::Radios;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

#include <fmt/format.h>

template<>
struct fmt::formatter<winrt::hstring>
{
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const winrt::hstring& str, FormatContext& ctx) -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{}", winrt::to_string(str));
    }
};

namespace winrt_util {
using namespace winrt::Windows::Foundation;

using PropertyStore = Collections::IMapView<winrt::hstring, IInspectable>;

template<typename T>
inline auto get_property(const PropertyStore& map, const winrt::hstring& key) -> std::optional<T>
{
    return map.HasKey(key)
                   ? std::optional(winrt::unbox_value<T>(map.Lookup(key)))
                   : std::nullopt;
}

template<typename T>
inline auto get_property_or(const PropertyStore& map, const winrt::hstring& key, T def) -> T
{
    const auto p = get_property<T>(map, key);

    return p.has_value() ? *p : def;
}

// TODO: There has to be a better way!

// Oh the horror
inline auto uuid_to_guid(const juce::Uuid& uuid) -> winrt::guid
{
    constexpr int uuid_size = 16;
    const auto*   bytes     = uuid.getRawData();

    const uint32_t d1 = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | (bytes[3] << 0);
    const uint16_t d2 = (bytes[4] << 8) | (bytes[5] << 0);
    const uint16_t d3 = (bytes[6] << 8) | (bytes[7] << 0);

    std::array<uint8_t, 8> d4{};
    std::copy(bytes + sizeof(d1) + sizeof(d2) + sizeof(d3), bytes + uuid_size, d4.begin());

    return {d1, d2, d3, d4};
}

// Oh jeez...
inline auto guid_to_uuid(const winrt::guid& guid) -> juce::Uuid
{
    constexpr int                  uuid_size = 16;
    std::array<uint8_t, uuid_size> bytes{};

    const auto [d1, d2, d3, d4] = guid;

    bytes[0] = (d1 >> 24) & 0xff;
    bytes[1] = (d1 >> 16) & 0xff;
    bytes[2] = (d1 >> 8) & 0xff;
    bytes[3] = (d1 >> 0) & 0xff;

    bytes[4] = (d2 >> 8) & 0xff;
    bytes[5] = (d2 >> 0) & 0xff;

    bytes[6] = (d3 >> 8) & 0xff;
    bytes[7] = (d3 >> 0) & 0xff;

    std::copy(std::cbegin(d4), std::cend(d4), bytes.begin() + sizeof(d1) + sizeof(d2) + sizeof(d3));

    return bytes.data();
}

inline auto to_mac_string(uint64_t addr) -> juce::String
{
    constexpr auto byte = [](uint64_t addr, unsigned int pos) -> uint8_t
    { return static_cast<uint8_t>((addr >> pos) & 0xFF); };

    const std::array<uint8_t, 6> bytes{byte(addr, 0), byte(addr, 8), byte(addr, 16), byte(addr, 24), byte(addr, 32), byte(addr, 40)};

    return juce::MACAddress(bytes.data()).toString();
}

inline auto from_mac_string(juce::StringRef str) -> uint64_t { return juce::MACAddress(str).toInt64(); }

//======================================================================================================================
template<typename T>
juce::String to_string(T t);

template<>
inline juce::String to_string<AsyncStatus>(AsyncStatus status)
{
    return status == AsyncStatus::Canceled ? "Canceled" : status == AsyncStatus::Error ? "Error"
                                                  : status == AsyncStatus::Completed   ? "Completed"
                                                  : status == AsyncStatus::Started     ? "Started"
                                                                                       : "Unkown";
}

template<>
inline juce::String to_string<GattCommunicationStatus>(GattCommunicationStatus status)
{
    return status == GattCommunicationStatus::AccessDenied ? "AccessDenied" : status == GattCommunicationStatus::ProtocolError ? "ProtocolError"
                                                                      : status == GattCommunicationStatus::Success             ? "Success"
                                                                      : status == GattCommunicationStatus::Unreachable         ? "Unreachable"
                                                                                                                               : "Unkown";
}

template<>
inline juce::String to_string<RadioKind>(RadioKind kind)
{
    return kind == RadioKind::Bluetooth ? "Bluetooth" : kind == RadioKind::FM        ? "FM"
                                                : kind == RadioKind::MobileBroadband ? "MobileBroadBand"
                                                : kind == RadioKind::Other           ? "Other"
                                                : kind == RadioKind::WiFi            ? "Wifi"
                                                                                     : "Unknown";
}


template<>
inline juce::String to_string<BluetoothConnectionStatus>(BluetoothConnectionStatus status)
{
    return status == BluetoothConnectionStatus::Connected ? "Connected" : status == BluetoothConnectionStatus::Disconnected ? "Disconnected"
                                                                                                                            : "Unknown (";
}

template<>
inline juce::String to_string<DeviceAccessStatus>(DeviceAccessStatus status)
{
    return status == DeviceAccessStatus::Allowed ? "Allowed" : status == DeviceAccessStatus::DeniedBySystem ? "DeniedBySystem"
                                                       : status == DeviceAccessStatus::DeniedByUser         ? "DeniedByUser"
                                                       : status == DeviceAccessStatus::Unspecified          ? "Unspecified"
                                                                                                            : "Unkown";
}

} // namespace winrt_util
