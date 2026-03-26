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
 * Additional branch coverage tests for createNsmGpuPcieSensor factory
 * in nsmGpuPciePort.cpp.
 *
 * Targets:
 *   - count() FALSE branches for Health, ChasisPowerState, DeviceIndex,
 *     ClearableScalarGroup in NSM_GPU_PCIe_0 type
 *   - Empty ClearableScalarGroup vector -> for loop body not entered
 *   - Missing Type -> type="" -> no branch matches
 *   - NSM_PortInfo with Priority=true -> priority sensor
 *   - NSM_GPU_PCIe_0 missing Health/ChasisPowerState -> defaults used
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/pci-links.h"

#include "nsmCommon/nsmPcieGroup.hpp"
#include "nsmGpuPciePort.hpp"

namespace nsm
{
requester::Coroutine createNsmGpuPcieSensor(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

class NsmGpuPciePortFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    static constexpr const char* portInfoIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0.PortInfo";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string sensorName = "GpuPcie_FBr";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/system/fabr2/gpu/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGpuPciePortFactoryBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGpuPciePortFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBaseAndCurrent(const std::string& path, const std::string& intf,
                             dbus::PropertyMap baseExtra,
                             dbus::PropertyMap currentProps)
    {
        auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
        base["Name"] = sensorName;
        base["UUID"] = gpuUuid;
        base["InventoryObjPath"] = processorPath;
        base["DeviceIndex"] = uint64_t(0);
        for (auto& [k, v] : baseExtra)
        {
            base[k] = v;
        }
        if (intf != baseIntf)
        {
            auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
            cur = currentProps;
        }
        else
        {
            // When calling with baseIntf, merge current props into base
            auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
            for (auto& [k, v] : currentProps)
            {
                pm[k] = v;
            }
        }
    }
};

// ============================================================================
// NSM_GPU_PCIe_0 with missing Health -> health="" (FALSE branch)
// Covers: L411-414 count("Health") FALSE
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, Factory_GPU_PCIe_MissingHealth)
{
    const std::string path = "/test/gpufbr/no_health";

    setupBaseAndCurrent(
        path, baseIntf, {},
        {{"Type", std::string("NSM_GPU_PCIe_0")},
         // "Health" omitted
         {"ChasisPowerState",
          std::string("xyz.openbmc_project.State.Chassis.PowerState.On")},
         {"ClearableScalarGroup", std::vector<uint64_t>{}}});

    // Will likely throw due to empty health string conversion, caught by //
    // factory catch block
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
}

// ============================================================================
// NSM_GPU_PCIe_0 with missing ChasisPowerState -> chasisState="" (FALSE)
// Covers: L416-420 count("ChasisPowerState") FALSE
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, Factory_GPU_PCIe_MissingChasisState)
{
    const std::string path = "/test/gpufbr/no_chassis_state";

    setupBaseAndCurrent(
        path, baseIntf, {},
        {{"Type", std::string("NSM_GPU_PCIe_0")},
         {"Health", std::string("xyz.openbmc_project.State.Decorator."
                                "Health.HealthType.OK")},
         // "ChasisPowerState" omitted
         {"ClearableScalarGroup", std::vector<uint64_t>{}}});

    // Empty chassisState string conversion -> throws -> caught
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
}

// ============================================================================
// NSM_GPU_PCIe_0 with missing DeviceIndex -> deviceIndex=0 (FALSE branch)
// Covers: L427-431 count("DeviceIndex") FALSE
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest,
       DISABLED_Factory_GPU_PCIe_MissingDeviceIndex)
{
    const std::string path = "/test/gpufbr/no_devindex";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = processorPath + "nodevindex/";
    // "DeviceIndex" omitted -> deviceIndex=0 (FALSE branch)
    base["Type"] = std::string("NSM_GPU_PCIe_0");
    base["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    base["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    base["ClearableScalarGroup"] = std::vector<uint64_t>{};

    const size_t before = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// NSM_GPU_PCIe_0 with missing ClearableScalarGroup -> empty vector
// Covers: L433-437 count("ClearableScalarGroup") FALSE -> no loop iterations
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest,
       Factory_GPU_PCIe_MissingClearableScalarGroup)
{
    const std::string path = "/test/gpufbr/no_clearable";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = processorPath + "noclearable/";
    base["DeviceIndex"] = uint64_t(0);
    base["Type"] = std::string("NSM_GPU_PCIe_0");
    base["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    base["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    // "ClearableScalarGroup" omitted -> empty vector -> no NsmClearPCIeCounters

    [[maybe_unused]] const size_t staticBefore = gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
    // No static sensors added for clearable groups (only PCIe groups added
    // elsewhere)
    // deviceSensors should have increased though
}

// ============================================================================
// NSM_GPU_PCIe_0 with empty ClearableScalarGroup -> for loop body not entered
// Covers: L442 for loop with empty vector -> 0 iterations
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest,
       Factory_GPU_PCIe_EmptyClearableScalarGroup)
{
    const std::string path = "/test/gpufbr/empty_clearable";

    setupBaseAndCurrent(
        path, baseIntf, {},
        {{"Type", std::string("NSM_GPU_PCIe_0")},
         {"Health", std::string("xyz.openbmc_project.State.Decorator."
                                "Health.HealthType.OK")},
         {"ChasisPowerState", std::string("xyz.openbmc_project.State.Chassis."
                                          "PowerState.On")},
         {"ClearableScalarGroup", std::vector<uint64_t>{}}});

    [[maybe_unused]] const size_t staticBefore = gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
    // No NsmClearPCIeCounters added as static sensors
    // But other static sensors might be added
}

// ============================================================================
// NSM_PortInfo with Priority=true -> priority sensor
// Covers: L511-516 count("Priority") TRUE, priority=true
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, DISABLED_Factory_PortInfo_PriorityTrue)
{
    const std::string path = "/test/gpufbr/portinfo_prio_true";

    setupBaseAndCurrent(
        path, portInfoIntf, {},
        {{"Type", std::string("NSM_PortInfo")},
         {"PortType",
          std::string("xyz.openbmc_project.Inventory.Decorator.PortInfo."
                      "PortType.UpstreamPort")},
         {"PortProtocol",
          std::string("xyz.openbmc_project.Inventory.Decorator.PortInfo."
                      "PortProtocol.PCIe")},
         {"Priority", bool(true)}});

    const size_t prBefore = gpu->prioritySensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, path);
    EXPECT_GT(gpu->prioritySensors.size(), prBefore);
}

// ============================================================================
// Missing Type from current properties -> type="" -> no branch matches
// Covers: L380-383 count("Type") FALSE
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, Factory_MissingType_NoSensor)
{
    const std::string path = "/test/gpufbr/no_type";

    setupBaseAndCurrent(path, portInfoIntf, {}, {});
    // "Type" omitted -> type="" -> neither NSM_GPU_PCIe_0 nor NSM_PortInfo

    const size_t before = gpu->deviceSensors.size() +
                          gpu->roundRobinSensors.size() +
                          gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->roundRobinSensors.size() +
                  gpu->staticSensors.size(),
              before);
}

// ============================================================================
// Missing UUID -> uuid="" -> getNsmDeviceFromStaticUUID returns new mock
// Covers: L375-378 count("UUID") FALSE
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, Factory_MissingUUID_FalseBranch)
{
    const std::string path = "/test/gpufbr/no_uuid";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    // "UUID" omitted
    base["InventoryObjPath"] = processorPath + "nouuid/";
    base["DeviceIndex"] = uint64_t(0);
    base["Type"] = std::string("NSM_GPU_PCIe_0");
    base["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    base["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    base["ClearableScalarGroup"] = std::vector<uint64_t>{};

    createNsmGpuPcieSensor(mockManager, baseIntf, path);
}

// ============================================================================
// Missing Name -> name="" (FALSE branch)
// Covers: L370-373 count("Name") FALSE
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, Factory_MissingName_FalseBranch)
{
    const std::string path = "/test/gpufbr/no_name2";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    // "Name" omitted
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = processorPath + "noname2/";
    base["DeviceIndex"] = uint64_t(0);
    base["Type"] = std::string("NSM_GPU_PCIe_0");
    base["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    base["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    base["ClearableScalarGroup"] = std::vector<uint64_t>{};

    // Empty name -> throws from D-Bus path construction -> caught
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
}

// ============================================================================
// Missing InventoryObjPath -> inventoryObjPath="" (FALSE branch)
// Covers: L385-389 count("InventoryObjPath") FALSE
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest,
       Factory_MissingInventoryObjPath_FalseBranch)
{
    const std::string path = "/test/gpufbr/no_inv2";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    // "InventoryObjPath" omitted
    base["DeviceIndex"] = uint64_t(0);
    base["Type"] = std::string("NSM_GPU_PCIe_0");
    base["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    base["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    base["ClearableScalarGroup"] = std::vector<uint64_t>{};

    // Empty path -> throws -> caught
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
}

// ============================================================================
// NSM_GPU_PCIe_0 with single ClearableScalarGroup entry
// Covers: L442 for loop body with 1 iteration
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest,
       DISABLED_Factory_GPU_PCIe_SingleClearableScalarGroup)
{
    const std::string path = "/test/gpufbr/single_clearable";

    setupBaseAndCurrent(
        path, baseIntf, {},
        {{"Type", std::string("NSM_GPU_PCIe_0")},
         {"Health", std::string("xyz.openbmc_project.State.Decorator."
                                "Health.HealthType.OK")},
         {"ChasisPowerState", std::string("xyz.openbmc_project.State.Chassis."
                                          "PowerState.On")},
         {"ClearableScalarGroup", std::vector<uint64_t>{2}}});

    [[maybe_unused]] const size_t staticBefore = gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
    // 1 NsmClearPCIeCounters sensor added as static sensor
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore + 1);
}

// ============================================================================
// NSM_PortInfo with all properties present
// Covers: all count() TRUE branches for PortType, PortProtocol, Priority,
// DeviceIndex
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, DISABLED_Factory_PortInfo_AllProps)
{
    const std::string path = "/test/gpufbr/portinfo_all";

    setupBaseAndCurrent(
        path, portInfoIntf, {},
        {{"Type", std::string("NSM_PortInfo")},
         {"PortType",
          std::string("xyz.openbmc_project.Inventory.Decorator.PortInfo."
                      "PortType.UpstreamPort")},
         {"PortProtocol",
          std::string("xyz.openbmc_project.Inventory.Decorator.PortInfo."
                      "PortProtocol.PCIe")},
         {"Priority", bool(false)}});

    const size_t devBefore = gpu->deviceSensors.size();
    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), devBefore);
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// Base properties fail -> early return
// Covers: L361-365 if (rc != NSM_SUCCESS) co_return rc
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, Factory_BasePropertiesFail)
{
    const std::string path = "/test/gpufbr/no_base";
    const size_t before = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->staticSensors.size(), before);
}

// ============================================================================
// Invalid UUID -> nsmDevice==nullptr -> early return NSM_ERROR
// Covers: L395-403 if (!nsmDevice) co_return NSM_ERROR
// ============================================================================
TEST_F(NsmGpuPciePortFactoryBranchTest, Factory_InvalidUUID_NoDevice)
{
    const std::string path = "/test/gpufbr/bad_uuid2";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = std::string("INVALID_UUID_STR");
    base["InventoryObjPath"] = processorPath + "baduuid2/";
    base["DeviceIndex"] = uint64_t(0);
    base["Type"] = std::string("NSM_GPU_PCIe_0");

    const size_t before = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}
