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

After discovery, you can connect to a BLE device

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

listener.child_added = [&](juce::ValueTree&, juce::ValueTree& vt)
{
    if (vt.hasType(ID::BLUETOOTH_DEVICE))
    {
        const auto name = vt.getProperty(ID::name).toString();

        if (name.containsIgnoreCase(my_device_name))
        {
            device = adapter.connect(vt, ble_callbacks);
        }
    }
};
```

You'll receive ValueTree changes for all devices on the `BleAdapter.state` as well

After connecting to a device, you initiate service discovery

```c++
listener.property_changed = [&](juce::ValueTree& vt, const juce::Identifier& id)
{
    if (vt.hasType(ID::BLUETOOTH_ADAPTER) && id == ID::status)
    {
        ...
    }
    else if (vt.hasType(ID::BLUETOOTH_DEVICE))
    {
        if (id == ID::is_connected && vt.getProperty(id))
        {
            genki::message(vt, ID::DISCOVER_SERVICES);
        }
    }
};
```

which will emit a message with `ID::SERVICES_DISCOVERED` on the device node, allowing us to discover characteristics for a given service

```c++
listener.child_added = [&](juce::ValueTree&, juce::ValueTree& vt)
{
    if (vt.hasType(ID::BLUETOOTH_DEVICE))
    {
        ...
    }
    else if (vt.hasType(ID::SERVICES_DISCOVERED))
    {
        const auto dev = vt.getParent(); // dev should be the same node as device->state
        jassert(dev.hasType(ID::BLUETOOTH_DEVICE));

        if (const auto srv = dev.getChildWithProperty(ID::uuid, HeartRateServiceUuid.toDashedString()); srv.isValid())
            genki::message(srv, ID::DISCOVER_CHARACTERISTICS);
    }
};
```

and finally enable notifications on the characteristic

```c++
listener.child_added = [&](juce::ValueTree&, juce::ValueTree& vt)
{
    if (vt.hasType(ID::BLUETOOTH_DEVICE))
    {
        ...
    }
    else if (vt.hasType(ID::SERVICES_DISCOVERED))
    {
        ...
    }
    else if (vt.hasType(ID::CHARACTERISTIC))
    {
        if (juce::Uuid(vt.getProperty(ID::uuid).toString()) == HeartRateCharacteristicUuid)
            genki::message(vt, ID::ENABLE_NOTIFICATIONS);
    }
};
```
