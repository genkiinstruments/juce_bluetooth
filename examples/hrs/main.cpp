#include <atomic>
#include <csignal>

#include "juce_bluetooth/juce_bluetooth.h"
#include <fmt/ranges.h>

#include "format.h"

std::atomic_bool term = false;
static void      signal_handler(int) { term.store(true); }

int main()
{
    fmt::print("JUCE Bluetooth Example - HRS\n");

    for (const auto& signum: {SIGTERM, SIGINT})
        std::signal(signum, signal_handler);

    juce::MessageManager::getInstance();

    genki::BleAdapter               adapter;
    genki::ValueTreeListener        listener{adapter.state};
    std::optional<genki::BleDevice> device;

    const genki::BleDevice::Callbacks ble_callbacks{
            .valueChanged = [](const juce::Uuid&, [[maybe_unused]] gsl::span<const gsl::byte> data)
            {
                // Step 6: Notifications from the device will be received here
                DBG(fmt::format("HRS notification: {}", data)); },
            .characteristicWritten = [](const juce::Uuid, bool) {},
    };

    const juce::Uuid HeartRateServiceUuid("0000-180D-8000-00805F9B34FB");
    const juce::Uuid HeartRateCharacteristicUuid("0000-2A37-8000-00805F9B34FB");

    // Used to identify our device during discovery
    const auto my_device_name = "wave";

    listener.property_changed = [&](juce::ValueTree& vt, const juce::Identifier& id)
    {
        if (vt.hasType(ID::BLUETOOTH_ADAPTER) && id == ID::status)
        {
            const auto is_powered_on = AdapterStatus((int) vt.getProperty(id)) == AdapterStatus::PoweredOn;

            fmt::print("{}\n", is_powered_on
                                       ? "Adapter powered on, starting scan..."
                                       : "Adapter powered off/disabled, stopping scan...");

            // Step 1: Start scanning for devices
            //         We can optionally filter advertised service UUIDs
            adapter.scan(is_powered_on);
        }
        else if (vt.hasType(ID::BLUETOOTH_DEVICE))
        {
            if (id == ID::is_connected && vt.getProperty(id))
            {
                fmt::print("Device connected:\n{}\n", vt);

                // Step 3: Initiate service discovery on the device.
                //         Will receive a SERVICES_DISCOVERED message when complete.
                genki::message(vt, ID::DISCOVER_SERVICES);
            }
        }
    };

    listener.child_added = [&](juce::ValueTree&, juce::ValueTree& vt)
    {
        if (vt.hasType(ID::BLUETOOTH_DEVICE))
        {
            const auto name = vt.getProperty(ID::name).toString();

            if (name.containsIgnoreCase(my_device_name))
            {
                // Step 2: Connect to the device.
                //         The returned device object can be used to write/disconnect from the device later on.
                device = adapter.connect(vt, ble_callbacks);
            }
        }
        else if (vt.hasType(ID::SERVICES_DISCOVERED))
        {
            const auto dev = vt.getParent(); // dev should be the same node as device->state
            jassert(dev.hasType(ID::BLUETOOTH_DEVICE));

            // Step 4: Discover characteristics on the service we're interested in
            if (const auto srv = dev.getChildWithProperty(ID::uuid, HeartRateServiceUuid.toDashedString()); srv.isValid())
            {
                genki::message(srv, ID::DISCOVER_CHARACTERISTICS);
            }
        }
        else if (vt.hasType(ID::CHARACTERISTIC))
        {
            // Step 5: Once we've found the characteristic, we can enable notfications on it
            if (juce::Uuid(vt.getProperty(ID::uuid).toString()) == HeartRateCharacteristicUuid)
            {
                genki::message(vt, ID::ENABLE_NOTIFICATIONS);
            }
        }
    };

    while (!term)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

    if (device.has_value())
        adapter.disconnect(*device);

    juce::MessageManager::deleteInstance();
    juce::DeletedAtShutdown::deleteAll();
}
