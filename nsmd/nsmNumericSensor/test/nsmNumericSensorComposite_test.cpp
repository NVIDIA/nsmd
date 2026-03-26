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

#include <limits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmNumericSensor/nsmNumericSensorComposite.hpp"
#include "nsmObjectFactory.hpp"

using namespace nsm;

struct NsmNumericSensorCompositeTest : public Test, public utils::DBusTest
{
    std::shared_ptr<NsmNumericSensorComposite>
        makeComposite(const std::string& suffix = "test",
                      const std::vector<utils::Association>& assocs = {})
    {
        return std::make_shared<NsmNumericSensorComposite>(
            utils::DBusHandler::getBus(), "Composite" + suffix, assocs,
            "TestType", "/test/sensors/power/Composite" + suffix, "GPU",
            "PhysicalSensor"
#ifdef NVIDIA_SHMEM
            ,
            nullptr
#endif
        );
    }
};

// Constructor: associations loop (empty → no iterations)
TEST_F(NsmNumericSensorCompositeTest, Constructor_EmptyAssociations_Succeeds)
{
    EXPECT_NO_THROW(makeComposite("_ctor"));
}

// Constructor: non-empty associations → loop executes
TEST_F(NsmNumericSensorCompositeTest, Constructor_WithAssociations_Succeeds)
{
    utils::Association assoc;
    assoc.forward = "powered_by";
    assoc.backward = "powering";
    assoc.absolutePath = "/xyz/openbmc_project/inventory/gpu0";
    EXPECT_NO_THROW(makeComposite("_assoc", {assoc}));
}

// Helper: reset nextUpdateTimestamp to force the value to be written to dbus
static void forceNextUpdate(NsmNumericSensorComposite& c)
{
    c.nextUpdateTimestamp = 0;
}

// updateCompositeReading: single valid value → valueIntf updated (initial
// nextUpdateTimestamp=0 means always update)
TEST_F(NsmNumericSensorCompositeTest,
       UpdateCompositeReading_SingleValue_UpdatesIntf)
{
    auto composite = makeComposite("_single");
    composite->updateCompositeReading("child1", 150.0);
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 150.0);
}

// updateCompositeReading: two valid values → sum written to valueIntf
TEST_F(NsmNumericSensorCompositeTest,
       UpdateCompositeReading_TwoValues_SumsToIntf)
{
    auto composite = makeComposite("_two");
    composite->updateCompositeReading("child1", 100.0);
    // Reset timestamp so next call is not throttled
    forceNextUpdate(*composite);
    composite->updateCompositeReading("child2", 200.0);
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 300.0);
}

// updateCompositeReading: child with NaN → totalValue becomes NaN, loop breaks
TEST_F(NsmNumericSensorCompositeTest,
       UpdateCompositeReading_NaNChild_ResultIsNaN)
{
    auto composite = makeComposite("_nan");
    composite->updateCompositeReading("child1", 100.0);
    forceNextUpdate(*composite);
    composite->updateCompositeReading("child2",
                                      std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(std::isnan(composite->valueIntf->value()));
}

// updateCompositeReading: update existing child → recalculates sum
TEST_F(NsmNumericSensorCompositeTest,
       UpdateCompositeReading_UpdateExistingChild_Recalculates)
{
    auto composite = makeComposite("_update");
    composite->updateCompositeReading("child1", 100.0);
    forceNextUpdate(*composite);
    composite->updateCompositeReading("child1", 250.0); // overwrite
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 250.0);
}

// updateCompositeReading: three children, one NaN → NaN result
TEST_F(NsmNumericSensorCompositeTest,
       UpdateCompositeReading_MultipleChildrenNaN_NaNResult)
{
    auto composite = makeComposite("_multinan");
    composite->updateCompositeReading("child1", 50.0);
    forceNextUpdate(*composite);
    composite->updateCompositeReading("child2", 75.0);
    forceNextUpdate(*composite);
    composite->updateCompositeReading("childNaN",
                                      std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(std::isnan(composite->valueIntf->value()));
}

// updateCompositeReading: NaN then valid update (NaN child replaced) → valid
TEST_F(NsmNumericSensorCompositeTest,
       UpdateCompositeReading_NaNChildReplaced_ValidResult)
{
    auto composite = makeComposite("_nanreplace");
    composite->updateCompositeReading("child1",
                                      std::numeric_limits<double>::quiet_NaN());
    // Replace NaN with valid value
    forceNextUpdate(*composite);
    composite->updateCompositeReading("child1", 42.0);
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 42.0);
}

// updateCompositeReading: timestamp < nextUpdateTimestamp → update throttled,
// valueIntf NOT updated (covers the else branch of line 107)
TEST_F(NsmNumericSensorCompositeTest,
       UpdateCompositeReading_ThrottledUpdate_ValueNotChanged)
{
    auto composite = makeComposite("_throttle");
    // First call: nextUpdateTimestamp==0 → update happens
    composite->updateCompositeReading("child1", 100.0);
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 100.0);
    // Pin nextUpdateTimestamp far into the future to force throttling
    composite->nextUpdateTimestamp = std::numeric_limits<uint64_t>::max();
    // Second call: timestamp < nextUpdateTimestamp → throttled, value stays
    composite->updateCompositeReading("child1", 200.0);
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 100.0);
}

// updateCompositeReading: non-null shmemSensor → if (shmemSensor) TRUE branch
// NsmNumericSensorShmem::updateReading is safe in tests (cacheTALData pushes to
// a static vector; no real shared memory access)
#ifdef NVIDIA_SHMEM
TEST_F(NsmNumericSensorCompositeTest,
       UpdateCompositeReading_WithShmemSensor_UpdatesValue)
{
    auto shmem = std::make_unique<NsmNumericSensorShmem>(
        "CompShmemBranch", "power",
        "/xyz/openbmc_project/inventory/system/chassis",
        std::make_unique<SMBPBIPowerSMBusSensorBytesConverter>());
    auto composite = std::make_shared<NsmNumericSensorComposite>(
        utils::DBusHandler::getBus(), "CompShmemBranch",
        std::vector<utils::Association>{}, "TestType",
        "/test/sensors/power/CompShmemBranch", "GPU", "PhysicalSensor",
        std::move(shmem));
    composite->updateCompositeReading("child1", 99.0);
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 99.0);
}
#endif

// =============================================================================
// CreateFPGATotalGPUPower factory tests (via NsmObjectFactory dispatch)
// =============================================================================

struct NsmNumericSensorCompositeFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_NumericCompositeSensor";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/composite0";
    const std::string name = "TotalGPUPower";
    const uuid_t gpuUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmNumericSensorCompositeFactoryTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmNumericSensorCompositeFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", gpuUuid},
        {"SensorType", std::string("power")},
        {"PhysicalContext", std::string("GPU")},
        {"Implementation", std::string("PhysicalSensor")},
    };
};

// Full property map + chassis association → sensor created and registered
// (NVIDIA_SHMEM block requires chassis association → must set up serviceMap)
TEST_F(NsmNumericSensorCompositeFactoryTest, Factory_FullProperties_Success)
{
    // Set up chassis association so NVIDIA_SHMEM block proceeds past empty
    // check
    const std::string assocIntf = interfaceName + ".Associations0";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {assocIntf}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(objPath, assocIntf);
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis");

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    propertyMap = basicProperties;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               objPath);

    EXPECT_GE(fpga->deviceSensors.size(), 1u);
}

// Missing UUID → parseStaticUuid throws → caught by createObjects → no sensor
TEST_F(NsmNumericSensorCompositeFactoryTest, Factory_MissingUUID_NoSensor)
{
    const std::string uniquePath =
        "/xyz/openbmc_project/inventory/system/composite_no_uuid";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props.erase("UUID");
    propertyMap = props;

    const size_t before = fpga->deviceSensors.size();

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               uniquePath);

    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Only UUID and Name → FALSE branches for
// SensorType/PhysicalContext/Implementation Under NVIDIA_SHMEM, no chassis
// association → chassis_association.empty() TRUE → co_return NSM_ERROR → no //
// sensor added
TEST_F(NsmNumericSensorCompositeFactoryTest, Factory_MinimalProperties_NoSensor)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/composite_minimal";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    propertyMap = {
        {"Name", name},
        {"UUID", gpuUuid},
    };

    const size_t before = fpga->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// With SensorType + PhysicalContext + Implementation + chassis assoc →
// TRUE branches for all property checks → sensor created
TEST_F(NsmNumericSensorCompositeFactoryTest,
       Factory_WithSensorTypeAndImplementation_Success)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/composite_full";
    // Set up chassis association (required by NVIDIA_SHMEM block)
    const std::string assocIntf = interfaceName + ".Associations0";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {assocIntf}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(testPath, assocIntf);
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis");

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props["SensorType"] = std::string("power");
    props["PhysicalContext"] = std::string("GPU");
    props["Implementation"] = std::string("PhysicalSensor");
    propertyMap = props;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(fpga->deviceSensors.size(), 1u);
}

// Only Name missing → FALSE branch for count("Name") → name="" →
// Under NVIDIA_SHMEM, no chassis association → chassis_association.empty() TRUE
// → co_return NSM_ERROR → no sensor added
TEST_F(NsmNumericSensorCompositeFactoryTest, Factory_MissingName_NoSensor)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/composite_noname";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props.erase("Name");
    propertyMap = props;

    const size_t before = fpga->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Association forward != "chassis" → L165 FALSE branch → loop exhausted →
// chassis_association stays empty → co_return NSM_ERROR → no sensor //

TEST_F(NsmNumericSensorCompositeFactoryTest,
       Factory_AssocForwardNotChassis_NoSensor)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/composite_badassoc";
    const std::string assocIntf = interfaceName + ".Associations0";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {assocIntf}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(testPath, assocIntf);
    assocPm["Forward"] =
        std::string("containing"); // NOT "chassis" → L165 FALSE
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis");

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    propertyMap = basicProperties;

    const size_t before = fpga->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Chassis association present + UUID missing → L172 FALSE branch taken
// → getNsmDeviceFromStaticUUID("") → parseStaticUuid throws → caught → no
// sensor
TEST_F(NsmNumericSensorCompositeFactoryTest,
       Factory_WithChassisAssoc_MissingUUID_NoSensor)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/composite_chassisnouuid";
    const std::string assocIntf = interfaceName + ".Associations0";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {assocIntf}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(testPath, assocIntf);
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis");

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props.erase("UUID"); // uuid="" → parseStaticUuid throws
    propertyMap = props;

    const size_t before = fpga->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_EQ(before, fpga->deviceSensors.size());
}
