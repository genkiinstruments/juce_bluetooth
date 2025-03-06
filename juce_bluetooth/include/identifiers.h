#pragma once

#include <juce_core/juce_core.h>

namespace ID {
#define DECLARE_ID(name) const juce::Identifier name (#name);

DECLARE_ID(BLUETOOTH_ADAPTER)
DECLARE_ID(status)

DECLARE_ID(BLUETOOTH_DEVICE)
DECLARE_ID(name)
DECLARE_ID(address)
DECLARE_ID(is_connected)
DECLARE_ID(max_value_length)
DECLARE_ID(rssi)
DECLARE_ID(last_seen)
DECLARE_ID(max_pdu_size)

DECLARE_ID(SERVICE)
DECLARE_ID(uuid)
DECLARE_ID(handle_start)
DECLARE_ID(handle_end)

DECLARE_ID(CHARACTERISTIC)
//DECLARE_ID(uuid)
DECLARE_ID(can_write_with_response)
DECLARE_ID(can_write_without_response)
DECLARE_ID(properties)
DECLARE_ID(handle)
DECLARE_ID(value_handle)

DECLARE_ID(SCAN)
DECLARE_ID(should_start)

// Commands/messages
DECLARE_ID(CONNECT)
DECLARE_ID(DISCOVER_SERVICES)
DECLARE_ID(SERVICES_DISCOVERED)
DECLARE_ID(DISCOVER_CHARACTERISTICS)
DECLARE_ID(ENABLE_NOTIFICATIONS)
DECLARE_ID(ENABLE_INDICATIONS)
DECLARE_ID(NOTIFICATIONS_ARE_ENABLED)

#undef DECLARE_ID

} // namespace ID

enum class AdapterStatus
{
    Disabled,
    PoweredOff,
    PoweredOn,
    Unauthorized,
};
