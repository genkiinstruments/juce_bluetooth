#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_WINDOWS

#pragma warning(push)
#pragma warning(disable : 4390) // empty controlled statement (; after LOG)

#include <combaseapi.h>
#include <winrt/base.h>
#include <winrt/windows.devices.bluetooth.advertisement.h>
#include <winrt/windows.devices.bluetooth.genericattributeprofile.h>
#include <winrt/windows.devices.enumeration.h>
#include <winrt/windows.devices.radios.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.storage.streams.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Radios;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;

#include <fmt/ranges.h>
#include <gsl/span_ext>

#include "format.h"
#include "juce_bluetooth.h"
#include "native/windows/winrt_utils.h"

using namespace juce;

//======================================================================================================================
#ifndef GENKI_BLUETOOTH_LOG_ENABLED
#define GENKI_BLUETOOTH_LOG_ENABLED 0
#endif

#define LOG(text) JUCE_BLOCK_WITH_FORCED_SEMICOLON(if (GENKI_BLUETOOTH_LOG_ENABLED) DBG((text));)

//======================================================================================================================
namespace genki {

struct WinBleDevice
{
    WinBleDevice(BluetoothLEDevice d, BleDevice::Callbacks cbs)
        : device(std::move(d)),
          session(nullptr),
          callbacks(std::move(cbs)) {}

    BluetoothLEDevice device;
    GattSession       session;

    BleDevice::Callbacks            callbacks;
    std::vector<GattDeviceService>  services;
    std::vector<GattCharacteristic> characteristics;

    //==================================================================================================================
    CriticalSection                                                            writeLock;
    std::deque<std::tuple<ValueTree, std::vector<gsl::byte>, GattWriteOption>> writes;
    bool                                                                       isWriteInProgress = false;
};

using BluetoothAddress = uint64_t;

static auto get_address(const ValueTree& vt) -> BluetoothAddress
{
    jassert(vt.hasType(ID::BLUETOOTH_DEVICE));
    return winrt_util::from_mac_string(vt.getProperty(ID::address).toString());
}

//======================================================================================================================
struct BleAdapter::Impl : private ValueTree::Listener
{
    //==================================================================================================================
    struct AdvertisementInfo
    {
        BluetoothAddress  address;
        winrt::hstring    name;
        int               rssi;
        std::vector<guid> serviceUuids;
    };

    //==================================================================================================================
    explicit Impl(ValueTree);
    ~Impl() override;

    //==================================================================================================================
    void write(const ValueTree& charact, gsl::span<const gsl::byte> data, bool withResponse);
    void connect(const ValueTree&, BleDevice::Callbacks);

    //==================================================================================================================
    void startScan(std::vector<guid>);
    void deviceDiscovered(const AdvertisementInfo&);
    void processPendingWrites();
    void discoverServices(const ValueTree&);
    void discoverCharacteristics(const ValueTree&);
    void enableNotifications(const ValueTree&, bool);
    void startDeviceWatcher();

    //==================================================================================================================
    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override
    {
        if (child.hasType(ID::DISCOVER_SERVICES))
        {
            jassert(parent.hasType(ID::BLUETOOTH_DEVICE));

            discoverServices(parent);
        }
        if (child.hasType(ID::DISCOVER_CHARACTERISTICS))
        {
            jassert(parent.hasType(ID::SERVICE));

            discoverCharacteristics(parent);
        }
        else if (child.hasType(ID::ENABLE_NOTIFICATIONS) || child.hasType(ID::ENABLE_INDICATIONS))
        {
            jassert(parent.hasType(ID::CHARACTERISTIC));

            enableNotifications(parent, child.hasType(ID::ENABLE_INDICATIONS));
        }
        else if (child.hasType(ID::SCAN))
        {
            if (child.getProperty(ID::should_start))
            {
                std::vector<guid> guids(static_cast<size_t>(child.getNumChildren()));
                std::transform(child.begin(), child.end(), guids.begin(),
                               [](const ValueTree& vt)
                               { return winrt_util::uuid_to_guid(vt.getProperty(ID::uuid).toString()); });

                startScan(std::move(guids));
            }
            else
            {
                if (advertisementWatcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started)
                    advertisementWatcher.Stop();
            }
        }
    }

    //==================================================================================================================
    ValueTree valueTree;

    //==================================================================================================================
    juce::CriticalSection                         advertisementLock;
    std::map<BluetoothAddress, AdvertisementInfo> advertisements;

    //==================================================================================================================
    CriticalSection                          devicesLock;
    std::map<BluetoothAddress, WinBleDevice> devices;

    //==================================================================================================================
    Radio                                          radio                = nullptr;
    Advertisement::BluetoothLEAdvertisementWatcher advertisementWatcher = nullptr;

    DeviceWatcher deviceWatcher;

    JUCE_DECLARE_WEAK_REFERENCEABLE(Impl)
};

BleAdapter::Impl::Impl(ValueTree vt)
    : valueTree(std::move(vt)),
      deviceWatcher([]
                    {
              // bb7bb05e-5972-42b5-94fc-76eaa7084d49 is the Bluetooth LE protocol ID, by the way...
              constexpr auto selector = L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\""
                                        " AND System.Devices.Aep.IsPaired:=System.StructuredQueryType.Boolean#True";

              const std::vector<winrt::hstring> props{L"System.Devices.Aep.IsConnected"};

              return DeviceInformation::CreateWatcher(selector, props, DeviceInformationKind::AssociationEndpoint); }())
{
    valueTree.addListener(this);

    deviceWatcher.Added([this](const DeviceWatcher&, const DeviceInformation& info)
                        {
        const auto is_connected = winrt_util::get_property_or(info.Properties(), L"System.Devices.Aep.IsConnected", false);

        LOG(fmt::format("Device added: {} - {}, {}", info.Name(), info.Id(), is_connected ? "connected" : "not connected"));

        if (is_connected)
        {
            BluetoothLEDevice::FromIdAsync(info.Id()).Completed([wr = WeakReference(this), name = info.Name()](const auto& sender, AsyncStatus status)
            {
                if (status != AsyncStatus::Completed)
                {
                    LOG(fmt::format("Bluetooth: Error getting device: {} - please try un-pairing through Windows settings and restarting the computer",
                            winrt::to_string(name), winrt_util::to_string(status)));

                    return;
                }

                const auto device = sender.GetResults();
                jassert(device != nullptr);

                const auto deviceName = juce::String(winrt::to_string(device.Name()));
                const auto address = winrt_util::to_mac_string(device.BluetoothAddress());
                const auto isConnected = device.ConnectionStatus() == BluetoothConnectionStatus::Connected;
                const auto lastSeen = (int) Time::getMillisecondCounter();

                MessageManager::callAsync([wr, deviceName, address, isConnected, lastSeen]()
                {
                    if (auto* p = wr.get())
                    {
                        p->valueTree.appendChild({ID::BLUETOOTH_DEVICE, {
                                {ID::name, deviceName},
                                {ID::address, address},
                                {ID::is_connected, isConnected},
                                {ID::last_seen, lastSeen}}
                        }, nullptr);
                    }
                });
            });
        } });

    deviceWatcher.Removed([wr = WeakReference(this)](const DeviceWatcher&, const DeviceInformationUpdate& info)
                          {
        LOG(fmt::format("Device removed: {}", info.Id()));

        BluetoothLEDevice::FromIdAsync(info.Id()).Completed([wr](const auto& sender, [[maybe_unused]] AsyncStatus status)
        {
            jassert(status == AsyncStatus::Completed);

            const auto device = sender.GetResults();
            jassert(device != nullptr);

            const auto address = winrt_util::to_mac_string(device.BluetoothAddress());

            MessageManager::callAsync([wr, address]()
            {
                if (auto* p = wr.get())
                {
                    const auto ch = p->valueTree.getChildWithProperty(ID::address, address);
                    p->valueTree.removeChild(ch, nullptr);
                }
            });
        }); });

    deviceWatcher.Updated([](const DeviceWatcher&, [[maybe_unused]] const DeviceInformationUpdate& info)
                          {
        // TODO: Does this need to be handled?
        DBG(fmt::format("Device updated: {}", info.Id())); });

    // Step 1: Check if there is any Bluetooth adapter available in the system
    LOG("Querying Bluetooth adapters");
    DeviceInformation::FindAllAsync(BluetoothAdapter::GetDeviceSelector()).Completed([wr = WeakReference(this)](const IAsyncOperation<DeviceInformationCollection>& sender, AsyncStatus status)
                                                                                     {
                //======================================================================================================
                if (status != AsyncStatus::Completed || sender.GetResults().Size() == 0)
                {
                    LOG(status != AsyncStatus::Completed
                        ? fmt::format("Query failed: {}", winrt_util::to_string(status))
                        : fmt::format("No Bluetooth adapter found", winrt_util::to_string(status))
                    );

                    MessageManager::callAsync([wr]()
                    {
                        if (auto* p = wr.get())
                            p->valueTree.setProperty(ID::status, (int) AdapterStatus::Disabled, nullptr);
                    });
                    return;
                }

                //======================================================================================================
                // Step 2: Get access to the default Bluetooth adapter
                LOG("Requesting access to default Bluetooth adapter");
                BluetoothAdapter::GetDefaultAsync().Completed(
                        [wr](const IAsyncOperation<BluetoothAdapter>& sender, AsyncStatus status)
                        {
                            //==========================================================================================
                            if (status != AsyncStatus::Completed)
                            {
                                MessageManager::callAsync([wr]()
                                {
                                    if (auto* p = wr.get())
                                        p->valueTree.setProperty(ID::status, (int) AdapterStatus::Disabled, nullptr);
                                });
                                return;
                            }

                            //==========================================================================================
                            const auto adapter         = sender.GetResults();
                            const bool is_le_supported = adapter.IsLowEnergySupported();
                            const auto addr_str        = winrt_util::to_mac_string(adapter.BluetoothAddress());

                            if (!is_le_supported)
                            {
                                LOG(fmt::format("Adapter ({}) does not support Bluetooth LE", addr_str));
                                MessageManager::callAsync([wr]()
                                {
                                    if (auto* p = wr.get())
                                        p->valueTree.setProperty(ID::status, (int) AdapterStatus::Disabled, nullptr);
                                });

                                return;
                            }

                            //==========================================================================================
                            // Step 3: Access the radio object associated with the adapter
                            LOG(fmt::format("Requesting access to radio for adapter: {}, LE is supported", addr_str));

                            adapter.GetRadioAsync().Completed(
                                    [wr](const IAsyncOperation<Radio>& rad, AsyncStatus status)
                                    {
                                        if (auto* p = wr.get())
                                        {
                                            //==============================================================================
                                            if (status != AsyncStatus::Completed)
                                            {
                                                LOG("Failed to get access to radio");
                                                MessageManager::callAsync([wr]()
                                                {
                                                    if (auto* p = wr.get())
                                                        p->valueTree.setProperty(ID::status, (int) AdapterStatus::Disabled, nullptr);
                                                });
                                                return;
                                            }

                                            //==============================================================================
                                            // Step 4: Listen to state changes in the Bluetooth Radio object
                                            const auto get_status = [](const auto& rad)
                                            {
                                                return rad.State() == RadioState::On ? AdapterStatus::PoweredOn :
                                                       rad.State() == RadioState::Off ? AdapterStatus::PoweredOff :
                                                       AdapterStatus::Disabled;
                                            };

                                            p->radio = rad.GetResults();
                                            p->radio.StateChanged([wr, get_status](auto, auto)
                                            {
                                                if (auto* p = wr.get())
                                                {
                                                    const int status = (int) get_status(p->radio);
                                                    MessageManager::callAsync([wr, status]()
                                                    {
                                                        if (auto* p = wr.get())
                                                            p->valueTree.setProperty(ID::status, status, nullptr);
                                                    });
                                                }
                                            });

                                            // Step 5: We most likely have access to a Bluetooth adapter that is enabled.
                                            const int initialStatus = (int) get_status(p->radio);
                                            MessageManager::callAsync([wr, initialStatus]()
                                            {
                                                if (auto* p = wr.get())
                                                    p->valueTree.setProperty(ID::status, initialStatus, nullptr);
                                            });
                                        }
                                    }
                            );
                        }
                ); });
}

BleAdapter::Impl::~Impl() = default;

void BleAdapter::Impl::startScan(std::vector<guid> guids)
{
    LOG("Bluetooth: Starting scan...");

    // Note: Using BluetoothLEAdvertisementFilter only gives you the advertising packets that contain the
    //       service UUID. Since the UUID almost fills the packet, the device name often comes in a separate
    //       advertising (scan response packet).
    advertisementWatcher = BluetoothLEAdvertisementWatcher();
    advertisementWatcher.ScanningMode(BluetoothLEScanningMode::Active);
    advertisementWatcher.Received([wr = WeakReference(this), guids{std::move(guids)}](const auto&, const auto& args)
                                  {
                                      if (auto* p = wr.get())
                                      {
                                          const auto addr   = args.BluetoothAddress();
                                          const auto advert = args.Advertisement();

                                          const juce::ScopedLock lock(p->advertisementLock);
                                          const auto [it, was_inserted] = p->advertisements.emplace(addr, AdvertisementInfo{addr});

                                          auto& device = it->second;

                                          device.rssi = args.RawSignalStrengthInDBm();

                                          if (!advert.LocalName().empty())
                                              device.name = advert.LocalName();

                                          if (advert.ServiceUuids().Size() != device.serviceUuids.size())
                                          {
                                              device.serviceUuids.clear();

                                              for (const auto& s: advert.ServiceUuids())
                                                  device.serviceUuids.emplace_back(s);
                                          }

                                          if (guids.empty())
                                              p->deviceDiscovered(device);

                                          for (const auto& guid: guids)
                                          {
                                              const auto& services = device.serviceUuids;

                                              if (const auto iit = std::find(services.cbegin(), services.cend(), guid); iit != services.cend() && !device.name.empty())
                                                  p->deviceDiscovered(device);
                                          }
                                      } });

    //==============================================================================================================
    {
        using namespace std::chrono;
        constexpr auto timeout = duration<double, std::milli>(1000);
        auto           start   = system_clock::now();

        // If we are starting the scan a short while after enabling the bluetooth adapter (e.g. bluetooth being turned on),
        // the watcher.Start() line may throw an exception due to the device "not being ready for use"
        while (advertisementWatcher.Status() != BluetoothLEAdvertisementWatcherStatus::Started && system_clock::now() - start <= timeout)
        {
            try
            {
                advertisementWatcher.Start();
            } catch (const hresult_error&)
            {
                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(50));
            }
        }

        if (system_clock::now() - start > timeout)
            LOG("Bluetooth: Timed out trying to start scan");

        LOG("Scan started successfully");
    }

    //==============================================================================================================
    startDeviceWatcher();
}

void BleAdapter::Impl::deviceDiscovered(const AdvertisementInfo& info)
{
    const auto   now          = (int) Time::getMillisecondCounter();
    const String name         = winrt::to_string(info.name);
    const String address      = winrt_util::to_mac_string(info.address);
    const int    rssi         = info.rssi;
    const bool   is_connected = false; // TODO: Does this apply?

    // ValueTree is not thread-safe - must modify on message thread
    MessageManager::callAsync([wr = WeakReference(this), name, address, rssi, is_connected, now]()
    {
        if (auto* p = wr.get())
        {
            if (auto ch = p->valueTree.getChildWithProperty(ID::address, address); ch.isValid())
            {
                ch.setProperty(ID::rssi, rssi, nullptr);
                ch.setProperty(ID::name, name, nullptr);
                ch.setProperty(ID::last_seen, now, nullptr);
            }
            else
            {
                p->valueTree.appendChild({ID::BLUETOOTH_DEVICE, {{ID::name, name}, {ID::address, address}, {ID::rssi, rssi}, {ID::is_connected, is_connected}, {ID::last_seen, now}}}, nullptr);
            }
        }
    });
}

void BleAdapter::Impl::connect(const ValueTree& deviceTree, BleDevice::Callbacks callbacks)
{
    jassert(deviceTree.hasType(ID::BLUETOOTH_DEVICE));

    DBG(fmt::format("Connecting:\n{}", deviceTree));

    BluetoothLEDevice::FromBluetoothAddressAsync(get_address(deviceTree)).Completed([wr = WeakReference(this), vt = deviceTree, cbs{std::move(callbacks)}](const auto& sender, [[maybe_unused]] AsyncStatus stat)
                                                                                    {
                if (auto* p = wr.get())
                {
                    jassert(stat == AsyncStatus::Completed);

                    const ScopedLock lock(p->devicesLock);

                    const auto[it, was_inserted] = p->devices.try_emplace(get_address(vt), sender.GetResults(), cbs);

                    if (!was_inserted)
                    {
                        DBG(fmt::format("Device was already inserted: {} {}", winrt_util::to_mac_string(it->first), it->second.device.Name()));
                        return;
                    }

                    auto& device = it->second.device;
                    jassert(device != nullptr);

                    device.ConnectionStatusChanged([wr, vt](const BluetoothLEDevice& d, const auto&) mutable
                    {
                        const bool isConnected = d.ConnectionStatus() == BluetoothConnectionStatus::Connected;

                        LOG(fmt::format("Connection status changed: {} ({}), {}",
                                winrt::to_string(d.Name()),
                                winrt_util::to_mac_string(d.BluetoothAddress()),
                                isConnected));

                        MessageManager::callAsync([wr, vt, isConnected]() mutable
                        {
                            if (auto* p = wr.get())
                            {
                                vt.setProperty(ID::is_connected, isConnected, nullptr);

                                if (!isConnected)
                                {
                                    const ScopedLock lock(p->devicesLock);
                                    p->devices.erase(p->devices.find(get_address(vt)));

                                    p->valueTree.removeChild(vt, nullptr);
                                }
                            }
                        });
                    });

                    GattSession::FromDeviceIdAsync(device.BluetoothDeviceId()).Completed(
                            [wr, vt](const auto& op, [[maybe_unused]] AsyncStatus stat) mutable
                            {
                                if (auto* p = wr.get())
                                {
                                    jassert(stat == AsyncStatus::Completed);

                                    const ScopedLock lock(p->devicesLock);

                                    if (const auto iit = p->devices.find(get_address(vt)); iit != p->devices.end())
                                    {
                                        auto& session = iit->second.session;

                                        session = op.GetResults();
                                        session.MaintainConnection(true);

                                        const int maxPduSize = session.MaxPduSize() - 3;
                                        MessageManager::callAsync([vt, maxPduSize]() mutable
                                        {
                                            vt.setProperty(ID::max_pdu_size, maxPduSize, nullptr);
                                        });

                                        session.MaxPduSizeChanged([vt](const GattSession& s, const auto&) mutable
                                        {
                                            const int maxPduSize = s.MaxPduSize() - 3;
                                            MessageManager::callAsync([vt, maxPduSize]() mutable
                                            {
                                                vt.setProperty(ID::max_pdu_size, maxPduSize, nullptr);
                                            });
                                        });
                                    }
                                }
                            }
                    );
                } });
}

void BleAdapter::Impl::processPendingWrites()
{
    const ScopedLock dLock(devicesLock);

    for (auto& [_, device]: devices)
    {
        const ScopedLock wLock(device.writeLock);

        if (device.writes.empty() || device.isWriteInProgress)
            continue;

        const auto& [vt, data, type] = device.writes.front();
        jassert(vt.hasType(ID::CHARACTERISTIC));

        const auto addr = get_address(getAncestor(vt, ID::BLUETOOTH_DEVICE));
        const auto uuid = vt.getProperty(ID::uuid).toString();
        const auto guid = winrt_util::uuid_to_guid(uuid);

        auto& ch = device.characteristics;
        if (const auto iit = std::find_if(ch.begin(), ch.end(), [&](const auto& c)
                                          { return c.Uuid() == guid; });
            iit != ch.end())
        {
            Windows::Storage::Streams::DataWriter writer;
            writer.ByteOrder(Windows::Storage::Streams::ByteOrder::LittleEndian);
            writer.WriteBytes({(const uint8_t*) data.data(), (const uint8_t*) data.data() + data.size()});

            DBG(fmt::format("Writing characteristic ({} response) {}: {}",
                            (type == GattWriteOption::WriteWithResponse ? "with" : "without"),
                            uuid,
                            juce::String::toHexString(data.data(), static_cast<int>(data.size()))));

            device.isWriteInProgress = true;
            iit->WriteValueWithResultAsync(writer.DetachBuffer(), type).Completed([wr = juce::WeakReference(this), charact = vt, addr, type = type](const IAsyncOperation<GattWriteResult>& sender, AsyncStatus status)
                                                                                  {
                        if (status != AsyncStatus::Completed)
                        {
                            LOG(fmt::format("Bluetooth: WriteValueWithResultAsync completed with error: {}", winrt_util::to_string(status)));
                            return;
                        }

                        const auto res         = sender.GetResults();
                        const auto comm_status = res.Status();

                        if (comm_status != GattCommunicationStatus::Success)
                        {
                            LOG(fmt::format("Bluetooth: Error writing characteristic: {}", winrt_util::to_string(comm_status)));

                            if (comm_status == GattCommunicationStatus::ProtocolError)
                                LOG(fmt::format("Protocol error: {}", static_cast<int>(res.ProtocolError().Value())));

                            return;
                        }

                        jassert(comm_status == GattCommunicationStatus::Success);

                        if (auto* p = wr.get())
                        {
                            {
                                const ScopedLock lock(p->devicesLock);

                                if (const auto it = p->devices.find(addr); it != p->devices.end())
                                {
                                    auto& dev = it->second;
                                    dev.isWriteInProgress = false;

                                    {
                                        const ScopedLock lk(dev.writeLock);
                                        dev.writes.pop_front();
                                    }

                                    if (type == GattWriteOption::WriteWithResponse && dev.callbacks.characteristicWritten != nullptr)
                                        dev.callbacks.characteristicWritten(charact.getProperty(ID::uuid).toString(), comm_status == GattCommunicationStatus::Success);
                                }
                            }

                            p->processPendingWrites();
                        } });
        }
    }
}

void BleAdapter::Impl::write(const ValueTree& charact, gsl::span<const gsl::byte> data, bool withResponse)
{
    jassert(charact.hasType(ID::CHARACTERISTIC));

    std::vector<gsl::byte> buf(data.size());
    std::copy(data.begin(), data.end(), buf.begin());

    const ScopedLock dLock(devicesLock);

    if (const auto it = devices.find(get_address(getAncestor(charact, ID::BLUETOOTH_DEVICE))); it != devices.end())
    {
        const auto type = withResponse ? GattWriteOption::WriteWithResponse : GattWriteOption::WriteWithoutResponse;

        const ScopedLock wLock(it->second.writeLock);
        it->second.writes.emplace_back(charact, std::move(buf), type);
    }

    processPendingWrites();
}

void BleAdapter::Impl::discoverServices(const ValueTree& deviceTree)
{
    jassert(deviceTree.hasType(ID::BLUETOOTH_DEVICE));

    const ScopedLock lock(devicesLock);

    if (const auto it = devices.find(get_address(deviceTree)); it != devices.end())
    {
        auto& device = it->second.device;

        device.GetGattServicesAsync(BluetoothCacheMode::Uncached).Completed([wr = WeakReference(this), vt = deviceTree](const IAsyncOperation<GattDeviceServicesResult>& sender, AsyncStatus status) mutable
                                                                            {
                    if (auto* p = wr.get())
                    {
                        if (status != AsyncStatus::Completed)
                        {
                            LOG(fmt::format("Bluetooth: GetGattServicesAsync failed: {}", winrt_util::to_string(status)));
                            return;
                        }

                        // Collect service UUIDs while holding the lock
                        std::vector<juce::String> serviceUuids;
                        {
                            const ScopedLock lock(p->devicesLock);
                            if (const auto   it = p->devices.find(get_address(vt)); it != p->devices.end())
                            {
                                for (const auto& s : sender.GetResults().Services())
                                {
                                    it->second.services.push_back(s);
                                    serviceUuids.push_back(winrt_util::guid_to_uuid(s.Uuid()).toDashedString());
                                }
                            }
                        }

                        // Modify ValueTree on message thread
                        MessageManager::callAsync([vt, serviceUuids = std::move(serviceUuids)]() mutable
                        {
                            for (const auto& uuid : serviceUuids)
                            {
                                auto svt = ValueTree{ID::SERVICE, {{ID::uuid, uuid}}};
                                vt.appendChild(svt, nullptr);
                            }

                            message(vt, ID::SERVICES_DISCOVERED);
                        });
                    } });
    }
}

void BleAdapter::Impl::discoverCharacteristics(const ValueTree& vt)
{
    jassert(vt.hasType(ID::SERVICE));

    const ScopedLock lock(devicesLock);

    if (const auto it = devices.find(get_address(vt.getParent())); it != devices.end())
    {
        const auto guid     = winrt_util::uuid_to_guid(vt.getProperty(ID::uuid).toString());
        auto&      services = it->second.services;

        if (const auto iit = std::find_if(services.cbegin(), services.cend(), [&](const auto& s)
                                          { return s.Uuid() == guid; });
            iit != services.cend())
        {
            iit->RequestAccessAsync().Completed([wr = WeakReference(this), s = *iit, svt = vt, addr = get_address(vt.getParent())](const auto&, AsyncStatus status)
                                                {
                if (status != AsyncStatus::Completed)
                {
                    LOG(fmt::format("Bluetooth: RequestAccessAsync failed: {}", winrt_util::to_string(status)));
                    return;
                }

                //==============================================================================
                s.GetCharacteristicsAsync().Completed([wr, svt = svt, addr](const auto& sender, AsyncStatus status) mutable
                {
                    if (auto* p = wr.get())
                    {
                        if (status != AsyncStatus::Completed)
                        {
                            LOG(fmt::format("Bluetooth: GetCharacteristicsAsync failed: {}", winrt_util::to_string(status)));
                            return;
                        }

                        //==========================================================================
                        const auto res      = sender.GetResults();

                        if (res.Status() != GattCommunicationStatus::Success)
                        {
                            LOG(fmt::format("Bluetooth: Error getting characteristics: {}", winrt_util::to_string(res.Status())));

                            if (res.Status() == GattCommunicationStatus::ProtocolError)
                                LOG(fmt::format("Protocol error: {}", static_cast<int>(res.ProtocolError().Value())));

                            return;
                        }

                        // Collect characteristic info while holding the lock
                        struct CharInfo { juce::String uuid; bool canWriteWithResponse; bool canWriteWithoutResponse; };
                        std::vector<CharInfo> charInfos;

                        {
                            const ScopedLock lock(p->devicesLock);
                            if (const auto   it = p->devices.find(addr); it != p->devices.end())
                            {
                                for (const auto& c : res.Characteristics())
                                {
                                    const auto test_property = [&](GattCharacteristicProperties prop)
                                    {
                                        return static_cast<bool>(static_cast<uint32_t>(c.CharacteristicProperties()) >> static_cast<uint32_t>(prop));
                                    };

                                    it->second.characteristics.push_back(c);
                                    charInfos.push_back({
                                        winrt_util::guid_to_uuid(c.Uuid()).toDashedString(),
                                        test_property(GattCharacteristicProperties::Write),
                                        test_property(GattCharacteristicProperties::WriteWithoutResponse)
                                    });
                                }
                            }
                        }

                        // Modify ValueTree on message thread
                        MessageManager::callAsync([svt, charInfos = std::move(charInfos)]() mutable
                        {
                            for (const auto& info : charInfos)
                            {
                                svt.appendChild({ID::CHARACTERISTIC, {
                                    {ID::uuid, info.uuid},
                                    {ID::can_write_with_response, info.canWriteWithResponse},
                                    {ID::can_write_without_response, info.canWriteWithoutResponse},
                                }}, nullptr);
                            }
                        });
                    }
                }); });
        }
    }
}

void BleAdapter::Impl::enableNotifications(const ValueTree& charact, bool shouldIndicate = false)
{
    jassert(charact.hasType(ID::CHARACTERISTIC));

    const auto address = get_address(getAncestor(charact, ID::BLUETOOTH_DEVICE));

    const ScopedLock lock(devicesLock);

    if (const auto it = devices.find(address); it != devices.end())
    {
        const auto uuid = charact.getProperty(ID::uuid).toString();
        const auto guid = winrt_util::uuid_to_guid(uuid);

        auto& ch = it->second.characteristics;
        if (const auto iit = std::find_if(ch.begin(), ch.end(), [&](const auto& c)
                                          { return c.Uuid() == guid; });
            iit != ch.end())
        {
            const auto type = shouldIndicate
                                      ? GattClientCharacteristicConfigurationDescriptorValue::Indicate
                                      : GattClientCharacteristicConfigurationDescriptorValue::Notify;

            iit->WriteClientCharacteristicConfigurationDescriptorWithResultAsync(type).Completed(
                    [charact](const auto& sender, [[maybe_unused]] AsyncStatus status)
                    {
                        jassert(status == AsyncStatus::Completed);

                        const auto res         = sender.GetResults();
                        const auto comm_status = res.Status();

                        if (comm_status != GattCommunicationStatus::Success)
                        {
                            LOG(fmt::format("Error enabling notifications: {}", winrt_util::to_string(comm_status)));

                            if (comm_status == GattCommunicationStatus::ProtocolError)
                                LOG(fmt::format("Protocol error: {}", static_cast<int>(res.ProtocolError().Value())));

                            return;
                        }

                        MessageManager::callAsync([charact]()
                        {
                            message(charact, ID::NOTIFICATIONS_ARE_ENABLED);
                        });
                    });

            iit->ValueChanged(
                    [wr = juce::WeakReference(this), address, uuid](const GattCharacteristic&, const GattValueChangedEventArgs& args)
                    {
                        if (auto* p = wr.get())
                        {
                            const auto buf = args.CharacteristicValue();

                            const ScopedLock lock(p->devicesLock);

                            if (const auto it = p->devices.find(address); it != p->devices.end())
                                it->second.callbacks.valueChanged(uuid, gsl::as_bytes(gsl::make_span(buf.data(), buf.Length())));
                        }
                    });
        }
    }
}

void BleAdapter::Impl::startDeviceWatcher()
{
    if (deviceWatcher.Status() == DeviceWatcherStatus::Started || deviceWatcher.Status() == DeviceWatcherStatus::EnumerationCompleted)
    {
        deviceWatcher.Stop();

        while (deviceWatcher.Status() == DeviceWatcherStatus::Stopping)
            std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(50));
    }

    deviceWatcher.Start();
}

//======================================================================================================================
BleAdapter::BleAdapter(ValueTree::Listener& l)
{
    state.addListener(&l);

    impl = std::make_unique<Impl>(state);
}

BleAdapter::BleAdapter() : impl(std::make_unique<Impl>(state)) { startTimer(500); }

BleAdapter::~BleAdapter() = default;

BleDevice BleAdapter::connect(const ValueTree& device, const BleDevice::Callbacks& callbacks) const
{
    jassert(device.isValid());
    jassert(device.hasType(ID::BLUETOOTH_DEVICE));

    impl->connect(device, callbacks);

    return BleDevice(device);
}

void BleAdapter::disconnect(const BleDevice& device)
{
    if (!device.state.isValid())
        return;

    DBG(fmt::format("Disconnect device:\n{}", device.state));

    const ScopedLock lock(impl->devicesLock);

    if (const auto it = impl->devices.find(get_address(device.state)); it != impl->devices.end())
    {
        it->second.device.Close();
        impl->devices.erase(it);

        state.removeChild(device.state, nullptr);
    }
}

size_t BleAdapter::getMaximumValueLength(const BleDevice& device)
{
    return static_cast<size_t>((int) device.state.getProperty(ID::max_pdu_size, 0));
}

//======================================================================================================================
void BleDevice::write(BleAdapter& adapter, const Uuid& charactUuid, gsl::span<const gsl::byte> data, bool withResponse)
{
    for (const auto& s: state)
        if (s.hasType(ID::SERVICE))
            if (const auto c = s.getChildWithProperty(ID::uuid, charactUuid.toDashedString()); c.isValid())
                adapter.impl->write(c, data, withResponse);
}

} // namespace genki

#pragma warning(pop)

#endif // JUCE_WINDOWS
