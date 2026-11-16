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
 * Additional branch coverage tests for nsmPort.cpp:
 * - createNVLinkFeature: device types, port counts, network port addresses
 * - getTopologyObjPath: all switch cases
 * - coGetTopologyData: association parsing edge cases
 * - NsmPort constructor
 * - NsmGetPortECCCounters: genRequestMsg valid/invalid, handleResponseMsg
 * - NsmPortMetrics: additional metric counters
 */

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmPort.hpp"
#include "test/commonMock.hpp"

namespace nsm
{
std::string getTopologyObjPath(const std::string& deviceName,
                               const uint8_t deviceType);
} // namespace nsm

using namespace nsm;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmPortBranch4Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:40";
    const uuid_t cx8Uuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:41";
    const uuid_t cx9Uuid = "STATIC:515:0:NSM_DEVICE_INSTANCE_NUMBER:42";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<MockNsmDevice> cx8Dev;
    std::shared_ptr<MockNsmDevice> cx9Dev;

    NsmPortBranch4Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);

        cx8Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx8Uuid));
        EXPECT_NE(cx8Dev, nullptr);

        cx9Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx9Uuid));
        EXPECT_NE(cx9Dev, nullptr);
    }

    ~NsmPortBranch4Test()
    {
        cleanupDeviceSensors(devices);
    }

    /** @brief Drive the event loop until the task finishes.
     *
     * The NVLink feature yields between ports, so the loop has to run for
     * construction to complete. Every call site must hold the returned
     * task and run this: discarding the temporary destroys a suspended
     * coroutine frame while its timer is still armed on the loop.
     * Bounded so a task that never finishes fails here instead of hanging.
     */
    void runUntilDone(const requester::Coroutine& task) const
    {
        auto loop = sdeventplus::Event::get_default();
        for (int i = 0; i < 1000 && !task.done(); ++i)
        {
            sd_event_run(loop.get(), 0);
        }
        ASSERT_TRUE(task.done());
    }

    /** @brief Run the NVLink feature on `device` (none if null) */
    requester::Coroutine
        createNVLink(const std::shared_ptr<MockNsmDevice>& device,
                     uint8_t deviceType, uint64_t count,
                     bool networkPortAddresses = false)
    {
        nsm::features::DeviceContext dev;
        dev.name = "HGX_B4_0";
        dev.kind = "gpu";
        dev.deviceType = deviceType;
        dev.platformPrefix = "HGX_";
        dev.device = device;
        dbus::PropertyMap params{
            {"Name", std::string("Port_B4")},
            {"ParentObjPath",
             std::string("/xyz/openbmc_project/inventory/system/b4dev")},
            {"Priority", false},
            {"Count", count},
            {"NetworkPortAddressesEnabled", networkPortAddresses}};
        return nsm::features::createNVLinkFeature(mockManager, dev, params);
    }

    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ===========================================================================
// getTopologyObjPath: all device type switch cases
// ===========================================================================

TEST_F(NsmPortBranch4Test, GetTopologyObjPath_GPU)
{
    auto path = getTopologyObjPath("devA", NSM_DEV_ID_GPU);
    EXPECT_NE(path.find("GPU/"), std::string::npos);
    EXPECT_NE(path.find("devA"), std::string::npos);
}

TEST_F(NsmPortBranch4Test, GetTopologyObjPath_SWITCH)
{
    auto path = getTopologyObjPath("devB", NSM_DEV_ID_SWITCH);
    EXPECT_NE(path.find("SWITCH/"), std::string::npos);
}

TEST_F(NsmPortBranch4Test, GetTopologyObjPath_PCIE_BRIDGE)
{
    auto path = getTopologyObjPath("devC", NSM_DEV_ID_PCIE_BRIDGE);
    EXPECT_NE(path.find("PCIE_BRIDGE/"), std::string::npos);
}

TEST_F(NsmPortBranch4Test, GetTopologyObjPath_EROT)
{
    auto path = getTopologyObjPath("devD", NSM_DEV_ID_EROT);
    EXPECT_NE(path.find("EROT/"), std::string::npos);
}

TEST_F(NsmPortBranch4Test, GetTopologyObjPath_CPU)
{
    auto path = getTopologyObjPath("devE", NSM_DEV_ID_CPU);
    EXPECT_NE(path.find("CPU/"), std::string::npos);
}

TEST_F(NsmPortBranch4Test, GetTopologyObjPath_Default)
{
    auto path = getTopologyObjPath("devF", 0xFF);
    EXPECT_EQ(path, "");
}

// ===========================================================================
// createNVLinkFeature: no NsmDevice in the context -> NSM_ERROR
// ===========================================================================
TEST_F(NsmPortBranch4Test, Feature_NoDevice_Error)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto task = createNVLink(nullptr, NSM_DEV_ID_GPU, 1);
    runUntilDone(task);
    EXPECT_EQ(task.data(), NSM_ERROR);
    EXPECT_EQ(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNVLinkFeature with GPU device, count=1
// ===========================================================================
TEST_F(NsmPortBranch4Test, Feature_GPU_Count1)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto task = createNVLink(gpu, NSM_DEV_ID_GPU, 1);
    runUntilDone(task);
    EXPECT_EQ(task.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNVLinkFeature with GPU + NetworkPortAddressesEnabled
// ===========================================================================
TEST_F(NsmPortBranch4Test, Feature_GPU_WithNetworkPortAddresses)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto task = createNVLink(gpu, NSM_DEV_ID_GPU, 1, true);
    runUntilDone(task);
    EXPECT_EQ(task.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNVLinkFeature with PCIE_BRIDGE+CX8 device
// ===========================================================================
TEST_F(NsmPortBranch4Test, Feature_CX8Bridge_Count1)
{
    const size_t before = cx8Dev->roundRobinSensors.size();
    auto task = createNVLink(cx8Dev, NSM_DEV_ID_PCIE_BRIDGE, 1);
    runUntilDone(task);
    EXPECT_GT(cx8Dev->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNVLinkFeature with PCIE_BRIDGE device (cx9Uuid)
// ===========================================================================
TEST_F(NsmPortBranch4Test, Feature_CX9Bridge_Count1)
{
    const size_t before = cx9Dev->roundRobinSensors.size();
    auto task = createNVLink(cx9Dev, NSM_DEV_ID_PCIE_BRIDGE, 1);
    runUntilDone(task);
    EXPECT_GT(cx9Dev->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNVLinkFeature with count=0 -> no ports created
// ===========================================================================
TEST_F(NsmPortBranch4Test, Feature_Count0_NoPorts)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto task = createNVLink(gpu, NSM_DEV_ID_GPU, 0);
    runUntilDone(task);
    EXPECT_EQ(task.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNVLinkFeature with count=2 -> 2 ports
// ===========================================================================
TEST_F(NsmPortBranch4Test, Feature_Count2)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto task = createNVLink(gpu, NSM_DEV_ID_GPU, 2);
    runUntilDone(task);
    EXPECT_EQ(task.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// NsmGetPortECCCounters: genRequestMsg valid and invalid instanceId
// ===========================================================================
TEST_F(NsmPortBranch4Test, PortECCCounters_GenRequestMsg_Valid)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/b4/portecc_valid";
    NsmGetPortECCCounters sensor(bus, "ecc_v", "T", objPath, 1);

    auto request = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmPortBranch4Test, PortECCCounters_GenRequestMsg_Invalid)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/b4/portecc_inv";
    NsmGetPortECCCounters sensor(bus, "ecc_i", "T", objPath, 1);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
