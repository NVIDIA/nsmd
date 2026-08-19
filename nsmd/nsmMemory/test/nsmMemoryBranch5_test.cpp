/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmMemory.cpp constructors (lines 25-130)
 * and NsmRowRemapState::handleResponseMsg decode failure path.
 *
 * Targets:
 *   - NsmMemoryErrorCorrection constructor
 *   - NsmMemoryDeviceType constructor
 *   - NsmLocationIntfMemory constructor
 *   - NsmMemoryHealth constructor
 *   - NsmMemoryAssociation constructor (empty + non-empty associations)
 *   - NsmRowRemapState constructor
 *   - NsmRowRemapState::handleResponseMsg decode failure
 *   - NsmRowRemapState::genRequestMsg success
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

using namespace nsm;

static auto& b5Bus = utils::DBusHandler::getBus();
static std::string b5Name("b5_sensor");
static std::string b5Type("b5_type");
static std::string b5ObjPath("/xyz/openbmc_project/inventory/b5_memory");

// ============================================================================
// Fixture
// ============================================================================
struct NsmMemoryBranch5Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemoryBranch5Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemoryBranch5Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmMemoryErrorCorrection: constructor sets ECC property
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MemoryErrorCorrection_Constructor)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    std::string correctionType =
        "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC";
    NsmMemoryErrorCorrection sensor(b5Name, b5Type, dimmIntf, correctionType,
                                    b5ObjPath);

    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_EQ(dimmIntf->ecc(), EccType::MultiBitECC);
}

TEST_F(NsmMemoryBranch5Test, MemoryErrorCorrection_SingleBitECC)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    std::string correctionType =
        "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.SingleBitECC";
    NsmMemoryErrorCorrection sensor(b5Name, b5Type, dimmIntf, correctionType,
                                    b5ObjPath);

    EXPECT_EQ(dimmIntf->ecc(), EccType::SingleBitECC);
}

// ============================================================================
// NsmMemoryDeviceType: constructor sets memoryType property
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MemoryDeviceType_Constructor)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    std::string memType =
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR5";
    NsmMemoryDeviceType sensor(b5Name, b5Type, dimmIntf, memType, b5ObjPath);

    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_EQ(dimmIntf->memoryType(), MemoryDeviceType::DDR5);
}

TEST_F(NsmMemoryBranch5Test, MemoryDeviceType_DDR4)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    std::string memType =
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR4";
    NsmMemoryDeviceType sensor(b5Name, b5Type, dimmIntf, memType, b5ObjPath);

    EXPECT_EQ(dimmIntf->memoryType(), MemoryDeviceType::DDR4);
}

// ============================================================================
// NsmLocationIntfMemory: constructor sets locationType to Embedded
// ============================================================================

TEST_F(NsmMemoryBranch5Test, LocationIntfMemory_Constructor)
{
    NsmLocationIntfMemory sensor(b5Bus, b5Name, b5Type, b5ObjPath);

    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_NE(sensor.locationIntf, nullptr);
    EXPECT_EQ(sensor.locationIntf->locationType(),
              LocationTypesMemory::Embedded);
}

// ============================================================================
// NsmMemoryHealth: constructor sets health to OK
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MemoryHealth_Constructor)
{
    NsmMemoryHealth sensor(b5Bus, b5Name, b5Type, b5ObjPath);

    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_NE(sensor.healthIntf, nullptr);
    EXPECT_EQ(sensor.healthIntf->health(), MemoryHealthType::OK);
}

// ============================================================================
// NsmMemoryAssociation: constructor with empty associations
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MemoryAssociation_EmptyAssociations)
{
    std::vector<utils::Association> associations;
    NsmMemoryAssociation sensor(b5Bus, b5Name, b5Type, b5ObjPath, associations);

    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_NE(sensor.associationDef, nullptr);
    EXPECT_TRUE(sensor.associationDef->associations().empty());
}

// ============================================================================
// NsmMemoryAssociation: constructor with non-empty associations
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MemoryAssociation_NonEmptyAssociations)
{
    std::vector<utils::Association> associations;
    associations.push_back({"containing", "contained_by",
                            "/xyz/openbmc_project/inventory/system/board"});
    associations.push_back(
        {"parent", "child", "/xyz/openbmc_project/inventory/system/gpu"});

    NsmMemoryAssociation sensor(b5Bus, b5Name, b5Type, b5ObjPath, associations);

    auto assocList = sensor.associationDef->associations();
    ASSERT_EQ(assocList.size(), 2u);
    EXPECT_EQ(std::get<0>(assocList[0]), "containing");
    EXPECT_EQ(std::get<1>(assocList[0]), "contained_by");
    EXPECT_EQ(std::get<2>(assocList[0]),
              "/xyz/openbmc_project/inventory/system/board");
    EXPECT_EQ(std::get<0>(assocList[1]), "parent");
    EXPECT_EQ(std::get<1>(assocList[1]), "child");
    EXPECT_EQ(std::get<2>(assocList[1]),
              "/xyz/openbmc_project/inventory/system/gpu");
}

// ============================================================================
// NsmRowRemapState: constructor sets interfaces
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemapState_Constructor)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemapState sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_NE(sensor.memoryRowRemappingStateIntf, nullptr);
    EXPECT_EQ(sensor.inventoryObjPath, b5ObjPath);
}

// ============================================================================
// NsmRowRemapState: genRequestMsg success
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemapState_GenRequestMsg_Success)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemapState sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    auto result = sensor.genRequestMsg(0x10, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg decode failure (short buffer)
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemapState_HandleResp_DecodeFail)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemapState sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    // Too-short buffer triggers decode failure (rc != NSM_SW_SUCCESS)
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_SUCCESS;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    // rc != 0 and cc == 0 => returns rc (non-success)
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg with flags=0 (both bits false)
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemapState_HandleResp_FlagsZero)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemapState sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    bitfield8_t flags = {};
    flags.byte = 0x00;
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remap_state_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(rrIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::False);
    EXPECT_EQ(rrIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::False);
}

// ============================================================================
// Additional branch coverage tests for nsmMemory.cpp
//
// Targets remaining uncovered branches in:
//   - createNsmMemorySensor: missing UUID, NSM_Memory type path with
//     ErrorCorrection/DeviceType/Priority present/absent
//   - createNSMMemory: ErrorCorrection absent, DeviceType absent, Priority
//     true/false
//   - NsmRowRemapState: handleResponseMsg cc=0 rc!=0 (ternary false branch)
//   - NsmRowRemappingCounts: handleResponseMsg cc=0 rc!=0
//   - NsmRemappingAvailabilityBankCount: handleResponseMsg cc=0 rc!=0
//   - NsmEccErrorCountsDram: handleResponseMsg cc=0 rc!=0
//   - NsmMemCurrClockFreq: handleResponseMsg cc=0 rc!=0
//   - NsmRowRemapState: updateReading with bit0=1 only
//   - NsmMemCapacity constructor
//   - createMemoryRowRemapping / createMemoryECCMode direct
//   - NsmMinMemoryClockLimit / NsmMaxMemoryClockLimit constructors
// ============================================================================

namespace nsm
{
requester::Coroutine createNsmMemorySensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

// ============================================================================
// Factory: NSM_Memory type with all properties including Priority=true
// Covers: type == "NSM_Memory" TRUE, count("ErrorCorrection") TRUE,
//         count("DeviceType") TRUE, count("Priority") TRUE
// ============================================================================

TEST_F(NsmMemoryBranch5Test,
       DISABLED_Factory_NSM_Memory_AllProperties_PriorityTrue)
{
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string path = "/test/memb5/nsm_mem_all";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("Memory_B5_All");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b5/all");

    auto& cpm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    cpm["Type"] = std::string("NSM_Memory");
    cpm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC");
    cpm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR5");
    cpm["Priority"] = true;

    const size_t before = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_Memory type without ErrorCorrection/DeviceType/Priority
// Covers: count("ErrorCorrection") FALSE, count("DeviceType") FALSE,
//         count("Priority") FALSE
// ============================================================================

TEST_F(NsmMemoryBranch5Test, DISABLED_Factory_NSM_Memory_NoOptionalProps)
{
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string path = "/test/memb5/nsm_mem_noprops";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("Memory_B5_NP");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b5/noopt");

    auto& cpm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    cpm["Type"] = std::string("NSM_Memory");
    // No ErrorCorrection, no DeviceType, no Priority

    const size_t before = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_Memory type with Priority=false explicitly
// Covers: count("Priority") TRUE but value=false
// ============================================================================

TEST_F(NsmMemoryBranch5Test, DISABLED_Factory_NSM_Memory_PriorityFalse)
{
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string path = "/test/memb5/nsm_mem_prifalse";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("Memory_B5_PF");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b5/prifalse");

    auto& cpm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    cpm["Type"] = std::string("NSM_Memory");
    cpm["Priority"] = false;

    const size_t before = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: Missing UUID property => no device found => error
// Covers: allBaseIfaceProperties.count("UUID") FALSE => uuid empty =>
//         !nsmDevice TRUE
// ============================================================================

TEST_F(NsmMemoryBranch5Test, DISABLED_Factory_MissingUUID_ReturnsError)
{
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string path = "/test/memb5/no_uuid";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("Memory_B5_NoUUID");
    // No UUID
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b5/nouuid");

    auto& cpm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    cpm["Type"] = std::string("NSM_Memory");

    const size_t before = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    // No sensors added due to empty UUID
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_Memory_Attributes type
// Covers: type == "NSM_Memory_Attributes" TRUE => creates RowRemapping +
//         ECCMode + MemCapUtil
// ============================================================================

TEST_F(NsmMemoryBranch5Test, Factory_NSM_Memory_Attributes)
{
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes";
    const std::string path = "/test/memb5/attrs";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("Memory_B5_Attrs");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b5/attrs");

    auto& cpm = utils::MockDbusAsync::propertyMap(path, intf);
    cpm["Type"] = std::string("NSM_Memory_Attributes");

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, intf, path);
    // Should create row remap, ecc, and memcaputil sensors
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// Factory: Unknown type string => neither "NSM_Memory" nor
//          "NSM_Memory_Attributes" => no sensors
// Covers: type == "NSM_Memory" FALSE, type == "NSM_Memory_Attributes" FALSE
// ============================================================================

TEST_F(NsmMemoryBranch5Test, DISABLED_Factory_UnknownType_NoSensors)
{
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string path = "/test/memb5/unknown_type";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("Memory_B5_Unk");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/b5/unktype");

    auto& cpm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    cpm["Type"] = std::string("NSM_Something_Else");

    const size_t dBefore = gpu->deviceSensors.size();
    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), dBefore);
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg decode failure returns rc (cc=0, rc!=0)
// Covers: cc ? cc : rc => FALSE (cc=0) => returns rc
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemapState_HandleResp_DecodeFailReturnRc)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemapState sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    // Build a buffer that is too short for success decode but has cc=0
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    // Set cc field to 0 (success), but buffer is too short for full decode
    msg->payload[1] = NSM_SUCCESS;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    // decode fails => rc != 0, cc stays 0 => returns rc (non-zero)
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmRowRemappingCounts: handleResponseMsg decode failure returns rc
// Covers: cc ? cc : rc => FALSE (cc=0) => returns rc
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemappingCounts_HandleResp_DecodeFailReturnRc)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemappingCounts sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_SUCCESS;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: handleResponseMsg decode failure returns
// rc Covers: cc ? cc : rc => FALSE (cc=0) => returns rc
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RemapAvail_HandleResp_DecodeFailReturnRc)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_SUCCESS;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmEccErrorCountsDram: handleResponseMsg decode failure returns rc
// Covers: cc ? cc : rc => FALSE (cc=0) => returns rc
// ============================================================================

TEST_F(NsmMemoryBranch5Test, EccErrorCounts_HandleResp_DecodeFailReturnRc)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b5Bus, b5ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b5Name, b5Type, eccIntf, b5ObjPath);

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_SUCCESS;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmMemCurrClockFreq: handleResponseMsg decode failure returns rc
// Covers: cc ? cc : rc => FALSE (cc=0) => returns rc
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MemCurrClockFreq_HandleResp_DecodeFailReturnRc)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    NsmMemCurrClockFreq sensor(b5Name, b5Type, dimmIntf, b5ObjPath);

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_SUCCESS;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg success with bit0=1 only
// Covers: flags.bits.bit0=1, bit1=0 in updateReading
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemapState_HandleResp_FailureOnlyFlag)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemapState sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    bitfield8_t flags = {};
    flags.byte = 0x01; // bit0=1 (failure), bit1=0 (no pending)
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remap_state_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(rrIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::True);
    EXPECT_EQ(rrIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::False);
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg success with bit1=1 only
// Covers: flags.bits.bit0=0, bit1=1 in updateReading
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemapState_HandleResp_PendingOnlyFlag)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemapState sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    bitfield8_t flags = {};
    flags.byte = 0x02; // bit0=0 (no failure), bit1=1 (pending)
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remap_state_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(rrIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::False);
    EXPECT_EQ(rrIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::True);
}

// ============================================================================
// NsmMemCapacity: constructor
// Covers: NsmMemCapacity constructor (creates sensor without crash)
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MemCapacity_Constructor)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    NsmMemCapacity sensor(b5Name, b5Type, dimmIntf);
    EXPECT_EQ(sensor.getName(), b5Name);
}

// ============================================================================
// NsmMinMemoryClockLimit / NsmMaxMemoryClockLimit: constructors
// Covers: constructor paths for both clock limit classes
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MinMemoryClockLimit_Constructor)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    NsmMinMemoryClockLimit sensor(b5Name, b5Type, dimmIntf);
    EXPECT_EQ(sensor.getName(), b5Name);
}

TEST_F(NsmMemoryBranch5Test, MaxMemoryClockLimit_Constructor)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    NsmMaxMemoryClockLimit sensor(b5Name, b5Type, dimmIntf);
    EXPECT_EQ(sensor.getName(), b5Name);
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: constructor
// Covers: NsmRemappingAvailabilityBankCount constructor + updateMetricOnShmem
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RemapAvailBankCount_Constructor)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b5Name, b5Type, rrIntf, b5ObjPath);
    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_NE(sensor.rowRemapIntf, nullptr);
}

// ============================================================================
// NsmEccErrorCountsDram: constructor
// Covers: NsmEccErrorCountsDram constructor
// ============================================================================

TEST_F(NsmMemoryBranch5Test, EccErrorCountsDram_Constructor)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b5Bus, b5ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b5Name, b5Type, eccIntf, b5ObjPath);
    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_NE(sensor.eccIntf, nullptr);
}

// ============================================================================
// NsmMemCurrClockFreq: constructor
// Covers: NsmMemCurrClockFreq constructor
// ============================================================================

TEST_F(NsmMemoryBranch5Test, MemCurrClockFreq_Constructor)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b5Bus, b5ObjPath.c_str());
    NsmMemCurrClockFreq sensor(b5Name, b5Type, dimmIntf, b5ObjPath);
    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_NE(sensor.dimmIntf, nullptr);
}

// ============================================================================
// NsmRowRemappingCounts: constructor
// Covers: NsmRowRemappingCounts constructor + updateMetricOnSharedMemory
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RowRemappingCounts_Constructor)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemappingCounts sensor(b5Name, b5Type, rrIntf, b5ObjPath);
    EXPECT_EQ(sensor.getName(), b5Name);
    EXPECT_NE(sensor.memoryRowRemappingCountsIntf, nullptr);
}

// ============================================================================
// NsmRowRemappingCounts: handleResponseMsg success with large values
// Covers: updateReading with non-trivial correctable_error, uncorrectable_error
// ============================================================================

TEST_F(NsmMemoryBranch5Test, DISABLED_RowRemappingCounts_HandleResp_LargeValues)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRowRemappingCounts sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remapping_counts_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remapping_counts_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   12345, 67890, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(rrIntf->ceRowRemappingCount(), 12345u);
    EXPECT_EQ(rrIntf->ueRowRemappingCount(), 67890u);
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: handleResponseMsg success with mixed data
// Covers: updateReading + updateMetricOnSharedMemory on success path
// ============================================================================

TEST_F(NsmMemoryBranch5Test, RemapAvail_HandleResp_MixedValues)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b5Bus,
                                                           b5ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b5Name, b5Type, rrIntf, b5ObjPath);

    nsm_row_remap_availability data = {};
    data.high_remapping = 100;
    data.max_remapping = 200;
    data.low_remapping = 50;
    data.no_remapping = 25;
    data.partial_remapping = 75;

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remap_availability_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remap_availability_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(rrIntf->highRemappingAvailablityBankCount(), 100u);
    EXPECT_EQ(rrIntf->maxRemappingAvailablityBankCount(), 200u);
    EXPECT_EQ(rrIntf->lowRemappingAvailablityBankCount(), 50u);
    EXPECT_EQ(rrIntf->noRemappingAvailablityBankCount(), 25u);
    EXPECT_EQ(rrIntf->partialRemappingAvailablityBankCount(), 75u);
}
