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
    EXPECT_FALSE(rrIntf->rowRemappingFailureState());
    EXPECT_FALSE(rrIntf->rowRemappingPendingState());
}
