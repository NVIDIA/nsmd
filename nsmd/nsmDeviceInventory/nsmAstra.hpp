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

#pragma once

#include "common/event.hpp"
#include "nsmNetworkAdapterRegistry.hpp"

#include <com/nvidia/Astra/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nsm
{

using AstraServer = sdbusplus::com::nvidia::server::Astra;
using AstraIntf =
    sdbusplus::server::object_t<AstraServer, Association::server::Definitions>;
using AstraState = AstraServer::State;
using AstraMode = AstraServer::AstraMode;

/**
 * @brief Sets and reports Astra mode across a baseboard's adapters.
 *
 * Exists only when configuration enables Astra, so its presence tells a
 * client the operation is offered.
 */
class NsmAstra : public AstraIntf
{
  public:
    using LogSink = std::function<void(
        const std::string& messageId,
        sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level level,
        const std::map<std::string, std::string>& data)>;

    /** @brief When set, receives event logs instead of the log service. */
    static inline LogSink logSink;

    /** @brief Settle time between write and verify. */
    static constexpr uint64_t defaultVerifyDelayUsec = 10'000'000;

    NsmAstra(sdbusplus::bus::bus& bus, const std::string& path,
             std::string parentChassis, std::string groupName,
             std::set<NetworkAdapterId> adapterIds,
             std::vector<std::string> fabricNames = {},
             uint64_t verifyDelayUsec = defaultVerifyDelayUsec);

    sdbusplus::message::object_path setAstraMode(AstraMode mode) override;

    /** @brief The configuration Name of this object's group. */
    const std::string& group() const
    {
        return groupName;
    }

  private:
    /** @brief Refresh associations and state as members are created. */
    void membersChanged();

    /** @brief Publish State and PendingState unless an operation is running. */
    void publishState();

    enum class Phase
    {
        Set,
        Rollback
    };

    /**
     * @brief Result of writing and verifying one device.
     *
     * Starts as a failure so a task that never reports cannot read as success.
     */
    struct Outcome
    {
        bool written = false;
        bool verified = false;
        std::string error = "device did not report a result";

        bool success() const
        {
            return written && verified;
        }
    };

    /** @brief The group split by whether a member can take the write. */
    struct Preflight
    {
        std::vector<NetworkAdapterRegistry::RecordPtr> ready;

        // Each member that cannot, and why.
        std::vector<std::pair<NetworkAdapterId, std::string>> unavailable;
    };

    /** @brief Preflight, set, roll back on failure, then publish the result. */
    requester::Coroutine
        doSetAstraMode(AstraMode mode, std::string operationPath,
                       std::shared_ptr<AsyncStatusIntf> statusInterface);

    /** @brief Write every member, wait out the settle time, verify. */
    requester::Coroutine
        runPhase(const std::vector<NetworkAdapterRegistry::RecordPtr>& members,
                 AstraMode mode, Phase phase, const std::string& operationPath);

    /** @brief Log each member's result; fails if any member failed. */
    requester::Coroutine reportPhaseResults(
        const std::vector<NetworkAdapterRegistry::RecordPtr>& members,
        const std::vector<Outcome>& outcomes, AstraMode mode, Phase phase,
        const std::string& operationPath);

    /** @brief Write @p mode as the device's pending mode; a throw fails. */
    requester::Coroutine setPendingMode(const NetworkAdapter& member,
                                        AstraMode mode, Outcome& outcome);

    /** @brief Read the device back and check its pending mode is @p mode. */
    requester::Coroutine verifyPendingMode(const NetworkAdapter& member,
                                           AstraMode mode, Outcome& outcome);

    /** @brief Check whether each named member can take the write. */
    Preflight performPreflightCheck() const;

    const std::string parentChassis;

    const std::string groupName;

    const std::set<NetworkAdapterId> adapterIds;
    const std::vector<std::string> fabricNames;

    // The listed adapters nsmd has created. Records are never replaced, so
    // this changes only as listed adapters are added.
    std::vector<NetworkAdapterRegistry::RecordPtr> foundMembers;

    const uint64_t verifyDelayUsec;
    const common::Event event;

    // Also holds publishState, so clients never see a half-written group. Set
    // by setAstraMode, not the worker, which starts a loop iteration later.
    bool operationInProgress = false;

    /** @brief Clear operationInProgress and publish the held state. */
    void endOperation();

    /** @brief End the operation, then set its status. */
    void finish(AsyncStatusIntf& statusInterface,
                AsyncOperationStatusType status);

    NetworkAdapterRegistry::Interest memberInterest;
};

} // namespace nsm
