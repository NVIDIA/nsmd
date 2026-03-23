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
 * nsmDeepCoverage_batch8.cpp
 *
 * Deep coverage tests targeting:
 * 1. nsmInventoryProperty.hpp - Uncovered template specializations
 *    (AssetTagIntf, VersionIntf, RevisionIntf, NsmMNNVLinkTopologyIntf,
 *     additional NsmAssetIntf branches, DimensionIntf remaining branches,
 *     PowerLimitIntf remaining branches, default/throw branches)
 * 2. nsmDevice.cpp - invokeLongRunningHandler with valid handler, additional
 *    findAggregatorByType scenarios, updateDiscoveryIdentifiers edge cases
 * 3. nsmChassisPCIeDevice.cpp - Standalone factory function tests
 *    (createChassisPCIeDeviceAsset, createChassisPCIeDeviceHealth,
 *     createChassisPCIeDevicePCIeDevice,
 * createChassisPCIeDeviceMultiPortPCIeDevice)
 * 4. nsmProcessor.cpp - Additional factory branch coverage
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "commonMock.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmChassis/nsmChassisPCIeDevice.hpp"
#include "nsmChassis/nsmPCIeFunction.hpp"
#include "nsmDevice.hpp"
#include "nsmEvent/nsmLongRunningEvent.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmMNNVLinkTopologyIntf.hpp"
#include "nsmNumericSensor/nsmNumericAggregator.hpp"
#include "nsmPCIeLinkSpeed.hpp"

#undef private
#undef protected

using namespace nsm;

static auto bus = sdbusplus::bus::new_default();

// ===========================================================================
// Section 1: NsmInventoryProperty template specialization coverage
// ===========================================================================

// ---------------------------------------------------------------------------
// NsmAssetIntf: DEVICE_PART_NUMBER branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_NsmAssetIntf_DevicePartNumber)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_dpn";
    std::string name = "DevPartNum";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, DEVICE_PART_NUMBER);

    std::string partNumber = "GPU-900-2G133-0020-000";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            partNumber.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, partNumber.size(), (uint8_t*)partNumber.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(assetIntf->partNumber(), partNumber);
}

// ---------------------------------------------------------------------------
// NsmAssetIntf: FRU_PART_NUMBER branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_NsmAssetIntf_FruPartNumber)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_fpn";
    std::string name = "FruPartNum";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, FRU_PART_NUMBER);

    std::string fruPart = "FRU-900-2G133-0020";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            fruPart.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, fruPart.size(), (uint8_t*)fruPart.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(assetIntf->partNumber(), fruPart);
}

// ---------------------------------------------------------------------------
// NsmAssetIntf: MARKETING_NAME branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_NsmAssetIntf_MarketingName)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mkt";
    std::string name = "MarketingName";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, MARKETING_NAME);

    std::string marketingName = "NVIDIA H100 SXM5 80GB";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            marketingName.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, marketingName.size(),
        (uint8_t*)marketingName.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(assetIntf->model(), marketingName);
}

// ---------------------------------------------------------------------------
// NsmAssetIntf: BUILD_DATE branch with non-zero value
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_NsmAssetIntf_BuildDateNonZero)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_bd1";
    std::string name = "BuildDate";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BUILD_DATE);

    // Verify constructor sets buildDate to nullDate
    EXPECT_EQ(assetIntf->buildDate(), "0000-00-00T00:00:00Z");

    std::string dateStr = "2024-01-15";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            dateStr.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, dateStr.size(), (uint8_t*)dateStr.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(assetIntf->buildDate(), dateStr);
}

// ---------------------------------------------------------------------------
// NsmAssetIntf: BUILD_DATE branch with "0" value -> should be nullDate
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_NsmAssetIntf_BuildDateZero)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_bd0";
    std::string name = "BuildDateZero";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    NsmInventoryProperty<NsmAssetIntf> invProp(provider, BUILD_DATE);

    std::string dateStr = "0";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            dateStr.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, dateStr.size(), (uint8_t*)dateStr.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(assetIntf->buildDate(), "0000-00-00T00:00:00Z");
}

// ---------------------------------------------------------------------------
// NsmAssetIntf: default branch -> throws runtime_error
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_NsmAssetIntf_DefaultThrows)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_defthrow";
    std::string name = "DefaultThrow";
    std::string type = "NSM_InventoryProperty";

    auto assetIntf = std::make_shared<NsmAssetIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmAssetIntf> provider(name, type, path, assetIntf);

    // Use ASSET_TAG which is not handled by NsmAssetIntf specialization
    NsmInventoryProperty<NsmAssetIntf> invProp(provider, ASSET_TAG);

    std::string data = "SomeAssetTag";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            data.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, data.size(), (uint8_t*)data.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act & Assert: handleResponseMsg calls handleResponse which throws
    EXPECT_THROW(invProp.handleResponseMsg(response, responseMsg.size()),
                 std::runtime_error);
}

// ---------------------------------------------------------------------------
// AssetTagIntf: ASSET_TAG branch with non-empty value
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_AssetTagIntf_NonEmpty)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_at1";
    std::string name = "AssetTag";
    std::string type = "NSM_InventoryProperty";

    auto assetTagIntf = std::make_shared<AssetTagIntf>(bus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider(name, type, path, assetTagIntf);

    NsmInventoryProperty<AssetTagIntf> invProp(provider, ASSET_TAG);

    std::string assetTag = "TAG-12345-ABCDEF";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            assetTag.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, assetTag.size(), (uint8_t*)assetTag.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(assetTagIntf->assetTag(), assetTag);
}

// ---------------------------------------------------------------------------
// AssetTagIntf: ASSET_TAG branch with empty value
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_AssetTagIntf_Empty)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_at0";
    std::string name = "AssetTagEmpty";
    std::string type = "NSM_InventoryProperty";

    auto assetTagIntf = std::make_shared<AssetTagIntf>(bus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider(name, type, path, assetTagIntf);

    NsmInventoryProperty<AssetTagIntf> invProp(provider, ASSET_TAG);

    // Empty string
    std::string assetTag = "";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            assetTag.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, assetTag.size(), (uint8_t*)assetTag.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(assetTagIntf->assetTag(), "");
}

// ---------------------------------------------------------------------------
// AssetTagIntf: default branch -> throws
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_AssetTagIntf_DefaultThrows)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_atdef";
    std::string name = "AssetTagDefault";
    std::string type = "NSM_InventoryProperty";

    auto assetTagIntf = std::make_shared<AssetTagIntf>(bus, path.c_str());
    NsmInterfaceProvider<AssetTagIntf> provider(name, type, path, assetTagIntf);

    // Use SERIAL_NUMBER which is not handled by AssetTagIntf
    NsmInventoryProperty<AssetTagIntf> invProp(provider, SERIAL_NUMBER);

    std::string data = "SomeSerial";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            data.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, data.size(), (uint8_t*)data.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act & Assert
    EXPECT_THROW(invProp.handleResponseMsg(response, responseMsg.size()),
                 std::runtime_error);
}

// ---------------------------------------------------------------------------
// DimensionIntf: PRODUCT_LENGTH (depth) branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_DimensionIntf_ProductLength)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_dim_l";
    std::string name = "DimLength";
    std::string type = "NSM_Dimension";

    auto dimIntf = std::make_shared<DimensionIntf>(bus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider(name, type, path, dimIntf);

    NsmInventoryProperty<DimensionIntf> invProp(provider, PRODUCT_LENGTH);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t lengthData[4] = {200, 0, 0, 0}; // Little endian: 200

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       lengthData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(dimIntf->depth(), 200u);
}

// ---------------------------------------------------------------------------
// DimensionIntf: PRODUCT_WIDTH branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_DimensionIntf_ProductWidth)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_dim_w";
    std::string name = "DimWidth";
    std::string type = "NSM_Dimension";

    auto dimIntf = std::make_shared<DimensionIntf>(bus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider(name, type, path, dimIntf);

    NsmInventoryProperty<DimensionIntf> invProp(provider, PRODUCT_WIDTH);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t widthData[4] = {150, 0, 0, 0}; // Little endian: 150

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       widthData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(dimIntf->width(), 150u);
}

// ---------------------------------------------------------------------------
// DimensionIntf: default branch -> throws
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_DimensionIntf_DefaultThrows)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_dim_def";
    std::string name = "DimDefault";
    std::string type = "NSM_Dimension";

    auto dimIntf = std::make_shared<DimensionIntf>(bus, path.c_str());
    NsmInterfaceProvider<DimensionIntf> provider(name, type, path, dimIntf);

    // Use SERIAL_NUMBER which is not handled by DimensionIntf
    NsmInventoryProperty<DimensionIntf> invProp(provider, SERIAL_NUMBER);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t data[4] = {1, 0, 0, 0};

    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act & Assert
    EXPECT_THROW(invProp.handleResponseMsg(response, responseMsg.size()),
                 std::runtime_error);
}

// ---------------------------------------------------------------------------
// PowerLimitIntf: MINIMUM_DEVICE_POWER_LIMIT branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_PowerLimitIntf_MinPowerLimit)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_plmin";
    std::string name = "MinPowerLimit";
    std::string type = "NSM_PowerLimit";

    auto powerLimitIntf = std::make_shared<PowerLimitIntf>(bus, path.c_str());
    NsmInterfaceProvider<PowerLimitIntf> provider(name, type, path,
                                                  powerLimitIntf);

    NsmInventoryProperty<PowerLimitIntf> invProp(provider,
                                                 MINIMUM_DEVICE_POWER_LIMIT);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint32_t powerValue = 200000; // 200W in mW
    uint8_t powerData[4] = {
        uint8_t(powerValue & 0xFF), uint8_t((powerValue >> 8) & 0xFF),
        uint8_t((powerValue >> 16) & 0xFF), uint8_t((powerValue >> 24) & 0xFF)};

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       powerData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(powerLimitIntf->minPowerWatts(), 200u);
}

// ---------------------------------------------------------------------------
// PowerLimitIntf: default branch -> throws
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_PowerLimitIntf_DefaultThrows)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_pldef";
    std::string name = "PowerLimitDefault";
    std::string type = "NSM_PowerLimit";

    auto powerLimitIntf = std::make_shared<PowerLimitIntf>(bus, path.c_str());
    NsmInterfaceProvider<PowerLimitIntf> provider(name, type, path,
                                                  powerLimitIntf);

    // Use SERIAL_NUMBER which is not handled by PowerLimitIntf
    NsmInventoryProperty<PowerLimitIntf> invProp(provider, SERIAL_NUMBER);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t data[4] = {1, 0, 0, 0};

    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act & Assert
    EXPECT_THROW(invProp.handleResponseMsg(response, responseMsg.size()),
                 std::runtime_error);
}

// ---------------------------------------------------------------------------
// VersionIntf: PCIERETIMER_0_EEPROM_VERSION branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_VersionIntf_RetimerEepromVersion)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_ver0";
    std::string name = "RetimerVersion";
    std::string type = "NSM_Version";

    auto versionIntf = std::make_shared<VersionIntf>(bus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider(name, type, path, versionIntf);

    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_0_EEPROM_VERSION);

    // Retimer EEPROM version is encoded as:
    // data[0].data[2].((data[4]<<8)|data[6]) We need at least 7 bytes in data
    std::vector<uint8_t> versionData = {1, 0, 2, 0, 0, 0, 3};
    // Expected string: "1.2.3"

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            versionData.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, versionData.size(), versionData.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(versionIntf->version(), "1.2.3");
}

// ---------------------------------------------------------------------------
// VersionIntf: PCIERETIMER_3_EEPROM_VERSION branch (different retimer index)
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_VersionIntf_Retimer3EepromVersion)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_ver3";
    std::string name = "Retimer3Version";
    std::string type = "NSM_Version";

    auto versionIntf = std::make_shared<VersionIntf>(bus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider(name, type, path, versionIntf);

    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_3_EEPROM_VERSION);

    // version: data[0]=5, data[2]=10, data[4]=0, data[6]=20
    // Expected: "5.10.20"
    std::vector<uint8_t> versionData = {5, 0, 10, 0, 0, 0, 20};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            versionData.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, versionData.size(), versionData.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(versionIntf->version(), "5.10.20");
}

// ---------------------------------------------------------------------------
// VersionIntf: PCIERETIMER_7_EEPROM_VERSION branch (highest retimer index)
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_VersionIntf_Retimer7EepromVersion)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_ver7";
    std::string name = "Retimer7Version";
    std::string type = "NSM_Version";

    auto versionIntf = std::make_shared<VersionIntf>(bus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider(name, type, path, versionIntf);

    NsmInventoryProperty<VersionIntf> invProp(provider,
                                              PCIERETIMER_7_EEPROM_VERSION);

    // version: data[0]=2, data[2]=0, data[4]=1, data[6]=0
    // data[4]<<8 = 256, data[4]<<8 | data[6] = 256
    // Expected: "2.0.256"
    std::vector<uint8_t> versionData = {2, 0, 0, 0, 1, 0, 0};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            versionData.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, versionData.size(), versionData.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(versionIntf->version(), "2.0.256");
}

// ---------------------------------------------------------------------------
// VersionIntf: default branch -> throws
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_VersionIntf_DefaultThrows)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_verdef";
    std::string name = "VersionDefault";
    std::string type = "NSM_Version";

    auto versionIntf = std::make_shared<VersionIntf>(bus, path.c_str());
    NsmInterfaceProvider<VersionIntf> provider(name, type, path, versionIntf);

    // Use SERIAL_NUMBER which is not handled by VersionIntf
    NsmInventoryProperty<VersionIntf> invProp(provider, SERIAL_NUMBER);

    std::vector<uint8_t> versionData = {1, 0, 2, 0, 0, 0, 3};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            versionData.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, versionData.size(), versionData.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act & Assert
    EXPECT_THROW(invProp.handleResponseMsg(response, responseMsg.size()),
                 std::runtime_error);
}

// ---------------------------------------------------------------------------
// RevisionIntf: INFO_ROM_VERSION branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_RevisionIntf_InfoRomVersion)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_rev";
    std::string name = "InfoRomVersion";
    std::string type = "NSM_Revision";

    auto revisionIntf = std::make_shared<RevisionIntf>(bus, path.c_str());
    NsmInterfaceProvider<RevisionIntf> provider(name, type, path, revisionIntf);

    NsmInventoryProperty<RevisionIntf> invProp(provider, INFO_ROM_VERSION);

    std::string versionStr = "535.104.05.01";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            versionStr.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, versionStr.size(), (uint8_t*)versionStr.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(revisionIntf->version(), versionStr);
}

// ---------------------------------------------------------------------------
// RevisionIntf: default branch -> throws
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_RevisionIntf_DefaultThrows)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_revdef";
    std::string name = "RevisionDefault";
    std::string type = "NSM_Revision";

    auto revisionIntf = std::make_shared<RevisionIntf>(bus, path.c_str());
    NsmInterfaceProvider<RevisionIntf> provider(name, type, path, revisionIntf);

    // Use SERIAL_NUMBER which is not handled by RevisionIntf
    NsmInventoryProperty<RevisionIntf> invProp(provider, SERIAL_NUMBER);

    std::string data = "SomeData";
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            data.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, data.size(), (uint8_t*)data.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act & Assert
    EXPECT_THROW(invProp.handleResponseMsg(response, responseMsg.size()),
                 std::runtime_error);
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: GPU_IBGUID branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_MNNVLinkTopologyIntf_GpuIbguid)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mnibguid";
    std::string name = "GpuIbguid";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider, GPU_IBGUID);

    // IBGUID is converted via convertHexToString
    std::vector<uint8_t> ibguidData = {0xAA, 0xBB, 0xCC, 0xDD,
                                       0x11, 0x22, 0x33, 0x44};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            ibguidData.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, ibguidData.size(), ibguidData.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    // ibguid() should be set to the hex string representation
    EXPECT_FALSE(mnIntf->ibguid().empty());
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: TRAY_SLOT_NUMBER branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep,
     HandleResponse_MNNVLinkTopologyIntf_TraySlotNumber)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mntray";
    std::string name = "TraySlotNumber";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          TRAY_SLOT_NUMBER);

    uint8_t slotData[4] = {5, 0, 0, 0}; // Little endian: 5

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       slotData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(mnIntf->traySlotNumber(), 5u);
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: TRAY_SLOT_INDEX branch
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep,
     HandleResponse_MNNVLinkTopologyIntf_TraySlotIndex)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mntrayidx";
    std::string name = "TraySlotIndex";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          TRAY_SLOT_INDEX);

    uint8_t indexData[4] = {3, 0, 0, 0}; // Little endian: 3

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       indexData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(mnIntf->traySlotIndex(), 3u);
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: GPU_HOST_ID branch (1-based)
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_MNNVLinkTopologyIntf_GpuHostId)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mnhostid";
    std::string name = "GpuHostId";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_HOST_ID);

    uint8_t hostIdData[4] = {7, 0, 0,
                             0}; // Little endian: 7, will become 8 (1-based)

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       hostIdData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(mnIntf->hostID(), 8u); // 7 + 1 = 8
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: GPU_MODULE_ID branch (1-based)
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep, HandleResponse_MNNVLinkTopologyIntf_GpuModuleId)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mnmodid";
    std::string name = "GpuModuleId";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_MODULE_ID);

    uint8_t moduleIdData[4] = {2, 0, 0,
                               0}; // Little endian: 2, will become 3 (1-based)

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       moduleIdData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(mnIntf->moduleID(), 3u); // 2 + 1 = 3
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: GPU_NVLINK_PEER_TYPE branch - Direct
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep,
     HandleResponse_MNNVLinkTopologyIntf_PeerTypeDirect)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mnptdirect";
    std::string name = "PeerTypeDirect";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_NVLINK_PEER_TYPE);

    // NSM_PEER_TYPE_DIRECT = 0x00
    uint8_t peerTypeData[4] = {0, 0, 0, 0};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       peerTypeData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(mnIntf->peerType(), "Direct");
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: GPU_NVLINK_PEER_TYPE branch - Bridge
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep,
     HandleResponse_MNNVLinkTopologyIntf_PeerTypeBridge)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mnptbridge";
    std::string name = "PeerTypeBridge";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider,
                                                          GPU_NVLINK_PEER_TYPE);

    // Non-zero = Bridge
    uint8_t peerTypeData[4] = {1, 0, 0, 0};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       peerTypeData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(mnIntf->peerType(), "Bridge");
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: CHASSIS_SERIAL_NUMBER branch with valid UTF-8
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep,
     HandleResponse_MNNVLinkTopologyIntf_ChassisSerialValid)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mnchassis";
    std::string name = "ChassisSerial";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(
        provider, CHASSIS_SERIAL_NUMBER);

    std::string chassisSerial = "SN-CHASSIS-12345";

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            chassisSerial.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    // Act
    uint8_t rc = encode_get_inventory_information_resp(
        0, cc, reason_code, chassisSerial.size(),
        (uint8_t*)chassisSerial.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = invProp.handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(mnIntf->chassisSerialNumber(), chassisSerial);
}

// ---------------------------------------------------------------------------
// NsmMNNVLinkTopologyIntf: default branch -> throws
// ---------------------------------------------------------------------------
TEST(NsmInventoryPropertyDeep,
     HandleResponse_MNNVLinkTopologyIntf_DefaultThrows)
{
    // Arrange
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/deep_batch8_mndefault";
    std::string name = "MNNVLinkDefault";
    std::string type = "NSM_MNNVLinkTopology";

    auto mnIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> provider(name, type, path,
                                                           mnIntf);

    // Use BUILD_DATE which is not handled by NsmMNNVLinkTopologyIntf
    NsmInventoryProperty<NsmMNNVLinkTopologyIntf> invProp(provider, BUILD_DATE);

    uint8_t data[4] = {1, 0, 0, 0};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(0, cc, reason_code, 4,
                                                       data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act & Assert
    EXPECT_THROW(invProp.handleResponseMsg(response, responseMsg.size()),
                 std::runtime_error);
}

// ===========================================================================
// Section 2: NsmDevice deep coverage
// ===========================================================================

// Mock for NsmLongRunningEvent that tracks calls
class TestMockNsmLongRunningEvent : public nsm::NsmLongRunningEvent
{
  public:
    TestMockNsmLongRunningEvent(const std::string& name,
                                const std::string& type) :
        NsmLongRunningEvent(name, type, true), handleCallCount(0),
        lastReturnCode(NSM_SW_SUCCESS)
    {}

    int handle(eid_t /*eid*/, NsmType /*type*/, NsmEventId /*eventId*/,
               const nsm_msg* /*event*/, size_t /*eventLen*/) override
    {
        handleCallCount++;
        return lastReturnCode;
    }

    int handleCallCount;
    int lastReturnCode;
};

// Mock NsmNumericAggregator for deep tests
class TestMockNsmNumericAggregator : public nsm::NsmNumericAggregator
{
  public:
    TestMockNsmNumericAggregator(const std::string& name,
                                 const std::string& type, bool priority) :
        NsmNumericAggregator(name, type, priority)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        return std::vector<uint8_t>{};
    }

  private:
    int handleSample(
        const nsm::NsmSensorAggregator::TelemetrySample& /*sample*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// ---------------------------------------------------------------------------
// invokeLongRunningHandler: decode_nsm_event fails (invalid event data)
// ---------------------------------------------------------------------------
TEST(NsmDeviceDeep, InvokeLongRunningHandler_DecodeEventFails_ReturnsError)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto lrEvent = std::make_shared<TestMockNsmLongRunningEvent>("LREvt",
                                                                 "LRType");
    nsmDevice.registerLongRunningHandler(1, 2, lrEvent);

    // Act: pass nullptr event data which will cause decode to fail
    int rc = nsmDevice.invokeLongRunningHandler(0, 0, 0, nullptr, 0);

    // Assert: decode should fail, returning an error
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(lrEvent->handleCallCount, 0);
}

// ---------------------------------------------------------------------------
// findAggregatorByType: single aggregator with exact match
// ---------------------------------------------------------------------------
TEST(NsmDeviceDeep, FindAggregatorByType_SingleAggregator_ExactMatch_ReturnsIt)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto aggregator = std::make_shared<TestMockNsmNumericAggregator>(
        "OnlyAggregator", "UniqueType", false);
    nsmDevice.sensorAggregators.push_back(aggregator);

    // Act
    auto result = nsmDevice.findAggregatorByType("UniqueType");

    // Assert
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result, aggregator);
    EXPECT_EQ(result->getType(), "UniqueType");
    EXPECT_EQ(result->getName(), "OnlyAggregator");
}

// ---------------------------------------------------------------------------
// findAggregatorByType: multiple aggregators, match at end of vector
// ---------------------------------------------------------------------------
TEST(NsmDeviceDeep,
     FindAggregatorByType_MultipleAggregators_MatchAtEnd_ReturnsCorrect)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto agg1 = std::make_shared<TestMockNsmNumericAggregator>("Agg1", "TypeA",
                                                               false);
    auto agg2 = std::make_shared<TestMockNsmNumericAggregator>("Agg2", "TypeB",
                                                               true);
    auto agg3 = std::make_shared<TestMockNsmNumericAggregator>("Agg3", "TypeC",
                                                               false);

    nsmDevice.sensorAggregators.push_back(agg1);
    nsmDevice.sensorAggregators.push_back(agg2);
    nsmDevice.sensorAggregators.push_back(agg3);

    // Act
    auto result = nsmDevice.findAggregatorByType("TypeC");

    // Assert
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result, agg3);
}

// ---------------------------------------------------------------------------
// updateDiscoveryIdentifiers: preferred medium replaces existing (different
// EID)
// ---------------------------------------------------------------------------
TEST(
    NsmDeviceDeep,
    UpdateDiscoveryIdentifiers_PreferredMedium_ReplacesExistingFruDeviceManager)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.uuid = "";

    eid_t eid1 = 10;
    uuid_t uuid1 = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    std::string assocPath = "/mctp/test/path1";
    std::string medium = "xyz.openbmc_project.MCTP.Binding.MCTPoverPCIe";
    std::string binding = "xyz.openbmc_project.MCTP.Binding.PCIe";

    // First update
    bool result1 = nsmDevice.updateDiscoveryIdentifiers(
        eid1, uuid1, 1, assocPath, medium, binding, 30);
    EXPECT_TRUE(result1);
    EXPECT_EQ(nsmDevice.getEid(), eid1);
    EXPECT_EQ(nsmDevice.getUuid(), uuid1);

    // Act: second update with same medium/binding but different EID
    eid_t eid2 = 20;
    uuid_t uuid2 = "ffffffff-1111-2222-3333-444444444444";
    std::string assocPath2 = "/mctp/test/path2";

    bool result2 = nsmDevice.updateDiscoveryIdentifiers(
        eid2, uuid2, 2, assocPath2, medium, binding, 30);

    // Assert: should update since same priority medium
    EXPECT_TRUE(result2);
    EXPECT_EQ(nsmDevice.getEid(), eid2);
    EXPECT_EQ(nsmDevice.getUuid(), uuid2);
}

// ---------------------------------------------------------------------------
// sensorAggregators: push_back multiple and verify findAggregatorByType
// traverses all
// ---------------------------------------------------------------------------
TEST(NsmDeviceDeep, SensorAggregators_PushBackMultiple_FindTraversesAll)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    for (int i = 0; i < 10; i++)
    {
        auto agg = std::make_shared<TestMockNsmNumericAggregator>(
            "Agg" + std::to_string(i), "Type" + std::to_string(i), false);
        nsmDevice.sensorAggregators.push_back(agg);
    }

    // Act & Assert: find each
    for (int i = 0; i < 10; i++)
    {
        auto result =
            nsmDevice.findAggregatorByType("Type" + std::to_string(i));
        EXPECT_NE(result, nullptr) << "Failed to find Type" << i;
        EXPECT_EQ(result->getName(), "Agg" + std::to_string(i));
    }

    // Verify non-existent type returns nullptr
    auto result = nsmDevice.findAggregatorByType("NonExistentType");
    EXPECT_EQ(result, nullptr);
}

// ===========================================================================
// Section 3: NsmChassisPCIeDevice standalone factory function tests
// ===========================================================================

// Test the NsmChassisPCIeDevice constructor and interface provider construction
TEST(NsmChassisPCIeDeviceDeep, Constructor_WithChassisAndName_CreatesProvider)
{
    // Arrange & Act
    NsmChassisPCIeDevice<PCIeDeviceIntf> pcieDevice("HGX_GPU_SXM_1",
                                                    "PCIeDevice_0");

    // Assert
    EXPECT_EQ(pcieDevice.getName(), "PCIeDevice_0");
    EXPECT_EQ(pcieDevice.getType(), "NSM_ChassisPCIeDevice");
}

TEST(NsmChassisPCIeDeviceDeep, Constructor_HealthIntf_CreatesHealthProvider)
{
    // Arrange & Act
    NsmChassisPCIeDevice<HealthIntf> healthDevice("HGX_GPU_SXM_2",
                                                  "PCIeDevice_1");

    // Assert
    EXPECT_EQ(healthDevice.getName(), "PCIeDevice_1");
    EXPECT_EQ(healthDevice.getType(), "NSM_ChassisPCIeDevice");
}

TEST(NsmChassisPCIeDeviceDeep, Constructor_UuidIntf_CreatesUuidProvider)
{
    // Arrange & Act
    NsmChassisPCIeDevice<UuidIntf> uuidDevice("HGX_GPU_SXM_3", "PCIeDevice_2");

    // Assert
    EXPECT_EQ(uuidDevice.getName(), "PCIeDevice_2");
    EXPECT_EQ(uuidDevice.getType(), "NSM_ChassisPCIeDevice");
}

TEST(NsmChassisPCIeDeviceDeep,
     Constructor_AssociationIntf_CreatesAssociationProvider)
{
    // Arrange & Act
    NsmChassisPCIeDevice<AssociationDefinitionsIntf> assocDevice(
        "HGX_GPU_SXM_4", "PCIeDevice_3");

    // Assert
    EXPECT_EQ(assocDevice.getName(), "PCIeDevice_3");
    EXPECT_EQ(assocDevice.getType(), "NSM_ChassisPCIeDevice");
}
