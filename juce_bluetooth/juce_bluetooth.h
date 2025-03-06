/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 juce_bluetooth
  vendor:             Genki
  version:            1.0.0
  name:               JUCE Bluetooth LE
  description:        Bluetooth LE classes for JUCE

  dependencies: juce_core, juce_data_structures
  OSXFrameworks: CoreBluetooth Foundation
  mingwLibs: WindowsApp.lib, cppwinrt
  linuxPackages: gattlib
  linuxLibs: bluetooth
  minimumCppStandard: 17
  searchpaths: include

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <gsl/span>

#include "include/identifiers.h"
#include "include/message.h"
#include "include/valuetrees.h"

//======================================================================================================================
namespace genki {

struct BleAdapter;

struct BleDevice
{
    struct Callbacks
    {
        std::function<void(const juce::Uuid&, gsl::span<const gsl::byte>)> valueChanged;
        std::function<void(const juce::Uuid&, bool)>                       characteristicWritten;
    };

    BleDevice() = default;

    explicit BleDevice(juce::ValueTree vt) : state(std::move(vt)) {}

    void write(BleAdapter&, const juce::Uuid& charact, gsl::span<const gsl::byte> data, bool withResponse = true);

    //==================================================================================================================
    juce::ValueTree state{};
};

//======================================================================================================================
struct BleAdapter : private juce::Timer
{
    static constexpr int TimeoutMs = 5000;

    BleAdapter();
    BleAdapter(juce::ValueTree::Listener&);
    ~BleAdapter() override;

    [[nodiscard]] AdapterStatus status() const
    {
        return state.hasProperty(ID::status)
                       ? AdapterStatus(static_cast<int>(state.getProperty(ID::status)))
                       : AdapterStatus::Disabled;
    }

    void scan(bool shouldStart, const std::initializer_list<juce::Uuid>& uuids = {})
    {
        juce::ValueTree vt{ID::SCAN, {{ID::should_start, shouldStart}}};

        for (const auto& uuid: uuids)
            vt.appendChild({ID::SERVICE, {{ID::uuid, uuid.toDashedString()}}}, nullptr);

        message(state, vt);
    }

    [[nodiscard]] BleDevice connect(const juce::ValueTree&, const BleDevice::Callbacks&) const;

    void disconnect(const BleDevice&);

    size_t getMaximumValueLength(const BleDevice&);

    void timerCallback() override
    {
        const auto now = static_cast<int>(juce::Time::getMillisecondCounter());

        std::vector<juce::ValueTree> to_remove{};

        for (auto d: state)
        {
            const int  last_seen    = d.getProperty(ID::last_seen);
            const bool is_connected = d.getProperty(ID::is_connected);

            if (d.hasType(ID::BLUETOOTH_DEVICE))
                if (!is_connected && (now - last_seen) > TimeoutMs)
                    to_remove.push_back(d);
        }

        for (const auto& vt: to_remove)
            state.removeChild(vt, nullptr);
    }

    //==================================================================================================================
    juce::ValueTree state{ID::BLUETOOTH_ADAPTER};

    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace genki
