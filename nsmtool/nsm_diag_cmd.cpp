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

// NSM: Nvidia Message type
//           - Network Ports            [Type 1]
//           - PCI links                [Type 2]
//           - Platform environments    [Type 3]
//           - Diagnostics              [Type 4]
//           - Device configuration     [Type 5]

#include "nsm_diag_cmd.hpp"

#include "base.h"
#include "diagnostics.h"

#include "cmd_helper.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>

#include <ctime>

namespace nsmtool
{

namespace diag
{

namespace
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class EnableDisableWriteProtected : public CommandInterface
{
  public:
    ~EnableDisableWriteProtected() = default;
    EnableDisableWriteProtected() = delete;
    EnableDisableWriteProtected(const EnableDisableWriteProtected&) = delete;
    EnableDisableWriteProtected(EnableDisableWriteProtected&&) = default;
    EnableDisableWriteProtected&
        operator=(const EnableDisableWriteProtected&) = delete;
    EnableDisableWriteProtected&
        operator=(EnableDisableWriteProtected&&) = default;

    using CommandInterface::CommandInterface;

    explicit EnableDisableWriteProtected(const char* type, const char* name,
                                         CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto writeProtectedGroup = app->add_option_group(
            "Required",
            "Data Index and Value for wich write protected will be set.");

        dataId = 0;
        writeProtectedGroup->add_option(
            "-d, --dataId", dataId,
            "Data Index of write protected:\n"
            "128: Retimer EEPROM\n"
            "129: Baseboard FRU EEPROM\n"
            "130: PEX SW EEPROM\n"
            "131: NVSW EEPROM (both)\n"
            "133: NVSW EEPROM 1\n"
            "134: NVSW EEPROM 2\n"
            "160: GPU 1-4 SPI Flash\n"
            "161: GPU 5-8 SPI Flash\n"
            "162-169: Individual GPU SPI flash 1-8\n"
            "176: HMC SPI Flash\n"
            "192-199: Retimer EEPROM\n"
            "232: CX7 FRU EEPROM\n"
            "233: HMC FRU EEPROM\n");
        value = 0;
        writeProtectedGroup->add_option("-V, --value", value,
                                        "Disable - 0 / Enable - 1");
        writeProtectedGroup->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_enable_disable_wp_req));
        int rc = NSM_SW_ERROR;
        switch ((diagnostics_enable_disable_wp_data_index)dataId)
        {
            case RETIMER_EEPROM:
            case BASEBOARD_FRU_EEPROM:
            case PEX_SW_EEPROM:
            case NVSW_EEPROM_BOTH:
            case NVSW_EEPROM_1:
            case NVSW_EEPROM_2:
            case GPU_1_4_SPI_FLASH:
            case GPU_5_8_SPI_FLASH:
            case GPU_SPI_FLASH_1:
            case GPU_SPI_FLASH_2:
            case GPU_SPI_FLASH_3:
            case GPU_SPI_FLASH_4:
            case GPU_SPI_FLASH_5:
            case GPU_SPI_FLASH_6:
            case GPU_SPI_FLASH_7:
            case GPU_SPI_FLASH_8:
            case HMC_SPI_FLASH:
            case RETIMER_EEPROM_1:
            case RETIMER_EEPROM_2:
            case RETIMER_EEPROM_3:
            case RETIMER_EEPROM_4:
            case RETIMER_EEPROM_5:
            case RETIMER_EEPROM_6:
            case RETIMER_EEPROM_7:
            case RETIMER_EEPROM_8:
            case CX7_FRU_EEPROM:
            case HMC_FRU_EEPROM:
            {
                auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
                rc = encode_enable_disable_wp_req(
                    instanceId,
                    (diagnostics_enable_disable_wp_data_index)dataId, value,
                    request);
            }
            break;
            default:
                std::cerr << "Invalid Data Id \n";
                break;
        }
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_enable_disable_wp_resp(responsePtr, payloadLength, &cc,
                                                &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) +
                          sizeof(nsm_enable_disable_wp_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t dataId;
    uint8_t value;
};
void registerCommand(CLI::App& app)
{
    auto diag = app.add_subcommand("diag", "Diagnostics type command");
    diag->require_subcommand(1);

    auto enableDisableWriteProtected = diag->add_subcommand(
        "EnableDisableWriteProtected", "Enable/Disable WriteProtected");
    commands.push_back(std::make_unique<EnableDisableWriteProtected>(
        "diag", "EnableDisableWriteProtected", enableDisableWriteProtected));
}

} // namespace diag

} // namespace nsmtool
