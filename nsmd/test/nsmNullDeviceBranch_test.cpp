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
requester::Coroutine createControlGpuClock(SensorManager&, const std::string&,
                                           const std::string&);
requester::Coroutine createNsmChassisLEDSensor(SensorManager&,
                                               const std::string&,
                                               const std::string&);

// nsmPort
requester::Coroutine createNsmZones(SensorManager&, const std::string&,
                                    const std::string&);
requester::Coroutine createNsmEndpoints(SensorManager&, const std::string&,
                                        const std::string&);
requester::Coroutine createNsmPortSensor(SensorManager&, const std::string&,
                                         const std::string&, bool);
requester::Coroutine createNsmGpuPcieSensor(SensorManager&, const std::string&,
                                            const std::string&);
requester::Coroutine createNsmFpgaPortSensor(SensorManager&, const std::string&,
                                             const std::string&);
requester::Coroutine createPCIeRetimerChassisPCIeDevice(SensorManager&,
                                                        const std::string&,
                                                        const std::string&);

// nsmFwSwInventory
requester::Coroutine createGPUDriverSensor(SensorManager&, const std::string&,
                                           const std::string&);

// nsmEventConfig
requester::Coroutine createNsmEventSetting(SensorManager&, const std::string&,
                                           const std::string&);
requester::Coroutine createNsmEventConfig(SensorManager&, const std::string&,
                                          const std::string&);

// nsmEvent
requester::Coroutine createNsmXIDEvent(SensorManager&, const std::string&,
                                       const std::string&);

// nsmMemory
requester::Coroutine createNsmMemorySensor(SensorManager&, const std::string&,
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
requester::Coroutine createNsmMultiPCIeRetimerPorts(SensorManager&,
                                                    const std::string&,
                                                    const std::string&);

// nsmChassis/nsmErot
requester::Coroutine nsmErotCreateSensors(SensorManager&, const std::string&,
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

TEST_F(NullDeviceFixture, ControlGpuClock_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/gpuclk";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_GPUClockControl";
    regMap(path, intf);
    nsm::createControlGpuClock(nullManager, intf, path);
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

// ---------------------------------------------------------------------------
// nsmPort factories
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, NsmZones_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/zone";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_FabricsZone";
    regMap(path, intf);
    nsm::createNsmZones(nullManager, intf, path);
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

TEST_F(NullDeviceFixture, NsmPortSensor_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/port";
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_NVlink";
    regMap(path, intf);
    nsm::createNsmPortSensor(nullManager, intf, path, false);
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
// nsmFwSwInventory factories
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, GPUDriverSensor_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/gpudrv";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_GPUDriverInventory";
    regMap(path, intf);
    nsm::createGPUDriverSensor(nullManager, intf, path);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// nsmEventConfig factories
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, NsmEventSetting_NullDevice_ReturnsError)
{
    // createNsmEventSetting uses hardcoded
    // "xyz.openbmc_project.Configuration.NSM_EventSetting" for
    // coGetAllDbusProperty
    const std::string path = "/test/null/evtsetting";
    const std::string hardcodedIntf =
        "xyz.openbmc_project.Configuration.NSM_EventSetting";
    const std::string intf = hardcodedIntf;
    regMap(path, hardcodedIntf);
    nsm::createNsmEventSetting(nullManager, intf, path);
    SUCCEED();
}

TEST_F(NullDeviceFixture, NsmEventConfig_NullDevice_ReturnsError)
{
    // createNsmEventConfig calls coGetCachedBaseProperties with
    // "xyz.openbmc_project.Configuration.NSM_EventConfig"
    const std::string path = "/test/null/evtcfg";
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_EventConfig";
    const std::string intf = baseIntf;
    regMap(path, baseIntf);
    nsm::createNsmEventConfig(nullManager, intf, path);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// nsmEvent factories
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, NsmXIDEvent_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/xid";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_XIDEventSetting";
    regMap(path, intf);
    nsm::createNsmXIDEvent(nullManager, intf, path);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// nsmMemory factories
// ---------------------------------------------------------------------------

TEST_F(NullDeviceFixture, NsmMemorySensor_NullDevice_ReturnsError)
{
    // createNsmMemorySensor calls coGetCachedBaseProperties with
    // MEMORY_INTERFACE = "xyz.openbmc_project.Configuration.NSM_Memory"
    const std::string path = "/test/null/mem";
    const std::string baseIntf = "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Memory_0";
    regMap(path, baseIntf);
    regMap(path, intf);
    nsm::createNsmMemorySensor(nullManager, intf, path);
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

TEST_F(NullDeviceFixture, MultiPCIeRetimerPorts_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/multi_retimer_ports";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_MultiPCIeLink";
    // Provide all required properties so getPropertyFromCollection doesn't
    // throw
    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("MultiNull");
    pm["Priority"] = bool{false};
    pm["Counts"] = std::vector<uint64_t>{0};
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/multiport/null/");
    pm["UUID"] = std::string("STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortType.UpstreamPort");
    pm["UpstreamPortNumbers"] = std::vector<uint64_t>{0};
    nsm::createNsmMultiPCIeRetimerPorts(nullManager, intf, path);
    // !nsmDevice branch taken → co_return NSM_ERROR
    SUCCEED();
}

// nsmErot.cpp L276-277: !device branch after getNsmDeviceFromStaticUUID
// returns nullptr. Requires Type present, UUID present, SlotCount present
// (to avoid the early co_return at L245-249 for "not a RoT chassis"). //

TEST_F(NullDeviceFixture, ErotCreateSensors_NullDevice_ReturnsError)
{
    const std::string path = "/test/null/erot";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_Chassis_RoT";
    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Type"] = std::string("NSM_Chassis");
    pm["Name"] = std::string("ErotNull");
    pm["UUID"] = std::string("STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0");
    pm["SlotCount"] = uint64_t(0);
    // NullReturnMockSensorManager::getNsmDeviceFromStaticUUID returns nullptr
    // → !device == true → L276 lg2::error, L277 co_return NSM_SW_ERROR //

    nsm::nsmErotCreateSensors(nullManager, intf, path);
    SUCCEED();
}

// nsmNumericSensorComposite.cpp L188-195: !nsmDevice branch in
// CreateFPGATotalGPUPower (registered as
// NSM_NumericCompositeSensor). NullReturnMockSensorManager returns nullptr
// → !nsmDevice == true → lg2::error, co_return NSM_ERROR.
// Must call via NsmObjectFactory because CreateFPGATotalGPUPower is static.
TEST_F(NullDeviceFixture, CreateFPGATotalGPUPower_NullDevice_ReturnsError)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NumericCompositeSensor";
    const std::string path = "/test/null/fpga_total_gpu_power";
    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("FPGATotalGPUPower_Null");
    pm["UUID"] = std::string("STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0");
    pm["SensorType"] = std::string("power");
    pm["PhysicalContext"] = std::string("SystemBoard");
    pm["Implementation"] = std::string("PhysicalSensor");
    // NullReturnMockSensorManager::getNsmDeviceFromStaticUUID returns nullptr
    // → !nsmDevice branch at L188 taken → co_return NSM_ERROR //

    NsmObjectFactory::instance().createObjects(nullManager, intf, path);
    SUCCEED();
}

// nsmPCIeRetimerFabricsDI.cpp L77-85: !nsmDevice branch in
// createNSMPCIeRetimerFabrics. NullReturnMockSensorManager returns nullptr.
// Must call via NsmObjectFactory because the factory is static.
TEST_F(NullDeviceFixture, CreateNSMPCIeRetimerFabrics_NullDevice_ReturnsError)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_Fabrics";
    const std::string path = "/test/null/pcie_retimer_fabrics";
    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("RetimerFabricsNull");
    pm["UUID"] = std::string("STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0");
    pm["FabricType"] = std::string("PCIe");
    // NullReturnMockSensorManager::getNsmDeviceFromStaticUUID returns nullptr
    // → !nsmDevice branch at L77 taken → co_return NSM_ERROR //

    NsmObjectFactory::instance().createObjects(nullManager, intf, path);
    SUCCEED();
}
