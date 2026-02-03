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
