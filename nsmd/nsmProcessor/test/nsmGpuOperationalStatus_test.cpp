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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmGpuOperationalStatus.hpp"

using namespace nsm;

using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::server::
    OperationalStatus::StateType;

// =============================================================================
// Fixture
// =============================================================================

struct NsmGpuOperationalStatusTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmGpuOperationalStatusTest() : SensorManagerTest(devices) {}
    ~NsmGpuOperationalStatusTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmGpuOperationalStatus>
        makeSensor(const std::string& suffix)
    {
        return std::make_shared<NsmGpuOperationalStatus>(
            utils::DBusHandler::getBus(), "GpuOp" + suffix, "NSM_Processor",
            "/xyz/openbmc_project/inventory/test/gpuop" + suffix);
    }

    std::shared_ptr<MockNsmDevice> makeDevice(int eid, bool online = true)
    {
        auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID",
                                                   std::to_string(eid), 0);
        dev->isDeviceActive = online;
        return dev;
    }
};

// =============================================================================
// update() branch coverage
// =============================================================================

// update(): eid == 0 → state=None, functional=false
TEST_F(NsmGpuOperationalStatusTest, Update_EidZero_StateNone_FunctionalFalse)
{
    auto sensor = makeSensor("_eid0");
    auto dev = makeDevice(0); // eid=0
    sensor->update(dev).detach();
    EXPECT_EQ(sensor->operationalStatusIntf->state(), StateType::None);
    EXPECT_FALSE(sensor->operationalStatusIntf->functional());
}

// update(): eid != 0, isOnline()=true → state=Enabled, functional=true
TEST_F(NsmGpuOperationalStatusTest,
       Update_NonZeroEidOnline_StateEnabled_FunctionalTrue)
{
    auto sensor = makeSensor("_online");
    auto dev = makeDevice(5, true); // eid=5, online
    sensor->update(dev).detach();
    EXPECT_EQ(sensor->operationalStatusIntf->state(), StateType::Enabled);
    EXPECT_TRUE(sensor->operationalStatusIntf->functional());
}

// update(): eid != 0, isOnline()=false → state=UnavailableOffline,
// functional=false
TEST_F(NsmGpuOperationalStatusTest,
       Update_NonZeroEidOffline_StateUnavailable_FunctionalFalse)
{
    auto sensor = makeSensor("_offline");
    auto dev = makeDevice(5, false); // eid=5, offline
    sensor->update(dev).detach();
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              StateType::UnavailableOffline);
    EXPECT_FALSE(sensor->operationalStatusIntf->functional());
}

// handleOfflineState(): sets UnavailableOffline + functional=false
TEST_F(NsmGpuOperationalStatusTest,
       HandleOfflineState_StateUnavailableOffline_FunctionalFalse)
{
    auto sensor = makeSensor("_hoff");
    sensor->handleOfflineState();
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              StateType::UnavailableOffline);
    EXPECT_FALSE(sensor->operationalStatusIntf->functional());
}
