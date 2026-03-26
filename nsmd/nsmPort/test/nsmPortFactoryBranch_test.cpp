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
 * Branch coverage tests for createNsmPortSensor factory coroutine
 * in nsmd/nsmPort/nsmPort.cpp.
 *
 * Targets the FALSE branches of:
 *   allCurrentIfaceProperties.count("Name")
 *   allCurrentIfaceProperties.count("ParentObjPath")
 *   allCurrentIfaceProperties.count("Priority")
 *   allCurrentIfaceProperties.count("Count")
 *   allCurrentIfaceProperties.count("DeviceType")
 *   allCurrentIfaceProperties.count("UUID")
 *
 * Also targets:
 *   coGetTopologyData: count("InventoryObjPath"), count("LogicalPortNumber"),
 *   count("Associations") FALSE branches
 */

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmPort.hpp"

#undef private
#undef protected

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmPortSensorGeneric(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath);
requester::Coroutine
    createNsmPortSensorWithNetworkPortAddresses(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath);
} // namespace nsm

// ============================================================================
// Fixture
// ============================================================================
struct NsmPortFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:8";
    const std::string portInterface =
        "xyz.openbmc_project.Configuration.NSM_NVLink";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPortFactoryBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// All optional properties absent
// Exercises ALL count() FALSE branches at once
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, DISABLED_Factory_AllPropsAbsent)
{
    const std::string objPath = "/xyz/openbmc_project/config/port_fb_1";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, portInterface);
    // Intentionally empty
    (void)propMap;

    // With empty UUID, getNsmDeviceFromStaticUUID returns a device
    // Count=0 means no port loop iterations
    createNsmPortSensorGeneric(mockManager, portInterface, objPath);
}

// ============================================================================
// Only Name and UUID present, rest absent
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, Factory_OnlyNameAndUuid)
{
    const std::string objPath = "/xyz/openbmc_project/config/port_fb_2";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, portInterface);
    propMap["Name"] = std::string("NVLink_0");
    propMap["UUID"] = gpuUuid;
    // ParentObjPath, Priority, Count, DeviceType absent

    createNsmPortSensorGeneric(mockManager, portInterface, objPath);
}

// ============================================================================
// With Count=0 - no port loop iterations
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, Factory_CountZero)
{
    const std::string objPath = "/xyz/openbmc_project/config/port_fb_3";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, portInterface);
    propMap["Name"] = std::string("NVLink_Zero");
    propMap["UUID"] = gpuUuid;
    propMap["ParentObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/GPU_0");
    propMap["Priority"] = false;
    propMap["Count"] = uint64_t{0};
    propMap["DeviceType"] = uint64_t{NSM_DEV_ID_GPU};

    createNsmPortSensorGeneric(mockManager, portInterface, objPath);

    // No port sensors created
}

// ============================================================================
// ParentObjPath absent but Count > 0 - exercises FALSE branch for
// ParentObjPath while still creating ports
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, Factory_NoParentObjPath)
{
    const std::string objPath = "/xyz/openbmc_project/config/port_fb_4";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, portInterface);
    propMap["Name"] = std::string("NVLink_NoParent");
    propMap["UUID"] = gpuUuid;
    // ParentObjPath absent
    propMap["Priority"] = false;
    propMap["Count"] = uint64_t{1};
    propMap["DeviceType"] = uint64_t{NSM_DEV_ID_GPU};

    createNsmPortSensorGeneric(mockManager, portInterface, objPath);

    // Port sensors created using empty parent path
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// DeviceType absent - exercises FALSE branch, default 0
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, Factory_NoDeviceType)
{
    const std::string objPath = "/xyz/openbmc_project/config/port_fb_5";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, portInterface);
    propMap["Name"] = std::string("NVLink_NoDevType");
    propMap["UUID"] = gpuUuid;
    propMap["ParentObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/GPU_1");
    propMap["Count"] = uint64_t{1};
    // DeviceType absent -> deviceType defaults to 0

    createNsmPortSensorGeneric(mockManager, portInterface, objPath);

    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Priority absent - exercises FALSE branch for Priority
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, Factory_NoPriority)
{
    const std::string objPath = "/xyz/openbmc_project/config/port_fb_6";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, portInterface);
    propMap["Name"] = std::string("NVLink_NoPrio");
    propMap["UUID"] = gpuUuid;
    propMap["ParentObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/GPU_2");
    propMap["Count"] = uint64_t{1};
    propMap["DeviceType"] = uint64_t{NSM_DEV_ID_GPU};
    // Priority absent

    createNsmPortSensorGeneric(mockManager, portInterface, objPath);

    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// WithNetworkPortAddresses factory - all props absent
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, DISABLED_FactoryWithNetAddr_AllPropsAbsent)
{
    const std::string objPath = "/xyz/openbmc_project/config/port_fb_7";
    const std::string netAddrInterface =
        "xyz.openbmc_project.Configuration.NSM_NVLinkWithNetworkPortAddresses";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath,
                                                      netAddrInterface);
    (void)propMap;

    createNsmPortSensorWithNetworkPortAddresses(mockManager, netAddrInterface,
                                                objPath);
}
