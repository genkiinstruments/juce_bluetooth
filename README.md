# JUCE Bluetooth modules

This repo contains a JUCE module for interacting with Bluetooth LE devices on macOS and Windows.

## Dependencies

The project depends on a few third-party libraries

* [GSL](https://github.com/microsoft/GSL)
* [fmt](https://github.com/fmtlib/fmt)
* [range-v3](https://github.com/ericniebler/range-v3)

These libraries are expected to be available and linked properly as part of your appliation build step.

[JUCE](https://github.com/juce-framework/JUCE) is expected to be already present in your project. 

The project assumes a CMake-based environment.

## Quickstart

At the heart of your application you'll instantiate a `BleAdapter` to manage discovery and connections.

```c++
genki::BleAdapter adapter;
```

This module uses JUCE's ValueTrees to pass messages back-and-forth, so make sure to listen to changes on the adapter's state.

```c++
genki::ValueTreeListener listener{adapter.state};

listener.property_changed = [&](juce::ValueTree& vt, const juce::Identifier& id)
{
    if (vt.hasType(ID::BLUETOOTH_ADAPTER) && id == ID::status)
    {
        const auto is_powered_on = AdapterStatus((int) vt.getProperty(id)) == AdapterStatus::PoweredOn;

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
```

After discovery, you can connect to a BLE device using its address

```c++
const BleDevice::Callbacks ble_callbacks{
    .valueChanged = [this](const juce::Uuid& uuid, gsl::span<const gsl::byte> data)
    {
        // Called when data is received on a characteristic via notifications or indications
    },
    .characteristicWritten = [this](const juce::Uuid& uuid, bool success)
    {
        // Called when a characteristic write has successfully been delivered to the peripheral
    },
};

const auto device = adapter.connect(address), cb);
```

You'll receive ValueTree changes for all devices on the `BleAdapter.state` as well

TODO: Add example with service discovery/notifications