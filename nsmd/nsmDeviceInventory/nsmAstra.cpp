// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES
/*
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nsmAstra.hpp"

#include "common/asyncScope.hpp"
#include "common/globals.hpp"
#include "common/sleep.hpp"
#include "common/spawn.hpp"
#include "nsmEvent.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace nsm
{

namespace
{

constexpr std::string_view baseRegistry = "Base.1.19.";
constexpr std::string_view resourceEventRegistry = "NvidiaResourceEvent.1.0.";

// Inventory chassis paths map one-to-one onto Redfish chassis URIs.
constexpr std::string_view inventoryChassisPrefix =
    "/xyz/openbmc_project/inventory/system/chassis/";
constexpr std::string_view redfishChassisPrefix = "/redfish/v1/Chassis/";
constexpr std::string_view eastWestControlProperty =
    "/Settings#/Oem/Nvidia/EastWestControlEnabled";

// Shared by every Astra entry; EventId separates one operation from another.
constexpr std::string_view logNamespace = "Astra";

constexpr std::string_view setOperation = "Astra SetAstraMode";
constexpr std::string_view rollbackOperation = "Astra SetAstraMode rollback";

/** @brief Run op(0..count) concurrently and wait for all of them. */
template <typename Op>
static requester::Coroutine fanOut(size_t count, Op op)
{
    common::AsyncScope scope;
    for (size_t index = 0; index < count; ++index)
    {
        scope.spawn([&op, index] { return op(index); });
    }
    co_await scope.join();
    co_return NSM_SW_SUCCESS;
}

/** @brief Join with commas, replacing any comma inside an argument. */
static std::string joinArgs(std::vector<std::string> args)
{
    std::string joined;
    for (auto& arg : args)
    {
        std::ranges::replace(arg, ',', ' ');
        if (!joined.empty())
        {
            joined += ',';
        }
        joined += arg;
    }
    return joined;
}

/**
 * @brief Log one device's outcome.
 *
 * EventId is the operation path, so bmcweb can select one operation's entries.
 */
static requester::Coroutine logOutcome(const std::string& operationPath,
                                       const std::string& messageId,
                                       Level severity,
                                       std::vector<std::string> args,
                                       std::string_view resolution)
{
    std::map<std::string, std::string> data{
        {"REDFISH_MESSAGE_ID", messageId},
        {"REDFISH_MESSAGE_ARGS", joinArgs(std::move(args))},
        {"namespace", std::string(logNamespace)},
        {"xyz.openbmc_project.Logging.Entry.EventId", operationPath},
        {"xyz.openbmc_project.Logging.Entry.Resolution",
         std::string(resolution)}};
    if (NsmAstra::logSink)
    {
        NsmAstra::logSink(messageId, severity, data);
        co_return NSM_SW_SUCCESS;
    }
    co_return co_await logEventAsync(messageId, severity, std::move(data));
}

/**
 * @brief The device could not be reached, or would not take the write.
 *
 * The event names the adapter by its Name; the journal adds its chassis,
 * since a Name is unique only within its chassis.
 */
static requester::Coroutine logDeviceError(const std::string& operationPath,
                                           const NetworkAdapterId& adapter,
                                           std::string_view operation,
                                           const std::string& error,
                                           Level severity,
                                           std::string_view resolution)
{
    lg2::error("NsmAstra: {OPERATION} failed for {NAME} on {CHASSIS}: {ERROR}",
               "OPERATION", operation, "NAME", adapter.name, "CHASSIS",
               adapter.chassis, "ERROR", error);
    return logOutcome(
        operationPath,
        std::string(resourceEventRegistry) + "DeviceDriverErrorsDetected",
        severity, {std::string(operation), adapter.name, error}, resolution);
}

/**
 * @brief Redfish path of the adapter's EastWestControlEnabled setting.
 *
 * The only device identifier in bmcweb's task message. Falls back to the
 * device name for an adapter outside the chassis subtree.
 */
static std::string settingsPropertyPath(const std::string& networkAdapterPath,
                                        const std::string& deviceName)
{
    if (!networkAdapterPath.starts_with(inventoryChassisPrefix))
    {
        lg2::error("NsmAstra: {PATH} is not under {PREFIX}", "PATH",
                   networkAdapterPath, "PREFIX", inventoryChassisPrefix);
        return deviceName;
    }
    return std::string(redfishChassisPrefix) +
           networkAdapterPath.substr(inventoryChassisPrefix.size()) +
           std::string(eastWestControlProperty);
}

/** @brief The requested mode landed on this device as a pending value. */
static requester::Coroutine
    logPropertyModified(const std::string& operationPath,
                        const NetworkAdapter& member, AstraMode mode)
{
    return logOutcome(operationPath,
                      std::string(baseRegistry) + "PropertyValueModified",
                      Level::Informational,
                      {settingsPropertyPath(member.path, member.name),
                       mode == AstraMode::Enabled ? "true" : "false"},
                      "Power cycle the baseboard to apply the change.");
}

static std::string stateName(EWTrafficMode mode)
{
    return mode == EWTrafficMode::Enabled ? "Enabled" : "Disabled";
}

/** @brief Astra is realized on ConnectX as controlled East/West traffic. */
static EWTrafficMode deviceModeOf(AstraMode mode)
{
    return mode == AstraMode::Enabled ? EWTrafficMode::Enabled
                                      : EWTrafficMode::Disabled;
}

static uint32_t rawMode(AstraMode mode)
{
    return mode == AstraMode::Enabled ? 1 : 0;
}

/** @brief Whether the adapter has a PCIe controlled East/West traffic mode. */
static bool isAstraConfigurable(const NetworkAdapter& member)
{
    return member.deviceModeIntf && member.getSensor && member.setSensor &&
           member.deviceModeIntf
               ->PCIeControlledEWTrafficServer::isModeConfigurable();
}

/** @brief Why a member cannot take part in the group, or empty if it can. */
static std::string_view unavailableReason(const NetworkAdapter& member)
{
    if (!isAstraConfigurable(member))
    {
        return "Astra is not configurable";
    }
    if (!member.device->isOnline())
    {
        return "device is not reachable";
    }
    if (member.device->isDiscoveryPending())
    {
        return "device discovery is incomplete";
    }
    return {};
}

/** @brief Empty until a response has carried the mode. */
static std::optional<EWTrafficMode> pendingModeOf(const NetworkAdapter& member)
{
    if (!member.getSensor->ewPendingModeRead())
    {
        return std::nullopt;
    }
    return member.deviceModeIntf->PCIeControlledEWTrafficServer::pendingMode();
}

/** @brief Empty until a response has carried the mode. */
static std::optional<EWTrafficMode> currentModeOf(const NetworkAdapter& member)
{
    if (!member.getSensor->ewCurrentModeRead())
    {
        return std::nullopt;
    }
    return member.deviceModeIntf->PCIeControlledEWTrafficServer::currentMode();
}

/**
 * @brief Fold one mode across the group.
 *
 * Error if members disagree or one is not configurable. Otherwise Unknown
 * while any named member is missing, unavailable, or not yet read.
 */
template <typename ModeOf>
static AstraState aggregateState(
    const std::vector<NetworkAdapterRegistry::RecordPtr>& members, size_t named,
    ModeOf modeOf)
{
    std::optional<EWTrafficMode> agreed;
    bool unknown = members.empty() || members.size() < named;
    for (const auto& member : members)
    {
        if (!isAstraConfigurable(*member))
        {
            return AstraState::Error;
        }
        const auto mode = unavailableReason(*member).empty() ? modeOf(*member)
                                                             : std::nullopt;
        if (!mode)
        {
            unknown = true;
            continue;
        }
        if (agreed && agreed != mode)
        {
            return AstraState::Error;
        }
        agreed = mode;
    }

    if (unknown)
    {
        return AstraState::Unknown;
    }
    return agreed == EWTrafficMode::Enabled ? AstraState::Enabled
                                            : AstraState::Disabled;
}

} // namespace

NsmAstra::NsmAstra(sdbusplus::bus::bus& bus, const std::string& path,
                   std::string parentChassis, std::string groupName,
                   std::set<NetworkAdapterId> adapterIds,
                   std::vector<std::string> fabricNames,
                   uint64_t verifyDelayUsec) :
    AstraIntf(bus, path.c_str()), parentChassis(std::move(parentChassis)),
    groupName(std::move(groupName)), adapterIds(std::move(adapterIds)),
    fabricNames(std::move(fabricNames)), verifyDelayUsec(verifyDelayUsec)
{
    memberInterest = NetworkAdapterRegistry::instance().watch(
        this->adapterIds,
        [this](const NetworkAdapterId&, common::RegistryEvent event) {
        if (event == common::RegistryEvent::Added)
        {
            membersChanged();
        }
        else
        {
            publishState();
        }
    });
    membersChanged();
}

void NsmAstra::membersChanged()
{
    foundMembers.clear();
    for (const auto& id : adapterIds)
    {
        if (auto member = NetworkAdapterRegistry::instance().find(id))
        {
            foundMembers.push_back(std::move(member));
        }
    }

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationList{{"chassis", "astra", parentChassis}};
    for (const auto& member : foundMembers)
    {
        associationList.emplace_back("network_adapters", "astra", member->path);
    }
    // The mapper holds each association until its fabric is created.
    for (const auto& name : fabricNames)
    {
        const auto fabricPath = fabricsInventoryBasePath / name;
        associationList.emplace_back("pcie_topologies", "astra",
                                     fabricPath.string());
    }
    associations(std::move(associationList));

    publishState();
}

void NsmAstra::endOperation()
{
    if (!std::exchange(operationInProgress, false))
    {
        return;
    }
    publishState();
}

void NsmAstra::finish(AsyncStatusIntf& statusInterface,
                      AsyncOperationStatusType status)
{
    endOperation();
    statusInterface.status(status);
}

void NsmAstra::publishState()
{
    if (operationInProgress)
    {
        return;
    }
    state(aggregateState(foundMembers, adapterIds.size(), currentModeOf));
    pendingState(
        aggregateState(foundMembers, adapterIds.size(), pendingModeOf));
}

NsmAstra::Preflight NsmAstra::performPreflightCheck() const
{
    Preflight split;
    for (const auto& id : adapterIds)
    {
        auto member = NetworkAdapterRegistry::instance().find(id);
        if (member == nullptr)
        {
            split.unavailable.emplace_back(id, "device is not discovered");
            continue;
        }
        auto reason = unavailableReason(*member);
        if (reason.empty() && member->setSensor->isPatchInProgress())
        {
            reason = "another operation is in progress";
        }
        if (reason.empty())
        {
            split.ready.push_back(std::move(member));
        }
        else
        {
            split.unavailable.emplace_back(id, std::string(reason));
        }
    }
    return split;
}

requester::Coroutine NsmAstra::setPendingMode(const NetworkAdapter& member,
                                              AstraMode mode, Outcome& outcome)
{
    AsyncSetOperationValueType value{
        std::vector<std::tuple<std::string, uint32_t>>{
            {"PCIeControlledEWTraffic", rawMode(mode)}}};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto task = member.setSensor->setPendingModes(value, &status,
                                                  member.device);
    uint8_t rc = co_await std::move(task);

    if (auto thrown = task.exception())
    {
        outcome.error = "internal error while writing to the device";
        lg2::error("NsmAstra: write to {NAME} on {CHASSIS} threw: {ERROR}",
                   "NAME", member.name, "CHASSIS", member.chassis, "ERROR",
                   common::describeException(thrown));
        co_return NSM_SW_ERROR;
    }
    if (rc != NSM_SW_SUCCESS)
    {
        outcome.error = status == AsyncOperationStatusType::Unavailable
                            ? "device is not ready"
                            : "set device mode failed";
        lg2::error("NsmAstra: write to {NAME} on {CHASSIS} returned {RC}",
                   "NAME", member.name, "CHASSIS", member.chassis, "RC", rc);
        co_return rc;
    }

    outcome.written = true;
    outcome.error.clear();
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmAstra::verifyPendingMode(const NetworkAdapter& member,
                                                 AstraMode mode,
                                                 Outcome& outcome)
{
    // Keep the write's error rather than verifying nothing.
    if (!outcome.written)
    {
        co_return NSM_SW_ERROR;
    }

    auto task = member.getSensor->update(member.device);
    uint8_t rc = co_await std::move(task);

    if (auto thrown = task.exception())
    {
        outcome.error = "internal error while reading from the device";
        lg2::error("NsmAstra: verification read of {NAME} on {CHASSIS} threw: "
                   "{ERROR}",
                   "NAME", member.name, "CHASSIS", member.chassis, "ERROR",
                   common::describeException(thrown));
        co_return NSM_SW_ERROR;
    }
    if (rc != NSM_SW_SUCCESS)
    {
        outcome.error = "device did not answer";
        lg2::error("NsmAstra: verification read of {NAME} on {CHASSIS} "
                   "returned {RC}",
                   "NAME", member.name, "CHASSIS", member.chassis, "RC", rc);
        co_return rc;
    }

    const auto pending = member.getSensor->latestEWPendingMode();
    if (!pending)
    {
        outcome.error = "verification response omitted PendingMode";
        co_return NSM_SW_ERROR;
    }
    if (*pending != deviceModeOf(mode))
    {
        outcome.error = "the new mode did not take effect";
        lg2::error("NsmAstra: {NAME} on {CHASSIS} reads PendingMode {READ}, "
                   "expected {EXPECTED}",
                   "NAME", member.name, "CHASSIS", member.chassis, "READ",
                   stateName(*pending), "EXPECTED",
                   stateName(deviceModeOf(mode)));
        co_return NSM_SW_ERROR;
    }

    outcome.verified = true;
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmAstra::runPhase(
    const std::vector<NetworkAdapterRegistry::RecordPtr>& members,
    AstraMode mode, Phase phase, const std::string& operationPath)
{
    std::vector<Outcome> outcomes(members.size());

    co_await fanOut(members.size(), [&](size_t index) {
        return setPendingMode(*members[index], mode, outcomes[index]);
    });

    if (verifyDelayUsec != 0)
    {
        auto delayRc = co_await common::Sleep(event, verifyDelayUsec,
                                              common::NonPriority);
        if (delayRc != NSM_SW_SUCCESS)
        {
            // Can't verify without the settle; verifyPendingMode skips these.
            lg2::error("NsmAstra: settle delay failed in {GROUP}", "GROUP",
                       groupName);
            for (auto& outcome : outcomes)
            {
                outcome.written = false;
                outcome.error = "verification delay failed";
            }
        }
    }

    co_await fanOut(members.size(), [&](size_t index) {
        return verifyPendingMode(*members[index], mode, outcomes[index]);
    });

    co_return co_await reportPhaseResults(members, outcomes, mode, phase,
                                          operationPath);
}

requester::Coroutine NsmAstra::reportPhaseResults(
    const std::vector<NetworkAdapterRegistry::RecordPtr>& members,
    const std::vector<Outcome>& outcomes, AstraMode mode, Phase phase,
    const std::string& operationPath)
{
    size_t failures = 0;
    for (size_t index = 0; index < members.size(); ++index)
    {
        const auto& member = *members[index];
        const auto adapter = member.id();
        const auto& outcome = outcomes[index];

        // A successful rollback is also a pending write, so it logs the same.
        if (outcome.success())
        {
            co_await logPropertyModified(operationPath, member, mode);
            continue;
        }

        ++failures;
        if (phase == Phase::Set)
        {
            co_await logDeviceError(
                operationPath, adapter, setOperation, outcome.error,
                Level::Warning,
                "Retry the operation. If it persists, collect device logs.");
        }
        else
        {
            co_await logDeviceError(
                operationPath, adapter, rollbackOperation, outcome.error,
                Level::Critical,
                "Disable Astra on this device before the next power cycle.");
        }
    }

    if (failures != 0)
    {
        lg2::error("NsmAstra: {PHASE} failed on {COUNT} of {TOTAL} in {GROUP}",
                   "PHASE", phase == Phase::Set ? "set" : "rollback", "COUNT",
                   failures, "TOTAL", members.size(), "GROUP", groupName);
        co_return NSM_SW_ERROR;
    }
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmAstra::doSetAstraMode(AstraMode mode, std::string operationPath,
                             std::shared_ptr<AsyncStatusIntf> statusInterface)
{
    // Covers exits that skip finish(), such as an exception.
    struct EndOperation
    {
        NsmAstra& astra;
        ~EndOperation()
        {
            astra.endOperation();
        }
    } operationGuard{*this};

    auto [ready, unavailable] = performPreflightCheck();
    if (!unavailable.empty())
    {
        lg2::error("NsmAstra: preflight rejected {COUNT} in {GROUP}", "COUNT",
                   unavailable.size(), "GROUP", groupName);
        for (const auto& [adapter, reason] : unavailable)
        {
            co_await logDeviceError(
                operationPath, adapter, setOperation, reason, Level::Warning,
                "Ensure the device is present and reachable, then retry the operation.");
        }
        finish(*statusInterface, AsyncOperationStatusType::Unavailable);
        co_return NSM_SW_ERROR;
    }
    if (ready.empty())
    {
        lg2::error("NsmAstra: group {GROUP} has no members", "GROUP",
                   groupName);
        finish(*statusInterface, AsyncOperationStatusType::Unavailable);
        co_return NSM_SW_ERROR;
    }

    if (co_await runPhase(ready, mode, Phase::Set, operationPath) ==
        NSM_SW_SUCCESS)
    {
        finish(*statusInterface, AsyncOperationStatusType::Success);
        co_return NSM_SW_SUCCESS;
    }

    // A rollback writes Disabled, so only a failed Enable has one to run; a
    // failed Disable reports its failures and leaves the adapters as they are.
    if (mode == AstraMode::Enabled)
    {
        // Best effort; a separate phase so a rollback cannot trigger another.
        co_await runPhase(ready, AstraMode::Disabled, Phase::Rollback,
                          operationPath);
    }
    finish(*statusInterface, AsyncOperationStatusType::WriteFailure);
    co_return NSM_SW_ERROR;
}

sdbusplus::message::object_path NsmAstra::setAstraMode(AstraMode mode)
{
    if (operationInProgress)
    {
        lg2::error("NsmAstra::setAstraMode: an operation is already running");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    const auto [operationPath, statusInterface] =
        AsyncOperationManager::getInstance()->getNewStatusInterface();
    if (operationPath.empty())
    {
        lg2::error("NsmAstra::setAstraMode: no operation object available");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    operationInProgress = true;

    // Deferred so nothing is published before this method replies and the
    // caller can subscribe; preflight may finish without suspending.
    common::spawnDetached([this, mode, operationPath, statusInterface] {
        return doSetAstraMode(mode, operationPath, statusInterface);
    });
    return operationPath;
}

} // namespace nsm
