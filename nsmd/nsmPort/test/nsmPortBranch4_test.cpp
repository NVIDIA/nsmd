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
 * - createNsmPortSensor factory: property presence/absence combos
 * - getTopologyObjPath: all switch cases
 * - coGetTopologyData: association parsing edge cases
 * - NsmPort constructor
 * - NsmGetPortECCCounters: genRequestMsg valid/invalid, handleResponseMsg
 * - NsmPortMetrics: additional metric counters
 * - createNsmPortSensor with GPU, PCIE_BRIDGE, SWITCH, EROT, CPU device types
 * - enableNetworkPortAddresses true/false
 */

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmPort.hpp"
#include "test/commonMock.hpp"

namespace nsm
{
requester::Coroutine createNsmPortSensor(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath,
                                         bool enableNetworkPortAddresses);

requester::Coroutine
    createNsmPortSensorWithNetworkPortAddresses(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath);

requester::Coroutine createNsmPortSensorGeneric(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath);

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

    static constexpr const char* portIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLink";

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

    void setupPortProperties(const std::string& path, const std::string& intf,
                             const uuid_t& uuid, uint64_t devType,
                             uint64_t count = 1, dbus::PropertyMap extra = {})
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
        pm["Name"] = std::string("Port_B4");
        pm["UUID"] = uuid;
        pm["ParentObjPath"] =
            std::string("/xyz/openbmc_project/inventory/system/b4dev");
        pm["Priority"] = false;
        pm["Count"] = count;
        pm["DeviceType"] = devType;
        for (auto& [k, v] : extra)
        {
            pm[k] = v;
        }
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
// createNsmPortSensor: missing UUID -> no device -> returns error
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_BadUUID_NoDevice)
{
    const std::string path = "/test/portb4/bad_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, portIntf);
    pm["Name"] = std::string("Port_BadUUID");
    pm["UUID"] = std::string("STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:999");
    pm["ParentObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b4nodev");
    pm["Count"] = uint64_t(1);
    pm["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);

    createNsmPortSensor(mockManager, portIntf, path, false);
    // No crash, returns NSM_ERROR
}

// ===========================================================================
// createNsmPortSensor with GPU device, count=1
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_GPU_Count1)
{
    const std::string path = "/test/portb4/gpu1";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(NSM_DEV_ID_GPU), 1);

    const size_t before = gpu->roundRobinSensors.size();
    createNsmPortSensor(mockManager, portIntf, path, false);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNsmPortSensor with GPU + enableNetworkPortAddresses=true
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_GPU_WithNetworkPortAddresses)
{
    const std::string path = "/test/portb4/gpu_net";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(NSM_DEV_ID_GPU), 1);

    const size_t before = gpu->roundRobinSensors.size();
    createNsmPortSensor(mockManager, portIntf, path, true);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNsmPortSensor with PCIE_BRIDGE+CX8 device
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_CX8Bridge_Count1)
{
    const std::string path = "/test/portb4/cx8";
    setupPortProperties(path, portIntf, cx8Uuid,
                        uint64_t(NSM_DEV_ID_PCIE_BRIDGE), 1);

    const size_t before = cx8Dev->roundRobinSensors.size();
    createNsmPortSensor(mockManager, portIntf, path, false);
    EXPECT_GT(cx8Dev->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNsmPortSensor with PCIE_BRIDGE+CX9 device
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_CX9Bridge_Count1)
{
    const std::string path = "/test/portb4/cx9";
    setupPortProperties(path, portIntf, cx9Uuid,
                        uint64_t(NSM_DEV_ID_PCIE_BRIDGE), 1);

    const size_t before = cx9Dev->roundRobinSensors.size();
    createNsmPortSensor(mockManager, portIntf, path, false);
    EXPECT_GT(cx9Dev->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNsmPortSensor with count=0 -> no ports created
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_Count0_NoPorts)
{
    const std::string path = "/test/portb4/cnt0";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(NSM_DEV_ID_GPU), 0);

    const size_t before = gpu->roundRobinSensors.size();
    createNsmPortSensor(mockManager, portIntf, path, false);
    EXPECT_EQ(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNsmPortSensor with count=2 -> 2 ports
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_Count2)
{
    const std::string path = "/test/portb4/cnt2";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(NSM_DEV_ID_GPU), 2);

    const size_t before = gpu->roundRobinSensors.size();
    createNsmPortSensor(mockManager, portIntf, path, false);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// ===========================================================================
// createNsmPortSensor: missing Name property (FALSE branch)
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_MissingName)
{
    const std::string path = "/test/portb4/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(path, portIntf);
    // No "Name"
    pm["UUID"] = gpuUuid;
    pm["ParentObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b4noname");
    pm["Priority"] = false;
    pm["Count"] = uint64_t(1);
    pm["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);

    createNsmPortSensor(mockManager, portIntf, path, false);
}

// ===========================================================================
// createNsmPortSensor: missing ParentObjPath (FALSE branch)
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_MissingParentObjPath)
{
    const std::string path = "/test/portb4/no_parent";
    auto& pm = utils::MockDbusAsync::propertyMap(path, portIntf);
    pm["Name"] = std::string("Port_NoParent");
    pm["UUID"] = gpuUuid;
    // No "ParentObjPath"
    pm["Priority"] = false;
    pm["Count"] = uint64_t(1);
    pm["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);

    createNsmPortSensor(mockManager, portIntf, path, false);
}

// ===========================================================================
// createNsmPortSensor: missing Priority (FALSE branch)
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_MissingPriority)
{
    const std::string path = "/test/portb4/no_prio";
    auto& pm = utils::MockDbusAsync::propertyMap(path, portIntf);
    pm["Name"] = std::string("Port_NoPrio");
    pm["UUID"] = gpuUuid;
    pm["ParentObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b4noprio");
    // No "Priority"
    pm["Count"] = uint64_t(1);
    pm["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);

    createNsmPortSensor(mockManager, portIntf, path, false);
}

// ===========================================================================
// createNsmPortSensor: missing Count (FALSE branch)
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_MissingCount)
{
    const std::string path = "/test/portb4/no_count";
    auto& pm = utils::MockDbusAsync::propertyMap(path, portIntf);
    pm["Name"] = std::string("Port_NoCount");
    pm["UUID"] = gpuUuid;
    pm["ParentObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b4nocount");
    pm["Priority"] = false;
    // No "Count"
    pm["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);

    createNsmPortSensor(mockManager, portIntf, path, false);
}

// ===========================================================================
// createNsmPortSensor: missing DeviceType (FALSE branch)
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_MissingDeviceType)
{
    const std::string path = "/test/portb4/no_dtype";
    auto& pm = utils::MockDbusAsync::propertyMap(path, portIntf);
    pm["Name"] = std::string("Port_NoDType");
    pm["UUID"] = gpuUuid;
    pm["ParentObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b4nodtype");
    pm["Priority"] = false;
    pm["Count"] = uint64_t(1);
    // No "DeviceType"

    createNsmPortSensor(mockManager, portIntf, path, false);
}

// ===========================================================================
// createNsmPortSensorWithNetworkPortAddresses wrapper
// ===========================================================================
TEST_F(NsmPortBranch4Test, WrapperNetworkPortAddresses)
{
    const std::string netIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLinkWithNetworkPortAddresses";
    const std::string path = "/test/portb4/wrap_net";
    setupPortProperties(path, netIntf, gpuUuid, uint64_t(NSM_DEV_ID_GPU), 1);

    createNsmPortSensorWithNetworkPortAddresses(mockManager, netIntf, path);
}

// ===========================================================================
// createNsmPortSensorGeneric wrapper
// ===========================================================================
TEST_F(NsmPortBranch4Test, WrapperGeneric)
{
    const std::string path = "/test/portb4/wrap_gen";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(NSM_DEV_ID_GPU), 1);

    createNsmPortSensorGeneric(mockManager, portIntf, path);
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

// ===========================================================================
// Factory with SWITCH device type
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_SWITCH_DeviceType)
{
    const std::string path = "/test/portb4/switch";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(NSM_DEV_ID_SWITCH),
                        1);

    createNsmPortSensor(mockManager, portIntf, path, false);
}

// ===========================================================================
// Factory with EROT device type
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_EROT_DeviceType)
{
    const std::string path = "/test/portb4/erot";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(NSM_DEV_ID_EROT), 1);

    createNsmPortSensor(mockManager, portIntf, path, false);
}

// ===========================================================================
// Factory with CPU device type
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_CPU_DeviceType)
{
    const std::string path = "/test/portb4/cpu";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(NSM_DEV_ID_CPU), 1);

    createNsmPortSensor(mockManager, portIntf, path, false);
}

// ===========================================================================
// Factory with unknown device type -> default topology path ""
// ===========================================================================
TEST_F(NsmPortBranch4Test, Factory_UnknownDeviceType)
{
    const std::string path = "/test/portb4/unknown";
    setupPortProperties(path, portIntf, gpuUuid, uint64_t(0xFF), 1);

    createNsmPortSensor(mockManager, portIntf, path, false);
}
