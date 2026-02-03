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

#include "device-configuration.h"

#define private public
#define protected public

#include "nsmNetworkAdapter.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmNetworkAdapterDI, Constructor)
{
    std::string name = "NetworkAdapter0";
    std::string type = "NSM_NetworkAdapter";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/system/";

    std::vector<utils::Association> associations;
    associations.push_back({"parent_chassis", "network_adapter",
                            "/xyz/openbmc_project/inventory/system"});

    NsmNetworkAdapterDI netAdapter(bus, name, associations, type,
                                   inventoryObjPath);

    EXPECT_EQ(netAdapter.getName(), name);
    EXPECT_EQ(netAdapter.getType(), type);
    EXPECT_NE(netAdapter.associationDefIntf, nullptr);
    EXPECT_NE(netAdapter.pcieDeviceIntf, nullptr);
    EXPECT_NE(netAdapter.networkInterfaceIntf, nullptr);
}

TEST(NsmDeviceProtectionOptions, Constructor)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    EXPECT_EQ(protOptions.getName(), name);
    EXPECT_EQ(protOptions.getType(), type);
    EXPECT_EQ(protOptions.objPath, path);
    EXPECT_NE(protOptions.protectionIntf, nullptr);
    EXPECT_FALSE(protOptions.asyncPatchInProgress);
}

TEST(NsmDeviceProtectionOptions, GenRequestMsg)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = protOptions.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

TEST(NsmDeviceProtectionOptions, HandleResponseMsg)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    // Create mock response
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t protection_mode = 1; // Some protection mode

    uint8_t rc = encode_get_protection_options_resp(0, cc, reason_code,
                                                    protection_mode, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST(NsmDeviceProtectionOptions, HandleResponseMsgError)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    // Create mock response with error
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_INVALID_RQD;
    uint8_t protection_mode = 0;

    uint8_t rc = encode_get_protection_options_resp(0, cc, reason_code,
                                                    protection_mode, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}
