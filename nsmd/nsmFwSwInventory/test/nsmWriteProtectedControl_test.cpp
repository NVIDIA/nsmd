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
#include "diagnostics.h"

#define private public
#define protected public

#include "nsmWriteProtectedControl.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmWriteProtectedControl, Constructor)
{
    std::filesystem::path path = "/xyz/openbmc_project/software/test";
    std::string name = "WriteProtected";
    std::string type = "NSM_WriteProtected";

    auto settingsIntf = std::make_shared<SettingsIntf>(bus, path.c_str());
    NsmInterfaceProvider<SettingsIntf> provider(name, type, path, settingsIntf);

    diagnostics_enable_disable_wp_data_index dataIndex = GPU_1_4_SPI_FLASH;

    NsmWriteProtectedControl wpControl(provider, dataIndex);

    EXPECT_EQ(wpControl.getName(), name);
    EXPECT_EQ(wpControl.getType(), type);
    EXPECT_EQ(wpControl.dataIndex, dataIndex);
}

TEST(NsmWriteProtectedControl, GenRequestMsg)
{
    std::filesystem::path path = "/xyz/openbmc_project/software/test";
    std::string name = "WriteProtected";
    std::string type = "NSM_WriteProtected";

    auto settingsIntf = std::make_shared<SettingsIntf>(bus, path.c_str());
    NsmInterfaceProvider<SettingsIntf> provider(name, type, path, settingsIntf);

    diagnostics_enable_disable_wp_data_index dataIndex = GPU_1_4_SPI_FLASH;

    NsmWriteProtectedControl wpControl(provider, dataIndex);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = wpControl.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
}

TEST(NsmWriteProtectedControl, HandleResponseSuccess)
{
    std::filesystem::path path = "/xyz/openbmc_project/software/test";
    std::string name = "WriteProtected";
    std::string type = "NSM_WriteProtected";

    auto settingsIntf = std::make_shared<SettingsIntf>(bus, path.c_str());
    NsmInterfaceProvider<SettingsIntf> provider(name, type, path, settingsIntf);

    diagnostics_enable_disable_wp_data_index dataIndex = GPU_1_4_SPI_FLASH;

    NsmWriteProtectedControl wpControl(provider, dataIndex);

    // Create mock response
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_fpga_diagnostics_settings_wp_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    nsm_fpga_diagnostics_settings_wp data = {};
    data.gpu1_4 = 1; // Write protected

    uint8_t rc = encode_get_fpga_diagnostics_settings_wp_resp(
        0, cc, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = wpControl.handleResponse(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify value was set
    EXPECT_EQ(settingsIntf->writeProtected(), true);
}

TEST(NsmWriteProtectedControl, HandleResponseError)
{
    std::filesystem::path path = "/xyz/openbmc_project/software/test";
    std::string name = "WriteProtected";
    std::string type = "NSM_WriteProtected";

    auto settingsIntf = std::make_shared<SettingsIntf>(bus, path.c_str());
    NsmInterfaceProvider<SettingsIntf> provider(name, type, path, settingsIntf);

    diagnostics_enable_disable_wp_data_index dataIndex = GPU_1_4_SPI_FLASH;

    NsmWriteProtectedControl wpControl(provider, dataIndex);

    // Create mock response with error
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_fpga_diagnostics_settings_wp_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_INVALID_RQD;
    nsm_fpga_diagnostics_settings_wp data = {};

    uint8_t rc = encode_get_fpga_diagnostics_settings_wp_resp(
        0, cc, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = wpControl.handleResponse(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmWriteProtectedControl, ConstructorWithDifferentIndex)
{
    std::filesystem::path path = "/xyz/openbmc_project/software/test";
    std::string name = "WriteProtected2";
    std::string type = "NSM_WriteProtected";

    auto settingsIntf = std::make_shared<SettingsIntf>(bus, path.c_str());
    NsmInterfaceProvider<SettingsIntf> provider(name, type, path, settingsIntf);

    diagnostics_enable_disable_wp_data_index dataIndex = GPU_SPI_FLASH_1;

    NsmWriteProtectedControl wpControl(provider, dataIndex);

    EXPECT_EQ(wpControl.dataIndex, GPU_SPI_FLASH_1);
}

// handleResponse: decode failure (buffer too small) → rc != NSM_SW_SUCCESS
// → `if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)` FALSE via rc!=0
TEST(NsmWriteProtectedControl, HandleResponseDecodeFailure)
{
    std::filesystem::path path = "/xyz/openbmc_project/software/test3";
    std::string name = "WriteProtected3";
    std::string type = "NSM_WriteProtected";

    auto settingsIntf = std::make_shared<SettingsIntf>(bus, path.c_str());
    NsmInterfaceProvider<SettingsIntf> provider(name, type, path, settingsIntf);

    NsmWriteProtectedControl wpControl(provider, GPU_1_4_SPI_FLASH);

    // Buffer with NSM_ERROR CC → decode_reason_code_and_cc safely reads CC
    std::vector<uint8_t> shortBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    shortBuf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code
    auto msg = reinterpret_cast<const nsm_msg*>(shortBuf.data());

    uint8_t rc = wpControl.handleResponse(msg, shortBuf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponse: success with writeProtected = false (data.gpu1_4 = 0)
// Covers the TRUE branch of the `if` and the NsmSetWriteProtected::getValue
// returning false
TEST(NsmWriteProtectedControl, HandleResponseSuccessNotProtected)
{
    std::filesystem::path path = "/xyz/openbmc_project/software/test4";
    std::string name = "WriteProtected4";
    std::string type = "NSM_WriteProtected";

    auto settingsIntf = std::make_shared<SettingsIntf>(bus, path.c_str());
    NsmInterfaceProvider<SettingsIntf> provider(name, type, path, settingsIntf);

    NsmWriteProtectedControl wpControl(provider, GPU_1_4_SPI_FLASH);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_fpga_diagnostics_settings_wp_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    nsm_fpga_diagnostics_settings_wp data = {};
    data.gpu1_4 = 0; // Not write protected

    uint8_t rc = encode_get_fpga_diagnostics_settings_wp_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = wpControl.handleResponse(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    EXPECT_EQ(settingsIntf->writeProtected(), false);
}
