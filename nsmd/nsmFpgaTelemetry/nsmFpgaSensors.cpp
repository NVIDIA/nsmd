// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "nsmFpgaSensors.hpp"

#include "nsmDevice.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <variant>

namespace nsm
{

namespace
{
#ifdef NVIDIA_SHMEM
std::unique_ptr<SMBusSensorBytesConverter>
    shmemConverter(const std::string& sensorType)
{
    if (sensorType == "temperature")
    {
        return std::make_unique<SMBPBITempSMBusSensorBytesConverter>();
    }
    if (sensorType == "power")
    {
        return std::make_unique<SMBPBIPowerSMBusSensorBytesConverter>();
    }
    return std::make_unique<Uint64SMBusSensorBytesConverter>();
}
#endif

// One interface instance per object path, shared by the items that publish
// on that path and dropped with the last of them
template <typename Intf>
std::shared_ptr<Intf> sharedInterface(sdbusplus::bus_t& bus,
                                      const std::string& path, bool& created)
{
    static std::map<std::string, std::weak_ptr<Intf>> instances;
    created = false;
    if (auto existing = instances[path].lock())
    {
        return existing;
    }
    auto instance = std::make_shared<Intf>(bus, path.c_str());
    instances[path] = instance;
    created = true;
    return instance;
}

std::shared_ptr<AssociationDefinitionsInft>
    makeAssociations(sdbusplus::bus_t& bus, const FpgaSensorInfo& info)
{
    if (info.associations.empty())
    {
        return nullptr;
    }
    bool created = false;
    auto intf = sharedInterface<AssociationDefinitionsInft>(
        bus, info.objectPath, created);
    if (created)
    {
        intf->associations(utils::getAssociations(info.associations));
    }
    return intf;
}

std::shared_ptr<NsmNumericSensorValueAggregate>
    makeValueAggregate(sdbusplus::bus_t& bus, const FpgaSensorInfo& info,
                       SensorUnit unit)
{
    const auto& spec = info.rule->numeric;
    return std::make_shared<NsmNumericSensorValueAggregate>(
#ifdef NVIDIA_SHMEM
        std::make_unique<NsmNumericSensorShmem>(
            info.name, spec.sensor_type, info.chassisAssociation,
            shmemConverter(spec.sensor_type)),
#endif
        std::make_unique<NsmNumericSensorDbusValue>(
            bus, info.name, spec.sensor_type, unit, info.associations,
            info.physicalContext, nullptr,
            std::numeric_limits<double>::infinity(), spec.max_value,
            spec.min_value, nullptr, nullptr));
}

// Text of every identity field of the rule, false when one does not decode
bool identityTexts(
    const nsm_fpga_rule& rule, std::span<const uint8_t> capture,
    std::span<const uint8_t> group,
    std::vector<std::pair<const nsm_fpga_identity_field*, std::string>>& out)
{
    size_t count = 0;
    const auto* fields = nsm_fpga_rule_identity(&rule, &count);
    for (size_t i = 0; fields != nullptr && i < count; ++i)
    {
        std::array<char, NSM_FPGA_IDENTITY_TEXT_MAX> text{};
        if (!nsm_fpga_decode_identity(&fields[i], capture.data(),
                                      capture.size(), group.data(),
                                      group.size(), text.data(), text.size()))
        {
            return false;
        }
        out.emplace_back(&fields[i], std::string(text.data()));
    }
    return !out.empty();
}

// Elements of an ARRAY rule up to the first that does not fit the capture
std::vector<double> decodeArray(const nsm_fpga_rule& rule,
                                std::span<const uint8_t> capture,
                                std::span<const uint8_t> group)
{
    std::vector<double> elements;
    for (size_t i = 0; i < rule.numeric.element_count; ++i)
    {
        double value = 0.0;
        if (!nsm_fpga_decode_element(&rule.numeric, i, capture.data(),
                                     capture.size(), group.data(), group.size(),
                                     &value))
        {
            break;
        }
        elements.push_back(value);
    }
    return elements;
}

} // namespace

std::optional<SensorUnit>
    NsmFpgaSensor::unitForSensorType(const std::string& sensorType)
{
    if (sensorType == "temperature")
    {
        return SensorUnit::DegreesC;
    }
    if (sensorType == "voltage")
    {
        return SensorUnit::Volts;
    }
    if (sensorType == "current")
    {
        return SensorUnit::Amperes;
    }
    if (sensorType == "power")
    {
        return SensorUnit::Watts;
    }
    return std::nullopt;
}

void NsmFpgaSensor::publishIdentity(const nsm_fpga_identity_field& field,
                                    const std::string& text)
{
    if (!assetIntf)
    {
        return;
    }
    switch (field.asset)
    {
        case NSM_FPGA_ASSET_MODEL:
            assetIntf->model(text);
            break;
        case NSM_FPGA_ASSET_PART:
            assetIntf->partNumber(text);
            break;
        case NSM_FPGA_ASSET_MANUFACTURER:
            assetIntf->manufacturer(text);
            break;
        case NSM_FPGA_ASSET_REVISION:
            if (revisionIntf)
            {
                revisionIntf->version(text);
            }
            break;
        default:
            assetIntf->serialNumber(text);
            break;
    }
}

void NsmFpgaSensor::clearIdentity()
{
    size_t count = 0;
    const auto* fields = nsm_fpga_rule_identity(rule, &count);
    for (size_t i = 0; fields != nullptr && i < count; ++i)
    {
        publishIdentity(fields[i], "");
    }
}

void NsmFpgaSensor::publishArray(const std::vector<double>& elements)
{
    using LaneStatus = FpgaLaneStatusIntf::LaneStatus;
    std::vector<LaneStatus> flags;
    for (double element : elements)
    {
        flags.push_back(element != 0.0 ? LaneStatus::Asserted
                                       : LaneStatus::Clear);
    }
    switch (rule->numeric.vector)
    {
        case NSM_FPGA_VECTOR_TX_POWER:
            metricsIntf->txOutputPowerMilliWatts(elements);
            break;
        case NSM_FPGA_VECTOR_RX_POWER:
            metricsIntf->rxInputPowerMilliWatts(elements);
            break;
        case NSM_FPGA_VECTOR_TX_FAULT:
            laneStatusIntf->txFault(flags);
            break;
        case NSM_FPGA_VECTOR_RX_LOS:
            laneStatusIntf->rxLossOfSignal(flags);
            break;
        default:
            break;
    }
}

std::optional<double> NsmFpgaSensor::decode(const nsm_fpga_rule& rule,
                                            std::span<const uint8_t> capture)
{
    double value = 0.0;
    if (!nsm_fpga_decode_numeric(&rule.numeric, capture.data(), capture.size(),
                                 nullptr, 0, &value))
    {
        return std::nullopt;
    }
    return value;
}

NsmFpgaSensor::NsmFpgaSensor(sdbusplus::bus_t& bus, const FpgaSensorInfo& info,
                             std::shared_ptr<NsmFpgaStatus> status,
                             std::shared_ptr<NsmFpgaController> controller) :
    NsmObject(info.name, info.type), rule(info.rule),
    objectPath(info.objectPath), status(std::move(status)),
    controller(std::move(controller)), refreshInterval(info.refreshIntervalUsec)
{
    if (rule->kind == NSM_FPGA_RULE_NUMERIC)
    {
        // Numeric rule: a scaled Sensor.Value, shmem and D-Bus
        sensorValue = makeValueAggregate(
            bus, info, unitForSensorType(rule->numeric.sensor_type).value());
        return;
    }
    associationIntf = makeAssociations(bus, info);
    bool created = false;
    if (rule->kind == NSM_FPGA_RULE_IDENTITY)
    {
        // Identity rule: Asset fields holding the one-time identifiers
        assetIntf = sharedInterface<AssetIntf>(bus, objectPath, created);
        size_t count = 0;
        const auto* fields = nsm_fpga_rule_identity(rule, &count);
        for (size_t i = 0; fields != nullptr && i < count; ++i)
        {
            if (fields[i].asset == NSM_FPGA_ASSET_REVISION)
            {
                revisionIntf =
                    sharedInterface<FpgaRevisionIntf>(bus, objectPath, created);
            }
        }
        return;
    }
    if (rule->kind == NSM_FPGA_RULE_ARRAY)
    {
        // Array rule: one per-lane vector, on the metrics or lane status
        const auto vector = rule->numeric.vector;
        if (vector == NSM_FPGA_VECTOR_TX_POWER ||
            vector == NSM_FPGA_VECTOR_RX_POWER)
        {
            metricsIntf = sharedInterface<PortOpticalModuleMetricsIntf>(
                bus, objectPath, created);
        }
        else
        {
            laneStatusIntf =
                sharedInterface<FpgaLaneStatusIntf>(bus, objectPath, created);
        }
        publishArray({});
        return;
    }
    if (const auto role = NsmFpgaPortObject::role(info.portProperty))
    {
        // Link item: one property of the Port object at the path
        port = NsmFpgaPortObject::getOrCreate(bus, objectPath, created);
        portRole = *role;
        port->describe(info.portType, info.portProtocol);
        port->addItem(getName(), portRole);
        return;
    }
    // State rule: OperationalStatus and Health on the object path
    operationalStatusIntf =
        std::make_unique<FpgaOperationalStatusIntf>(bus, objectPath.c_str());
    healthIntf = std::make_unique<FpgaHealthIntf>(bus, objectPath.c_str());
    availabilityIntf = std::make_unique<AvailabilityIntf>(bus,
                                                          objectPath.c_str());
    availabilityIntf->available(false);
    if (info.powerState)
    {
        powerStateIntf =
            std::make_unique<FpgaPowerStateIntf>(bus, objectPath.c_str());
        powerStateIntf->powerState(FpgaPowerStateIntf::State::Unknown);
    }
    operationalStatusIntf->state(
        FpgaOperationalStatusIntf::StateType::Starting);
    operationalStatusIntf->functional(true);
    healthIntf->health(FpgaHealthIntf::HealthType::OK);
}

void NsmFpgaSensor::publish(uint32_t value)
{
    raw = value;
    haveReading = true;
    // A word the map does not name is unavailable with a warning
    uint8_t state = NSM_FPGA_STATE_UNAVAILABLE_OFFLINE;
    uint8_t health = NSM_FPGA_HEALTH_WARNING;
    nsm_fpga_decode_state(rule, value, &state, &health);
    publishState(state, health);
    if (availabilityIntf)
    {
        availabilityIntf->available(true);
    }
}

void NsmFpgaSensor::publishState(uint8_t state, uint8_t health)
{
    using StateType = FpgaOperationalStatusIntf::StateType;
    using HealthType = FpgaHealthIntf::HealthType;
    if (port)
    {
        port->publish(getName(), state, health);
        return;
    }
    if (!operationalStatusIntf || !healthIntf)
    {
        return;
    }
    static constexpr std::array<StateType, 6> states = {
        StateType::Enabled,
        StateType::Starting,
        StateType::StandbyOffline,
        StateType::Absent,
        StateType::UnavailableOffline,
        StateType::Fault};
    static constexpr std::array<HealthType, 3> healths = {
        HealthType::OK, HealthType::Warning, HealthType::Critical};
    const StateType type =
        state < states.size() ? states[state] : StateType::UnavailableOffline;
    operationalStatusIntf->state(type);
    operationalStatusIntf->functional(type != StateType::Fault &&
                                      type != StateType::UnavailableOffline);
    healthIntf->health(health < healths.size() ? healths[health]
                                               : HealthType::Warning);
    if (powerStateIntf)
    {
        using PowerType = FpgaPowerStateIntf::State;
        // Ready is On, bring-up PoweringOn, a low power module Off
        static constexpr std::array<PowerType, 6> powers = {
            PowerType::On,      PowerType::PoweringOn, PowerType::Off,
            PowerType::Unknown, PowerType::Unknown,    PowerType::Unknown};
        powerStateIntf->powerState(state < powers.size() ? powers[state]
                                                         : PowerType::Unknown);
    }
}

void NsmFpgaSensor::markUnavailable()
{
    haveReading = false;
    if (availabilityIntf)
    {
        availabilityIntf->available(false);
    }
    if (sensorValue)
    {
        sensorValue->updateReading(std::numeric_limits<double>::quiet_NaN());
        return;
    }
    if (rule->kind == NSM_FPGA_RULE_ARRAY)
    {
        publishArray({});
        return;
    }
    if (rule->kind == NSM_FPGA_RULE_IDENTITY)
    {
        clearIdentity();
        return;
    }
    if (port)
    {
        port->markUnavailable(getName());
        return;
    }
    // No reading: the whole state object says so, not only its State
    publishState(NSM_FPGA_STATE_UNAVAILABLE_OFFLINE, NSM_FPGA_HEALTH_OK);
}

void NsmFpgaSensor::publishGroupState(uint8_t state, uint8_t health)
{
    // Only a state object can show the group's state; the rest go empty
    markUnavailable();
    publishState(state, health);
}

int NsmFpgaSensor::finish(int rc, uint32_t value)
{
    bool ok = rc == NSM_SW_SUCCESS;
    if (!ok && controller->lastEndedGroup())
    {
        // A normal outcome of the round, not a read failure
        publishGroupState(controller->lastGroupState(),
                          controller->lastGroupHealth());
        return NSM_SW_SUCCESS;
    }
    if (ok && rule->kind != NSM_FPGA_RULE_IDENTITY &&
        value == NSM_FPGA_WORD_UNAVAILABLE)
    {
        // Absent or unpowered slice: report unavailable, not decoded bits
        markUnavailable();
        return NSM_SW_SUCCESS;
    }
    // A validity condition that fails is an empty result, not a failure
    bool applies = true;
    if (ok && rule->valid_if.buffer != NSM_FPGA_BUF_NONE)
    {
        const auto capture = controller->lastCapture();
        const auto group = controller->roundBuffer();
        applies = nsm_fpga_compare_holds(&rule->valid_if, capture.data(),
                                         capture.size(), group.data(),
                                         group.size(), value);
    }
    if (ok && applies)
    {
        ok = publishCapture(value);
    }
    if (!ok || !applies)
    {
        markUnavailable();
    }
    // Logs the first result, every change of outcome and so each recovery
    const bool changed = !lastReadOk.has_value() || *lastReadOk != ok;
    lastReadOk = ok;
    if (changed)
    {
        if (ok)
        {
            lg2::info("FPGA read {NAME} returned {RAW} after {N} exchanges",
                      "NAME", getName(), "RAW", lg2::hex, value, "N",
                      controller->lastExchanges());
        }
        else
        {
            lg2::error("FPGA read {NAME} failed: {ERROR}", "NAME", getName(),
                       "ERROR",
                       rc == NSM_SW_SUCCESS ? "value outside the valid range"
                                            : controller->lastError());
        }
    }
    if (ok)
    {
        return NSM_SW_SUCCESS;
    }
    // A read that worked but produced an invalid value is still an error
    return rc == NSM_SW_SUCCESS ? NSM_SW_ERROR : rc;
}

bool NsmFpgaSensor::publishCapture(uint32_t value)
{
    switch (rule->kind)
    {
        case NSM_FPGA_RULE_NUMERIC:
        {
            const auto numeric = decode(*rule, controller->lastCapture());
            if (!numeric)
            {
                return false;
            }
            sensorValue->updateReading(*numeric);
            return true;
        }
        case NSM_FPGA_RULE_IDENTITY:
        {
            std::vector<std::pair<const nsm_fpga_identity_field*, std::string>>
                texts;
            if (!identityTexts(*rule, controller->lastCapture(),
                               controller->roundBuffer(), texts))
            {
                return false;
            }
            // One-time identifiers: each field on its Asset property
            for (const auto& [field, text] : texts)
            {
                publishIdentity(*field, text);
            }
            haveReading = true;
            return true;
        }
        case NSM_FPGA_RULE_ARRAY:
        {
            const auto elements = decodeArray(*rule, controller->lastCapture(),
                                              controller->roundBuffer());
            if (elements.empty())
            {
                return false;
            }
            publishArray(elements);
            haveReading = true;
            return true;
        }
        default:
            publish(value);
            return true;
    }
}

requester::Coroutine NsmFpgaSensor::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    if (!controller || !nsmDevice)
    {
        // coverity[missing_return]
        co_return NSM_SW_ERROR_NULL;
    }
    const auto fpgaStatus = controller->statusObject();
    if (fpgaStatus && !fpgaStatus->enabled())
    {
        markUnavailable();
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    uint32_t value = 0;
    auto rc = co_await controller->read(nsmDevice, rule, value);
    // coverity[missing_return]
    co_return finish(rc, value);
}

requester::Coroutine
    NsmFpgaSensor::updateInRound(std::shared_ptr<NsmDevice> nsmDevice,
                                 uint64_t roundStartUsec)
{
    if (!controller || !nsmDevice)
    {
        // coverity[missing_return]
        co_return NSM_SW_ERROR_NULL;
    }
    uint32_t value = 0;
    if (!controller->roundEndedEarly() &&
        rule->run_if.buffer != NSM_FPGA_BUF_NONE)
    {
        // The item does not apply this round: no rows, nothing stale
        const auto group = controller->roundBuffer();
        if (!nsm_fpga_compare_holds(&rule->run_if, nullptr, 0, group.data(),
                                    group.size(), 0))
        {
            markUnavailable();
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        }
    }
    auto rc = co_await controller->readItem(nsmDevice, rule, value);
    if (rc == NSM_SW_SUCCESS)
    {
        // Stamped with the round start so the next round due at the item's
        // interval finds it due as well
        lastReadUsec = roundStartUsec;
    }
    // coverity[missing_return]
    co_return finish(rc, value);
}

NsmFpgaGroup::NsmFpgaGroup(const std::string& name) :
    NsmObject(name, "NSM_FpgaGroup")
{}

void NsmFpgaGroup::bind(std::shared_ptr<NsmFpgaController> groupController,
                        const nsm_fpga_group* groupDefinition)
{
    controller = std::move(groupController);
    definition = groupDefinition;
}

void NsmFpgaGroup::addItem(std::shared_ptr<NsmFpgaSensor> item)
{
    refreshInterval = items.empty() ? item->refreshIntervalUsec()
                                    : std::min(refreshInterval,
                                               item->refreshIntervalUsec());
    items.push_back(std::move(item));
}

bool NsmFpgaGroup::needsUpdate(const uint64_t& nowUsec) const
{
    return lastRoundUsec == 0 || pending ||
           nowUsec - lastRoundUsec > refreshInterval;
}

requester::Coroutine NsmFpgaGroup::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    lastRoundUsec = NsmFpgaController::nowUsec();
    if (!controller)
    {
        // Unbound group: every item reads in its own episode
        for (auto& item : items)
        {
            co_await item->update(nsmDevice);
        }
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    if (!controller->enabled())
    {
        for (auto& item : items)
        {
            item->markUnavailable();
        }
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    // One round: the group's setup rows, the items in series from where the
    // last round stopped, its cleanup
    bool ran = false;
    const size_t start = pending ? nextItem : 0;
    pending = false;
    nextItem = 0;
    const uint64_t now = NsmFpgaController::nowUsec();
    auto body = [this, nsmDevice, &ran, start, now]() -> requester::Coroutine {
        ran = true;
        size_t started = 0;
        for (size_t step = 0; step < items.size(); ++step)
        {
            const size_t index = (start + step) % items.size();
            auto& item = items[index];
            if (item->getRule()->kind == NSM_FPGA_RULE_IDENTITY &&
                item->hasReading())
            {
                // Identity is read once; it re-runs after it was cleared
                continue;
            }
            if (item->getRule()->command_count > 0 &&
                !controller->roundEndedEarly() && !item->isDue(now))
            {
                // Inside its own interval the item keeps its reading
                continue;
            }
            if (started > 0 && controller->episodeOverSlice())
            {
                // The episode used its slice: the rest wait for the next
                // round, due at once, so no episode holds the grant long
                pending = true;
                nextItem = index;
                break;
            }
            started++;
            co_await item->updateInRound(nsmDevice, now);
        }
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    };
    auto rc = co_await controller->runGroup(nsmDevice, definition,
                                            getName().c_str(), body);
    if (!ran)
    {
        // The round never reached the items (bank unavailable, fenced, open
        // refused, renew failed, shutting down): no item keeps a stale value
        for (auto& item : items)
        {
            item->markUnavailable();
        }
    }
    // coverity[missing_return]
    co_return rc;
}

namespace
{
std::map<std::string, std::shared_ptr<NsmFpgaGroup>>& groupRegistry()
{
    static std::map<std::string, std::shared_ptr<NsmFpgaGroup>> groups;
    return groups;
}
} // namespace

std::shared_ptr<NsmFpgaGroup>
    NsmFpgaGroup::getOrCreate(const uuid_t& uuid, const std::string& group,
                              bool& created)
{
    created = false;
    const std::string key = uuid + "/" + group;
    auto& registry = groupRegistry();
    if (auto it = registry.find(key); it != registry.end())
    {
        return it->second;
    }
    auto object = std::make_shared<NsmFpgaGroup>(group);
    registry[key] = object;
    created = true;
    return object;
}

void NsmFpgaGroup::clear()
{
    groupRegistry().clear();
}

requester::Coroutine createNsmFpgaTelemetry(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath)
{
    // Entity manager can re-send InterfacesAdded (e.g. on restart). Create each
    // config path once so the D-Bus objects and the group's per-round reads do
    // not double up; a failed attempt is not recorded, so it can still retry.
    static std::set<std::string> createdPaths;
    if (createdPaths.count(objPath))
    {
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    }
    auto& bus = utils::DBusHandler::getBus();
    auto properties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());
    FpgaSensorInfo info{};
    info.type = interface.substr(interface.find_last_of('.') + 1);
    info.name = utils::makeDBusNameValid(
        utils::getOptionalString(properties, "Name").value_or(""));
    const uuid_t uuid =
        utils::getOptionalString(properties, "UUID").value_or("");
    info.physicalContext =
        utils::getOptionalString(properties, "PhysicalContext").value_or("");
    info.objectPath =
        utils::getOptionalString(properties, "InventoryObjPath")
            .value_or("/xyz/openbmc_project/inventory/system/chassis/" +
                      info.name);
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      info.associations);
    if (const auto refreshMs = utils::getOptionalUnsigned(properties,
                                                          "RefreshIntervalMs");
        refreshMs && *refreshMs > 0)
    {
        info.refreshIntervalUsec = *refreshMs *
                                   NsmFpgaController::usecPerMillisecond;
    }
    if (const auto powerState = properties.find("PowerState");
        powerState != properties.end())
    {
        const auto* wanted = std::get_if<bool>(&powerState->second);
        info.powerState = wanted != nullptr && *wanted;
    }
    for (const auto& association : info.associations)
    {
        if (association.forward == "chassis")
        {
            info.chassisAssociation = association.absolutePath;
        }
    }
    info.portProperty =
        utils::getOptionalString(properties, "PortProperty").value_or("");
    info.portType =
        utils::getOptionalString(properties, "PortType").value_or("");
    info.portProtocol =
        utils::getOptionalString(properties, "PortProtocol").value_or("");

    // The Name selects the rule: station, register and output
    const nsm_fpga_map* map = nsm_fpga_map_p0();
    info.rule = nsm_fpga_find_rule(map, info.name.c_str());
    if (info.rule == nullptr || !nsm_fpga_rule_valid(map, info.rule))
    {
        lg2::error(
            "NSM_FpgaTelemetry entry {NAME} has no rule yet, no sensor created",
            "NAME", info.name, "PATH", objPath);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    if (info.rule->kind == NSM_FPGA_RULE_NUMERIC &&
        !NsmFpgaSensor::unitForSensorType(info.rule->numeric.sensor_type))
    {
        lg2::error(
            "NSM_FpgaTelemetry entry rejected: rule {RULE} has no sensor unit",
            "RULE", info.rule->name);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    if (!info.portProperty.empty() &&
        (!NsmFpgaPortObject::role(info.portProperty) ||
         info.rule->kind != NSM_FPGA_RULE_STATE))
    {
        lg2::error(
            "NSM_FpgaTelemetry entry {NAME} rejected: PortProperty {PROPERTY} needs a known property and a state rule",
            "NAME", info.name, "PROPERTY", info.portProperty);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "NSM_FpgaTelemetry entry {NAME} rejected: UUID matches no NsmDevice",
            "UUID", uuid, "NAME", info.name);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    // The discovery check entry owns the status object and its path; an
    // item that arrives first polls once the check has created it
    auto status = NsmFpgaStatus::get(uuid);
    auto controller = NsmFpgaController::getOrCreate(uuid, status);
    auto sensor = std::make_shared<NsmFpgaSensor>(bus, info, status,
                                                  controller);
    // The item holds the rule and publishes; its group reads it, so it is
    // registered on the device but not scheduled on its own.
    nsmDevice->addDeviceSensors(sensor);
    const std::string groupId = info.rule->group ? info.rule->group
                                                 : info.rule->name;
    bool groupCreated = false;
    auto group = NsmFpgaGroup::getOrCreate(uuid, groupId, groupCreated);
    group->addItem(sensor);
    if (groupCreated)
    {
        group->bind(controller, nsm_fpga_find_group(map, groupId.c_str()));
        // The group is the scheduled unit, polled round robin
        std::shared_ptr<NsmObject> groupObject = group;
        nsmDevice->addSensor(groupObject, PollingType::RoundRobin);
    }
    createdPaths.insert(objPath);
    lg2::info(
        "Created NSM FPGA sensor {NAME} from rule {RULE} in group {GROUP}",
        "UUID", uuid, "NAME", info.name, "RULE", info.rule->name, "GROUP",
        groupId, "CMDS", info.rule->command_count);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmFpgaTelemetry,
    "xyz.openbmc_project.Configuration.NSM_FpgaTelemetry")

} // namespace nsm
