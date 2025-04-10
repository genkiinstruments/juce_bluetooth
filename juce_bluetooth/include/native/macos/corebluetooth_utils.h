#pragma once

#import <CoreBluetooth/CoreBluetooth.h>

namespace genki::corebluetooth_utils {

inline juce::Uuid to_juce_uuid(const CBUUID* _Nonnull uuid)
{
    // Standard Bluetooth UUIDs are just represented by their 16-bit UUID.
    // Need to pad these with the 128-bit base UUID.

    using namespace juce;

    const NSString* str = [uuid UUIDString];

    return [str length] == 4
                   ? Uuid(juce::String("0000") + String([str UTF8String]).toLowerCase() + juce::String("-0000-1000-8000-00805f9b34fb"))
                   : Uuid(String([[uuid UUIDString] UTF8String]));
}

inline CBUUID* _Nonnull from_juce_uuid(const juce::Uuid& uuid)
{
    NSString* uuid_str = [NSString stringWithUTF8String:uuid.toDashedString().toStdString().c_str()];
    return [CBUUID UUIDWithString:uuid_str];
}

inline juce::String get_uuid_string(CBUUID* _Nonnull uuid) { return to_juce_uuid(uuid).toDashedString(); }
inline juce::String get_address_string(NSUUID* _Nonnull uuid) { return juce::String([[uuid UUIDString] UTF8String]).toLowerCase(); }

} // namespace genki::corebluetooth_utils
