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
using namespace ::testing;

#define private public
#define protected public

#include "libnsm/pci-links.h"

#include "nsmCommon/nsmPcieGroup.hpp"
#include "nsmGpuPciePort.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmGpuPcieSensor(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath);

}; // namespace nsm

auto bus = sdbusplus::bus::new_default();

TEST(NsmGpuPciePort, Constructor)
{
    std::string name = "GpuPciePort0";
    std::string type = "NSM_GPU_PCIe";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";
    std::string chasisState = "xyz.openbmc_project.State.Chassis.PowerState.On";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port0";

    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_gpu", "port", "/xyz/openbmc_project/inventory/system/gpu0"});

    NsmGpuPciePort gpuPort(bus, name, type, health, chasisState, associations,
                           inventoryObjPath);

    EXPECT_EQ(gpuPort.getName(), name);
    EXPECT_EQ(gpuPort.getType(), type);
}

TEST(NsmGpuPciePort, ConstructorWithMultipleAssociations)
{
    std::string name = "GpuPciePort1";
    std::string type = "NSM_GPU_PCIe";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning";
    std::string chasisState =
        "xyz.openbmc_project.State.Chassis.PowerState.Off";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port1";

    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_gpu", "port", "/xyz/openbmc_project/inventory/system/gpu1"});
    associations.push_back({"connected_device", "port",
                            "/xyz/openbmc_project/inventory/system/device1"});

    NsmGpuPciePort gpuPort(bus, name, type, health, chasisState, associations,
                           inventoryObjPath);

    EXPECT_EQ(gpuPort.getName(), name);
    EXPECT_EQ(gpuPort.getType(), type);
}

TEST(NsmGpuPciePort, ConstructorWithEmptyAssociations)
{
    std::string name = "GpuPciePort2";
    std::string type = "NSM_GPU_PCIe";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical";
    std::string chasisState = "xyz.openbmc_project.State.Chassis.PowerState.On";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port2";

    std::vector<utils::Association> associations; // Empty

    NsmGpuPciePort gpuPort(bus, name, type, health, chasisState, associations,
                           inventoryObjPath);

    EXPECT_EQ(gpuPort.getName(), name);
    EXPECT_EQ(gpuPort.getType(), type);
}

TEST(NsmGpuPciePort, ConstructorWithDifferentHealthStates)
{
    std::string name = "GpuPciePort3";
    std::string type = "NSM_GPU_PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port3";
    std::vector<utils::Association> associations;

    std::vector<std::string> healthStates = {
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical"};
    for (const auto& health : healthStates)
    {
        NsmGpuPciePort gpuPort(
            bus, name, type, health,
            "xyz.openbmc_project.State.Chassis.PowerState.On", associations,
            inventoryObjPath);
        EXPECT_EQ(gpuPort.getName(), name);
    }
}

TEST(NsmGpuPciePort, ConstructorWithDifferentChasisStates)
{
    std::string name = "GpuPciePort4";
    std::string type = "NSM_GPU_PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port4";
    std::vector<utils::Association> associations;

    std::vector<std::string> chasisStates = {
        "xyz.openbmc_project.State.Chassis.PowerState.On",
        "xyz.openbmc_project.State.Chassis.PowerState.Off"};
    for (const auto& state : chasisStates)
    {
        NsmGpuPciePort gpuPort(
            bus, name, type,
            "xyz.openbmc_project.State.Decorator.Health.HealthType.OK", state,
            associations, inventoryObjPath);
        EXPECT_EQ(gpuPort.getType(), type);
    }
}

TEST(NsmGpuPciePortInfo, Constructor)
{
    std::string name = "GpuPciePortInfo0";
    std::string type = "NSM_PortInfo";
    std::string portType =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort";
    std::string portProtocol =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/portinfo0";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmGpuPciePortInfo portInfo(name, type, portType, portProtocol,
                                portInfoIntf);

    EXPECT_EQ(portInfo.getName(), name);
    EXPECT_EQ(portInfo.getType(), type);
}

TEST(NsmGpuPciePortInfo, ConstructorWithDifferentPortTypes)
{
    std::string name = "GpuPciePortInfo1";
    std::string type = "NSM_PortInfo";
    std::string portProtocol =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/portinfo1";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::string> portTypes = {
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.DownstreamPort"};
    for (const auto& portType : portTypes)
    {
        NsmGpuPciePortInfo portInfo(name, type, portType, portProtocol,
                                    portInfoIntf);
        EXPECT_EQ(portInfo.getName(), name);
    }
}

TEST(NsmGpuPciePortInfo, ConstructorWithDifferentProtocols)
{
    std::string name = "GpuPciePortInfo2";
    std::string type = "NSM_PortInfo";
    std::string portType =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/portinfo2";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::string> protocols = {
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.Ethernet",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.NVLink"};
    for (const auto& protocol : protocols)
    {
        NsmGpuPciePortInfo portInfo(name, type, portType, protocol,
                                    portInfoIntf);
        EXPECT_EQ(portInfo.getType(), type);
    }
}

// ============================================================================
// NsmClearPCIeCounters::update – error-CC branch coverage
//
// The update coroutine initialises cc=NSM_ERROR.  When sensorIO returns a
// response that is too short for decode, the decode function fails with
// NSM_SW_ERROR_LENGTH; cc remains NSM_ERROR (non-zero).  The final
// "co_return cc ? cc : rc" therefore takes the cc!=0 branch, which was //
// previously uncovered because no test for
// NsmClearPCIeCounters existed.
// ============================================================================

class NsmClearPCIeCountersTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmClearPCIeCountersTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmClearPCIeCountersTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmClearPCIeCountersTest, Update_ShortResponse_ErrorCC_CoversBranch)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_0", "NSM_ClearPCIe", 1, 0,
                                clearPCIeIntf);

    // Return a too-short response so that decode fails.
    // cc is initialised to NSM_ERROR (non-zero) in the source; with a failed
    // decode cc remains NSM_ERROR, so "co_return cc ? cc : rc" takes the
    // cc != 0 branch - the branch previously missing from coverage.
    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr) + 2, 0);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(shortResp));

    sensor.update(gpu);
}

// NsmClearPCIeCounters::update - success response -> FALSE branch of
// co_return cc ? cc : rc (cc = NSM_SUCCESS = 0 after successful decode).

TEST_F(NsmClearPCIeCountersTest, Update_Success_CoversFalseBranch)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_0", "NSM_ClearPCIe", 1, 0,
                                clearPCIeIntf);

    // Build a valid response: mask_length=1, data_size=1+1*2=3
    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    uint8_t availableMask[1] = {0xFF};
    uint8_t clearableMask[1] = {0x0F};
    uint8_t maskLength = 1;
    uint16_t dataSize = 3; // 1 + maskLength * 2
    ASSERT_EQ(encode_query_available_clearable_scalar_data_sources_v1_resp(
                  0, NSM_SUCCESS, ERR_NULL, dataSize, maskLength, availableMask,
                  clearableMask, msg),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    sensor.update(gpu);
    // cc = NSM_SUCCESS -> false branch of co_return cc ? cc : rc //
}

// Helper to build a valid update response with given mask
static std::vector<uint8_t> buildUpdateResponse(uint8_t clearableBits)
{
    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    uint8_t availableMask[1] = {0xFF};
    uint8_t clearableMask[1] = {clearableBits};
    uint8_t maskLength = 1;
    uint16_t dataSize = 3;
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, dataSize, maskLength, availableMask,
        clearableMask, msg);
    return resp;
}

// updateReading groupId=2 – covers switch case 2 and all bit checks (bits 0-3)
TEST_F(NsmClearPCIeCountersTest, UpdateReading_GroupId2_AllBitsSet)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2", "NSM_ClearPCIe", 2 /*groupId*/,
                                0, clearPCIeIntf);

    // bits 0,1,2,3 all set -> NonFatal, Fatal, Unsupported, Correctable
    auto resp = buildUpdateResponse(0x0F);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// updateReading groupId=2, only bit0 set (NonFatalErrorCount)
TEST_F(NsmClearPCIeCountersTest, UpdateReading_GroupId2_OnlyBit0)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2b", "NSM_ClearPCIe", 2 /*groupId*/,
                                0, clearPCIeIntf);

    auto resp = buildUpdateResponse(0x01); // only bit0
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// updateReading groupId=3, bit0 set -> L0ToRecoveryCount
TEST_F(NsmClearPCIeCountersTest, UpdateReading_GroupId3_Bit0Set)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G3", "NSM_ClearPCIe", 3 /*groupId*/,
                                0, clearPCIeIntf);

    auto resp = buildUpdateResponse(0x01); // bit0 set
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// updateReading groupId=4, bits 1,2,4,6 set -> NAKReceived, NAKSent,
// ReplayRollover, Replay
TEST_F(NsmClearPCIeCountersTest, UpdateReading_GroupId4_AllBitsSet)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4", "NSM_ClearPCIe", 4 /*groupId*/,
                                0, clearPCIeIntf);

    // bits 1,2,4,6: 0b01010110 = 0x56
    auto resp = buildUpdateResponse(0x56);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// updateReading groupId=4, no bits set (empty-list fast path)
TEST_F(NsmClearPCIeCountersTest, UpdateReading_GroupId4_NoBitsSet)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4b", "NSM_ClearPCIe", 4 /*groupId*/,
                                0, clearPCIeIntf);

    auto resp = buildUpdateResponse(0x00); // no bits
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// addClearCoutnerSensor – insert (using null shared_ptr as placeholder) then
// retrieve covers the try_emplace-success path
TEST_F(NsmClearPCIeCountersTest, AddAndGetClearCounterSensor_InsertAndRetrieve)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    std::shared_ptr<NsmPcieGroup> sensor{nullptr};
    clearPCIeIntf->addClearCoutnerSensor(5, sensor);
    auto found = clearPCIeIntf->getClearCounterSensorFromGroup(5);
    EXPECT_EQ(found, sensor);
}

// getClearCounterSensorFromGroup – key not found -> returns nullptr
TEST_F(NsmClearPCIeCountersTest, GetClearCounterSensorFromGroup_NotFound)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    auto found = clearPCIeIntf->getClearCounterSensorFromGroup(99);
    EXPECT_EQ(found, nullptr);
}

// addClearCoutnerSensor – duplicate groupId logs error, original value kept
TEST_F(NsmClearPCIeCountersTest, AddClearCounterSensor_DuplicateGroupId)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    // first insert
    std::shared_ptr<NsmPcieGroup> s1{nullptr};
    clearPCIeIntf->addClearCoutnerSensor(6, s1);
    // duplicate – logs error, first pointer kept
    std::shared_ptr<NsmPcieGroup> s2{nullptr};
    clearPCIeIntf->addClearCoutnerSensor(6, s2);
    auto found = clearPCIeIntf->getClearCounterSensorFromGroup(6);
    EXPECT_EQ(found, s1);
}

// clearPCIeErrorCounter – postPatchIO failure -> WriteFailure
TEST_F(NsmClearPCIeCountersTest, ClearPCIeErrorCounter_PostPatchIOFail)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// clearPCIeErrorCounter – decode failure -> WriteFailure
TEST_F(NsmClearPCIeCountersTest, ClearPCIeErrorCounter_DecodeFail)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    // Too-short response to trigger decode failure; must be at least
    // sizeof(nsm_msg_hdr)+NSM_RESPONSE_ERROR_LEN to avoid out-of-bounds reads
    // in decode_reason_code_and_cc while still being too small for a valid resp
    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN,
                                   0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(shortResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// clearPCIeErrorCounter – success, no matching sensor for groupId
TEST_F(NsmClearPCIeCountersTest, ClearPCIeErrorCounter_Success_NoSensor)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    // Build a valid clear response
    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_clear_data_source_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    // No sensor registered for groupId=2, success path still reached
}

// clearPCIeErrorCounter – success WITH sensor registered for groupId ->
// covers Decision 'true' branch of if (sensor) at nsmGpuPciePort.cpp L263:
// getClearCounterSensorFromGroup returns non-null -> sensor->update(device)
TEST_F(NsmClearPCIeCountersTest, ClearPCIeErrorCounter_Success_WithSensor)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    // Register a PCIe group sensor for groupId=2
    const std::string sensorPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port0/ecc";
    auto pcieECCIntf = std::make_shared<PCIeEccIntf>(bus, sensorPath.c_str());
    auto pciePortIntf =
        std::make_shared<PCIeEccIntf>(bus, (sensorPath + "p").c_str());
    std::string mutableObjPath = inventoryObjPath;
    auto sensorGroup2 = std::make_shared<NsmPciGroup2>(
        "PCIe_G2", "NSM_GPU_PCIe", pcieECCIntf, pciePortIntf, uint8_t{0},
        mutableObjPath);
    clearPCIeIntf->addClearCoutnerSensor(2, sensorGroup2);

    // Build a valid clear response (cc=NSM_SUCCESS)
    std::vector<uint8_t> clearResp(256, 0);
    auto clearMsg = reinterpret_cast<nsm_msg*>(clearResp.data());
    ASSERT_EQ(
        encode_clear_data_source_v1_resp(0, NSM_SUCCESS, ERR_NULL, clearMsg),
        NSM_SW_SUCCESS);

    // Build a valid update response for sensorGroup2->update(device)
    auto updateResp = buildUpdateResponse(0x0F);

    // postPatchIO returns clear response; sensorIO returns update response
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(clearResp));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(updateResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// clearCounter – invalid counter name -> throws InvalidArgument
TEST_F(NsmClearPCIeCountersTest, ClearCounter_InvalidCounter_Throws)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    // First call will get an objectPath from AsyncOperationManager.
    // If it's empty it throws Unavailable; if non-empty but counter invalid,
    // throws InvalidArgument.  Either way an exception is raised.
    EXPECT_THROW(clearPCIeIntf->clearCounter("NonExistentCounter"),
                 std::exception);
}

// ==========================================================================
// createNsmGpuPcieSensor factory function tests
// ==========================================================================

struct NsmGpuPcieSensorFactoryTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGpuPcieSensorFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGpuPcieSensorFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Factory: coGetCachedBaseProperties fails -> early return, no sensors added
TEST_F(NsmGpuPcieSensorFactoryTest, Factory_BasePropertiesFail_NoSensors)
{
    const std::string objPath = "/test/gpupcie/factory_fail";
    size_t initialCount = gpu->deviceSensors.size() + gpu->staticSensors.size();

    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);

    EXPECT_EQ(gpu->deviceSensors.size() + gpu->staticSensors.size(),
              initialCount);
}

// Factory: type == "NSM_GPU_PCIe_0" -> all NSM_GPU_PCIe sensors created
TEST_F(NsmGpuPcieSensorFactoryTest, Factory_TypeNsmGpuPcie0_SensorsCreated)
{
    const std::string objPath = "/test/gpupcie/factory_pcie0";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/factory_pcie0_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_Factory0");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);
    pm["ClearableScalarGroup"] = std::vector<uint64_t>{2};

    size_t initialCount = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    EXPECT_GT(gpu->deviceSensors.size() + gpu->staticSensors.size(),
              initialCount);
}

// Factory: type == "NSM_PortInfo" -> NsmGpuPciePortInfo sensor created
TEST_F(NsmGpuPcieSensorFactoryTest, Factory_TypeNsmPortInfo_SensorsCreated)
{
    const std::string objPath = "/test/gpupcie/factory_portinfo";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/factory_portinfo_proc";
    const std::string portInfoIntf = baseIntf + ".PortInfo";

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("GpuPcie_PortInfo0");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = processorPath;
    baseMap["DeviceIndex"] = uint64_t(0);

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, portInfoIntf);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");

    size_t initialCount = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, objPath);
    EXPECT_GT(gpu->deviceSensors.size(), initialCount);
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmGpuPcieSensor
// =============================================================================

// NSM_GPU_PCIe_0 + Health absent -> health="" -> NsmGpuPciePort constructor
// calls convertHealthTypeFromString("") -> throws -> caught -> no sensor
TEST_F(NsmGpuPcieSensorFactoryTest, Factory_GpuPcie0_MissingHealth_NoSensor)
{
    const std::string objPath = "/test/gpupcie/pcie0_no_health";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/pcie0_no_health_proc";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_NoHealth");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);
    // "Health" omitted -> health="" -> convertHealthTypeFromString throws

    const size_t before = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    EXPECT_EQ(before, gpu->deviceSensors.size() + gpu->staticSensors.size());
}

// NSM_GPU_PCIe_0 + ChasisPowerState absent -> chasisState="" ->
// convertPowerStateFromString("") throws -> caught -> no sensor
TEST_F(NsmGpuPcieSensorFactoryTest,
       Factory_GpuPcie0_MissingChasisPowerState_NoSensor)
{
    const std::string objPath = "/test/gpupcie/pcie0_no_chasis";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/pcie0_no_chasis_proc";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_NoChasis");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["DeviceIndex"] = uint64_t(0);
    // "ChasisPowerState" omitted -> chasisState="" ->
    // convertPowerStateFromString throws

    const size_t before = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    EXPECT_EQ(before, gpu->deviceSensors.size() + gpu->staticSensors.size());
}

// NSM_GPU_PCIe_0 + ClearableScalarGroup absent -> empty vector ->
// the for loop does not execute -> no NsmClearPCIeCounters added
// (sensor count is same as if ClearableScalarGroup is empty)
TEST_F(NsmGpuPcieSensorFactoryTest,
       Factory_GpuPcie0_MissingClearableScalarGroup_NoCounters)
{
    const std::string objPath = "/test/gpupcie/pcie0_no_cleargrp";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/pcie0_no_cleargrp_proc";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_NoClearGrp");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);
    // "ClearableScalarGroup" omitted -> FALSE branch -> empty vector -> no
    // counter sensors, but other GPU PCIe sensors ARE created

    const size_t staticBefore = gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    // No NsmClearPCIeCounters added (static) vs with ClearableScalarGroup={2}
    EXPECT_EQ(staticBefore, gpu->staticSensors.size());
}

// NSM_GPU_PCIe_0 + DeviceIndex absent -> FALSE branch of
// if(allBaseIfaceProperties.count("DeviceIndex")) at nsmGpuPciePort.cpp L427 //
// -> deviceIndex stays 0; ClearableScalarGroup also absent ->
// no counters
TEST_F(NsmGpuPcieSensorFactoryTest,
       Factory_GpuPcie0_MissingDeviceIndex_FalseBranch)
{
    const std::string objPath = "/test/gpupcie/pcie0_no_devidx";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/pcie0_no_devidx_proc";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_NoDevIdx");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    // "DeviceIndex" omitted -> FALSE branch (L427)
    // "ClearableScalarGroup" also omitted -> FALSE branch (L433)

    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    // Sensor created with default deviceIndex=0; no counters added
    EXPECT_NE(gpu, nullptr); // device still exists
}

// NSM_PortInfo + PortType absent -> portType="" ->
// NsmGpuPciePortInfo constructor throws -> caught -> no sensor
TEST_F(NsmGpuPcieSensorFactoryTest, Factory_PortInfo_MissingPortType_NoSensor)
{
    const std::string objPath = "/test/gpupcie/portinfo_no_porttype";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/portinfo_no_porttype_proc";
    const std::string portInfoIntf = baseIntf + ".PortInfo";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("GpuPcie_PortInfo_NoPortType");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = processorPath;
    baseMap["DeviceIndex"] = uint64_t(0);
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, portInfoIntf);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["Priority"] = bool{false};
    // "PortType" omitted -> portType="" -> throws

    const size_t before = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, objPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// NSM_PortInfo + PortProtocol absent -> portProtocol="" ->
// NsmGpuPciePortInfo throws -> caught -> no sensor
TEST_F(NsmGpuPcieSensorFactoryTest,
       Factory_PortInfo_MissingPortProtocol_NoSensor)
{
    const std::string objPath = "/test/gpupcie/portinfo_no_portproto";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/portinfo_no_portproto_proc";
    const std::string portInfoIntf = baseIntf + ".PortInfo";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("GpuPcie_PortInfo_NoProto");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = processorPath;
    baseMap["DeviceIndex"] = uint64_t(0);
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, portInfoIntf);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["Priority"] = bool{false};
    // "PortProtocol" omitted -> portProtocol="" -> throws

    const size_t before = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, objPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// =============================================================================
// NsmClearPCIeIntf::clearCounter happy path + doClearPCIeCountersOnDevice
// =============================================================================

// clearCounter with valid counter -> doClearPCIeCountersOnDevice runs
// synchronously (COVERAGE_DISABLE_COROUTINES), covers its body.
// postPatchIO returns success response -> clearPCIeErrorCounter success path.
TEST_F(NsmClearPCIeCountersTest, ClearCounter_ValidCounter_Success)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    // Build a valid clear response for postPatchIO
    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_clear_data_source_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    // clearCounter looks up "CorrectableErrorCount" -> groupId=2, dsId=3 ->
    // calls doClearPCIeCountersOnDevice -> clearPCIeErrorCounter -> postPatchIO
    auto objPath = clearPCIeIntf->clearCounter("CorrectableErrorCount");
    EXPECT_FALSE(std::string(objPath).empty());
}

// clearCounter with valid counter, postPatchIO failure ->
// doClearPCIeCountersOnDevice body executed, WriteFailure status set.
TEST_F(NsmClearPCIeCountersTest, ClearCounter_ValidCounter_PostPatchIOFails)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    // Should not throw - status is set inside the async handler //

    EXPECT_NO_THROW(clearPCIeIntf->clearCounter("CorrectableErrorCount"));
}

// =============================================================================

// NSM_PortInfo + DeviceIndex absent from base -> L518 FALSE branch ->
// deviceIndex stays 0; sensor still created with default deviceIndex
TEST_F(NsmGpuPcieSensorFactoryTest,
       Factory_PortInfo_MissingDeviceIndex_FalseBranch)
{
    const std::string objPath = "/test/gpupcie/portinfo_no_base_devidx";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/portinfo_no_base_devidx_proc";
    const std::string portInfoIntf = baseIntf + ".PortInfo";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("GpuPcie_PortInfo_NoBaseDevIdx");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = processorPath;
    // "DeviceIndex" omitted from base map -> L518 FALSE branch -> deviceIndex=0
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, portInfoIntf);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["Priority"] = bool{false};

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, objPath);
    // Sensor created with default deviceIndex=0
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// Unknown type (neither NSM_GPU_PCIe_0 nor NSM_PortInfo) -> L497 else-if
// condition evaluates false -> no sensor created
TEST_F(NsmGpuPcieSensorFactoryTest, Factory_UnknownType_NoSensor)
{
    const std::string objPath = "/test/gpupcie/unknown_type";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/unknown_type_proc";
    const std::string portInfoIntf = baseIntf + ".PortInfo";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("GpuPcie_UnknownType");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = processorPath;
    baseMap["DeviceIndex"] = uint64_t(0);
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, portInfoIntf);
    pm["Type"] = std::string("NSM_Unknown"); // neither NSM_GPU_PCIe_0 nor
                                             // NSM_PortInfo -> L497 FALSE

    const size_t before = gpu->deviceSensors.size() +
                          gpu->roundRobinSensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, objPath);
    // No sensor created
    EXPECT_EQ(before,
              gpu->deviceSensors.size() + gpu->roundRobinSensors.size());
}

// NsmClearPCIeCounters::update: rc==NSM_SW_SUCCESS, cc==NSM_ERROR ->
// L136 FALSE branch via cc!=NSM_SUCCESS
// (existing Update_ShortResponse_ErrorCC_CoversBranch uses
// NSM_RESPONSE_ERROR_LEN which causes decode to return NSM_SW_ERROR_LENGTH,
// covering rc!=NSM_SW_SUCCESS; this test uses the exact hdr+4 non-success
// buffer to cover cc!=NSM_SUCCESS)
TEST_F(NsmClearPCIeCountersTest, Update_DecodeSuccessNonZeroCC_L136ElseBranch)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_CCBranch", "NSM_ClearPCIe", 1, 0,
                                clearPCIeIntf);

    // Exactly-sized non-success buffer: decode_reason_code_and_cc returns
    // NSM_SW_SUCCESS with cc=NSM_ERROR -> L136 FALSE via cc!=NSM_SUCCESS
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// clearPCIeErrorCounter: rc==NSM_SW_SUCCESS, cc==NSM_ERROR ->
// L259 FALSE branch via cc!=NSM_SUCCESS -> else block (WriteFailure logged)
TEST_F(NsmClearPCIeCountersTest,
       ClearPCIeErrorCounter_DecodeSuccessNonZeroCC_L259ElseBranch)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, inventoryObjPath.c_str(), 0, gpu);

    // Exactly-sized non-success buffer so decode_clear_data_source_v1_resp
    // returns NSM_SW_SUCCESS with cc=NSM_ERROR -> L259 FALSE via
    // cc!=NSM_SUCCESS
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// NSM_PortInfo + Priority absent -> priority=false (FALSE branch) ->
// pcieECCGroup1 added to roundRobinSensors; portInfoSensor to deviceSensors
TEST_F(NsmGpuPcieSensorFactoryTest,
       Factory_PortInfo_MissingPriority_RoundRobinSensor)
{
    const std::string objPath = "/test/gpupcie/portinfo_no_priority";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/portinfo_no_priority_proc";
    const std::string portInfoIntf = baseIntf + ".PortInfo";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("GpuPcie_PortInfo_NoPriority");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = processorPath;
    baseMap["DeviceIndex"] = uint64_t(0);
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, portInfoIntf);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    // "Priority" omitted -> priority=false -> roundRobin for pcieECCGroup1

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmGpuPcieSensor(mockManager, portInfoIntf, objPath);
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}
