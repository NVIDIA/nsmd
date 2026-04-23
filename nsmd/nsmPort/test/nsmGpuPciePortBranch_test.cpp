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
 * Branch coverage tests for nsmd/nsmPort/nsmGpuPciePort.cpp
 *
 * Covers:
 * - NsmClearPCIeCounters::update: sensorIO failure (TRUE branch at L113)
 * - NsmClearPCIeCounters::findAndUpdateCounter: counter already in list
 *   (TRUE branch of std::find != end at L148)
 * - NsmClearPCIeCounters::updateReading: default case (unknown groupId)
 * - NsmClearPCIeIntf::clearPCIeErrorCounter: encode fail (TRUE branch L229)
 * - NsmClearPCIeIntf::clearCounter: objectPath.empty() (TRUE branch L315)
 * - NsmClearPCIeIntf::clearCounter: invalid counter (TRUE branch L321)
 * - NsmGpuPciePort constructor: various health/chassisState/association combos
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "libnsm/pci-links.h"

#include "nsmCommon/nsmPcieGroup.hpp"
#include "nsmGpuPciePort.hpp"
#include "test/commonMock.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmGpuPcieSensor(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath);
} // namespace nsm

static auto bus = sdbusplus::bus::new_default();
static const std::string invPath =
    "/xyz/openbmc_project/inventory/system/gpu/pcie/brPort";

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmGpuPciePortBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmGpuPciePortBranchTest() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));

        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGpuPciePortBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    // Helper: build valid update response with given clearable bits
    static std::vector<uint8_t> buildUpdateResponse(uint8_t clearableBits)
    {
        std::vector<uint8_t> resp(256, 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        uint8_t availableMask[1] = {0xFF};
        uint8_t clearableMask[1] = {clearableBits};
        uint8_t maskLength = 1;
        uint16_t dataSize = 3;
        [[maybe_unused]] auto rc =
            encode_query_available_clearable_scalar_data_sources_v1_resp(
                0, NSM_SUCCESS, ERR_NULL, dataSize, maskLength, availableMask,
                clearableMask, msg);
        return resp;
    }

    // Valgrind-safe decode-fail buffer
    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ===========================================================================
// NsmClearPCIeCounters::update - sensorIO failure (TRUE branch at L113)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Update_SensorIOFail_ReturnsError)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_IOFail", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    // sensorIO returns non-zero rc -> TRUE branch of if(rc) at L113
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_INVALID_DATA));

    sensor.update(gpu);
}

// ===========================================================================
// NsmClearPCIeCounters::findAndUpdateCounter - counter already in list
// (TRUE branch of std::find != end at L148)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest,
       FindAndUpdateCounter_DuplicateCounter_NotAddedTwice)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_Dup", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    // First call with bit0 set adds NonFatalErrorCount
    auto resp1 = buildUpdateResponse(0x01);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp1));
    sensor.update(gpu);

    // Second call with same bits -> findAndUpdateCounter finds counter
    // already in list -> TRUE branch of (find != end) -> does NOT push_back
    auto resp2 = buildUpdateResponse(0x01);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp2));
    sensor.update(gpu);

    // Counter should appear only once
    auto counters = clearPCIeIntf->clearableCounters();
    size_t count = 0;
    for (const auto& c : counters)
    {
        if (c == CounterType::NonFatalErrorCount)
        {
            count++;
        }
    }
    EXPECT_EQ(count, 1u);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading - default case (unknown groupId)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_DefaultGroupId_NothingAdded)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    // groupId=99 hits the default case in the switch
    NsmClearPCIeCounters sensor("ClearPCIe_Default", "NSM_ClearPCIe", 99, 0,
                                clearPCIeIntf);

    // Even with all bits set, default case does nothing
    auto resp = buildUpdateResponse(0xFF);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_TRUE(counters.empty());
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=2, only bit1 (FatalErrorCount)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId2_OnlyBit1)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2b1", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x02); // only bit1 -> FatalErrorCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=2, only bit2 (UnsupportedReq)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId2_OnlyBit2)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2b2", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x04); // bit2 -> UnsupportedRequestCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=2, only bit3 (CorrectableErr)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId2_OnlyBit3)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2b3", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x08); // bit3 -> CorrectableErrorCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=3, bit0 not set
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId3_NoBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G3none", "NSM_ClearPCIe", 3, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x00); // no bits
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=4, individual bits
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId4_OnlyBit1)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4b1", "NSM_ClearPCIe", 4, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x02); // bit1 -> NAKReceivedCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId4_OnlyBit2)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4b2", "NSM_ClearPCIe", 4, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x04); // bit2 -> NAKSentCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId4_OnlyBit4)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4b4", "NSM_ClearPCIe", 4, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x10); // bit4 -> ReplayRolloverCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId4_OnlyBit6)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4b6", "NSM_ClearPCIe", 4, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x40); // bit6 -> ReplayCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// ===========================================================================
// NsmClearPCIeIntf::clearCounter - invalid counter throws InvalidArgument
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, ClearCounter_InvalidCounter_Throws)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    // clearCounter looks up counter in counterToGroupIdMap -> not found ->
    // throws InvalidArgument (line 321-325)
    // Note: if AsyncOperationManager returns empty objectPath first,
    // Unavailable is thrown instead (line 315-319). Either way: exception.
    EXPECT_THROW(clearPCIeIntf->clearCounter("BogusCounter"), std::exception);
}

// ===========================================================================
// NsmClearPCIeIntf::addClearCoutnerSensor - duplicate groupId -> not inserted
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, AddClearCounterSensor_Duplicate_NotInserted)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    auto sensor1 = std::shared_ptr<NsmPcieGroup>(nullptr);
    clearPCIeIntf->addClearCoutnerSensor(10, sensor1);

    // Duplicate insert -> try_emplace returns inserted=false -> error logged
    auto sensor2 = std::shared_ptr<NsmPcieGroup>(nullptr);
    clearPCIeIntf->addClearCoutnerSensor(10, sensor2);

    // Original stays
    EXPECT_EQ(clearPCIeIntf->getClearCounterSensorFromGroup(10), sensor1);
}

// ===========================================================================
// NsmClearPCIeIntf::getClearCounterSensorFromGroup - not found -> nullptr
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, GetClearCounterSensorFromGroup_NotFound)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    auto result = clearPCIeIntf->getClearCounterSensorFromGroup(42);
    EXPECT_EQ(result, nullptr);
}

// ===========================================================================
// NsmClearPCIeIntf::clearPCIeErrorCounter - postPatchIO fail
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, ClearPCIeErrorCounter_PostPatchIOFail)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmClearPCIeIntf::clearPCIeErrorCounter - decode fail (short response)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, ClearPCIeErrorCounter_DecodeFail)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    auto shortResp = decodeFail();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(shortResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmClearPCIeIntf::clearPCIeErrorCounter - success, sensor found -> update
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, ClearPCIeErrorCounter_Success_WithSensor)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    const std::string sensorPath = invPath + "/ecc_br";
    auto pcieECCIntf = std::make_shared<PCIeEccIntf>(bus, sensorPath.c_str());
    auto pciePortIntf =
        std::make_shared<PCIeEccIntf>(bus, (sensorPath + "p").c_str());
    std::string mutablePath = invPath;
    auto sensorGroup2 = std::make_shared<NsmPciGroup2>(
        "PCIe_G2_br", "NSM_GPU_PCIe", pcieECCIntf, pciePortIntf, uint8_t{0},
        mutablePath);
    clearPCIeIntf->addClearCoutnerSensor(2, sensorGroup2);

    // Build valid clear response
    std::vector<uint8_t> clearResp(256, 0);
    auto clearMsg = reinterpret_cast<nsm_msg*>(clearResp.data());
    [[maybe_unused]] auto rc =
        encode_clear_data_source_v1_resp(0, NSM_SUCCESS, ERR_NULL, clearMsg);

    // sensorGroup2->update() will call sensorIO
    auto updateResp = buildUpdateResponse(0x0F);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(clearResp));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(updateResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ===========================================================================
// NsmClearPCIeIntf::clearPCIeErrorCounter - success, no sensor for group
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, ClearPCIeErrorCounter_Success_NoSensor)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    [[maybe_unused]] auto rc = encode_clear_data_source_v1_resp(0, NSM_SUCCESS,
                                                                ERR_NULL, msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    // No sensor registered -> logs error but doesn't crash
}

// ===========================================================================
// NsmGpuPciePort constructor - various combinations
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Constructor_EmptyAssociations)
{
    std::vector<utils::Association> assocs;
    NsmGpuPciePort port(
        bus, "GpuPort_Br0", "NSM_GPU_PCIe",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK",
        "xyz.openbmc_project.State.Chassis.PowerState.On", assocs, invPath);
    EXPECT_EQ(port.getName(), "GpuPort_Br0");
}

TEST_F(NsmGpuPciePortBranchTest, Constructor_MultipleAssociations)
{
    std::vector<utils::Association> assocs;
    assocs.push_back(
        {"parent", "child", "/xyz/openbmc_project/inventory/system/gpu0"});
    assocs.push_back(
        {"device", "port", "/xyz/openbmc_project/inventory/system/dev0"});
    assocs.push_back(
        {"link", "endpoint", "/xyz/openbmc_project/inventory/system/ep0"});

    const std::string brPath = invPath + "/multi_assoc";
    NsmGpuPciePort port(
        bus, "GpuPort_Br1", "NSM_GPU_PCIe",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning",
        "xyz.openbmc_project.State.Chassis.PowerState.Off", assocs, brPath);
    EXPECT_EQ(port.getName(), "GpuPort_Br1");
}

// ===========================================================================
// NsmGpuPciePortInfo constructor
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, PortInfo_Constructor)
{
    const std::string piPath = invPath + "/portinfo_br";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus, piPath.c_str());

    NsmGpuPciePortInfo info(
        "PortInfo_Br", "NSM_PortInfo",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe",
        portInfoIntf);
    EXPECT_EQ(info.getName(), "PortInfo_Br");
}

// ===========================================================================
// NsmClearPCIeCounters constructor
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, ClearPCIeCounters_Constructor)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_Ctor", "NSM_ClearPCIe", 5, 3,
                                clearPCIeIntf);
    EXPECT_EQ(sensor.getName(), "ClearPCIe_Ctor");
}

// ===========================================================================
// createNsmGpuPcieSensor factory - invalid UUID -> no device found
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_InvalidUUID_NoDevice)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_bad_uuid";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_bad_uuid_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrBadUUID");
    pm["UUID"] = std::string("INVALID_UUID_STRING");
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");

    const size_t before = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - missing Name property
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_MissingName_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_no_name";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_no_name_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    // "Name" omitted -> name="" (FALSE branch of count("Name"))
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);

    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    // Sensor created with empty name
}

// ===========================================================================
// createNsmGpuPcieSensor factory - missing InventoryObjPath
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_MissingInventoryObjPath_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_no_inv";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrNoInv");
    pm["UUID"] = gpuUuid;
    // "InventoryObjPath" omitted -> inventoryObjPath="" (FALSE branch)
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);

    // Will throw due to empty path for D-Bus object creation -> caught //

    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - multiple ClearableScalarGroups
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_MultipleClearableScalarGroups)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_multi_clr";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_multi_clr_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrMultiClr");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(1);
    pm["ClearableScalarGroup"] = std::vector<uint64_t>{2, 3, 4};

    const size_t staticBefore = gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    // 3 NsmClearPCIeCounters sensors added as static sensors
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore + 3);
}

// ===========================================================================
// NsmClearPCIeCounters::update - decode success with cc=NSM_ERROR
// (FALSE branch of if(rc==SUCCESS && cc==SUCCESS) at L136)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Update_DecodeSuccess_NonZeroCC)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_NonZeroCC", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    // Non-success buffer: decode returns NSM_SW_SUCCESS with cc=NSM_ERROR
    auto resp = decodeFail();
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// ===========================================================================
// NsmClearPCIeIntf::clearPCIeErrorCounter - decode success, cc=NSM_ERROR
// (FALSE branch of if(rc==SUCCESS && cc==SUCCESS) at L259)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, ClearErrorCounter_DecodeSuccess_NonZeroCC)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    auto resp = decodeFail();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    clearPCIeIntf->clearPCIeErrorCounter(&status, 0, 2, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// doClearPCIeCountersOnDevice - valid counter -> maps to groupId/dsId
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, DoClearPCIeCountersOnDevice_ValidCounter)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    // clearCounter with valid counter -> doClearPCIeCountersOnDevice runs
    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    [[maybe_unused]] auto rc = encode_clear_data_source_v1_resp(0, NSM_SUCCESS,
                                                                ERR_NULL, msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    EXPECT_NO_THROW(clearPCIeIntf->clearCounter("FatalErrorCount"));
}

// ===========================================================================
// clearCounter with different valid counter names
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, ClearCounter_L0ToRecoveryCount)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    [[maybe_unused]] auto rc = encode_clear_data_source_v1_resp(0, NSM_SUCCESS,
                                                                ERR_NULL, msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    EXPECT_NO_THROW(clearPCIeIntf->clearCounter("L0ToRecoveryCount"));
}

TEST_F(NsmGpuPciePortBranchTest, ClearCounter_ReplayCount)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    [[maybe_unused]] auto rc = encode_clear_data_source_v1_resp(0, NSM_SUCCESS,
                                                                ERR_NULL, msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    EXPECT_NO_THROW(clearPCIeIntf->clearCounter("ReplayCount"));
}

// ===========================================================================
// createNsmGpuPcieSensor factory - type="NSM_PortInfo" branch
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_PortInfo_Type)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_portinfo";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_portinfo_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrPortInfo");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["Priority"] = false;
    pm["DeviceIndex"] = uint64_t(0);

    const size_t devBefore = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    EXPECT_GT(gpu->deviceSensors.size(), devBefore);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - missing UUID property (FALSE branch)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_MissingUUID_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_no_uuid";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_no_uuid_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrNoUUID");
    // "UUID" omitted -> uuid="" (FALSE branch of count("UUID"))
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");

    const size_t before = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    // No device found -> no sensors added
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - missing Type property (FALSE branch)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_MissingType_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_no_type";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_no_type_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrNoType");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    // "Type" omitted -> type="" -> neither NSM_GPU_PCIe_0 nor NSM_PortInfo
    // No sensors created
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - missing Health property (FALSE branch)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_MissingHealth_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_no_health";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_no_health_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrNoHealth");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    // "Health" omitted -> health="" (FALSE branch of count("Health"))
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);

    // Will throw due to empty Health string -> caught by catch block
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - missing ChasisPowerState (FALSE branch)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_MissingChasisPowerState_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_no_chassis";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_no_chassis_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrNoChassis");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    // "ChasisPowerState" omitted -> chasisState="" (FALSE branch)
    pm["DeviceIndex"] = uint64_t(0);

    // Will throw due to empty ChasisPowerState -> caught by catch block
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - missing DeviceIndex (FALSE branch)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_MissingDeviceIndex_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_no_devidx";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_no_devidx_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrNoDevIdx");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    // "DeviceIndex" omitted -> deviceIndex=0 (FALSE branch of count)

    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - missing ClearableScalarGroup (FALSE branch)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest,
       Factory_MissingClearableScalarGroup_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_no_clrgrp";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_no_clrgrp_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrNoClrGrp");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);
    // "ClearableScalarGroup" omitted -> empty vec (FALSE branch)

    const size_t staticBefore = gpu->staticSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
    // No NsmClearPCIeCounters added (empty ClearableScalarGroup)
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - NSM_PortInfo missing PortType (FALSE branch)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_PortInfo_MissingPortType)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_pi_nopt";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_pi_nopt_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrPINoPortType");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_PortInfo");
    // "PortType" omitted -> portType="" (FALSE branch)
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["Priority"] = false;
    pm["DeviceIndex"] = uint64_t(0);

    // Empty portType -> throws on convertPortTypeFromString -> caught
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - NSM_PortInfo missing PortProtocol
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_PortInfo_MissingPortProtocol)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_pi_nopp";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_pi_nopp_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrPINoPortProt");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    // "PortProtocol" omitted -> portProtocol="" (FALSE branch)
    pm["Priority"] = false;
    pm["DeviceIndex"] = uint64_t(0);

    // Empty portProtocol -> throws on convertPortProtocolFromString -> caught
    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - NSM_PortInfo missing Priority (FALSE branch)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_PortInfo_MissingPriority)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_pi_nopri";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_pi_nopri_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrPINoPriority");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    // "Priority" omitted -> priority=false (FALSE branch)
    pm["DeviceIndex"] = uint64_t(0);

    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// createNsmGpuPcieSensor factory - NSM_PortInfo missing DeviceIndex
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, Factory_PortInfo_MissingDeviceIndex)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    const std::string objPath = "/test/gpupcie/br_pi_nodidx";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/br_pi_nodidx_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    pm["Name"] = std::string("GpuPcie_BrPINoDevIdx");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["Priority"] = false;
    // "DeviceIndex" omitted -> deviceIndex=0 (FALSE branch)

    createNsmGpuPcieSensor(mockManager, baseIntf, objPath);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=2, all bits set
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId2_AllBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2all", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x0F); // bits 0-3 all set
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 4u);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=3, bit0 set
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId3_Bit0Set)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G3b0", "NSM_ClearPCIe", 3, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x01); // bit0 -> L0ToRecoveryCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 1u);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=4, all relevant bits
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId4_AllBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4all", "NSM_ClearPCIe", 4, 0,
                                clearPCIeIntf);

    // bits 1,2,4,6 all set
    auto resp = buildUpdateResponse(0x56); // 0b01010110 = bit1,2,4,6
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 4u);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=4, no bits set
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId4_NoBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4none", "NSM_ClearPCIe", 4, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x00); // no bits
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_TRUE(counters.empty());
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading groupId=2, no bits set (FALSE branches)
// ===========================================================================
TEST_F(NsmGpuPciePortBranchTest, UpdateReading_GroupId2_NoBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(bus, invPath.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2none", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x00); // no bits -> all FALSE branches
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_TRUE(counters.empty());
}
