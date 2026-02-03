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

#include "test/mockDBusHandler.hpp"
using namespace ::testing;

#include "device-configuration.h"

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmErrorInjection.hpp"

using namespace nsm;

using Type = ErrorInjectionCapabilityIntf::Type;

TEST(GetErrorInjectionBitPosition, MemoryErrors)
{
    EXPECT_EQ(EI_MEMORY_ERRORS,
              getErrorInjectionBitPosition(Type::MemoryErrors));
}

TEST(GetErrorInjectionBitPosition, PCIeErrors)
{
    EXPECT_EQ(EI_PCI_ERRORS, getErrorInjectionBitPosition(Type::PCIeErrors));
}

TEST(GetErrorInjectionBitPosition, NVLinkErrors)
{
    EXPECT_EQ(EI_NVLINK_ERRORS,
              getErrorInjectionBitPosition(Type::NVLinkErrors));
}

TEST(GetErrorInjectionBitPosition, ThermalErrors)
{
    EXPECT_EQ(EI_THERMAL_ERRORS,
              getErrorInjectionBitPosition(Type::ThermalErrors));
}

TEST(GetErrorInjectionBitPosition, FatalErrors)
{
    EXPECT_EQ(EI_DEVICE_ERRORS,
              getErrorInjectionBitPosition(Type::FatalErrors));
}

TEST(GetErrorInjectionBitPosition, PortRecoveryErrors)
{
    EXPECT_EQ(EI_DEVICE_ERRORS,
              getErrorInjectionBitPosition(Type::PortRecoveryErrors));
}

TEST(GetErrorInjectionBitPosition, USBBridgeEmulationErrors)
{
    EXPECT_EQ(EI_DEVICE_ERRORS,
              getErrorInjectionBitPosition(Type::USBBridgeEmulationErrors));
}

TEST(GetErrorInjectionBitPosition, LeakDetectionErrors)
{
    EXPECT_EQ(EI_DEVICE_ERRORS,
              getErrorInjectionBitPosition(Type::LeakDetectionErrors));
}

TEST(GetErrorInjectionBitPosition, GPIOSpoofingErrors)
{
    EXPECT_EQ(EI_GPIO_SPOOFING,
              getErrorInjectionBitPosition(Type::GPIOSpoofingErrors));
}

TEST(GetErrorInjectionBitPosition, UnknownReturnsZero)
{
    EXPECT_EQ(0, getErrorInjectionBitPosition(Type::Unknown));
}

auto bus = sdbusplus::bus::new_default();

TEST(NsmErrorInjection, Constructor)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "ErrorInjection";
    std::string type = "NSM_ErrorInjection";

    auto errorInjIntf = std::make_shared<ErrorInjectionIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionIntf> provider(name, type, path,
                                                      errorInjIntf);

    NsmErrorInjection errorInj(provider);

    EXPECT_EQ(errorInj.getName(), name);
    EXPECT_EQ(errorInj.getType(), type);
}

TEST(NsmErrorInjection, GenRequestMsg)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "ErrorInjection";
    std::string type = "NSM_ErrorInjection";

    auto errorInjIntf = std::make_shared<ErrorInjectionIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionIntf> provider(name, type, path,
                                                      errorInjIntf);

    NsmErrorInjection errorInj(provider);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = errorInj.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

TEST(NsmErrorInjection, HandleResponseMsg)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "ErrorInjection";
    std::string type = "NSM_ErrorInjection";

    auto errorInjIntf = std::make_shared<ErrorInjectionIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionIntf> provider(name, type, path,
                                                      errorInjIntf);

    NsmErrorInjection errorInj(provider);

    // Create mock response
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_mode_v1_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    nsm_error_injection_mode_v1 data = {};
    data.mode = 1; // Enabled
    data.flags.bits.bit0 = 1;

    uint8_t rc = encode_get_error_injection_mode_v1_resp(0, cc, reason_code,
                                                         &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = errorInj.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify values were set
    EXPECT_EQ(errorInjIntf->errorInjectionModeEnabled(), 1);
    EXPECT_EQ(errorInjIntf->persistentDataModified(), 1);
}

TEST(NsmErrorInjectionSupported, Constructor)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "ErrorInjectionSupported";
    std::string type = "NSM_ErrorInjectionSupported";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());

    // Set type to non-Unknown to avoid exception
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::FatalErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);

    NsmErrorInjectionSupported errorInjSupp(provider);

    EXPECT_EQ(errorInjSupp.getName(), name);
    EXPECT_EQ(errorInjSupp.getType(), type);
}

TEST(NsmErrorInjectionSupported, GenRequestMsg)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "ErrorInjectionSupported";
    std::string type = "NSM_ErrorInjectionSupported";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::FatalErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);

    NsmErrorInjectionSupported errorInjSupp(provider);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = errorInjSupp.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}
