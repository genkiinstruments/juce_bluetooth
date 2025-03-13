#pragma once

#include <juce_core/juce_core.h>
#include <gattlib.h>

namespace genki::gattlib_utils {

inline juce::Uuid to_juce_uuid(const uuid_t& uuid)
{
    // Standard Bluetooth UUIDs are just represented by their 16-bit UUID.
    // Need to pad these with the 128-bit base UUID.

    using namespace juce;

    std::array<char, MAX_LEN_UUID_STR + 1> str{};

    [[maybe_unused]] const auto ret = gattlib_uuid_to_string(&uuid, str.data(), str.size());
    jassert(ret == GATTLIB_SUCCESS);

    return strnlen(str.data(), str.size()) == 4
           ? Uuid(String("0000") + String(str.data()).toLowerCase() + String("-0000-1000-8000-00805f9b34fb"))
           : Uuid(String(str.data()));
}

inline uuid_t from_juce_uuid(const juce::Uuid& uuid)
{
    uuid_t u{};

    const juce::String uuid_str = uuid.toDashedString();
    [[maybe_unused]] const auto ret = gattlib_string_to_uuid(uuid_str.getCharPointer(), static_cast<size_t>(uuid_str.length()) + 1, &u);

    jassert(ret == GATTLIB_SUCCESS);

    return u;
}

inline juce::String get_uuid_string(const uuid_t& uuid) { return to_juce_uuid(uuid).toDashedString(); }
// inline juce::String get_address_string(NSUUID* _Nonnull uuid) { return juce::String([[uuid UUIDString] UTF8String]).toLowerCase(); }

} // namespace genki::corebluetooth_utils
