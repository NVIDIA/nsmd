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
using namespace ::testing;

#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmInventoryProperty.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmInventoryPropertyBase, Constructor)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "InventoryProperty";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    nsm_inventory_property_identifiers property = SERIAL_NUMBER;

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, property);

    EXPECT_EQ(invProp.getName(), name);
    EXPECT_EQ(invProp.getType(), type);
    EXPECT_EQ(invProp.property, SERIAL_NUMBER);
}

TEST(NsmInventoryPropertyBase, GenRequestMsg)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "InventoryProperty";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BOARD_PART_NUMBER);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = invProp.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));
}

TEST(NsmInventoryPropertyBase, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device_bad";
    std::string name = "InventoryPropertyBad";
    std::string type = "NSM_InventoryPropertyBad";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BOARD_PART_NUMBER);

    auto request = invProp.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmInventoryProperty, HandleResponseSerialNumber)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "SerialNumber";
    std::string type = "NSM_SerialNumber";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    // Create mock response
    std::string serialNumber = "SN123456789";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            serialNumber.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, serialNumber.size(), (uint8_t*)serialNumber.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify serial number was set
    EXPECT_EQ(assetIntf->serialNumber(), serialNumber);
}

TEST(NsmInventoryProperty, HandleResponsePartNumber)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "PartNumber";
    std::string type = "NSM_PartNumber";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BOARD_PART_NUMBER);

    // Create mock response
    std::string partNumber = "PN900-12345-0000";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            partNumber.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, partNumber.size(), (uint8_t*)partNumber.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify part number was set
    EXPECT_EQ(assetIntf->partNumber(), partNumber);
}

TEST(NsmInventoryProperty, HandleResponseDimension)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "Dimension";
    std::string type = "NSM_Dimension";

    auto dimIntf = std::make_shared<DimensionIntf>(bus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider(name, type, path, dimIntf);

    NsmInventoryProperty<DimensionIntf> invProp(provider, PRODUCT_HEIGHT);

    // Create mock response with height value (100)
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t heightData[4] = {100, 0, 0, 0}; // Little endian: 100

    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       heightData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify height was set
    EXPECT_EQ(dimIntf->height(), 100);
}

TEST(NsmInventoryProperty, HandleResponsePowerLimit)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "PowerLimit";
    std::string type = "NSM_PowerLimit";

    auto powerLimitIntf = std::make_shared<PowerLimitIntf>(bus, path.c_str());
    NsmInterfaceProvider<PowerLimitIntf> provider(name, type, path,
                                                  powerLimitIntf);

    NsmInventoryProperty<PowerLimitIntf> invProp(provider,
                                                 MAXIMUM_DEVICE_POWER_LIMIT);

    // Create mock response with max power (700000 mW = 700W)
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint32_t powerValue = 700000; // 700W in mW
    uint8_t powerData[4] = {
        uint8_t(powerValue & 0xFF), uint8_t((powerValue >> 8) & 0xFF),
        uint8_t((powerValue >> 16) & 0xFF), uint8_t((powerValue >> 24) & 0xFF)};

    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       powerData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify max power was set (converted from mW to W)
    EXPECT_EQ(powerLimitIntf->maxPowerWatts(), 700);
}

TEST(NsmInventoryProperty, HandleResponseError)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "InventoryProperty";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    // Create mock response with error
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_INVALID_RQD;

    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 0,
                                                       nullptr, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmInventoryProperty, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/decode";
    std::string name = "InventoryProperty";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    // 7-byte buffer: safely reads cc=0 at payload[1] but too short for
    // decode_get_inventory_information_resp which calls decode_common_resp
    // requiring at least sizeof(nsm_msg_hdr)+sizeof(nsm_common_resp)=11 bytes
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = invProp.handleResponseMsg(response, responseMsg.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmInventoryPropertyBase, ConstructorWithVariousProperties)
{
    // Test to explicitly cover base class constructor lines 25-28
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "TestInventoryProperty";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    // Test with different property identifiers to ensure constructor is covered
    std::vector<nsm_inventory_property_identifiers> properties = {
        SERIAL_NUMBER, MARKETING_NAME, DEVICE_PART_NUMBER, BOARD_PART_NUMBER};

    for (const auto& prop : properties)
    {
        NsmInventoryProperty<NsmAssetIntf> invProp(provider, prop);
        EXPECT_EQ(invProp.property, prop);
        EXPECT_EQ(invProp.getName(), name);
    }
}

// ============================================================================
// default: branch coverage — each template specialization throws
// std::runtime_error("Not implemented PDI") when an unsupported property is
// passed.  Access is possible because the test file sets
// #define protected public.
// ============================================================================

TEST(NsmInventoryProperty, HandleResponse_NsmAssetIntf_DefaultThrows)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/asset_default";
    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, assetIntf);

    // ASSET_TAG is not in the NsmAssetIntf switch → default: throw
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, ASSET_TAG);
    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

TEST(NsmInventoryProperty, HandleResponse_AssetTagIntf_DefaultThrows)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/assettag_default";
    auto intf = std::make_shared<AssetTagIntf>(bus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider("a", "t", path, intf);

    // SERIAL_NUMBER is not in the AssetTagIntf switch → default: throw
    NsmInventoryProperty<AssetTagIntf> invProp(provider, SERIAL_NUMBER);
    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

TEST(NsmInventoryProperty, HandleResponse_DimensionIntf_DefaultThrows)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/dimension_default";
    auto intf = std::make_shared<DimensionIntf>(bus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider("a", "t", path, intf);

    // SERIAL_NUMBER is not in the DimensionIntf switch → default: throw
    NsmInventoryProperty<DimensionIntf> invProp(provider, SERIAL_NUMBER);
    Response data = {0, 0, 0, 0};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

TEST(NsmInventoryProperty, HandleResponse_PowerLimitIntf_DefaultThrows)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/powerlimit_default";
    auto intf = std::make_shared<PowerLimitIntf>(bus, path.c_str());
    NsmInterfaceProvider<PowerLimitIntf> provider("a", "t", path, intf);

    // SERIAL_NUMBER is not in the PowerLimitIntf switch → default: throw
    NsmInventoryProperty<PowerLimitIntf> invProp(provider, SERIAL_NUMBER);
    Response data = {0, 0, 0, 0};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

TEST(NsmInventoryProperty, HandleResponse_VersionIntf_DefaultThrows)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/version_default";
    auto intf = std::make_shared<VersionIntf>(bus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);

    // SERIAL_NUMBER is not in the VersionIntf switch → default: throw
    NsmInventoryProperty<VersionIntf> invProp(provider, SERIAL_NUMBER);
    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

TEST(NsmInventoryProperty, HandleResponse_RevisionIntf_DefaultThrows)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/revision_default";
    auto intf = std::make_shared<RevisionIntf>(bus, path.c_str());
    NsmInterfaceProvider<RevisionIntf> provider("a", "t", path, intf);

    // SERIAL_NUMBER is not in the RevisionIntf switch → default: throw
    NsmInventoryProperty<RevisionIntf> invProp(provider, SERIAL_NUMBER);
    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

TEST(NsmInventoryProperty, HandleResponse_NsmMNNVLinkTopologyIntf_DefaultThrows)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_default";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);

    // SERIAL_NUMBER is not in the NsmMNNVLinkTopologyIntf switch → default:
    // throw
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          SERIAL_NUMBER);
    Response data = {'X'};
    EXPECT_THROW(invProp.handleResponse(data), std::runtime_error);
}

// AssetTagIntf with empty data → if (assetTag.empty()) TRUE branch → invokes ""
TEST(NsmInventoryProperty, HandleResponse_AssetTagIntf_EmptyTag)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/assettag_empty";
    auto intf = std::make_shared<AssetTagIntf>(bus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<AssetTagIntf> invProp(provider, ASSET_TAG);
    Response data = {}; // empty → assetTag.empty() TRUE → invoke(pdiMethod, "")
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

// VersionIntf with PCIERETIMER_0_EEPROM_VERSION → covers the case body
// (builds version string from data bytes 0, 2, 4, 6)
TEST(NsmInventoryProperty, HandleResponse_VersionIntf_PcieRetimer0EepromVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/version_retimer";
    auto intf = std::make_shared<VersionIntf>(bus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_0_EEPROM_VERSION);
    // data[0]=1, data[2]=2, data[4]=0, data[6]=3 → "1.2.3"
    Response data(8, 0);
    data[0] = 1;
    data[2] = 2;
    data[4] = 0;
    data[6] = 3;
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

// RevisionIntf with INFO_ROM_VERSION → covers the case body
TEST(NsmInventoryProperty, HandleResponse_RevisionIntf_InfoRomVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/revision_inforom";
    auto intf = std::make_shared<RevisionIntf>(bus, path.c_str());
    NsmInterfaceProvider<RevisionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<RevisionIntf> invProp(provider, INFO_ROM_VERSION);
    Response data = {'1', '.', '0'};
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

// NsmMNNVLinkTopologyIntf GPU_IBGUID → covers GPU_IBGUID case
TEST(NsmInventoryProperty, HandleResponse_NsmMNNVLinkTopologyIntf_GpuIbguid)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_ibguid";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider, GPU_IBGUID);
    Response data = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

// NsmMNNVLinkTopologyIntf CHASSIS_SERIAL_NUMBER with valid UTF-8 →
// isValid == true → invoke(pdiMethod, chassisSerialNumber) (TRUE branch L255)
TEST(NsmInventoryProperty,
     HandleResponse_NsmMNNVLinkTopologyIntf_ChassisSerialValidUtf8)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_serial_valid";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(
        provider, CHASSIS_SERIAL_NUMBER);
    Response data = {'S', 'N', '1', '2', '3'}; // valid ASCII/UTF-8
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

// NsmMNNVLinkTopologyIntf CHASSIS_SERIAL_NUMBER with invalid UTF-8 →
// isValid == false → invoke(pdiMethod, "") (FALSE/else branch L260)
TEST(NsmInventoryProperty,
     HandleResponse_NsmMNNVLinkTopologyIntf_ChassisSerialInvalidUtf8)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_serial_invalid";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(
        provider, CHASSIS_SERIAL_NUMBER);
    Response data = {0x80}; // invalid UTF-8 → isValidDbusString returns false
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc!=NSM_SUCCESS → FALSE branch.
// 9-byte buffer: decode_reason_code_and_cc returns NSM_SW_SUCCESS with
// cc=NSM_ERROR → handleResponse NOT called, cc returned.
TEST(NsmInventoryProperty, HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsCC)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/cc_err";
    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("inv", "NSM_Inv", path,
                                                assetIntf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, SERIAL_NUMBER);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    uint8_t rc = invProp.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmAssetIntf — uncovered switch cases
// ============================================================================

TEST(NsmInventoryProperty, HandleResponse_NsmAssetIntf_MarketingName)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/asset_marketing";
    auto intf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, MARKETING_NAME);
    Response data = {'G', 'P', 'U', ' ', 'A', '1', '0', '0'};
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->model(), "GPU A100");
}

TEST(NsmInventoryProperty, HandleResponse_NsmAssetIntf_DevicePartNumber)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/asset_devpn";
    auto intf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, DEVICE_PART_NUMBER);
    Response data = {'D', 'P', 'N', '-', '1', '2', '3'};
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->partNumber(), "DPN-123");
}

TEST(NsmInventoryProperty, HandleResponse_NsmAssetIntf_FruPartNumber)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/asset_frupn";
    auto intf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, FRU_PART_NUMBER);
    Response data = {'F', 'R', 'U', '-', '4', '5', '6'};
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->partNumber(), "FRU-456");
}

// BUILD_DATE "0" → nullDate branch (TRUE)
TEST(NsmInventoryProperty, HandleResponse_NsmAssetIntf_BuildDate_Zero)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/asset_builddate_zero";
    auto intf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BUILD_DATE);
    Response data = {'0'};
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->buildDate(), nullDate);
}

// BUILD_DATE non-"0" → use actual date string branch (FALSE)
TEST(NsmInventoryProperty, HandleResponse_NsmAssetIntf_BuildDate_NonZero)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/asset_builddate_real";
    auto intf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BUILD_DATE);
    std::string date = "2024-01-15";
    Response data(date.begin(), date.end());
    EXPECT_NO_THROW(invProp.handleResponse(data));
    EXPECT_EQ(intf->buildDate(), date);
}

// ============================================================================
// DimensionIntf — uncovered switch cases
// ============================================================================

TEST(NsmInventoryProperty, HandleResponse_DimensionIntf_ProductLength)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/dim_length";
    auto intf = std::make_shared<DimensionIntf>(bus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<DimensionIntf> invProp(provider, PRODUCT_LENGTH);
    uint8_t data[4] = {200, 0, 0, 0}; // little-endian 200
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->depth(), 200u);
}

TEST(NsmInventoryProperty, HandleResponse_DimensionIntf_ProductWidth)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/dim_width";
    auto intf = std::make_shared<DimensionIntf>(bus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<DimensionIntf> invProp(provider, PRODUCT_WIDTH);
    uint8_t data[4] = {150, 0, 0, 0};
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->width(), 150u);
}

// ============================================================================
// PowerLimitIntf — uncovered switch case
// ============================================================================

TEST(NsmInventoryProperty, HandleResponse_PowerLimitIntf_MinPowerLimit)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/pwr_min";
    auto intf = std::make_shared<PowerLimitIntf>(bus, path.c_str());
    NsmInterfaceProvider<PowerLimitIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<PowerLimitIntf> invProp(provider,
                                                 MINIMUM_DEVICE_POWER_LIMIT);
    uint32_t mw = 100000; // 100W in mW
    uint8_t data[4] = {uint8_t(mw & 0xFF), uint8_t((mw >> 8) & 0xFF),
                       uint8_t((mw >> 16) & 0xFF), uint8_t((mw >> 24) & 0xFF)};
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->minPowerWatts(), 100u);
}

// ============================================================================
// VersionIntf — remaining PCIe retimer cases (1-7 all use same code path)
// ============================================================================

TEST(NsmInventoryProperty, HandleResponse_VersionIntf_PcieRetimer1EepromVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/version_retimer1";
    auto intf = std::make_shared<VersionIntf>(bus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_1_EEPROM_VERSION);
    Response data(8, 0);
    data[0] = 2;
    data[2] = 0;
    data[4] = 0;
    data[6] = 1;
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

TEST(NsmInventoryProperty, HandleResponse_VersionIntf_PcieRetimer4EepromVersion)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/version_retimer4";
    auto intf = std::make_shared<VersionIntf>(bus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider("a", "t", path, intf);
    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_4_EEPROM_VERSION);
    Response data(8, 0);
    data[0] = 3;
    data[2] = 1;
    data[4] = 0;
    data[6] = 5;
    EXPECT_NO_THROW(invProp.handleResponse(data));
}

// ============================================================================
// NsmMNNVLinkTopologyIntf — uncovered switch cases
// ============================================================================

TEST(NsmInventoryProperty,
     HandleResponse_NsmMNNVLinkTopologyIntf_TraySlotNumber)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_trayslot";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          TRAY_SLOT_NUMBER);
    uint8_t data[4] = {3, 0, 0, 0}; // slot 3
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->traySlotNumber(), 3u);
}

TEST(NsmInventoryProperty, HandleResponse_NsmMNNVLinkTopologyIntf_TraySlotIndex)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_trayidx";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          TRAY_SLOT_INDEX);
    uint8_t data[4] = {1, 0, 0, 0};
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->traySlotIndex(), 1u);
}

TEST(NsmInventoryProperty, HandleResponse_NsmMNNVLinkTopologyIntf_GpuHostId)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_hostid";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_HOST_ID);
    uint8_t data[4] = {0, 0, 0, 0}; // 0-based → invokes 0+1=1
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->hostID(), 1u);
}

TEST(NsmInventoryProperty, HandleResponse_NsmMNNVLinkTopologyIntf_GpuModuleId)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_moduleid";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_MODULE_ID);
    uint8_t data[4] = {2, 0, 0, 0}; // 0-based → invokes 2+1=3
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->moduleID(), 3u);
}

// GPU_NVLINK_PEER_TYPE == NSM_PEER_TYPE_DIRECT (0) → "Direct"
TEST(NsmInventoryProperty,
     HandleResponse_NsmMNNVLinkTopologyIntf_PeerTypeDirect)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_peer_direct";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_NVLINK_PEER_TYPE);
    uint8_t data[4] = {NSM_PEER_TYPE_DIRECT, 0, 0, 0};
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->peerType(), "Direct");
}

// GPU_NVLINK_PEER_TYPE != NSM_PEER_TYPE_DIRECT → "Bridge"
TEST(NsmInventoryProperty,
     HandleResponse_NsmMNNVLinkTopologyIntf_PeerTypeBridge)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/nvlink_peer_bridge";
    auto intf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider("a", "t", path,
                                                           intf);
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_NVLINK_PEER_TYPE);
    uint8_t data[4] = {1, 0, 0, 0}; // non-zero → "Bridge"
    Response resp(data, data + 4);
    EXPECT_NO_THROW(invProp.handleResponse(resp));
    EXPECT_EQ(intf->peerType(), "Bridge");
}
