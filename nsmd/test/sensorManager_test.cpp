/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
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

/*
 * sensorManager_test.cpp
 *
 * Branch-coverage tests for SensorManagerImpl::deviceTask() using B1
 * (virtual doSleep) and Step3 (virtual pollPrioritySensors /
 * pollNonPrioritySensors / refreshCommandMatrix).
 *
 * Branches exercised:
 *   1. pollingIsRunning = false  → loop skipped, immediate return
 *   2. !nsmDevice->isOnline()   → doSleep(Priority) + continue
 *   3. isOnline + noPriority    → pollPriority + pollNonPriority +
 * doSleep(NonPriority)
 *   4. isOnline + hasPriority   → pollPriority + pollNonPriority +
 * doSleep(Priority)
 *   5. !allCommandCodesRetrieved → refreshCommandMatrix called
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::_;

#include "base.h"
#include "platform-environmental.h"

#include "common/test/mockEvent.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>

#define private public
#define protected public

#include "commonMock.hpp"
#include "sensorManager.hpp"

#undef private
#undef protected

using namespace ::testing;

// ============================================================================
// MockSensorManagerImpl
// ============================================================================

class MockSensorManagerImpl : public nsm::SensorManagerImpl
{
  public:
    MockSensorManagerImpl(
        sdbusplus::bus::bus& bus, common::Event& event,
        nsm::RequesterHandler& handler, nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        nsm::NsmDeviceTable& nsmDevices, mctp_socket::Manager& sockManager) :
        SensorManagerImpl(bus, event, handler, instanceIdDb, objServer,
                          eidTable, nsmDevices, sockManager, false)
    {}

    MOCK_METHOD(requester::Coroutine, pollPrioritySensors,
                (std::shared_ptr<nsm::NsmDevice>, const uint64_t&), (override));

    MOCK_METHOD(requester::Coroutine, pollNonPrioritySensors,
                (std::shared_ptr<nsm::NsmDevice>, const uint64_t&), (override));

    MOCK_METHOD(requester::Coroutine, refreshCommandMatrix,
                (std::shared_ptr<nsm::NsmDevice>), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

struct SensorManagerTest : public ::testing::Test
{
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    NiceMock<common::test::MockEvent> event;
    nsm::InstanceIdDb instanceIdDb;
    mctp_socket::Manager sockManager;
    nsm::RequesterHandler handler;
    nsm::NsmDeviceTable nsmDevices;
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> eidTable;
    std::unique_ptr<MockSensorManagerImpl> mgr;

    SensorManagerTest() :
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus)),
        event(), handler(event, instanceIdDb, sockManager, false),
        mgr(std::make_unique<MockSensorManagerImpl>(
            *systemBus, event, handler, instanceIdDb, *objServer, eidTable,
            nsmDevices, sockManager))
    {}

    ~SensorManagerTest()
    {
        for (auto& dev : nsmDevices)
        {
            if (dev)
            {
                if (dev->task.handle && !dev->task.handle.done())
                {
                    dev->task.handle.destroy();
                }
                dev->task.handle = nullptr;
                if (dev->longRunningTask.handle &&
                    !dev->longRunningTask.handle.done())
                {
                    dev->longRunningTask.handle.destroy();
                }
                dev->longRunningTask.handle = nullptr;
                dev->deviceSensors.clear();
                dev->prioritySensors.clear();
                dev->roundRobinSensors.clear();
                dev->nsmMsgHandler.reset();
                dev->objServer.reset();
            }
        }
        nsmDevices.clear();
    }
};

// Helper: returns a WillOnce/WillRepeatedly action for mocked coroutine methods
// that return NSM_SW_SUCCESS immediately (no suspension).
static auto immediateCoroutine()
{
    return [](auto...) -> requester::Coroutine { co_return NSM_SW_SUCCESS; };
}

// Force-destroy a coroutine frame suspended on Sleep (the mocked event
// loop never resumes it, so detach() alone would leak the frame).
static void destroyCoroutine(requester::Coroutine&& co)
{
    if (co.handle && !co.handle.done())
    {
        co.handle.destroy();
    }
    co.handle = nullptr;
}

// ============================================================================
// deviceTask() coroutine coverage (B1 + Step 3)
// ============================================================================

/**
 * DeviceTask_PollingNotRunning_ReturnsImmediately
 * If pollingIsRunning is false, the while loop is not entered and the
 * coroutine returns NSM_SW_SUCCESS without calling doSleep.
 */
TEST_F(SensorManagerTest, DeviceTask_PollingNotRunning_ReturnsImmediately)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "1", 0);
    mgr->pollingIsRunning = false;

    EXPECT_CALL(event, now(_, _)).Times(0);

    auto cr = mgr->deviceTask(nsmDevice);
    EXPECT_EQ(cr.data(), NSM_SW_SUCCESS);
}

/**
 * DeviceTask_InactiveDevice_SleepsWithPriority
 * When the device is offline, deviceTask sleeps with Priority and then
 * continues to the next loop iteration. The doSleep mock clears
 * pollingIsRunning so the loop exits after one iteration.
 */
TEST_F(SensorManagerTest, DeviceTask_InactiveDevice_SleepsWithPriority)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "1", 0);
    nsmDevice->isDeviceActive = false; // offline
    mgr->pollingIsRunning = true;

    // Iteration 1: now() sets t0, Sleep is no-op, continue.
    // Iteration 2: now() sets t0 and breaks loop.
    EXPECT_CALL(event, now(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(0), Return(0)))
        .WillOnce(DoAll(SetArgPointee<1>(0), InvokeWithoutArgs([this]() {
        mgr->pollingIsRunning = false;
    }),
                        Return(0)));

    destroyCoroutine(mgr->deviceTask(nsmDevice));
}

/**
 * DeviceTask_ActiveDevice_NoPrioritySensors_SleepsNonPriority
 * An online device with no priority sensors triggers pollPrioritySensors,
 * pollNonPrioritySensors, and then doSleep with NonPriority.
 */
TEST_F(SensorManagerTest,
       DeviceTask_ActiveDevice_NoPrioritySensors_SleepsNonPriority)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "1", 0);
    mgr->pollingIsRunning = true;

    ON_CALL(event, now(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(0), Return(0)));
    EXPECT_CALL(*mgr, pollPrioritySensors(_, _)).WillOnce(immediateCoroutine());
    EXPECT_CALL(*mgr, pollNonPrioritySensors(_, _))
        .WillOnce([this](auto, auto) -> requester::Coroutine {
        mgr->pollingIsRunning = false;
        co_return NSM_SW_SUCCESS;
    });

    destroyCoroutine(mgr->deviceTask(nsmDevice));
}

/**
 * DeviceTask_ActiveDevice_WithPrioritySensors_SleepsPriority
 * An online device that has at least one priority sensor triggers
 * doSleep with Priority (not NonPriority).
 */
TEST_F(SensorManagerTest,
       DeviceTask_ActiveDevice_WithPrioritySensors_SleepsPriority)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "1", 0);
    auto sensor = std::make_shared<MockSensor>("prio_sensor", "NSM_GPU");
    nsmDevice->prioritySensors.push(sensor);
    mgr->pollingIsRunning = true;

    ON_CALL(event, now(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(0), Return(0)));
    EXPECT_CALL(*mgr, pollPrioritySensors(_, _)).WillOnce(immediateCoroutine());
    EXPECT_CALL(*mgr, pollNonPrioritySensors(_, _))
        .WillOnce([this](auto, auto) -> requester::Coroutine {
        mgr->pollingIsRunning = false;
        co_return NSM_SW_SUCCESS;
    });

    destroyCoroutine(mgr->deviceTask(nsmDevice));
}

/**
 * DeviceTask_ActiveDevice_CommandsNotRetrieved_CallsRefreshMatrix
 * An online device where allCommandCodesAreRetrieved() returns false causes
 * refreshCommandMatrix to be called before polling proceeds.
 */
TEST_F(SensorManagerTest,
       DeviceTask_ActiveDevice_CommandsNotRetrieved_CallsRefreshMatrix)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "1", 0);
    nsmDevice->areMessageTypesRetrieved = false;
    mgr->pollingIsRunning = true;

    ON_CALL(event, now(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(0), Return(0)));
    EXPECT_CALL(*mgr, refreshCommandMatrix(_))
        .Times(1)
        .WillOnce(immediateCoroutine());
    EXPECT_CALL(*mgr, pollPrioritySensors(_, _)).WillOnce(immediateCoroutine());
    EXPECT_CALL(*mgr, pollNonPrioritySensors(_, _))
        .WillOnce([this](auto, auto) -> requester::Coroutine {
        mgr->pollingIsRunning = false;
        co_return NSM_SW_SUCCESS;
    });

    destroyCoroutine(mgr->deviceTask(nsmDevice));
}

// ============================================================================
// SensorManagerStaticTest – tests for static-member helpers and startPolling
// ============================================================================

/**
 * Fixture that resets all static members of SensorManagerImpl before each
 * test so tests are fully isolated from each other and from the isMCTPReady()
 * call issued by the constructor.
 */
struct SensorManagerStaticTest : public SensorManagerTest
{
    void SetUp() override
    {
        nsm::SensorManagerImpl::isReadyForReadinessCheck = false;
        nsm::SensorManagerImpl::isMCTPReadyCheck = false;
        nsm::SensorManagerImpl::isEMReadyCheck = false;
        nsm::SensorManagerImpl::readynessFailureMap.clear();
    }
};

/**
 * isNSMPollReady(): outer-if taken, inner assignment false (both flags off)
 * → returns false, isReadyForReadinessCheck stays false.
 */
TEST_F(SensorManagerStaticTest, IsNSMPollReady_BothFalse_ReturnsFalse)
{
    nsm::SensorManagerImpl::isMCTPReadyCheck = false;
    nsm::SensorManagerImpl::isEMReadyCheck = false;

    EXPECT_FALSE(mgr->isNSMPollReady());
    EXPECT_FALSE(nsm::SensorManagerImpl::isReadyForReadinessCheck);
}

/**
 * isNSMPollReady(): outer-if taken, MCTP ready but EM not ready
 * → assignment still false, inner-if not taken, returns false.
 */
TEST_F(SensorManagerStaticTest, IsNSMPollReady_MCTPReadyEMNotReady_ReturnsFalse)
{
    nsm::SensorManagerImpl::isMCTPReadyCheck = true;
    nsm::SensorManagerImpl::isEMReadyCheck = false;

    EXPECT_FALSE(mgr->isNSMPollReady());
    EXPECT_FALSE(nsm::SensorManagerImpl::isReadyForReadinessCheck);
}

/**
 * isNSMPollReady(): outer-if taken, both flags true → assignment true,
 * inner-if taken (sets map entry), returns true.
 */
TEST_F(SensorManagerStaticTest, IsNSMPollReady_BothReady_BecomesReady)
{
    nsm::SensorManagerImpl::isMCTPReadyCheck = true;
    nsm::SensorManagerImpl::isEMReadyCheck = true;

    EXPECT_TRUE(mgr->isNSMPollReady());
    EXPECT_TRUE(nsm::SensorManagerImpl::isReadyForReadinessCheck);
    EXPECT_EQ(nsm::SensorManagerImpl::readynessFailureMap.at("isNSMPollReady"),
              "true");
}

/**
 * isNSMPollReady(): isReadyForReadinessCheck already true → outer-if skipped,
 * returns true regardless of individual flag values.
 */
TEST_F(SensorManagerStaticTest, IsNSMPollReady_AlreadyReady_SkipsOuterIf)
{
    nsm::SensorManagerImpl::isReadyForReadinessCheck = true;
    // Individual flags deliberately wrong – outer-if must be skipped.
    nsm::SensorManagerImpl::isMCTPReadyCheck = false;
    nsm::SensorManagerImpl::isEMReadyCheck = false;

    EXPECT_TRUE(mgr->isNSMPollReady());
}

/**
 * dumpReadinessLogs(): empty map → loop body not entered, no crash.
 */
TEST_F(SensorManagerStaticTest, DumpReadinessLogs_EmptyMap_NoOp)
{
    nsm::SensorManagerImpl::readynessFailureMap.clear();
    EXPECT_NO_THROW(mgr->dumpReadinessLogs());
}

/**
 * dumpReadinessLogs(): non-empty map → loop body executed for each entry,
 * no crash.
 */
TEST_F(SensorManagerStaticTest, DumpReadinessLogs_NonEmptyMap_LogsEntries)
{
    nsm::SensorManagerImpl::readynessFailureMap["isMCTPReady"] = "True";
    nsm::SensorManagerImpl::readynessFailureMap["isEMReady"] = "True";
    EXPECT_NO_THROW(mgr->dumpReadinessLogs());
}

/**
 * markEMReady(): sets readynessFailureMap["isEMReady"]="True" and calls
 * isNSMPollReady(), which is safe with all flags false (returns false).
 */
TEST_F(SensorManagerStaticTest, MarkEMReady_SetsReadynessMapEntry)
{
    EXPECT_NO_THROW(mgr->markEMReady());
    EXPECT_EQ(nsm::SensorManagerImpl::readynessFailureMap.at("isEMReady"),
              "True");
}

/**
 * startPolling(): with an empty nsmDevices table the for-loop is not entered.
 * pollingIsRunning is set to true.
 */
TEST_F(SensorManagerTest, StartPolling_EmptyNsmDevices_NoTasksCreated)
{
    nsmDevices.clear();
    mgr->pollingIsRunning = false;
    EXPECT_NO_THROW(mgr->startPolling());
    EXPECT_TRUE(mgr->pollingIsRunning);
}

/**
 * DeviceTask_DeviceGoesOfflineDuringRefresh_SkipsSensorPolling
 * L731: allCommandCodesAreRetrieved() returns false → refreshCommandMatrix is
 * called. Inside refreshCommandMatrix the device goes offline. L736:
 * isOnline() → false → pollPrioritySensors/pollNonPrioritySensors NOT called.
 */
TEST_F(SensorManagerTest,
       DeviceTask_DeviceGoesOfflineDuringRefresh_SkipsSensorPolling)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "2", 0);
    nsmDevice->areMessageTypesRetrieved =
        false; // allCommandCodesAreRetrieved() → false
    mgr->pollingIsRunning = true;

    // now() call 1: t0, call 2: t1 (sleepTime calc), call 3: t0 iter2 → break
    EXPECT_CALL(event, now(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(0), Return(0)))
        .WillOnce(DoAll(SetArgPointee<1>(0), Return(0)))
        .WillOnce(DoAll(SetArgPointee<1>(0), InvokeWithoutArgs([this]() {
        mgr->pollingIsRunning = false;
    }),
                        Return(0)));
    // refreshCommandMatrix sets device offline
    EXPECT_CALL(*mgr, refreshCommandMatrix(_))
        .Times(1)
        .WillOnce([&nsmDevice](auto) -> requester::Coroutine {
        nsmDevice->isDeviceActive = false; // device goes offline
        co_return NSM_SW_SUCCESS;
    });
    // pollPrioritySensors and pollNonPrioritySensors must NOT be called
    EXPECT_CALL(*mgr, pollPrioritySensors(_, _)).Times(0);
    EXPECT_CALL(*mgr, pollNonPrioritySensors(_, _)).Times(0);

    destroyCoroutine(mgr->deviceTask(nsmDevice));
}

/**
 * DeviceTask_PollingTimeExceeded_SleepsAllowedBuffer
 * L748-750: (t1 - t0) >= pollingTimeInUsec → sleepTime = allowedBufferInUsec
 * (the ternary FALSE branch). This is triggered by returning a large timestamp
 * on the second getCurrentTimeUsec() call so that t1-t0 >= 150,000 µs.
 */
TEST_F(SensorManagerTest, DeviceTask_PollingTimeExceeded_SleepsAllowedBuffer)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "3", 0);
    mgr->pollingIsRunning = true;

    // now() returns t0=0 then t1=200000 (exceeds pollingTimeInUsec),
    // then repeats 0 for subsequent iterations.
    EXPECT_CALL(event, now(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(0), Return(0)))
        .WillOnce(DoAll(SetArgPointee<1>(200000), Return(0)))
        .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return(0)));
    EXPECT_CALL(*mgr, pollPrioritySensors(_, _))
        .WillRepeatedly(immediateCoroutine());
    // Break loop on first pollNonPrioritySensors call
    EXPECT_CALL(*mgr, pollNonPrioritySensors(_, _))
        .WillOnce([this](auto, auto) -> requester::Coroutine {
        mgr->pollingIsRunning = false;
        co_return NSM_SW_SUCCESS;
    });

    destroyCoroutine(mgr->deviceTask(nsmDevice));
}

/**
 * StartPolling_NonEmptyNsmDevices_EntersForLoopBody
 * L511: for (auto& nsmDevice : nsmDevices) – TRUE branch when nsmDevices is
 * non-empty. The device task is created (task.done() == true on default
 * Coroutine). The inactive device path calls doSleep(Priority), which clears
 * pollingIsRunning so the task exits.
 */
TEST_F(SensorManagerTest, StartPolling_NonEmptyNsmDevices_EntersForLoopBody)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "5", 0);
    nsmDevice->isDeviceActive = false; // offline → inactive path
    nsmDevices.push_back(nsmDevice);

    // Iteration 1: now() sets t0, Sleep is no-op, continue.
    // Iteration 2: now() sets t0 and breaks loop.
    EXPECT_CALL(event, now(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(0), Return(0)))
        .WillOnce(DoAll(SetArgPointee<1>(0), InvokeWithoutArgs([this]() {
        mgr->pollingIsRunning = false;
    }),
                        Return(0)));

    EXPECT_NO_THROW(mgr->startPolling());
}

// ============================================================================
// isMCTPReady() branch coverage
// ============================================================================

/**
 * IsMCTPReady_DBusFails_CatchBlockExecuted
 * In the unit-test environment bus.call() to a non-existent D-Bus service
 * throws, triggering the catch(std::exception&) block (L403-409).
 * Also covers L413 TRUE + L416 FALSE (ready stays false, isMCTPReadyCheck
 * stays false after the catch).
 */
TEST_F(SensorManagerStaticTest, IsMCTPReady_DBusFails_CatchBlockExecuted)
{
    nsm::SensorManagerImpl::isMCTPReadyCheck = false;
    nsm::SensorManagerImpl::isEMReadyCheck = false;

    // isMCTPReady() makes a real D-Bus call; in the test environment the
    // service does not exist, so bus.call() throws → catch block executes.
    bool result = mgr->isMCTPReady();
    EXPECT_FALSE(result); // L425: always returns false
    // Exception message stored in readynessFailureMap
    EXPECT_GT(nsm::SensorManagerImpl::readynessFailureMap.count("isMCTPReady"),
              0u);
}

/**
 * IsMCTPReady_AlreadySet_OuterIfSkipped
 * L413: if (!isMCTPReadyCheck) → FALSE when isMCTPReadyCheck is already true.
 * The inner block (L415-421) is NOT entered; isMCTPReadyCheck stays true.
 */
TEST_F(SensorManagerStaticTest, IsMCTPReady_AlreadySet_OuterIfSkipped)
{
    nsm::SensorManagerImpl::isMCTPReadyCheck = true; // already set
    nsm::SensorManagerImpl::isEMReadyCheck = false;

    bool result = mgr->isMCTPReady();
    EXPECT_FALSE(result);
    // isMCTPReadyCheck must remain true (block was skipped)
    EXPECT_TRUE(nsm::SensorManagerImpl::isMCTPReadyCheck);
}
