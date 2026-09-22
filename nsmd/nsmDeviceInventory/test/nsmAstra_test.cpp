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

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public
#include "nsmAstra.hpp"
#include "nsmAstraRegistry.hpp"
#include "nsmNetworkAdapterRegistry.hpp"
#undef protected
#undef private

namespace nsm
{
requester::Coroutine createAstra(SensorManager& manager,
                                 const std::string& interface,
                                 const std::string& objPath);
} // namespace nsm

namespace
{

using namespace nsm;

auto testBus = sdbusplus::bus::new_default();

// The parent chassis of every adapter a test creates, unless it says otherwise.
const std::string memberChassis = "astra_member_chassis";

static std::vector<uint8_t> setResponse(uint8_t cc = NSM_SUCCESS)
{
    // A failed reply is shorter: a reason code, with no data size.
    auto size = cc == NSM_SUCCESS ? sizeof(nsm_common_resp)
                                  : sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + size, 0);
    auto* message = reinterpret_cast<nsm_msg*>(response.data());
    EXPECT_EQ(encode_set_device_mode_settings_v2_resp(0, cc, ERR_NULL, message),
              NSM_SW_SUCCESS);
    return response;
}

/** @brief An empty pending mode is sent as the no-change byte. */
static std::vector<uint8_t> getResponse(EWTrafficMode current,
                                        std::optional<EWTrafficMode> pending)
{
    std::array<uint8_t, PCIE_DEVICE_MODE_DATA_SIZE> currentData;
    std::array<uint8_t, PCIE_DEVICE_MODE_DATA_SIZE> pendingData;
    currentData.fill(0xFF);
    pendingData.fill(0xFF);
    currentData[SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC] =
        current == EWTrafficMode::Enabled ? 1 : 0;
    if (pending)
    {
        pendingData[SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC] =
            pending == EWTrafficMode::Enabled ? 1 : 0;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            currentData.size() + pendingData.size() - 1,
        0);
    auto* message = reinterpret_cast<nsm_msg*>(response.data());
    EXPECT_EQ(encode_get_device_mode_settings_v2_resp(
                  0, NSM_SUCCESS, ERR_NULL, currentData.data(),
                  currentData.size(), pendingData.data(), pendingData.size(),
                  message),
              NSM_SW_SUCCESS);
    return response;
}

template <typename... Args>
static requester::Coroutine returnResponse(std::vector<uint8_t> response,
                                           uint8_t rc, Args&... output)
{
    auto outputs = std::tie(output...);
    auto& responseMsg = std::get<0>(outputs);
    auto& responseLen = std::get<1>(outputs);
    responseLen = response.size();
    if (!response.empty())
    {
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(response.size())),
            [](const nsm_msg* ptr) { free(const_cast<nsm_msg*>(ptr)); });
        memcpy(const_cast<nsm_msg*>(responseMsg.get()), response.data(),
               response.size());
    }
    co_return rc;
}

static auto sensorReply(std::vector<uint8_t> response,
                        uint8_t rc = NSM_SW_SUCCESS)
{
    return [response = std::move(response),
            rc](eid_t, Request&, std::shared_ptr<const nsm_msg>& responseMsg,
                size_t& responseLen, bool) -> requester::Coroutine {
        co_return co_await returnResponse(response, rc, responseMsg,
                                          responseLen);
    };
}

static auto setReply(std::vector<uint8_t> response, uint8_t rc = NSM_SW_SUCCESS)
{
    return [response = std::move(response),
            rc](eid_t, Request&, std::shared_ptr<const nsm_msg>& responseMsg,
                size_t& responseLen) -> requester::Coroutine {
        co_return co_await returnResponse(response, rc, responseMsg,
                                          responseLen);
    };
}

static void runToCompletion(requester::Coroutine& task)
{
#ifndef COVERAGE_DISABLE_COROUTINES
    common::Event event;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(2);
    while (!task.done() && std::chrono::steady_clock::now() < deadline)
    {
        event.run(std::chrono::microseconds(1000));
    }
#endif
    ASSERT_TRUE(task.done());
}

struct MemberBundle
{
    MemberBundle(const std::string& suffix, const std::string& adapterPath,
                 const std::string& chassis = memberChassis) :
        device(std::make_shared<NiceMock<MockNsmDevice>>(
            0, 0, "MCTP_EID", "1", NSM_DEV_ROLE_RESERVED)),
        deviceModeIntf(std::make_shared<PCIeDeviceModeIntf>(
            testBus,
            ("/xyz/openbmc_project/inventory/astra_test/settings/" + suffix)
                .c_str())),
        getSensor(std::make_shared<NsmPCIeDeviceModeDeviceModeSettingsV2Get>(
            suffix + "_get", "test", ewBitmap(), deviceModeIntf,
            NetworkAdapterId{chassis, suffix})),
        setSensor(std::make_shared<NsmPCIeDeviceModeDeviceModeSettingsV2Set>(
            suffix + "_set", "test", ewBitmap(), deviceModeIntf,
            NetworkAdapterId{chassis, suffix})),
        member{device,         chassis,   suffix,   adapterPath,
               deviceModeIntf, getSensor, setSensor}
    {
        deviceModeIntf->PCIeControlledEWTrafficServer::isModeConfigurable(true);
        deviceModeIntf->PCIeControlledEWTrafficServer::currentMode(
            EWTrafficMode::Disabled);
        deviceModeIntf->PCIeControlledEWTrafficServer::pendingMode(
            EWTrafficMode::Disabled);
    }

    static uint8_t ewBitmap()
    {
        return static_cast<uint8_t>(1U << SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC);
    }

    std::shared_ptr<NiceMock<MockNsmDevice>> device;
    std::shared_ptr<PCIeDeviceModeIntf> deviceModeIntf;
    std::shared_ptr<NsmPCIeDeviceModeDeviceModeSettingsV2Get> getSensor;
    std::shared_ptr<NsmPCIeDeviceModeDeviceModeSettingsV2Set> setSensor;
    NetworkAdapter member;
};

/** @brief One poll of the member, answered with these modes. */
static void pollMember(MemberBundle& bundle, EWTrafficMode current,
                       std::optional<EWTrafficMode> pending)
{
    EXPECT_CALL(*bundle.device, sensorIO(_, _, _, _, _))
        .WillOnce(sensorReply(getResponse(current, pending)))
        .RetiresOnSaturation();
    auto task = bundle.getSensor->update(bundle.device);
    runToCompletion(task);
}

using EntryLevel =
    sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

struct LoggedEntry
{
    std::string messageId;
    EntryLevel level;
    std::map<std::string, std::string> data;
};

static std::vector<LoggedEntry> logged;

static void recordEntry(const std::string& messageId, EntryLevel level,
                        const std::map<std::string, std::string>& data)
{
    logged.push_back({messageId, level, data});
}

static void resetRegistry()
{
    AstraRegistry::instance().astras.clear();
    NetworkAdapterRegistry::instance().records.clear();
}

/** @brief The Astra object on @p chassis, or null. */
static std::shared_ptr<NsmAstra> astraOn(const std::string& chassis)
{
    const auto& astras = AstraRegistry::instance().astras;
    auto found = astras.find(chassis);
    return found == astras.end() ? nullptr : found->second;
}

class AstraTest : public Test, public utils::DBusTest
{
  protected:
    void SetUp() override
    {
        const auto id = std::to_string(nextId++);
        group = "astra_group_" + id;
        adapterPath = "/xyz/openbmc_project/inventory/astra_adapter_" + id;
        astraPath = "/xyz/openbmc_project/inventory/astra_object_" + id;
        chassisPath = "/xyz/openbmc_project/inventory/astra_chassis_" + id;
        statusPath = "/xyz/openbmc_project/inventory/astra_status_" + id;
        bundle = std::make_unique<MemberBundle>("bundle_" + id, adapterPath);
        status = std::make_shared<AsyncStatusIntf>(testBus, statusPath.c_str());
        status->status(AsyncOperationStatusType::InProgress);
        logged.clear();
        NsmAstra::logSink = recordEntry;
    }

    void TearDown() override
    {
        NsmAstra::logSink = nullptr;
        resetRegistry();
        status.reset();
        astra.reset();
        bundle.reset();
    }

    /** @brief Create and register the group over adapters on memberChassis. */
    void addAstra(const std::vector<std::string>& adapterNames,
                  std::vector<std::string> fabricNames = {})
    {
        std::set<NetworkAdapterId> adapterIds;
        for (const auto& name : adapterNames)
        {
            adapterIds.insert({memberChassis, name});
        }
        astra = std::make_shared<NsmAstra>(testBus, astraPath, chassisPath,
                                           group, std::move(adapterIds),
                                           std::move(fabricNames), 0);
        const auto owner = AstraRegistry::instance().claimChassis(
            chassisPath, [this] { return astra; });
        EXPECT_FALSE(owner.has_value());
    }

    void registerMemberFirst()
    {
        NetworkAdapterRegistry::instance().add(bundle->member);
        addAstra({bundle->member.name});
    }

    requester::Coroutine start(AstraMode mode)
    {
        astra->operationInProgress = true;
        return astra->doSetAstraMode(mode, statusPath, status);
    }

    static inline size_t nextId = 0;
    std::string group;
    std::string adapterPath;
    std::string astraPath;
    std::string chassisPath;
    std::string statusPath;
    std::unique_ptr<MemberBundle> bundle;
    std::shared_ptr<NsmAstra> astra;
    std::shared_ptr<AsyncStatusIntf> status;
};

TEST_F(AstraTest, StateAndPendingStateFoldTheirOwnModes)
{
    MemberBundle second("second_" + std::to_string(nextId), adapterPath + "_2");
    addAstra({bundle->member.name, second.member.name});
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    NetworkAdapterRegistry::instance().add(bundle->member);
    NetworkAdapterRegistry::instance().add(second.member);
    pollMember(*bundle, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    pollMember(second, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Enabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);

    // Requested but not yet in effect: the two properties diverge.
    pollMember(*bundle, EWTrafficMode::Disabled, EWTrafficMode::Enabled);
    pollMember(second, EWTrafficMode::Disabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Disabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);

    // The second member agrees on CurrentMode but not on PendingMode.
    pollMember(second, EWTrafficMode::Disabled, EWTrafficMode::Disabled);
    EXPECT_EQ(astra->state(), AstraState::Disabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Error);

    pollMember(second, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Error);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);

    pollMember(second, EWTrafficMode::Disabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Disabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);
}

TEST_F(AstraTest, MemberNotYetReadMakesUnknown)
{
    MemberBundle second("second_" + std::to_string(nextId), adapterPath + "_2");
    NetworkAdapterRegistry::instance().add(bundle->member);
    NetworkAdapterRegistry::instance().add(second.member);
    addAstra({bundle->member.name, second.member.name});
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    pollMember(*bundle, EWTrafficMode::Disabled, EWTrafficMode::Disabled);
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    // A poll that fails is not a read.
    EXPECT_CALL(*second.device, sensorIO(_, _, _, _, _))
        .WillOnce(sensorReply({}, NSM_SW_ERROR))
        .RetiresOnSaturation();
    auto failedPoll = second.getSensor->update(second.device);
    runToCompletion(failedPoll);
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    // CurrentMode alone settles only State.
    pollMember(second, EWTrafficMode::Disabled, std::nullopt);
    EXPECT_EQ(astra->state(), AstraState::Disabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    pollMember(second, EWTrafficMode::Disabled, EWTrafficMode::Disabled);
    EXPECT_EQ(astra->state(), AstraState::Disabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Disabled);
}

TEST_F(AstraTest, DisagreementOutranksAnUnreadMember)
{
    MemberBundle second("second_" + std::to_string(nextId), adapterPath + "_2");
    MemberBundle third("third_" + std::to_string(nextId), adapterPath + "_3");
    NetworkAdapterRegistry::instance().add(bundle->member);
    NetworkAdapterRegistry::instance().add(second.member);
    NetworkAdapterRegistry::instance().add(third.member);
    addAstra({bundle->member.name, second.member.name, third.member.name});

    pollMember(*bundle, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    pollMember(second, EWTrafficMode::Disabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Error);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);
}

TEST_F(AstraTest, NamedAdapterNotYetCreatedIsUnknown)
{
    MemberBundle second("second_" + std::to_string(nextId), adapterPath + "_2");
    addAstra({bundle->member.name, second.member.name});
    NetworkAdapterRegistry::instance().add(bundle->member);
    pollMember(*bundle, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    NetworkAdapterRegistry::instance().add(second.member);
    pollMember(second, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Enabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);
}

TEST_F(AstraTest, UnnamedAdapterIsNotAMember)
{
    MemberBundle other("other_" + std::to_string(nextId), adapterPath + "_o");
    registerMemberFirst();
    NetworkAdapterRegistry::instance().add(other.member);
    pollMember(*bundle, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    pollMember(other, EWTrafficMode::Disabled, EWTrafficMode::Disabled);

    EXPECT_EQ(astra->state(), AstraState::Enabled);
    EXPECT_EQ(astra->associations().size(), 2);
}

// Platforms reuse adapter Names across chassis.
TEST_F(AstraTest, SameNameOnAnotherChassisIsNotAMember)
{
    MemberBundle twin("twin_" + std::to_string(nextId), adapterPath + "_twin",
                      "another_chassis");
    twin.member.name = bundle->member.name;
    NetworkAdapterRegistry::instance().add(twin.member);
    addAstra({bundle->member.name});
    ASSERT_EQ(astra->associations().size(), 1);

    NetworkAdapterRegistry::instance().add(bundle->member);
    ASSERT_EQ(astra->associations().size(), 2);
    EXPECT_EQ(std::get<2>(astra->associations()[1]), adapterPath);
}

TEST_F(AstraTest, MemberThatIsNotReadyMakesUnknownOrError)
{
    auto& adapters = NetworkAdapterRegistry::instance();
    registerMemberFirst();
    pollMember(*bundle, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    ASSERT_EQ(astra->state(), AstraState::Enabled);

    bundle->device->isDeviceActive = false;
    adapters.changed(bundle->member.id());
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    bundle->device->isDeviceActive = true;
    bundle->device->discoveryPending = true;
    adapters.changed(bundle->member.id());
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    bundle->device->discoveryPending = false;
    bundle->deviceModeIntf->PCIeControlledEWTrafficServer::isModeConfigurable(
        false);
    adapters.changed(bundle->member.id());
    EXPECT_EQ(astra->state(), AstraState::Error);
    EXPECT_EQ(astra->pendingState(), AstraState::Error);

    bundle->deviceModeIntf->PCIeControlledEWTrafficServer::isModeConfigurable(
        true);
    adapters.changed(bundle->member.id());
    EXPECT_EQ(astra->state(), AstraState::Enabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);
}

// Going offline notifies the group through handleOfflineState; coming back
// counts from the member's next poll.
TEST_F(AstraTest, MemberGoingOfflineAndBackRefolds)
{
    MemberBundle second("second_" + std::to_string(nextId), adapterPath + "_2");
    NetworkAdapterRegistry::instance().add(bundle->member);
    NetworkAdapterRegistry::instance().add(second.member);
    addAstra({bundle->member.name, second.member.name});
    bundle->device->deviceSensors.push_back(bundle->getSensor);
    pollMember(*bundle, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    pollMember(second, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    ASSERT_EQ(astra->state(), AstraState::Enabled);

    auto offline = bundle->device->setOffline();
    runToCompletion(offline);
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    // As setOnline leaves it: unread until its own poll.
    bundle->device->isDeviceActive = true;
    pollMember(second, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);

    pollMember(*bundle, EWTrafficMode::Enabled, EWTrafficMode::Enabled);
    EXPECT_EQ(astra->state(), AstraState::Enabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);
}

TEST_F(AstraTest, AssociationsWorkForEitherRegistrationOrder)
{
    addAstra({bundle->member.name});
    NetworkAdapterRegistry::instance().add(bundle->member);

    const auto associations = astra->associations();
    ASSERT_EQ(associations.size(), 2);
    EXPECT_EQ(std::get<0>(associations[0]), "chassis");
    EXPECT_EQ(std::get<0>(associations[1]), "network_adapters");
    EXPECT_EQ(std::get<2>(associations[1]), adapterPath);

    // One reverse name, so every endpoint reaches Astra the same way.
    for (const auto& association : associations)
    {
        EXPECT_EQ(std::get<1>(association), "astra");
    }

    const std::string secondGroup = group + "_member_first";
    auto secondAstra = std::make_shared<NsmAstra>(
        testBus, astraPath + "_member_first", chassisPath + "_member_first",
        secondGroup,
        std::set<NetworkAdapterId>{{memberChassis, bundle->member.name}},
        std::vector<std::string>{}, 0);
    AstraRegistry::instance().claimChassis(chassisPath + "_member_first",
                                           [&] { return secondAstra; });
    ASSERT_EQ(secondAstra->associations().size(), 2);
    EXPECT_EQ(std::get<2>(secondAstra->associations()[1]), adapterPath);
}

// The mapper holds the association until the fabric is created.
TEST_F(AstraTest, NamedFabricIsAssociatedBeforeItIsCreated)
{
    const std::string fabric = "HGX_PCIeTopology_8";
    const std::string fabricPath =
        "/xyz/openbmc_project/inventory/system/fabrics/" + fabric;
    addAstra({bundle->member.name}, {fabric});
    ASSERT_EQ(astra->associations().size(), 2);
    EXPECT_EQ(std::get<0>(astra->associations()[1]), "pcie_topologies");
    EXPECT_EQ(std::get<1>(astra->associations()[1]), "astra");
    EXPECT_EQ(std::get<2>(astra->associations()[1]), fabricPath);

    NetworkAdapterRegistry::instance().add(bundle->member);
    const auto associations = astra->associations();
    ASSERT_EQ(associations.size(), 3);
    EXPECT_EQ(std::get<0>(associations[1]), "network_adapters");
    EXPECT_EQ(std::get<0>(associations[2]), "pcie_topologies");
    EXPECT_EQ(std::get<2>(associations[2]), fabricPath);
}

// bmcweb reads the args by position and filters entries by EventId.
TEST_F(AstraTest, WrittenDeviceLogsItsSettingsPropertyAndValue)
{
    MemberBundle nic("nic_" + std::to_string(nextId),
                     "/xyz/openbmc_project/inventory/system/chassis/"
                     "HGX_ConnectX_0/NetworkAdapters/ConnectX_NIC_0");
    NetworkAdapterRegistry::instance().add(nic.member);
    addAstra({nic.member.name});
    EXPECT_CALL(*nic.device, postPatchIO(_, _, _, _))
        .WillOnce(setReply(setResponse()));
    EXPECT_CALL(*nic.device, sensorIO(_, _, _, _, _))
        .WillOnce(sensorReply(
            getResponse(EWTrafficMode::Disabled, EWTrafficMode::Enabled)));

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    ASSERT_EQ(logged.size(), 1);
    auto& entry = logged[0];
    EXPECT_EQ(entry.messageId, "Base.1.19.PropertyValueModified");
    EXPECT_EQ(entry.level, EntryLevel::Informational);
    EXPECT_EQ(entry.data["REDFISH_MESSAGE_ID"], entry.messageId);
    EXPECT_EQ(entry.data["REDFISH_MESSAGE_ARGS"],
              "/redfish/v1/Chassis/HGX_ConnectX_0/NetworkAdapters/"
              "ConnectX_NIC_0/Settings#/Oem/Nvidia/EastWestControlEnabled,"
              "true");
    EXPECT_EQ(entry.data["xyz.openbmc_project.Logging.Entry.EventId"],
              statusPath);
    EXPECT_EQ(entry.data["namespace"], "Astra");
    EXPECT_FALSE(entry.data.contains("DEVICE_NAME"));
}

TEST_F(AstraTest, VerifiedReadbackCompletesTheSet)
{
    registerMemberFirst();
    EXPECT_CALL(*bundle->device, postPatchIO(_, _, _, _))
        .WillOnce(setReply(setResponse()));
    EXPECT_CALL(*bundle->device, sensorIO(_, _, _, _, _))
        .WillOnce(sensorReply(
            getResponse(EWTrafficMode::Disabled, EWTrafficMode::Enabled)));

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    EXPECT_EQ(status->status(), AsyncOperationStatusType::Success);
    EXPECT_EQ(astra->state(), AstraState::Disabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);
}

TEST_F(AstraTest, MismatchTriggersVerifiedRollback)
{
    registerMemberFirst();
    EXPECT_CALL(*bundle->device, postPatchIO(_, _, _, _))
        .Times(2)
        .WillRepeatedly(setReply(setResponse()));
    EXPECT_CALL(*bundle->device, sensorIO(_, _, _, _, _))
        .WillOnce(sensorReply(
            getResponse(EWTrafficMode::Disabled, EWTrafficMode::Disabled)))
        .WillOnce(sensorReply(
            getResponse(EWTrafficMode::Disabled, EWTrafficMode::Disabled)));

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    EXPECT_EQ(status->status(), AsyncOperationStatusType::WriteFailure);
    EXPECT_EQ(astra->state(), AstraState::Disabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Disabled);

    ASSERT_EQ(logged.size(), 2);
    EXPECT_EQ(logged[0].messageId,
              "NvidiaResourceEvent.1.0.DeviceDriverErrorsDetected");
    EXPECT_EQ(logged[0].level, EntryLevel::Warning);
    EXPECT_EQ(logged[0].data["REDFISH_MESSAGE_ARGS"],
              "Astra SetAstraMode," + bundle->member.name +
                  ",the new mode did not take effect");
    EXPECT_EQ(logged[1].messageId, "Base.1.19.PropertyValueModified");
    EXPECT_TRUE(logged[1].data["REDFISH_MESSAGE_ARGS"].ends_with(",false"));
}

TEST_F(AstraTest, FailedDisableRunsNoRollback)
{
    registerMemberFirst();
    EXPECT_CALL(*bundle->device, postPatchIO(_, _, _, _))
        .WillOnce(setReply(setResponse()));
    EXPECT_CALL(*bundle->device, sensorIO(_, _, _, _, _))
        .WillOnce(sensorReply(
            getResponse(EWTrafficMode::Enabled, EWTrafficMode::Enabled)));

    auto task = start(AstraMode::Disabled);
    runToCompletion(task);

    EXPECT_EQ(status->status(), AsyncOperationStatusType::WriteFailure);

    ASSERT_EQ(logged.size(), 1);
    EXPECT_EQ(logged[0].messageId,
              "NvidiaResourceEvent.1.0.DeviceDriverErrorsDetected");
    EXPECT_EQ(logged[0].level, EntryLevel::Warning);
    EXPECT_EQ(logged[0].data["REDFISH_MESSAGE_ARGS"],
              "Astra SetAstraMode," + bundle->member.name +
                  ",the new mode did not take effect");
}

TEST_F(AstraTest, ReadErrorTriggersRollbackAndRollbackMismatchFails)
{
    registerMemberFirst();
    EXPECT_CALL(*bundle->device, postPatchIO(_, _, _, _))
        .Times(2)
        .WillRepeatedly(setReply(setResponse()));
    EXPECT_CALL(*bundle->device, sensorIO(_, _, _, _, _))
        .WillOnce(sensorReply({}, NSM_SW_ERROR))
        .WillOnce(sensorReply(
            getResponse(EWTrafficMode::Disabled, EWTrafficMode::Enabled)));

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    EXPECT_EQ(status->status(), AsyncOperationStatusType::WriteFailure);
    EXPECT_EQ(astra->state(), AstraState::Disabled);
    EXPECT_EQ(astra->pendingState(), AstraState::Enabled);

    ASSERT_EQ(logged.size(), 2);
    EXPECT_EQ(logged[0].level, EntryLevel::Warning);
    EXPECT_EQ(logged[0].data["REDFISH_MESSAGE_ARGS"],
              "Astra SetAstraMode," + bundle->member.name +
                  ",device did not answer");
    EXPECT_EQ(logged[1].level, EntryLevel::Critical);
    EXPECT_EQ(logged[1].data["REDFISH_MESSAGE_ARGS"],
              "Astra SetAstraMode rollback," + bundle->member.name +
                  ",the new mode did not take effect");
}

TEST_F(AstraTest, FailedWriteSaysWhetherTheDeviceWasReady)
{
    MemberBundle second("second_" + std::to_string(nextId), adapterPath + "_2");
    NetworkAdapterRegistry::instance().add(bundle->member);
    NetworkAdapterRegistry::instance().add(second.member);
    addAstra({bundle->member.name, second.member.name});
    EXPECT_CALL(*bundle->device, postPatchIO(_, _, _, _))
        .WillOnce(setReply(setResponse(NSM_ERR_NOT_READY)))
        .WillOnce(setReply(setResponse()));
    EXPECT_CALL(*second.device, postPatchIO(_, _, _, _))
        .WillOnce(setReply({}, NSM_SW_ERROR_TIMEOUT))
        .WillOnce(setReply(setResponse()));
    for (auto* member : {bundle.get(), &second})
    {
        EXPECT_CALL(*member->device, sensorIO(_, _, _, _, _))
            .WillOnce(sensorReply(
                getResponse(EWTrafficMode::Disabled, EWTrafficMode::Disabled)));
    }

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    EXPECT_EQ(status->status(), AsyncOperationStatusType::WriteFailure);
    std::vector<std::string> errors;
    for (auto& entry : logged)
    {
        if (entry.messageId ==
            "NvidiaResourceEvent.1.0.DeviceDriverErrorsDetected")
        {
            errors.push_back(entry.data["REDFISH_MESSAGE_ARGS"]);
        }
    }
    EXPECT_THAT(errors, UnorderedElementsAre(
                            "Astra SetAstraMode," + bundle->member.name +
                                ",device is not ready",
                            "Astra SetAstraMode," + second.member.name +
                                ",set device mode failed"));
}

TEST_F(AstraTest, ReadinessChangeDuringAnOperationWaitsForItsEnd)
{
    registerMemberFirst();
    pollMember(*bundle, EWTrafficMode::Disabled, EWTrafficMode::Disabled);
    ASSERT_EQ(astra->state(), AstraState::Disabled);

    auto stateDuring = AstraState::Error;
    auto pendingStateDuring = AstraState::Error;
    EXPECT_CALL(*bundle->device, postPatchIO(_, _, _, _))
        .WillOnce([&](eid_t, Request&,
                      std::shared_ptr<const nsm_msg>& responseMsg,
                      size_t& responseLen) {
        bundle->device->isDeviceActive = false;
        bundle->getSensor->handleOfflineState();
        stateDuring = astra->state();
        pendingStateDuring = astra->pendingState();
        return returnResponse(setResponse(), NSM_SW_SUCCESS, responseMsg,
                              responseLen);
    });
    EXPECT_CALL(*bundle->device, sensorIO(_, _, _, _, _))
        .WillOnce(sensorReply(
            getResponse(EWTrafficMode::Disabled, EWTrafficMode::Enabled)));

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    EXPECT_EQ(stateDuring, AstraState::Disabled);
    EXPECT_EQ(pendingStateDuring, AstraState::Disabled);
    EXPECT_EQ(status->status(), AsyncOperationStatusType::Success);
    EXPECT_EQ(astra->state(), AstraState::Unknown);
    EXPECT_EQ(astra->pendingState(), AstraState::Unknown);
}

TEST_F(AstraTest, WriteFailureSkipsTheVerificationRead)
{
    registerMemberFirst();
    EXPECT_CALL(*bundle->device, postPatchIO(_, _, _, _))
        .Times(2)
        .WillRepeatedly(setReply({}, NSM_SW_ERROR));
    EXPECT_CALL(*bundle->device, sensorIO(_, _, _, _, _)).Times(0);

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    EXPECT_EQ(status->status(), AsyncOperationStatusType::WriteFailure);
}

TEST_F(AstraTest, MembersThatCannotTakeTheWriteBlockTheOperation)
{
    MemberBundle notConfigurable("nocfg_" + std::to_string(nextId),
                                 adapterPath + "_nocfg");
    // As registered for an adapter configured without PCIe device mode.
    notConfigurable.member.deviceModeIntf = nullptr;
    notConfigurable.member.getSensor = nullptr;
    notConfigurable.member.setSensor = nullptr;

    MemberBundle offline("offline_" + std::to_string(nextId),
                         adapterPath + "_offline");
    offline.device->isDeviceActive = false;

    MemberBundle discovering("discovering_" + std::to_string(nextId),
                             adapterPath + "_discovering");
    discovering.device->discoveryPending = true;

    MemberBundle busy("busy_" + std::to_string(nextId), adapterPath + "_busy");
    busy.setSensor->asyncPatchInProgress = true;

    const std::string missing = "missing_" + std::to_string(nextId);

    NetworkAdapterRegistry::instance().add(bundle->member);
    NetworkAdapterRegistry::instance().add(notConfigurable.member);
    NetworkAdapterRegistry::instance().add(offline.member);
    NetworkAdapterRegistry::instance().add(discovering.member);
    NetworkAdapterRegistry::instance().add(busy.member);
    addAstra({bundle->member.name, notConfigurable.member.name,
              offline.member.name, discovering.member.name, busy.member.name,
              missing});
    EXPECT_EQ(astra->state(), AstraState::Error);

    // No device is written while any member cannot take the write.
    EXPECT_CALL(*bundle->device, postPatchIO(_, _, _, _)).Times(0);
    EXPECT_CALL(*notConfigurable.device, postPatchIO(_, _, _, _)).Times(0);
    EXPECT_CALL(*offline.device, postPatchIO(_, _, _, _)).Times(0);
    EXPECT_CALL(*discovering.device, postPatchIO(_, _, _, _)).Times(0);
    EXPECT_CALL(*busy.device, postPatchIO(_, _, _, _)).Times(0);

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    EXPECT_EQ(status->status(), AsyncOperationStatusType::Unavailable);
    std::vector<std::string> reasons;
    for (auto& entry : logged)
    {
        EXPECT_EQ(entry.level, EntryLevel::Warning);
        reasons.push_back(entry.data["REDFISH_MESSAGE_ARGS"]);
    }
    EXPECT_THAT(
        reasons,
        UnorderedElementsAre(
            "Astra SetAstraMode," + notConfigurable.member.name +
                ",Astra is not configurable",
            "Astra SetAstraMode," + offline.member.name +
                ",device is not reachable",
            "Astra SetAstraMode," + discovering.member.name +
                ",device discovery is incomplete",
            "Astra SetAstraMode," + busy.member.name +
                ",another operation is in progress",
            "Astra SetAstraMode," + missing + ",device is not discovered"));
}

TEST_F(AstraTest, GroupWithNoMembersIsUnavailable)
{
    addAstra({});

    auto task = start(AstraMode::Enabled);
    runToCompletion(task);

    EXPECT_EQ(status->status(), AsyncOperationStatusType::Unavailable);
}

TEST_F(AstraTest, SetAstraModeRejectsASecondOperation)
{
    registerMemberFirst();
    astra->operationInProgress = true;

    EXPECT_THROW(astra->setAstraMode(AstraMode::Enabled),
                 sdbusplus::error::xyz::openbmc_project::common::Unavailable);
}

class AstraFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    AstraFactoryTest() : SensorManagerTest(devices) {}

    ~AstraFactoryTest() override
    {
        resetRegistry();
    }

    static constexpr auto interface =
        "xyz.openbmc_project.Configuration.NSM_Astra";

    /** @brief Publish the record's NetworkAdapters as entity-manager does. */
    void setAdapterEntries(const std::vector<NetworkAdapterId>& entries)
    {
        dbus::Interfaces interfaces{interface};
        for (size_t index = 0; index < entries.size(); ++index)
        {
            auto entry = std::string(interface) + ".NetworkAdapters" +
                         std::to_string(index);
            auto& properties = utils::MockDbusAsync::propertyMap(objPath,
                                                                 entry);
            properties["Chassis"] = entries[index].chassis;
            properties["Name"] = entries[index].name;
            interfaces.push_back(std::move(entry));
        }
        // The mapper lists interfaces in string order.
        std::ranges::sort(interfaces);
        utils::MockDbusAsync::serviceMap() = {
            {utils::entityManagerServiceStr, interfaces}};
    }

    NsmDeviceTable devices;
    const std::string group = "astra_factory_group";
    const std::string objPath = "/xyz/test/astra_factory";
    const std::string chassis = "/xyz/test";
};

TEST_F(AstraFactoryTest, ConfigurationWithoutANameIsRejected)
{
    utils::MockDbusAsync::propertyMap(objPath, interface);

    auto task = createAstra(mockManager, interface, objPath);
    runToCompletion(task);

    EXPECT_EQ(task.data(), NSM_ERR_INVALID_DATA);
    EXPECT_EQ(astraOn(chassis), nullptr);
}

// Both objects would take the same path below the chassis.
TEST_F(AstraFactoryTest, SecondGroupOnTheSameChassisIsRejected)
{
    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;
    auto first = createAstra(mockManager, interface, objPath);
    runToCompletion(first);
    ASSERT_EQ(first.data(), NSM_SUCCESS);

    const std::string otherGroup = group + "_other";
    const std::string otherPath = chassis + "/astra_factory_other";
    auto& other = utils::MockDbusAsync::propertyMap(otherPath, interface);
    other["Name"] = otherGroup;
    auto second = createAstra(mockManager, interface, otherPath);
    runToCompletion(second);

    EXPECT_EQ(second.data(), NSM_ERR_INVALID_DATA);
    ASSERT_NE(astraOn(chassis), nullptr);
    EXPECT_EQ(astraOn(chassis)->group(), group);
}

// The inventory scan and InterfacesAdded can both queue one record.
TEST_F(AstraFactoryTest, RecordQueuedTwiceGetsOneAstraObject)
{
    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;
    auto first = createAstra(mockManager, interface, objPath);
    runToCompletion(first);
    auto second = createAstra(mockManager, interface, objPath);
    runToCompletion(second);

    EXPECT_EQ(first.data(), NSM_SUCCESS);
    EXPECT_EQ(second.data(), NSM_SUCCESS);
    EXPECT_EQ(AstraRegistry::instance().astras.size(), 1u);
    ASSERT_NE(astraOn(chassis), nullptr);
    EXPECT_EQ(astraOn(chassis)->group(), group);
}

// Otherwise a later record for the group would read as already created.
TEST_F(AstraFactoryTest, ThrowingFactoryLeavesTheChassisFree)
{
    const auto throwing = []() -> std::shared_ptr<NsmAstra> {
        throw std::runtime_error("object path in use");
    };
    EXPECT_THROW(AstraRegistry::instance().claimChassis(chassis, throwing),
                 std::runtime_error);
    EXPECT_EQ(astraOn(chassis), nullptr);

    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;
    auto task = createAstra(mockManager, interface, objPath);
    runToCompletion(task);

    EXPECT_EQ(task.data(), NSM_SUCCESS);
    ASSERT_NE(astraOn(chassis), nullptr);
    EXPECT_EQ(astraOn(chassis)->group(), group);
}

// Entry indexes run past a single digit.
TEST_F(AstraFactoryTest, RecordReadsEveryAdapterEntry)
{
    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;
    std::vector<NetworkAdapterId> entries;
    for (size_t index = 0; index < 11; ++index)
    {
        entries.push_back({"HGX_ConnectX_" + std::to_string(index),
                           "ConnectX_NIC_" + std::to_string(index)});
    }
    setAdapterEntries(entries);

    auto task = createAstra(mockManager, interface, objPath);
    runToCompletion(task);

    ASSERT_EQ(task.data(), NSM_SUCCESS);
    const auto astra = astraOn(chassis);
    ASSERT_NE(astra, nullptr);
    EXPECT_EQ(astra->adapterIds,
              std::set<NetworkAdapterId>(entries.begin(), entries.end()));
}

// Names repeat across chassis, so a Name alone does not identify an adapter.
TEST_F(AstraFactoryTest, AdaptersListedByNameAloneAreRejected)
{
    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;
    properties["NetworkAdapters"] = std::vector<std::string>{"ConnectX_NIC_0"};

    auto task = createAstra(mockManager, interface, objPath);
    runToCompletion(task);

    EXPECT_EQ(task.data(), NSM_ERR_INVALID_DATA);
    EXPECT_EQ(astraOn(chassis), nullptr);
}

TEST_F(AstraFactoryTest, AdapterEntryWithoutAChassisIsRejected)
{
    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;
    setAdapterEntries(
        {{"HGX_ConnectX_0", "ConnectX_NIC_0"}, {"", "ConnectX_NIC_1"}});

    auto task = createAstra(mockManager, interface, objPath);
    runToCompletion(task);

    EXPECT_EQ(task.data(), NSM_ERR_INVALID_DATA);
    EXPECT_EQ(astraOn(chassis), nullptr);
}

// Two concurrent writes to one adapter would fail the operation every time.
TEST_F(AstraFactoryTest, AdapterListedTwiceIsRejected)
{
    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;
    setAdapterEntries({{"HGX_ConnectX_0", "ConnectX_NIC_0"},
                       {"HGX_ConnectX_1", "ConnectX_NIC_1"},
                       {"HGX_ConnectX_0", "ConnectX_NIC_0"}});

    auto task = createAstra(mockManager, interface, objPath);
    runToCompletion(task);

    EXPECT_EQ(task.data(), NSM_ERR_INVALID_DATA);
    EXPECT_EQ(astraOn(chassis), nullptr);
}

TEST_F(AstraFactoryTest, RecordWithoutFabricsStillGetsAnAstraObject)
{
    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;

    auto task = createAstra(mockManager, interface, objPath);
    runToCompletion(task);

    EXPECT_EQ(task.data(), NSM_SUCCESS);
    const auto astra = astraOn(chassis);
    ASSERT_NE(astra, nullptr);

    const auto associations = astra->associations();
    ASSERT_EQ(associations.size(), 1u);
    EXPECT_EQ(std::get<2>(associations[0]), chassis);
}

TEST_F(AstraFactoryTest, ConfiguredGroupGetsAnAstraObject)
{
    const std::string fabric = "HGX_PCIeTopology_8";
    const std::string fabricPath =
        "/xyz/openbmc_project/inventory/system/fabrics/" + fabric;
    auto& properties = utils::MockDbusAsync::propertyMap(objPath, interface);
    properties["Name"] = group;
    properties["PCIeTopologies"] = std::vector<std::string>{fabric};

    auto task = createAstra(mockManager, interface, objPath);
    runToCompletion(task);

    EXPECT_EQ(task.data(), NSM_SUCCESS);
    const auto astra = astraOn(chassis);
    ASSERT_NE(astra, nullptr);

    const auto associations = astra->associations();
    ASSERT_EQ(associations.size(), 2u);
    EXPECT_EQ(std::get<2>(associations[0]), chassis);
    EXPECT_EQ(std::get<0>(associations[1]), "pcie_topologies");
    EXPECT_EQ(std::get<2>(associations[1]), fabricPath);
}

} // namespace
