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

#include <cmath>
#include <limits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "features/creators.hpp"
#include "nsmNumericSensor/nsmNumericSensorComposite.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace nsm;

struct NsmNumericSensorCompositeDevicesStorage
{
    NsmDeviceTable devices;
};

struct NsmNumericSensorCompositeTest :
    private NsmNumericSensorCompositeDevicesStorage,
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmNumericSensorCompositeTest() : SensorManagerTest(devices) {}
    ~NsmNumericSensorCompositeTest() override
    {
        cleanupDeviceSensors(devices);
    }

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
// createNumericCompositeSensorFeature tests
// =============================================================================

struct NsmNumericSensorCompositeFactoryDevicesStorage
{
    NsmDeviceTable devices;
};

struct NsmNumericSensorCompositeFactoryTest :
    private NsmNumericSensorCompositeFactoryDevicesStorage,
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "TotalGPUPower";
    const uuid_t gpuUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

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

    nsm::features::DeviceContext makeDev(bool withDevice = true)
    {
        nsm::features::DeviceContext dev;
        dev.name = "HGX_FPGA_0";
        dev.kind = "fpga";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = fpga;
        }
        return dev;
    }

    dbus::PropertyMap basicParams = {
        {"Name", name},
        {"SensorType", std::string("power")},
        {"ChassisName", std::string("HGX_Chassis_0")},
        {"PhysicalContext", std::string("GPU")},
        {"Implementation", std::string("PhysicalSensor")},
    };
};

// Full params → composite sensor created, added to the device and registered
// by its sensor path with a chassis association
TEST_F(NsmNumericSensorCompositeFactoryTest, Feature_FullParams_Success)
{
    const size_t before = fpga->deviceSensors.size();
    auto rc = nsm::features::createNumericCompositeSensorFeature(
        mockManager, makeDev(), basicParams);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(fpga->deviceSensors.size(), before + 1);

    const std::string path = "/xyz/openbmc_project/sensors/power/" + name;
    auto it = mockManager.objectPathToSensorMap.find(path);
    ASSERT_NE(it, mockManager.objectPathToSensorMap.end());
    auto composite =
        std::dynamic_pointer_cast<NsmNumericSensorComposite>(it->second);
    ASSERT_NE(composite, nullptr);
    EXPECT_EQ(composite->getType(), "NSM_NumericCompositeSensor");
    const auto& assocs = composite->associationDefinitionsInft->associations();
    ASSERT_EQ(assocs.size(), 1u);
    EXPECT_EQ(std::get<0>(assocs[0]), "chassis");
    EXPECT_EQ(std::get<1>(assocs[0]), "all_sensors");
    EXPECT_EQ(std::get<2>(assocs[0]),
              "/xyz/openbmc_project/inventory/system/chassis/HGX_Chassis_0");
    EXPECT_TRUE(std::isnan(composite->valueIntf->value()));
}

// No NsmDevice → NSM_ERROR, nothing created
TEST_F(NsmNumericSensorCompositeFactoryTest, Feature_NullDevice_ReturnsError)
{
    const size_t before = fpga->deviceSensors.size();
    auto rc = nsm::features::createNumericCompositeSensorFeature(
        mockManager, makeDev(false), basicParams);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(fpga->deviceSensors.size(), before);
}

// Required param missing → params.at() throws, no sensor added
TEST_F(NsmNumericSensorCompositeFactoryTest, Feature_MissingChassisName_Throws)
{
    auto params = basicParams;
    params.erase("ChassisName");
    const size_t before = fpga->deviceSensors.size();
    EXPECT_THROW_COROUTINE(nsm::features::createNumericCompositeSensorFeature(
                               mockManager, makeDev(), params),
                           std::out_of_range);
    EXPECT_EQ(fpga->deviceSensors.size(), before);
}
