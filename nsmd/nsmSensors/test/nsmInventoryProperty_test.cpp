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
