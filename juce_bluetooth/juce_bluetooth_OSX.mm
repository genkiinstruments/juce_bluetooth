#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_MAC

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdirect-ivar-access"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wdeprecated"

#include <range/v3/view.hpp>
#include <gsl/span_ext>
#include <fmt/format.h>

#include <CoreBluetooth/CoreBluetooth.h>

#include "juce_bluetooth.h"
#include "include/native/macos/corebluetooth_utils.h"
#include "include/native/macos/retained.h"
#include "include/format.h"
#include "include/ranges.h"

using namespace juce;
using namespace genki::corebluetooth_utils;

#ifndef GENKI_BLUETOOTH_LOG_ENABLED
#define GENKI_BLUETOOTH_LOG_ENABLED 0
#endif

#define LOG(text) JUCE_BLOCK_WITH_FORCED_SEMICOLON(if (GENKI_BLUETOOTH_LOG_ENABLED) DBG(text); )

namespace genki {

//======================================================================================================================
struct LambdaTimer : public juce::Timer
{
    LambdaTimer(std::function<void()> cb, int intervalMs) : callback(std::move(cb)) { startTimer(intervalMs); }

    ~LambdaTimer() override { stopTimer(); }

    void timerCallback() override { if (callback) callback(); }

    std::function<void()> callback;
};

} // namespace genki

//======================================================================================================================
@interface OSXAdapter : NSObject
@end

@interface OSXAdapter () <CBCentralManagerDelegate, CBPeripheralDelegate>
{
    ValueTree                                                               valueTree;
    CriticalSection                                                         peripheralsLock;
    std::map<String, std::pair<CBPeripheral*, genki::BleDevice::Callbacks>> peripherals;
}

@property(nonatomic, strong) CBCentralManager* _Nullable centralManager;

@end

@implementation OSXAdapter

- (nonnull instancetype)initWithValueTree:(const ValueTree&)vt {
    jassert(vt.hasType(ID::BLUETOOTH_ADAPTER));

    self = [super init];

    valueTree = vt;

    return self;
}

- (void)setup {
    self.centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
}

- (void)dealloc {
    const ScopedLock lock(peripheralsLock);

    for (auto& [_, p]: peripherals)
    {
        auto& [peripheral, cbs] = p;
        ignoreUnused(cbs);

        // TODO: Seems like the retain count is unusually high during destruction, might have something to do with
        //       [peripheral discoverCharacteristicsForService]. Need to figure out if that's normal...
        [peripheral release];
    }

    [super dealloc];
}

//======================================================================================================================
- (void)centralManager:(nonnull CBCentralManager*)central didConnectPeripheral:(nonnull CBPeripheral*)peripheral {
    juce::ignoreUnused(central);

    const auto name         = String([[peripheral name] UTF8String]);
    const auto addr_str     = get_address_string([peripheral identifier]);
    const bool is_connected = [peripheral state] == CBPeripheralStateConnected;
    const auto max_pdu      = (int) [self getMaximumValueLengthForPeripheral:peripheral];
    const auto now          = (int) Time::getMillisecondCounter();

    if (auto ch = valueTree.getChildWithProperty(ID::address, addr_str); ch.isValid())
    {
        ch.setProperty(ID::is_connected, is_connected, nullptr);
        ch.setProperty(ID::max_pdu_size, max_pdu, nullptr);
    }
    else
    {
        valueTree.appendChild({ID::BLUETOOTH_DEVICE, {{ID::name, name}, {ID::address, addr_str}, {ID::is_connected, is_connected}, {ID::last_seen, now}, {ID::max_pdu_size, max_pdu}}}, nullptr);
    }
}

- (void)centralManager:(CBCentralManager*)central didFailToConnectPeripheral:(CBPeripheral*)peripheral error:(NSError*)error {
    LOG(fmt::format("Bluetooth: Failed to connect: {}", [error.localizedDescription UTF8String]));
}

- (void) centralManager:(nonnull
CBCentralManager*)central
didDisconnectPeripheral:
        (nonnull
CBPeripheral*)peripheral
                  error:
                          (nullable
                  NSError*)error {
    juce::ignoreUnused(central, error);

    const auto addr_str = get_address_string([peripheral identifier]);

    if (error)
    {
        LOG(fmt::format("Bluetooth device disconnected, error code:{}, {}",
                [error code], [[error localizedDescription] UTF8String]));
    }

    const ScopedLock lock(peripheralsLock);

    if (const auto it = peripherals.find(addr_str); it != peripherals.end())
    {
        [it->second.first release];

        peripherals.erase(it);
    }

    valueTree.removeChild(valueTree.getChildWithProperty(ID::address, addr_str), nullptr);
}

- (void)centralManager:(nonnull CBCentralManager*)central didDiscoverPeripheral:(nonnull CBPeripheral*)peripheral
     advertisementData:(nonnull NSDictionary<NSString*, id>*)advertisementData
                  RSSI:(nonnull NSNumber*)RSSI {
    juce::ignoreUnused(central, advertisementData);

    [self addPeripheral:peripheral RSSI:RSSI];
}

- (void)centralManagerDidUpdateState:(nonnull CBCentralManager*)central {
    juce::ignoreUnused(central);

    const auto status = [self.centralManager state];

    valueTree.setProperty(ID::status, static_cast<int>(
            status == CBManagerStatePoweredOff ? AdapterStatus::PoweredOff :
            status == CBManagerStatePoweredOn ? AdapterStatus::PoweredOn :
            AdapterStatus::Disabled), nullptr);
}

//======================================================================================================================
- (void)peripheral:(nonnull CBPeripheral*)peripheral didUpdateNotificationStateForCharacteristic:(nonnull CBCharacteristic*)characteristic error:(nullable NSError*)error {

    const auto addr_str = get_address_string([peripheral identifier]);

    for (const auto& vt: valueTree.getChildWithProperty(ID::address, addr_str))
        if (vt.hasType(ID::SERVICE))
            if (auto ch = vt.getChildWithProperty(ID::uuid, get_uuid_string([characteristic UUID])); ch.isValid())
                genki::message(ch, {ID::NOTIFICATIONS_ARE_ENABLED, {}});
}

- (void)peripheral:(nonnull CBPeripheral*)peripheral didDiscoverServices:(nullable NSError*)error {

    const auto addr_str = get_address_string([peripheral identifier]);
    auto       vt       = valueTree.getChildWithProperty(ID::address, addr_str);

    const NSArray<CBService*>* cb_services = [peripheral services];

    for (unsigned int i = 0; i < [cb_services count]; ++i)
    {
        CBService* service = cb_services[i];
        vt.appendChild({ID::SERVICE, {{ID::uuid, get_uuid_string([service UUID])}}}, nullptr);
    }

    genki::message(vt, ID::SERVICES_DISCOVERED);
}

- (void)peripheral:(nonnull CBPeripheral*)peripheral didDiscoverCharacteristicsForService:(nonnull CBService*)service error:(nullable NSError*)error {
    const auto addr_str     = get_address_string([peripheral identifier]);
    const auto service_uuid = get_uuid_string([service UUID]);

    auto vt = valueTree.getChildWithProperty(ID::address, addr_str).getChildWithProperty(ID::uuid, service_uuid);

    jassert(vt.isValid() && vt.hasType(ID::SERVICE));

    const NSArray<CBCharacteristic*>* cb_characts = [service characteristics];

    for (unsigned int i = 0; i < [cb_characts count]; ++i)
    {
        CBCharacteristic* charact = cb_characts[i];
        vt.appendChild({ID::CHARACTERISTIC, {
                {ID::uuid, get_uuid_string([charact UUID])},
                {ID::can_write_with_response, static_cast<bool>(charact.properties >> CBCharacteristicPropertyWrite)},
                {ID::can_write_without_response, static_cast<bool>(charact.properties >> CBCharacteristicPropertyWriteWithoutResponse)},
        }}, nullptr);
    }
}

- (void)peripheral:(nonnull CBPeripheral*)peripheral didUpdateValueForCharacteristic:(nonnull CBCharacteristic*)characteristic error:(nullable NSError*)error {
    if (auto* uuid = [[[characteristic service] peripheral] identifier]; uuid != nil)
    {
        const auto addr_str = get_address_string(uuid);

        const ScopedLock lock(peripheralsLock);

        if (const auto it = peripherals.find(addr_str); it != peripherals.end())
        {
            const NSData* ns_data = [characteristic value];
            const NSUInteger len = [ns_data length];
            const auto* bytes = static_cast<const unsigned char*>([ns_data bytes]);

            auto& [_, callbacks] = it->second;

            callbacks.valueChanged(
                    Uuid(get_uuid_string([characteristic UUID])),
                    gsl::as_bytes(gsl::make_span(bytes, static_cast<size_t>(len)))
            );
        }
    }
}

- (void)peripheral:(nonnull CBPeripheral*)peripheral didWriteValueForCharacteristic:(nonnull CBCharacteristic*)characteristic error:(nullable NSError*)error {
    if (auto* uuid = [[[characteristic service] peripheral] identifier]; uuid != nil)
    {
        const auto addr_str = get_address_string(uuid);

        if (error != nil)
            LOG(fmt::format("Bluetooth: Error writing characteristic: {}", [[error localizedDescription] UTF8String]));

        const ScopedLock lock(peripheralsLock);

        if (const auto it = peripherals.find(addr_str); it != peripherals.end())
        {
            auto& [_, callbacks] = it->second;

            if (callbacks.characteristicWritten) callbacks.characteristicWritten(Uuid(get_uuid_string([characteristic UUID])), error == nil);
        }
    }
}

//======================================================================================================================
- (CBPeripheral*)getPeripheral:(const Uuid&)address {
    jassert(!address.isNull());

    const ScopedLock lock(peripheralsLock);
    const auto       it = peripherals.find(address.toDashedString());

    return it != peripherals.end() ? it->second.first : nil;
}

- (void)connect:(const ValueTree&)device
  withCallbacks:
          (const genki::BleDevice::Callbacks&)callbacks {

    const String name    = device.getProperty(ID::name);
    const String address = device.getProperty(ID::address);

    LOG(fmt::format("Bluetooth: Connecting to device: {} ({})", name, address));

    const ScopedLock lock(peripheralsLock);

    if (const auto it = peripherals.find(address); it != peripherals.end())
    {
        auto& [p, cbs] = it->second;
        cbs = callbacks;

        [self.centralManager connectPeripheral:p options:nil];
    }
    else
    {
        jassertfalse;
        LOG(fmt::format("Bluetooth: Address not found: {}", address));
    }
}

- (void)disconnect:(const ValueTree&)device {
    const auto address = device.getProperty(ID::address).toString();

    const ScopedLock lock(peripheralsLock);

    if (const auto it = peripherals.find(address); it != peripherals.end())
    {
        [self.centralManager cancelPeripheralConnection:it->second.first];

        peripherals.erase(it);
        valueTree.removeChild(valueTree.getChildWithProperty(ID::address, address), nullptr);
    }
}

- (size_t)getMaximumValueLengthForPeripheral:(const CBPeripheral* _Nonnull)peripheral {
    if (@available(macOS 10.12, *))
        return [peripheral maximumWriteValueLengthForType:CBCharacteristicWriteType::CBCharacteristicWriteWithoutResponse];
    else
        return 20;
}

- (void)addPeripheral:(CBPeripheral* _Nonnull)peripheral
                 RSSI:
                         (NSNumber*)RSSI {
    const auto name         = String([[peripheral name] UTF8String]);
    const auto addr_str     = get_address_string([peripheral identifier]);
    const bool is_connected = [peripheral state] == CBPeripheralStateConnected;
    const auto rssi         = RSSI != nil ? static_cast<int>([RSSI intValue]) : 0;

    const auto now = (int) Time::getMillisecondCounter();

    if (auto ch = valueTree.getChildWithProperty(ID::address, addr_str); ch.isValid())
    {
        ch.setProperty(ID::rssi, rssi, nullptr);
        if (name.isNotEmpty())
            ch.setProperty(ID::name, name, nullptr);

        ch.setProperty(ID::last_seen, now, nullptr);
    }
    else
    {
        {
            const ScopedLock lock(peripheralsLock);

            if (const auto it = peripherals.find(addr_str); it == peripherals.end())
            {
                [peripheral setDelegate:self];

                peripherals.insert({addr_str, std::make_pair([peripheral retain], genki::BleDevice::Callbacks{})});
            }
        }

        valueTree.appendChild({ID::BLUETOOTH_DEVICE, {{ID::name, name}, {ID::address, addr_str}, {ID::rssi, rssi}, {ID::is_connected, is_connected}, {ID::last_seen, now}}}, nullptr);
    }
}

@end


//======================================================================================================================
namespace genki {

struct BleAdapter::Impl : private ValueTree::Listener
{
    explicit Impl(ValueTree);
    ~Impl()
    override;

    void write(const ValueTree& charact, gsl::span<const gsl::byte> data, bool withResponse) const
    {
        if (auto* ch = getCharacteristic(charact); ch != nil)
        {
            NSData* buf = [NSData dataWithBytes:data.data() length:(unsigned long) data.size()];
            const auto type = withResponse ? CBCharacteristicWriteWithResponse : CBCharacteristicWriteWithoutResponse;

            LOG("Write characteristic: " << charact.getProperty(ID::uuid).toString()
                                         << " with data: " << String::toHexString([buf bytes], static_cast<int>([buf length])));

            [[[ch service] peripheral] writeValue:buf forCharacteristic:ch type:type];
        }
    }

    [[nodiscard]] CBCharacteristic* getCharacteristic(const ValueTree& charact) const
    {
        jassert(charact.hasType(ID::CHARACTERISTIC));

        const auto& device = getAncestor(charact, ID::BLUETOOTH_DEVICE);
        jassert(device.isValid());

        if (auto* p = [adapter getPeripheral:device.getProperty(ID::address).toString()])
        {
            const NSArray<CBService*>* srv = [p services];

            for (unsigned int i = 0; i < [srv count]; ++i)
            {
                const NSArray<CBCharacteristic*>* chr = [srv[i] characteristics];

                for (unsigned int j = 0; j < [chr count]; ++j)
                    if (get_uuid_string([chr[j] UUID]) == charact.getProperty(ID::uuid).toString())
                        return chr[j];
            }
        }

        return nil;
    }

    //==================================================================================================================
    void valueTreeChildAdded(ValueTree& parent, ValueTree& child)
    override
    {
        if (child.hasType(ID::DISCOVER_SERVICES))
        {
            const auto& device = parent;
            jassert(device.hasType(ID::BLUETOOTH_DEVICE));

            if (auto* p = [adapter getPeripheral:device.getProperty(ID::address).toString()])
                [p discoverServices:nil];
        }
        if (child.hasType(ID::DISCOVER_CHARACTERISTICS))
        {
            const auto device = getAncestor(child, ID::BLUETOOTH_DEVICE);
            jassert(device.isValid());

            const auto& service = parent;
            jassert(service.hasType(ID::SERVICE));

            const CBUUID* cbuuid = corebluetooth_utils::from_juce_uuid(service.getProperty(ID::uuid).toString());

            if (auto* p = [adapter getPeripheral:device.getProperty(ID::address).toString()])
            {
                const NSArray<CBService*>* cb_services = [p services];

                for (unsigned int i = 0; i < [cb_services count]; ++i)
                    if ([[cb_services[i] UUID] isEqualTo:cbuuid])
                        [p discoverCharacteristics:nil forService:cb_services[i]];
            }
        }
        else if (child.hasType(ID::ENABLE_NOTIFICATIONS) || child.hasType(ID::ENABLE_INDICATIONS))
        {
            jassert(parent.hasType(ID::CHARACTERISTIC));

            if (auto* ch = getCharacteristic(parent); ch != nil)
                [[[ch service] peripheral] setNotifyValue:YES forCharacteristic:ch];
        }
        else if (child.hasType(ID::SCAN))
        {
            if (child.getProperty(ID::should_start))
            {
                using namespace ranges;

                const auto rng       = ValueTreeRange(child);
                const auto uuid_strs = rng | views::transform(property_as<String>(ID::uuid));

                LOG(fmt::format("Bluetooth: Starting scan for services:\n{}",
                        uuid_strs
                        | views::transform(to_string_view)
                        | views::join('\n')
                        | to<std::string>())
                );

                const auto cb_uuids = uuid_strs | views::transform(from_juce_uuid) | to<std::vector>();

                NSArray* ns_uuids = [NSArray arrayWithObjects:cb_uuids.data() count:cb_uuids.size()];

                connectedDevicePoll = std::make_unique<LambdaTimer>(
                        [this, us = Retained<NSArray*>(ns_uuids)]
                        {
                            NSArray<CBPeripheral*>* ps = [[adapter centralManager] retrieveConnectedPeripheralsWithServices:us.get()];

                            for (unsigned int i = 0; i < [ps count]; ++i)
                                [adapter addPeripheral:ps[i] RSSI:nil];
                        }, 500);

                [[adapter centralManager] scanForPeripheralsWithServices:ns_uuids
                                                                 options:@{CBCentralManagerScanOptionAllowDuplicatesKey: @(YES)}];
            }
            else
            {
                LOG("Bluetooth - Stopping scan...");
                [[adapter centralManager] stopScan];
                connectedDevicePoll = nullptr;
            }
        }
        else if (child.hasType(ID::BLUETOOTH_DEVICE))
        {
            LOG(fmt::format("Bluetooth: Device added: {} ({}), {}",
                    child.getProperty(ID::name).toString(),
                    child.getProperty(ID::address).toString(),
                    child.getProperty(ID::is_connected) ? "connected" : "not connected"));
        }
    }

    void valueTreeChildRemoved(ValueTree&, ValueTree&, int)
    override {}

    OSXAdapter* adapter = nullptr;
    ValueTree valueTree;

    std::unique_ptr<LambdaTimer> connectedDevicePoll;
};

BleAdapter::Impl::Impl(ValueTree
                       vt) : valueTree(std::move(vt))
{
    valueTree.addListener(this);

    adapter = [[OSXAdapter alloc] initWithValueTree:valueTree];
    [adapter setup];
}

BleAdapter::Impl::~Impl() { [adapter release]; }

//======================================================================================================================
BleAdapter::BleAdapter(ValueTree::Listener& l)
{
    state.addListener(&l);

    impl = std::make_unique<Impl>(state);
}

BleAdapter::BleAdapter() : impl(std::make_unique<Impl>(state)) { startTimer(500); }

BleAdapter::~BleAdapter() =
default;

BleDevice BleAdapter::connect(const ValueTree& device, const BleDevice::Callbacks& callbacks) const
{
    jassert(device.isValid());
    jassert(device.hasType(ID::BLUETOOTH_DEVICE));

    [impl->adapter connect:device withCallbacks:callbacks];

    return BleDevice(device);
}

void BleAdapter::disconnect(const BleDevice& device)
{
    LOG("Disconnect: " << device.state.toXmlString());

    [impl->adapter disconnect:device.state];
}

size_t BleAdapter::getMaximumValueLength(const BleDevice& device)
{
    if (const auto* p = [impl->adapter getPeripheral:device.state.getProperty(ID::address).toString()])
        return [impl->adapter getMaximumValueLengthForPeripheral:p];

    jassertfalse;
    return 0;
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

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif // JUCE_MAC
