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
 * Branch coverage tests for nsmInventoryProperty.hpp / .cpp
 *
 * Targets uncovered branches:
 *   - NsmInventoryPropertyBase::genRequestMsg: encode fail (instanceId=255)
 *   - NsmInventoryPropertyBase::handleResponseMsg: success, error CC, decode
 * fail
 *   - NsmInventoryProperty<NsmAssetIntf> constructor: is_same_v<NsmAssetIntf>
 *     TRUE branch (calls buildDate(nullDate))
 *   - NsmInventoryProperty<AssetTagIntf> constructor: is_same_v FALSE (no
 * buildDate)
 *   - handleResponse template specialization branches for each interface type
 *   - AssetTagIntf: empty vs non-empty asset tag branches
 *   - NsmAssetIntf: BUILD_DATE "0" vs non-"0" branches
 *   - NsmMNNVLinkTopologyIntf: CHASSIS_SERIAL_NUMBER valid/invalid UTF-8
 *   - NsmMNNVLinkTopologyIntf: GPU_NVLINK_PEER_TYPE Direct vs Bridge
 *   - VersionIntf: remaining PCIERETIMER cases (2,3,5,6,7)
 */

#include "test/mockDBusHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmAssetIntf.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmMNNVLinkTopologyIntf.hpp"

#undef private
#undef protected

using namespace nsm;

static auto& branchBus = utils::DBusHandler::getBus();

// ============================================================================
// Helper: build a successful inventory information response
// ============================================================================
static std::vector<uint8_t> buildInvResponse(const uint8_t* data,
                                             uint16_t dataSize)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_resp) +
                                 dataSize,
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          const_cast<uint8_t*>(data), msg);
    return buf;
}

// Helper: build a non-success CC response (minimum 7 bytes for valgrind safety)
static std::vector<uint8_t> buildErrorCCResponse()
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    return buf;
}

// ============================================================================
// genRequestMsg: encode fail (instanceId > NSM_INSTANCE_MAX) → nullopt
// ============================================================================
TEST(NsmInventoryPropertyBranch, GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_gen_fail";
    auto assetIntf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("inv", "NSM_Inv", path,
                                                assetIntf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BOARD_PART_NUMBER);

    auto result = invProp.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// genRequestMsg: valid instanceId → returns request
// ============================================================================
TEST(NsmInventoryPropertyBranch, GenRequestMsg_ValidInstanceId_ReturnsRequest)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_gen_ok";
    auto assetIntf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("inv", "NSM_Inv", path,
                                                assetIntf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    auto result = invProp.genRequestMsg(10, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));
}

// ============================================================================
// handleResponseMsg: success (rc==0, cc==0) → calls handleResponse
// ============================================================================
TEST(NsmInventoryPropertyBranch, HandleResponseMsg_Success)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_resp_ok";
    auto assetIntf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("inv", "NSM_Inv", path,
                                                assetIntf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    std::string serial = "SN-BRANCH-001";
    auto buf = buildInvResponse(reinterpret_cast<const uint8_t*>(serial.data()),
                                serial.size());
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = invProp.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(assetIntf->serialNumber(), serial);
}

// ============================================================================
// handleResponseMsg: cc != NSM_SUCCESS → does NOT call handleResponse
// ============================================================================
TEST(NsmInventoryPropertyBranch, HandleResponseMsg_ErrorCC_ReturnsCC)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_resp_cc";
    auto assetIntf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("inv", "NSM_Inv", path,
                                                assetIntf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    auto buf = buildErrorCCResponse();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = invProp.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// handleResponseMsg: decode fail (truncated buffer)
// ============================================================================
TEST(NsmInventoryPropertyBranch, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_resp_dec";
    auto assetIntf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("inv", "NSM_Inv", path,
                                                assetIntf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = invProp.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmAssetIntf constructor: is_same_v<NsmAssetIntf> TRUE → buildDate(nullDate)
// ============================================================================
TEST(NsmInventoryPropertyBranch, Constructor_NsmAssetIntf_SetsBuildDateNull)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ctor_asset";
    auto assetIntf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("inv", "NSM_Inv", path,
                                                assetIntf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BOARD_PART_NUMBER);

    // Constructor should have set buildDate to nullDate
    EXPECT_EQ(assetIntf->buildDate(), nullDate);
}

// ============================================================================
// AssetTagIntf constructor: is_same_v<NsmAssetIntf> FALSE → no buildDate call
// ============================================================================
TEST(NsmInventoryPropertyBranch, Constructor_AssetTagIntf_NoBuildDate)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ctor_tag";
    auto intf = std::make_shared<AssetTagIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider("inv", "NSM_Inv", path, intf);
    NsmInventoryProperty<AssetTagIntf> invProp(provider, ASSET_TAG);

    // Should not crash; AssetTagIntf has no buildDate method
    EXPECT_EQ(invProp.getName(), "inv");
}

// ============================================================================
// NsmAssetIntf handleResponse: all switch cases via handleResponseMsg
// ============================================================================
TEST(NsmInventoryPropertyBranch, HandleResponse_NsmAssetIntf_BoardPartNumber)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_bpn";
    auto intf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BOARD_PART_NUMBER);

    std::string pn = "BPN-900-123";
    auto buf = buildInvResponse(reinterpret_cast<const uint8_t*>(pn.data()),
                                pn.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->partNumber(), pn);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_NsmAssetIntf_SerialNumber)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_sn";
    auto intf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    std::string sn = "SN-99887766";
    auto buf = buildInvResponse(reinterpret_cast<const uint8_t*>(sn.data()),
                                sn.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->serialNumber(), sn);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_NsmAssetIntf_MarketingName)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_mkn";
    auto intf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, MARKETING_NAME);

    std::string model = "GPU B200";
    auto buf = buildInvResponse(reinterpret_cast<const uint8_t*>(model.data()),
                                model.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->model(), model);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_NsmAssetIntf_DevicePartNumber)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_dpn";
    auto intf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, DEVICE_PART_NUMBER);

    std::string dpn = "DPN-555";
    auto buf = buildInvResponse(reinterpret_cast<const uint8_t*>(dpn.data()),
                                dpn.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->partNumber(), dpn);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_NsmAssetIntf_FruPartNumber)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_fpn";
    auto intf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, FRU_PART_NUMBER);

    std::string fpn = "FRU-777";
    auto buf = buildInvResponse(reinterpret_cast<const uint8_t*>(fpn.data()),
                                fpn.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->partNumber(), fpn);
}

// BUILD_DATE "0" → nullDate (TRUE branch)
TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmAssetIntf_BuildDate_Zero_NullDate)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_bd0";
    auto intf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BUILD_DATE);

    std::string dateVal = "0";
    auto buf = buildInvResponse(
        reinterpret_cast<const uint8_t*>(dateVal.data()), dateVal.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->buildDate(), nullDate);
}

// BUILD_DATE non-"0" → actual date (FALSE branch)
TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmAssetIntf_BuildDate_NonZero_ActualDate)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_bdr";
    auto intf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BUILD_DATE);

    std::string dateVal = "2025-03-18";
    auto buf = buildInvResponse(
        reinterpret_cast<const uint8_t*>(dateVal.data()), dateVal.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->buildDate(), dateVal);
}

// NsmAssetIntf default branch → throw
TEST(NsmInventoryPropertyBranch, HandleResponse_NsmAssetIntf_Default_Throws)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_asset_def";
    auto intf = std::make_shared<NsmAssetIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, ASSET_TAG);

    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

// ============================================================================
// AssetTagIntf handleResponse branches
// ============================================================================
TEST(NsmInventoryPropertyBranch, HandleResponse_AssetTagIntf_NonEmptyTag)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_at_ne";
    auto intf = std::make_shared<AssetTagIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<AssetTagIntf> invProp(provider, ASSET_TAG);

    std::string tag = "MY-ASSET-TAG";
    auto buf = buildInvResponse(reinterpret_cast<const uint8_t*>(tag.data()),
                                tag.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->assetTag(), tag);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_AssetTagIntf_EmptyTag)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_at_e";
    auto intf = std::make_shared<AssetTagIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<AssetTagIntf> invProp(provider, ASSET_TAG);

    // Empty data → empty assetTag → if (assetTag.empty()) TRUE
    Response data = {};
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->assetTag(), "");
}

TEST(NsmInventoryPropertyBranch, HandleResponse_AssetTagIntf_Default_Throws)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_at_def";
    auto intf = std::make_shared<AssetTagIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<AssetTagIntf> invProp(provider, SERIAL_NUMBER);

    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

// ============================================================================
// DimensionIntf handleResponse branches
// ============================================================================
TEST(NsmInventoryPropertyBranch, HandleResponse_DimensionIntf_ProductLength)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_dim_l";
    auto intf = std::make_shared<DimensionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<DimensionIntf> invProp(provider, PRODUCT_LENGTH);

    uint8_t data[4] = {42, 0, 0, 0};
    auto buf = buildInvResponse(data, 4);
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->depth(), 42u);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_DimensionIntf_ProductHeight)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_dim_h";
    auto intf = std::make_shared<DimensionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<DimensionIntf> invProp(provider, PRODUCT_HEIGHT);

    uint8_t data[4] = {88, 0, 0, 0};
    auto buf = buildInvResponse(data, 4);
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->height(), 88u);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_DimensionIntf_ProductWidth)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_dim_w";
    auto intf = std::make_shared<DimensionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<DimensionIntf> invProp(provider, PRODUCT_WIDTH);

    uint8_t data[4] = {33, 0, 0, 0};
    auto buf = buildInvResponse(data, 4);
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->width(), 33u);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_DimensionIntf_Default_Throws)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_dim_def";
    auto intf = std::make_shared<DimensionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<DimensionIntf> invProp(provider, SERIAL_NUMBER);

    Response data = {0, 0, 0, 0};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

// ============================================================================
// PowerLimitIntf handleResponse branches
// ============================================================================
TEST(NsmInventoryPropertyBranch, HandleResponse_PowerLimitIntf_MinPowerLimit)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_pwr_min";
    auto intf = std::make_shared<PowerLimitIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<PowerLimitIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<PowerLimitIntf> invProp(provider,
                                                 MINIMUM_DEVICE_POWER_LIMIT);

    uint32_t mw = 200000; // 200W
    uint8_t data[4] = {uint8_t(mw & 0xFF), uint8_t((mw >> 8) & 0xFF),
                       uint8_t((mw >> 16) & 0xFF), uint8_t((mw >> 24) & 0xFF)};
    auto buf = buildInvResponse(data, 4);
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->minPowerWatts(), 200u);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_PowerLimitIntf_MaxPowerLimit)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_pwr_max";
    auto intf = std::make_shared<PowerLimitIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<PowerLimitIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<PowerLimitIntf> invProp(provider,
                                                 MAXIMUM_DEVICE_POWER_LIMIT);

    uint32_t mw = 900000; // 900W
    uint8_t data[4] = {uint8_t(mw & 0xFF), uint8_t((mw >> 8) & 0xFF),
                       uint8_t((mw >> 16) & 0xFF), uint8_t((mw >> 24) & 0xFF)};
    auto buf = buildInvResponse(data, 4);
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->maxPowerWatts(), 900u);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_PowerLimitIntf_Default_Throws)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_pwr_def";
    auto intf = std::make_shared<PowerLimitIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<PowerLimitIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<PowerLimitIntf> invProp(provider, SERIAL_NUMBER);

    Response data = {0, 0, 0, 0};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

// ============================================================================
// VersionIntf handleResponse branches (covering remaining retimer cases)
// ============================================================================
TEST(NsmInventoryPropertyBranch,
     HandleResponse_VersionIntf_PcieRetimer2EepromVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ver_rt2";
    auto intf = std::make_shared<VersionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_2_EEPROM_VERSION);

    Response data(8, 0);
    data[0] = 5;
    data[2] = 3;
    data[4] = 0;
    data[6] = 10;
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->version(), "5.3.10");
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_VersionIntf_PcieRetimer3EepromVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ver_rt3";
    auto intf = std::make_shared<VersionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_3_EEPROM_VERSION);

    Response data(8, 0);
    data[0] = 1;
    data[2] = 0;
    data[4] = 1;
    data[6] = 0;
    // version = "1.0.256" since (data[4]<<8)|data[6] = (1<<8)|0 = 256
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->version(), "1.0.256");
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_VersionIntf_PcieRetimer5EepromVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ver_rt5";
    auto intf = std::make_shared<VersionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_5_EEPROM_VERSION);

    Response data(8, 0);
    data[0] = 2;
    data[2] = 1;
    data[4] = 0;
    data[6] = 7;
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->version(), "2.1.7");
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_VersionIntf_PcieRetimer6EepromVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ver_rt6";
    auto intf = std::make_shared<VersionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_6_EEPROM_VERSION);

    Response data(8, 0);
    data[0] = 4;
    data[2] = 2;
    data[4] = 0;
    data[6] = 15;
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->version(), "4.2.15");
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_VersionIntf_PcieRetimer7EepromVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ver_rt7";
    auto intf = std::make_shared<VersionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_7_EEPROM_VERSION);

    Response data(8, 0);
    data[0] = 9;
    data[2] = 8;
    data[4] = 0;
    data[6] = 1;
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->version(), "9.8.1");
}

TEST(NsmInventoryPropertyBranch, HandleResponse_VersionIntf_Default_Throws)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ver_def";
    auto intf = std::make_shared<VersionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider, SERIAL_NUMBER);

    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

// ============================================================================
// RevisionIntf handleResponse branches
// ============================================================================
TEST(NsmInventoryPropertyBranch, HandleResponse_RevisionIntf_InfoRomVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_rev_ir";
    auto intf = std::make_shared<RevisionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<RevisionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<RevisionIntf> invProp(provider, INFO_ROM_VERSION);

    std::string ver = "2.1.0";
    auto buf = buildInvResponse(reinterpret_cast<const uint8_t*>(ver.data()),
                                ver.size());
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->version(), ver);
}

TEST(NsmInventoryPropertyBranch, HandleResponse_RevisionIntf_Default_Throws)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_rev_def";
    auto intf = std::make_shared<RevisionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<RevisionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<RevisionIntf> invProp(provider, SERIAL_NUMBER);

    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

// ============================================================================
// NsmMNNVLinkTopologyIntf handleResponse branches
// ============================================================================
TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_GpuIbguid)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_ibg";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider, GPU_IBGUID);

    Response data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_ChassisSerialValid)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_csv";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(
        provider, CHASSIS_SERIAL_NUMBER);

    Response data = {'A', 'B', 'C', '1', '2', '3'};
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->chassisSerialNumber(), "ABC123");
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_ChassisSerialInvalid)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_csi";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(
        provider, CHASSIS_SERIAL_NUMBER);

    // Invalid UTF-8 byte
    Response data = {0x80};
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->chassisSerialNumber(), "");
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_TraySlotNumber)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_tsn";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          TRAY_SLOT_NUMBER);

    uint8_t data[4] = {5, 0, 0, 0};
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->traySlotNumber(), 5u);
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_TraySlotIndex)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_tsi";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          TRAY_SLOT_INDEX);

    uint8_t data[4] = {2, 0, 0, 0};
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->traySlotIndex(), 2u);
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_GpuHostId)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_hid";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_HOST_ID);

    uint8_t data[4] = {3, 0, 0, 0}; // 0-based → 3+1=4
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->hostID(), 4u);
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_GpuModuleId)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_mid";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_MODULE_ID);

    uint8_t data[4] = {0, 0, 0, 0}; // 0-based → 0+1=1
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->moduleID(), 1u);
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_PeerTypeDirect)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_ptd";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_NVLINK_PEER_TYPE);

    uint8_t data[4] = {NSM_PEER_TYPE_DIRECT, 0, 0, 0};
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->peerType(), "Direct");
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_PeerTypeBridge)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_ptb";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_NVLINK_PEER_TYPE);

    uint8_t data[4] = {99, 0, 0, 0}; // non-DIRECT → "Bridge"
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->peerType(), "Bridge");
}

TEST(NsmInventoryPropertyBranch,
     HandleResponse_NsmMNNVLinkTopologyIntf_Default_Throws)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_nvl_def";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(branchBus,
                                                          path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          SERIAL_NUMBER);

    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

// ============================================================================
// VersionIntf: retimer 0 (already tested elsewhere but via handleResponseMsg)
// ============================================================================
TEST(NsmInventoryPropertyBranch,
     HandleResponse_VersionIntf_PcieRetimer0_ViaHandleResponseMsg)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/branch_ver_rt0_msg";
    auto intf = std::make_shared<VersionIntf>(branchBus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_0_EEPROM_VERSION);

    // data[0]=3, data[2]=5, data[4]=0, data[6]=12 → "3.5.12"
    uint8_t rawData[8] = {3, 0, 5, 0, 0, 0, 12, 0};
    auto buf = buildInvResponse(rawData, 8);
    auto rc = invProp.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                        buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->version(), "3.5.12");
}
