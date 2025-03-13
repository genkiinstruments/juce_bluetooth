#include <atomic>
#include <csignal>

#include "juce_bluetooth/juce_bluetooth.h"
#include <fmt/ranges.h>

#include "format.h"

std::atomic_bool term = false;
static void signal_handler(int) { term.store(true); }

int main()
{
    fmt::print("JUCE Bluetooth Example - Discover\n");

    for (const auto& signum: {SIGTERM, SIGINT})
        std::signal(signum, signal_handler);

    juce::MessageManager::getInstance();

    genki::BleAdapter               adapter;
    genki::ValueTreeListener        listener{adapter.state};
    std::optional<genki::BleDevice> device;

    // Used to identify our device during discovery
    const auto my_device_name = "wave";

    {
        // Adapter status might have updated before we had a chance to attach the listener
        const auto is_powered_on = adapter.status() == AdapterStatus::PoweredOn;
        fmt::print("Adapter is powered {}\n", is_powered_on ? "on" : "off");

        adapter.scan(is_powered_on);
    }

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
                device = adapter.connect(vt, genki::BleDevice::Callbacks{
                        .valueChanged = [](const juce::Uuid&, gsl::span<const gsl::byte>) {},
                        .characteristicWritten = [](const juce::Uuid, bool) {},
                });

                adapter.scan(false);
            }
        }
        else if (vt.hasType(ID::SERVICES_DISCOVERED))
        {
            const auto dev = vt.getParent(); // dev should be the same node as device->state
            jassert(dev.hasType(ID::BLUETOOTH_DEVICE));

            // Step 4: Discover characteristics on the services
            for (const auto& child : dev)
                if (child.hasType(ID::SERVICE))
                    genki::message(child, ID::DISCOVER_CHARACTERISTICS);
        }
        else if (vt.hasType(ID::CHARACTERISTIC))
        {
            // Step 5: This is where we'll know which characteristics are available on the device
            fmt::print("Characteristic added: {}\n", vt);
        }
    };

    listener.child_removed = [&](juce::ValueTree&, juce::ValueTree& vt, int)
    {
        if (vt.hasType(ID::BLUETOOTH_DEVICE))
        {
            if (device.has_value() && device->state.getProperty(ID::address).toString() == vt.getProperty(ID::address).toString())
            {
                fmt::print("Device disconnected:\n{}\n", vt);

                device.reset();
                adapter.scan(true);
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
