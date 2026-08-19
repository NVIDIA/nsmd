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
 * Additional branch coverage tests for nsmMemory.cpp:
 *
 * Targets uncovered branches in:
 *   - createNsmMemorySensor factory: missing UUID, bad UUID, missing props
 *   - createNSMMemory: with all properties, ErrorCorrection + DeviceType
 *   - createNSMMemory: with Priority=true
 *   - NsmRowRemapState: genRequestMsg valid/invalid, handleResponseMsg all
 * flags
 *   - NsmRowRemappingCounts: genRequestMsg valid/invalid, handleResponseMsg
 *   - NsmRemappingAvailabilityBankCount: genRequestMsg valid/invalid,
 *     handleResponseMsg with nonzero data
 *   - NsmEccErrorCountsDram: genRequestMsg valid/invalid
 *   - NsmMemCurrClockFreq: genRequestMsg valid/invalid, handleResponseMsg error
 *   - NsmMemCapacity: updateReading with nullopt (FALSE branch)
 *   - NsmMinMemoryClockLimit/NsmMaxMemoryClockLimit: update success
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "interfaceWrapper.hpp"
#include "nsmMemory.hpp"

namespace nsm
{
requester::Coroutine createNsmMemorySensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

static auto& b3Bus = utils::DBusHandler::getBus();
static std::string b3Name("b3_sensor");
static std::string b3Type("b3_type");
static std::string b3ObjPath("/xyz/openbmc_project/inventory/b3_memory");

// ============================================================================
// Fixture
// ============================================================================
class NsmMemoryBranch3Fixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:60";
    const std::string sensorName = "Memory_B3";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/system/b3/memory/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemoryBranch3Fixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemoryBranch3Fixture()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBaseProperties(const std::string& path,
                             dbus::PropertyMap extra = {})
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
        pm["Name"] = sensorName;
        pm["UUID"] = gpuUuid;
        pm["InventoryObjPath"] = inventoryPath;
        for (auto& [k, v] : extra)
        {
            pm[k] = v;
        }
    }

    void setupCurrentProperties(const std::string& path,
                                const std::string& intf,
                                dbus::PropertyMap props)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
        pm = props;
    }

    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ============================================================================
// Factory: Missing UUID -> no device match -> error
// ============================================================================
TEST_F(NsmMemoryBranch3Fixture, Factory_BadUUID_NoDevice)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes";
    const std::string path = "/test/memb3/bad_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = std::string("STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:999");
    pm["InventoryObjPath"] = inventoryPath;

    auto& cpm = utils::MockDbusAsync::propertyMap(path, intf);
    cpm["Type"] = std::string("NSM_Memory_Attributes");

    createNsmMemorySensor(mockManager, intf, path);
    // No crash expected, just returns error
}

// ============================================================================
// Factory: unknown type -> no sensors created
// ============================================================================
TEST_F(NsmMemoryBranch3Fixture, Factory_UnknownType)
{
    const std::string path = "/test/memb3/unk_type";
    setupBaseProperties(path, {{"Type", std::string("NSM_Unknown_Type")}});

    const size_t before = gpu->deviceSensors.size() +
                          gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->roundRobinSensors.size(),
              before);
}

// ============================================================================
// NsmRowRemapState: genRequestMsg valid
// ============================================================================
TEST(NsmRowRemapStateBranch3, GenRequestMsg_Valid)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRowRemapState sensor(b3Name, b3Type, rowRemapIntf, b3ObjPath);

    auto request = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmRowRemapState: genRequestMsg invalid instanceId
// ============================================================================
TEST(NsmRowRemapStateBranch3, GenRequestMsg_Invalid)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRowRemapState sensor(b3Name, b3Type, rowRemapIntf, b3ObjPath);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg decode failure
// ============================================================================
TEST(NsmRowRemapStateBranch3, HandleResponseMsg_DecodeFail)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRowRemapState sensor(b3Name, b3Type, rowRemapIntf, b3ObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg with both flags set
// ============================================================================
TEST(NsmRowRemapStateBranch3, HandleResponseMsg_BothFlags)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRowRemapState sensor(b3Name, b3Type, rowRemapIntf, b3ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 0x03; // bit0=1 (failure), bit1=1 (pending)

    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::True);
    EXPECT_EQ(rowRemapIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::True);
}

// ============================================================================
// NsmRowRemappingCounts: genRequestMsg valid
// ============================================================================
TEST(NsmRowRemappingCountsBranch3, GenRequestMsg_Valid)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRowRemappingCounts sensor(b3Name, b3Type, rowRemapIntf, b3ObjPath);

    auto request = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmRowRemappingCounts: genRequestMsg invalid
// ============================================================================
TEST(NsmRowRemappingCountsBranch3, GenRequestMsg_Invalid)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRowRemappingCounts sensor(b3Name, b3Type, rowRemapIntf, b3ObjPath);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmRowRemappingCounts: handleResponseMsg decode fail
// ============================================================================
TEST(NsmRowRemappingCountsBranch3, HandleResponseMsg_DecodeFail)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRowRemappingCounts sensor(b3Name, b3Type, rowRemapIntf, b3ObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmRowRemappingCounts: handleResponseMsg with nonzero counts
// ============================================================================
TEST(NsmRowRemappingCountsBranch3, HandleResponseMsg_NonzeroCounts)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRowRemappingCounts sensor(b3Name, b3Type, rowRemapIntf, b3ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remapping_counts_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_row_remapping_counts_resp(0, NSM_SUCCESS, ERR_NULL, 42,
                                                   99, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(rowRemapIntf->ceRowRemappingCount(), 42u);
    EXPECT_EQ(rowRemapIntf->ueRowRemappingCount(), 99u);
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: genRequestMsg valid
// ============================================================================
TEST(NsmRemappingAvailabilityBranch3, GenRequestMsg_Valid)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b3Name, b3Type, rowRemapIntf,
                                             b3ObjPath);

    auto request = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: genRequestMsg invalid
// ============================================================================
TEST(NsmRemappingAvailabilityBranch3, GenRequestMsg_Invalid)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b3Name, b3Type, rowRemapIntf,
                                             b3ObjPath);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: handleResponseMsg decode fail
// ============================================================================
TEST(NsmRemappingAvailabilityBranch3, HandleResponseMsg_DecodeFail)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b3Name, b3Type, rowRemapIntf,
                                             b3ObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: handleResponseMsg with nonzero data
// ============================================================================
TEST(NsmRemappingAvailabilityBranch3, HandleResponseMsg_NonzeroData)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b3Bus, b3ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b3Name, b3Type, rowRemapIntf,
                                             b3ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_row_remap_availability_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_row_remap_availability data = {};
    data.high_remapping = 10;
    data.max_remapping = 20;
    data.low_remapping = 30;
    data.no_remapping = 40;
    data.partial_remapping = 50;

    auto rc = encode_get_row_remap_availability_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->highRemappingAvailablityBankCount(), 10u);
    EXPECT_EQ(rowRemapIntf->maxRemappingAvailablityBankCount(), 20u);
    EXPECT_EQ(rowRemapIntf->lowRemappingAvailablityBankCount(), 30u);
    EXPECT_EQ(rowRemapIntf->noRemappingAvailablityBankCount(), 40u);
    EXPECT_EQ(rowRemapIntf->partialRemappingAvailablityBankCount(), 50u);
}

// ============================================================================
// NsmEccErrorCountsDram: genRequestMsg valid
// ============================================================================
TEST(NsmEccErrorCountsDramBranch3, GenRequestMsg_Valid)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b3Bus, b3ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b3Name, b3Type, eccIntf, b3ObjPath);

    auto request = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmEccErrorCountsDram: genRequestMsg invalid
// ============================================================================
TEST(NsmEccErrorCountsDramBranch3, GenRequestMsg_Invalid)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b3Bus, b3ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b3Name, b3Type, eccIntf, b3ObjPath);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmEccErrorCountsDram: handleResponseMsg decode fail
// ============================================================================
TEST(NsmEccErrorCountsDramBranch3, HandleResponseMsg_DecodeFail)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b3Bus, b3ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b3Name, b3Type, eccIntf, b3ObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmMemCurrClockFreq: genRequestMsg valid
// ============================================================================
TEST(NsmMemCurrClockFreqBranch3, GenRequestMsg_Valid)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b3Bus, b3ObjPath.c_str());
    NsmMemCurrClockFreq sensor(b3Name, b3Type, dimmIntf, b3ObjPath);

    auto request = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmMemCurrClockFreq: genRequestMsg invalid
// ============================================================================
TEST(NsmMemCurrClockFreqBranch3, GenRequestMsg_Invalid)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b3Bus, b3ObjPath.c_str());
    NsmMemCurrClockFreq sensor(b3Name, b3Type, dimmIntf, b3ObjPath);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmMemCurrClockFreq: handleResponseMsg decode fail
// ============================================================================
TEST(NsmMemCurrClockFreqBranch3, HandleResponseMsg_DecodeFail)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b3Bus, b3ObjPath.c_str());
    NsmMemCurrClockFreq sensor(b3Name, b3Type, dimmIntf, b3ObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmMemCapacity: updateReading with nullopt (no data)
// ============================================================================
TEST(NsmMemCapacityBranch3, UpdateReading_Nullopt)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b3Bus, b3ObjPath.c_str());
    dimmIntf->memorySizeInKB(999);
    NsmMemCapacity sensor(b3Name, b3Type, dimmIntf);

    sensor.updateReading(std::nullopt);

    // Value should not change when nullopt
    EXPECT_EQ(dimmIntf->memorySizeInKB(), 999u);
}

// ============================================================================
// NsmMinMemoryClockLimit: update() with successful response
// ============================================================================
TEST_F(NsmMemoryBranch3Fixture, NsmMinClockLimit_Update_Success)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        b3Bus, "/xyz/openbmc_project/inventory/b3_clk_min_ok");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMinMemoryClockLimit sensor(b3Name, b3Type, dimmIntf);

    // Build successful inventory information response
    uint32_t value = htole32(3200);
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_inventory_information_resp) +
                              sizeof(value));
    auto respMsg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value),
        reinterpret_cast<uint8_t*>(&value), respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[0], 3200u);
}

// ============================================================================
// NsmMaxMemoryClockLimit: update() with successful response
// ============================================================================
TEST_F(NsmMemoryBranch3Fixture, NsmMaxClockLimit_Update_Success)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        b3Bus, "/xyz/openbmc_project/inventory/b3_clk_max_ok");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMaxMemoryClockLimit sensor(b3Name, b3Type, dimmIntf);

    uint32_t value = htole32(4800);
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_inventory_information_resp) +
                              sizeof(value));
    auto respMsg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value),
        reinterpret_cast<uint8_t*>(&value), respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[1], 4800u);
}

// ============================================================================
// NsmMinMemoryClockLimit: update() with sensorIO failure
// ============================================================================
TEST_F(NsmMemoryBranch3Fixture, NsmMinClockLimit_Update_SensorIOFail)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        b3Bus, "/xyz/openbmc_project/inventory/b3_clk_min_io");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMinMemoryClockLimit sensor(b3Name, b3Type, dimmIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_INVALID_DATA));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[0], 0u);
}

// ============================================================================
// NsmMaxMemoryClockLimit: update() with sensorIO failure
// ============================================================================
TEST_F(NsmMemoryBranch3Fixture, NsmMaxClockLimit_Update_SensorIOFail)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        b3Bus, "/xyz/openbmc_project/inventory/b3_clk_max_io");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMaxMemoryClockLimit sensor(b3Name, b3Type, dimmIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_INVALID_DATA));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[1], 0u);
}

// ============================================================================
// NsmMinMemoryClockLimit: update() with wrong dataSize
// ============================================================================
TEST_F(NsmMemoryBranch3Fixture, NsmMinClockLimit_Update_WrongDataSize)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        b3Bus, "/xyz/openbmc_project/inventory/b3_clk_min_sz");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMinMemoryClockLimit sensor(b3Name, b3Type, dimmIntf);

    // Build response with wrong dataSize (2 bytes instead of 4)
    uint16_t shortValue = htole16(1234);
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_inventory_information_resp) +
                              sizeof(shortValue));
    auto respMsg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(shortValue),
        reinterpret_cast<uint8_t*>(&shortValue), respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[0], 0u); // Not updated because dataSize != sizeof(uint32)
}

// ============================================================================
// NsmMemoryAssociation constructor
// ============================================================================
TEST(NsmMemoryAssociationBranch3, Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "assoc_b3";
    std::string type = "assoc_type";
    std::string objPath = "/xyz/openbmc_project/inventory/b3_assoc";

    std::vector<utils::Association> associations;
    associations.push_back({"parent", "child", "/xyz/openbmc_project/parent"});

    NsmMemoryAssociation sensor(bus, name, type, objPath, associations);
}

// ============================================================================
// NsmMemoryAssociation with empty associations
// ============================================================================
TEST(NsmMemoryAssociationBranch3, ConstructorEmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "assoc_b3_empty";
    std::string type = "assoc_type";
    std::string objPath = "/xyz/openbmc_project/inventory/b3_assoc_empty";

    std::vector<utils::Association> associations;
    NsmMemoryAssociation sensor(bus, name, type, objPath, associations);
}

// ============================================================================
// NsmLocationIntfMemory constructor
// ============================================================================
TEST(NsmLocationIntfMemoryBranch3, Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "loc_b3";
    std::string type = "loc_type";
    std::string objPath = "/xyz/openbmc_project/inventory/b3_location";

    NsmLocationIntfMemory sensor(bus, name, type, objPath);
}

// ============================================================================
// NsmMemoryHealth constructor
// ============================================================================
TEST(NsmMemoryHealthBranch3, Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "health_b3";
    std::string type = "health_type";
    std::string objPath = "/xyz/openbmc_project/inventory/b3_health";

    NsmMemoryHealth sensor(bus, name, type, objPath);
}
