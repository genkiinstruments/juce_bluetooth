#include "juce_bluetooth/juce_bluetooth.h"

#include <atomic>
#include <csignal>

#include "format.h"

std::atomic_bool term = false;
static void signal_handler(int) { term.store(true); }

int main()
{
    fmt::print("JUCE Bluetooth Example - scan\n");

    for (const auto& signum: {SIGTERM, SIGINT})
        std::signal(signum, signal_handler);

    juce::MessageManager::getInstance();

    genki::BleAdapter        adapter;
    genki::ValueTreeListener listener{adapter.state};

    listener.property_changed = [&](juce::ValueTree& vt, const juce::Identifier& id) {
        if (vt.hasType(ID::BLUETOOTH_ADAPTER) && id == ID::status)
        {
            const auto is_powered_on = adapter.status() == AdapterStatus::PoweredOn;

            fmt::print("{}\n", is_powered_on
                               ? "Adapter powered on, starting scan..."
                               : "Adapter powered off/disabled, stopping scan...");

            adapter.scan(is_powered_on);
        }
        else if (vt.hasType(ID::BLUETOOTH_DEVICE) && id == ID::last_seen)
        {
            if (vt.getProperty(ID::name).toString().isNotEmpty())
            {
                fmt::print("{} {} - rssi: {}\n",
                        vt.getProperty(ID::name).toString().toStdString(),
                        vt.getProperty(ID::address).toString().toStdString(),
                        (int) vt.getProperty(ID::rssi));
            }
        }
    };

    listener.child_added = [](juce::ValueTree&, juce::ValueTree&) {};

    while (!term)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

    juce::MessageManager::deleteInstance();
    juce::DeletedAtShutdown::deleteAll();
}
