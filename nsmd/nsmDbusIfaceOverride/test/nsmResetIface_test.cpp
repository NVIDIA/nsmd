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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmResetIface.hpp"

using namespace nsm;

struct NsmResetIntfTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string resetPath =
        "/xyz/openbmc_project/control/processor/reset0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmResetIntfTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmResetIntfTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmResetIntfTest, testNsmResetIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetIntf = std::make_shared<NsmResetIntf>(bus, resetPath.c_str());

    EXPECT_NE(resetIntf, nullptr);
}

TEST_F(NsmResetIntfTest, testNsmResetIntfReset)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetIntf = std::make_shared<NsmResetIntf>(bus, resetPath.c_str());

    int result = resetIntf->reset();
    EXPECT_EQ(result, 0);
}

TEST_F(NsmResetIntfTest, testNsmResetAsyncIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t deviceIndex = 0;

    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, resetPath.c_str(), gpu, deviceIndex);

    EXPECT_NE(resetAsyncIntf, nullptr);
    EXPECT_EQ(resetAsyncIntf->device, gpu);
    EXPECT_EQ(resetAsyncIntf->deviceIndex, deviceIndex);
}

TEST_F(NsmResetIntfTest, testNsmResetDeviceIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string deviceResetPath = "/xyz/openbmc_project/control/reset0";

    auto resetDeviceIntf =
        std::make_shared<NsmResetDeviceIntf>(bus, deviceResetPath.c_str());

    EXPECT_NE(resetDeviceIntf, nullptr);
}

TEST_F(NsmResetIntfTest, testNsmResetDeviceIntfReset)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string deviceResetPath = "/xyz/openbmc_project/control/reset0";

    auto resetDeviceIntf =
        std::make_shared<NsmResetDeviceIntf>(bus, deviceResetPath.c_str());

    EXPECT_NO_THROW(resetDeviceIntf->reset());
}

TEST_F(NsmResetIntfTest, testNsmNetworkDeviceResetAsyncIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string networkResetPath =
        "/xyz/openbmc_project/control/network_reset0";

    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, networkResetPath.c_str(), gpu);

    EXPECT_NE(networkResetIntf, nullptr);
    EXPECT_EQ(networkResetIntf->device, gpu);
}

TEST_F(NsmResetIntfTest, testMultipleResetInterfaces)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetIntf1 = std::make_shared<NsmResetIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset1");
    auto resetIntf2 = std::make_shared<NsmResetIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset2");

    EXPECT_NE(resetIntf1, nullptr);
    EXPECT_NE(resetIntf2, nullptr);
    EXPECT_NE(resetIntf1, resetIntf2);
}

TEST_F(NsmResetIntfTest, testResetAsyncWithDifferentDeviceIndex)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetAsync1 = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset1", gpu, 0);
    auto resetAsync2 = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset2", gpu, 1);

    EXPECT_EQ(resetAsync1->deviceIndex, 0);
    EXPECT_EQ(resetAsync2->deviceIndex, 1);
}
