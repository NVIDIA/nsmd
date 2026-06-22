/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Branch coverage batch 2 for nsmd/sensorManager.cpp
 *
 * Covers branches not hit by sensorManagerBranch_test.cpp:
 * - SensorManager::getInstance(): !sensorManagerInstance → throw (TRUE branch)
 * - SensorManagerImpl::initialize(): sensorManagerInstance non-null → throw
 * - isNSMPollReady(): isReadyForReadinessCheck already true → outer-if skipped
 * - isNSMPollReady(): both flags true → inner-if TRUE →
 * isReadyForReadinessCheck=true
 * - isMCTPReady(): D-Bus fails → catch block; isMCTPReadyCheck already true
 * - pollNonPrioritySensors(): device !isReady && !hasSensorsToUpdate →
 * markDeviceAsReady
 * - pollNonPrioritySensors(): RoundRobin sensor refreshed → markDeviceAsReady
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

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
#include "nsmServiceReadyInterface.hpp"
#include "sensorManager.hpp"
#include "sensorQueueMap.hpp"

#undef private
#undef protected

using namespace ::testing;

// sensorManagerInstance has external linkage (see sensorManager.cpp comment)
// but is not declared in any header — declare it here for test access.
namespace nsm
{
extern std::unique_ptr<SensorManager> sensorManagerInstance;
} // namespace nsm

// ============================================================================
// Reuse MockSensorManagerImplPolling from sensorManagerBranch_test.cpp
// (duplicate class definition for isolation)
// ============================================================================

class MockSMImpl2 : public nsm::SensorManagerImpl
{
  public:
    MockSMImpl2(
        sdbusplus::bus_t& bus, common::Event& event,
        nsm::RequesterHandler& handler, nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        nsm::NsmDeviceTable& nsmDevices, mctp_socket::Manager& sockManager) :
        SensorManagerImpl(bus, event, handler, instanceIdDb, objServer,
                          eidTable, nsmDevices, sockManager, false)
    {}
};

// ============================================================================
// Fixture
// ============================================================================

struct SensorManagerBranch2Test : public ::testing::Test
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
    std::unique_ptr<MockSMImpl2> mgr;

    SensorManagerBranch2Test() :
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus)),
        event(), handler(event, instanceIdDb, sockManager, false),
        mgr(std::make_unique<MockSMImpl2>(*systemBus, event, handler,
                                          instanceIdDb, *objServer, eidTable,
                                          nsmDevices, sockManager))
    {}

    void SetUp() override
    {
        nsm::SensorManagerImpl::isReadyForReadinessCheck = false;
        nsm::SensorManagerImpl::isMCTPReadyCheck = false;
        nsm::SensorManagerImpl::isEMReadyCheck = false;
        nsm::SensorManagerImpl::readynessFailureMap.clear();
    }
};

// ============================================================================
// SensorManager::getInstance(): !sensorManagerInstance → throw (TRUE branch)
// ============================================================================

TEST(SensorManagerGetInstance, NoInstance_Throws)
{
    // Save and null out the global instance
    auto saved = std::move(nsm::sensorManagerInstance);
    nsm::sensorManagerInstance = nullptr;

    EXPECT_THROW(nsm::SensorManager::getInstance(), std::runtime_error);

    // Restore
    nsm::sensorManagerInstance = std::move(saved);
}

// ============================================================================
// SensorManagerImpl::initialize(): sensorManagerInstance non-null → throw
// (Line 66 TRUE branch: "already initialized" path)
// ============================================================================

TEST_F(SensorManagerBranch2Test, Initialize_AlreadyInitialized_Throws)
{
    // The fixture creates mgr via constructor, not via initialize(), so
    // sensorManagerInstance is null. Call initialize() once to set it, then
    // call again to trigger the "already initialized" throw.
    nsm::sensorManagerInstance.reset();
    ASSERT_NO_THROW(nsm::SensorManagerImpl::initialize(
        *systemBus, event, handler, instanceIdDb, *objServer, eidTable,
        nsmDevices, sockManager, false));
    ASSERT_NE(nsm::sensorManagerInstance, nullptr);

    // Second call → already initialized → throws
    EXPECT_THROW(nsm::SensorManagerImpl::initialize(
                     *systemBus, event, handler, instanceIdDb, *objServer,
                     eidTable, nsmDevices, sockManager, false),
                 std::logic_error);

    // Clean up global so other tests are not affected
    nsm::sensorManagerInstance.reset();
}

// ============================================================================
// isNSMPollReady(): isReadyForReadinessCheck already true → outer-if skipped
// (Line 488 FALSE branch)
// ============================================================================

TEST_F(SensorManagerBranch2Test, IsNSMPollReady_AlreadySet_SkipsOuter)
{
    nsm::SensorManagerImpl::isReadyForReadinessCheck = true;
    bool result = mgr->isNSMPollReady();
    EXPECT_TRUE(result);
    // stays true without re-entering the outer if
    EXPECT_TRUE(nsm::SensorManagerImpl::isReadyForReadinessCheck);
}

// ============================================================================
// isNSMPollReady(): both MCTP and EM ready → isReadyForReadinessCheck=true
// (Line 488 TRUE + Line 490 TRUE inner-if)
// ============================================================================

TEST_F(SensorManagerBranch2Test, IsNSMPollReady_BothReady_SetsFlag)
{
    nsm::SensorManagerImpl::isReadyForReadinessCheck = false;
    nsm::SensorManagerImpl::isMCTPReadyCheck = true;
    nsm::SensorManagerImpl::isEMReadyCheck = true;

    bool result = mgr->isNSMPollReady();
    EXPECT_TRUE(result);
    EXPECT_TRUE(nsm::SensorManagerImpl::isReadyForReadinessCheck);
}

// ============================================================================
// isNSMPollReady(): MCTP false, EM true → remains false
// ============================================================================

TEST_F(SensorManagerBranch2Test, IsNSMPollReady_MCTPFalse_StaysFalse)
{
    nsm::SensorManagerImpl::isReadyForReadinessCheck = false;
    nsm::SensorManagerImpl::isMCTPReadyCheck = false;
    nsm::SensorManagerImpl::isEMReadyCheck = true;

    bool result = mgr->isNSMPollReady();
    EXPECT_FALSE(result);
    EXPECT_FALSE(nsm::SensorManagerImpl::isReadyForReadinessCheck);
}

// ============================================================================
// isMCTPReady(): D-Bus call fails → catch block + isMCTPReadyCheck=false
// (Line 444 catch block; outer-if TRUE path → sets isMCTPReadyCheck=false)
// ============================================================================

TEST_F(SensorManagerBranch2Test, IsMCTPReady_DBusFails_CatchBlock)
{
    nsm::SensorManagerImpl::isMCTPReadyCheck = false;
    bool result = mgr->isMCTPReady();
    EXPECT_FALSE(result);
    EXPECT_GT(nsm::SensorManagerImpl::readynessFailureMap.count("isMCTPReady"),
              0u);
}

// ============================================================================
// isMCTPReady(): isMCTPReadyCheck already true → outer-if skipped
// (Line 444 FALSE branch)
// ============================================================================

TEST_F(SensorManagerBranch2Test, IsMCTPReady_AlreadySet_SkipsOuter)
{
    nsm::SensorManagerImpl::isMCTPReadyCheck = true;
    bool result = mgr->isMCTPReady();
    EXPECT_FALSE(result); // always returns false at end regardless
    EXPECT_TRUE(nsm::SensorManagerImpl::isMCTPReadyCheck);
}

// ============================================================================
// pollNonPrioritySensors(): device NOT ready, no sensors remain →
// markDeviceAsReady + checkAllDevicesReady (L718 TRUE branch)
// ============================================================================

struct SensorManagerBranch2NsmSvcTest : public SensorManagerBranch2Test
{
    nsm::NsmServiceReadyIntf* svcReadyPtr = nullptr;
    nsm::NsmDeviceTable svcDevices;

    void SetUp() override
    {
        SensorManagerBranch2Test::SetUp();
        if (!nsm::NsmServiceReadyIntf::instance)
        {
            svcReadyPtr = new nsm::NsmServiceReadyIntf(
                *systemBus, "/test/svc_rdy_br2", svcDevices);
            nsm::NsmServiceReadyIntf::instance = svcReadyPtr;
        }
    }

    void TearDown() override
    {
        if (svcReadyPtr)
        {
            nsm::NsmServiceReadyIntf::instance = nullptr;
            delete svcReadyPtr;
            svcReadyPtr = nullptr;
        }
    }
};

TEST_F(SensorManagerBranch2NsmSvcTest,
       PollNonPrioritySensors_EmptyQueues_DeviceNotReady_MarksReady)
{
    auto device = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "30", 0);
    device->isDeviceActive = true;
    device->isDeviceReady = false; // device NOT ready

    ON_CALL(event, now(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(0), Return(0)));

    // All queues empty + device not ready → L718: markDeviceAsReady called
    auto cr = mgr->pollNonPrioritySensors(device, 0);
    EXPECT_EQ(cr.data(), NSM_SW_SUCCESS);
}

// ============================================================================
// pollNonPrioritySensors(): RoundRobin sensor that is already refreshed →
// device not ready → markDeviceAsReady (L664 TRUE branch)
// ============================================================================

TEST_F(SensorManagerBranch2NsmSvcTest,
       PollNonPrioritySensors_RRRefreshedSensor_DeviceNotReady_MarksReady)
{
    auto device = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "31", 0);
    device->isDeviceActive = true;
    device->isDeviceReady = false; // NOT ready

    auto sensor = std::make_shared<MockSensor>("rr_refreshed", "NSM_GPU",
                                               NSM_SW_SUCCESS);
    sensor->isRefreshed =
        true; // already refreshed → triggers markDeviceAsReady
    sensor->refreshLimitInUsec = 1000000; // won't need update
    sensor->setLastUpdatedTimeStamp(UINT64_MAX);
    device->roundRobinSensors.push(sensor);

    ON_CALL(event, now(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(0), Return(0)));

    auto cr = mgr->pollNonPrioritySensors(device, 0);
    EXPECT_EQ(cr.data(), NSM_SW_SUCCESS);
    EXPECT_TRUE(device->isReady());
}
