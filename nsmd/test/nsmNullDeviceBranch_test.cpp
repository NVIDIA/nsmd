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

// Tests for factory-function `if (!nsmDevice)` branches.
// Uses NullReturnMockSensorManager (always returns nullptr) to exercise the
// "UUID matches no NsmDevice" error paths in all non-static factory coroutines.

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

// nsmObjectFactory.hpp needs #define protected public in effect,
// which mockSensorManager.hpp sets before including sensorManager.hpp.
// Include it after mockSensorManager.hpp with a separate group separator
// so clang-format does not reorder it before the mock headers.
#define private public
#define protected public

#include "features/creators.hpp"
#include "nsmObjectFactory.hpp"

#undef private
#undef protected

namespace nsm
{
// nsmChassis
requester::Coroutine createPCIeRetimerChassis(SensorManager&,
                                              const std::string&,
                                              const std::string&);
requester::Coroutine createControlGpuPower(SensorManager&, const std::string&,
                                           const std::string&);
requester::Coroutine createPowerSubSystem(SensorManager&, const std::string&,
                                          const std::string&);
requester::Coroutine createNsmChassisLEDSensor(SensorManager&,
                                               const std::string&,
                                               const std::string&);

// nsmPort
requester::Coroutine createNsmEndpoints(SensorManager&, const std::string&,
                                        const std::string&);
requester::Coroutine createNsmGpuPcieSensor(SensorManager&, const std::string&,
                                            const std::string&);
requester::Coroutine createNsmFpgaPortSensor(SensorManager&, const std::string&,
                                             const std::string&);
requester::Coroutine createPCIeRetimerChassisPCIeDevice(SensorManager&,
                                                        const std::string&,
                                                        const std::string&);

// nsmProcessor
requester::Coroutine createNsmResetSensor(SensorManager&, const std::string&,
                                          const std::string&);
requester::Coroutine createNsmFpgaProcessorSensor(SensorManager&,
                                                  const std::string&,
                                                  const std::string&);

// nsmNumericSensor
requester::Coroutine makeNsmAltitudePressure(SensorManager&, const std::string&,
                                             const std::string&);
// nsmPort factories (static removed)
requester::Coroutine createNsmPCIeRetimerPorts(SensorManager&,
                                               const std::string&,
                                               const std::string&);

} // namespace nsm

// ---------------------------------------------------------------------------
// Common fixture
// ---------------------------------------------------------------------------
struct NullDeviceFixture : public Test, public utils::DBusTest
{
    NsmDeviceTable devices;
    NiceMock<NullReturnMockSensorManager> nullManager{devices};

    NullDeviceFixture()
    {
        sensorManagerInstance.reset(&nullManager);
    }
    ~NullDeviceFixture() override
    {
        sensorManagerInstance.release();
        devices.clear();
    }

    // Register a minimal property map so coGetAllDbusProperty returns
    // synchronously instead of suspending forever.
    void regMap(const std::string& path, const std::string& intf)
    {
        utils::MockDbusAsync::propertyMap(path, intf);
    }
};

// ---------------------------------------------------------------------------
// nsmChassis factories
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, PCIeRetimerChassis_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/retimer";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer";
    regMap(path, intf);
    nsm::createPCIeRetimerChassis(nullManager, intf, path);
    // !nsmDevice branch taken → co_return NSM_ERROR (no throw, no crash) //

    SUCCEED();
}

TEST_F(NullDeviceFixture, ControlGpuPower_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/gpupwr";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_GPUPowerControl";
    regMap(path, intf);
    nsm::createControlGpuPower(nullManager, intf, path);
    SUCCEED();
}

TEST_F(NullDeviceFixture, PowerSubSystem_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/pss";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_PowerSubSystem";
    regMap(path, intf);
    nsm::createPowerSubSystem(nullManager, intf, path);
    SUCCEED();
}

TEST_F(NullDeviceFixture, ChassisLED_NullDevice_ReturnsError)
{
    // createNsmChassisLEDSensor calls coGetCachedBaseProperties with
    // NSM_ChassisLED, then coGetAllDbusProperty with the passed interface.
    const std::string path = "/test/null/led";
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_ChassisLED";
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_NvlinkLed";
    regMap(path, baseIntf);
    regMap(path, intf);
    nsm::createNsmChassisLEDSensor(nullManager, intf, path);
    SUCCEED();
}

TEST_F(NullDeviceFixture, NsmEndpoints_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/ep";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_FabricsEndpoint";
    regMap(path, intf);
    nsm::createNsmEndpoints(nullManager, intf, path);
    SUCCEED();
}

TEST_F(NullDeviceFixture, NsmGpuPcieSensor_NullDevice_ReturnsError)
{
    // createNsmGpuPcieSensor calls coGetCachedBaseProperties with
    // GPU_PCIe_INTERFACE = "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0"
    const std::string path = "/test/null/gpupcie";
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_1";
    regMap(path, baseIntf);
    regMap(path, intf);
    nsm::createNsmGpuPcieSensor(nullManager, intf, path);
    SUCCEED();
}

TEST_F(NullDeviceFixture, NsmFpgaPortSensor_NullDevice_ReturnsError)
{
    // createNsmFpgaPortSensor calls coGetCachedBaseProperties with
    // FPGA_PORT_INTERFACE = "xyz.openbmc_project.Configuration.NSM_FpgaPort"
    const std::string path = "/test/null/fpgaport";
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    regMap(path, baseIntf);
    regMap(path, intf);
    nsm::createNsmFpgaPortSensor(nullManager, intf, path);
    SUCCEED();
}

TEST_F(NullDeviceFixture, PCIeRetimerPCIeDevice_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/retimerpd";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_PCIeDevices";
    regMap(path, intf);
    nsm::createPCIeRetimerChassisPCIeDevice(nullManager, intf, path);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// nsmProcessor factories
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, NsmResetSensor_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/reset";
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_GpuReset";
    regMap(path, intf);
    nsm::createNsmResetSensor(nullManager, intf, path);
    SUCCEED();
}

TEST_F(NullDeviceFixture, NsmFpgaProcessorSensor_NullDevice_ReturnsError)
{
    // createNsmFpgaProcessorSensor calls coGetCachedBaseProperties with
    // FPGA_PROCESSOR_INTERFACE =
    // "xyz.openbmc_project.Configuration.NSM_FpgaProcessor"
    const std::string path = "/test/null/fpgaproc";
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaProcessor";
    const std::string intf = baseIntf;
    regMap(path, baseIntf);
    nsm::createNsmFpgaProcessorSensor(nullManager, intf, path);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// nsmNumericSensor factories
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, NsmAltitudePressure_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/altpres";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_AltitudePressure";
    regMap(path, intf);
    nsm::makeNsmAltitudePressure(nullManager, intf, path);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// nsmPort static factory functions (static removed)
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, PCIeRetimerPorts_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/retimer_ports";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_PCIeLink";
    // Register UUID so getNsmDeviceFromStaticUUID is called (and returns null)
    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["UUID"] = std::string("STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0");
    nsm::createNsmPCIeRetimerPorts(nullManager, intf, path);
    // !nsmDevice branch taken → co_return NSM_ERROR
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Feature creators: a device record whose NsmDevice cannot be found
// (DeviceContext::device == nullptr) fails the feature with NSM_ERROR before
// any param is read or any object is created
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, FeatureCreators_NoNsmDevice_ReturnError)
{
    struct Creator
    {
        const char* name;
        nsm::features::CreatorFn create;
    };
    const Creator creators[] = {
        {"RoT", nsm::features::createRoTFeature},
        {"ClockLimit", nsm::features::createClockLimitFeature},
        {"FabricsZone", nsm::features::createFabricsZoneFeature},
        {"NVLink", nsm::features::createNVLinkFeature},
        {"GPUSWInventory", nsm::features::createGPUSWInventoryFeature},
        {"EventSetting", nsm::features::createEventSettingFeature},
        {"EventConfig", nsm::features::createEventConfigFeature},
        {"XIDEvent", nsm::features::createXIDEventFeature},
        {"Memory", nsm::features::createMemoryFeature},
        {"NumericCompositeSensor",
         nsm::features::createNumericCompositeSensorFeature},
    };

    nsm::features::DeviceContext dev;
    dev.name = "NullDevice";
    dev.staticUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    ASSERT_EQ(dev.device, nullptr);

    for (const auto& creator : creators)
    {
        SCOPED_TRACE(creator.name);
        auto rc = creator.create(nullManager, dev, {});
        EXPECT_FALSE(rc.exception());
        EXPECT_EQ(rc.data(), NSM_ERROR);
    }
}
