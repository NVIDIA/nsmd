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

TEST(NsmErrorInjectionSupported, HandleResponseMsg_Supported)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "ErrorInjectionSupported";
    std::string type = "NSM_ErrorInjectionSupported";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);

    NsmErrorInjectionSupported errorInjSupp(provider);

    uint8_t instanceId = 1;
    nsm_error_injection_types_mask data;
    memset(&data, 0, sizeof(data));
    // Set bit 0 (EI_MEMORY_ERRORS)
    data.mask[0] = 0x01;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_types_mask_resp));
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_supported_error_injection_types_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsgPtr);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjSupp.handleResponseMsg(responseMsgPtr,
                                                 responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_TRUE(errorInjCapIntf->supported());
}

TEST(NsmErrorInjectionSupported, HandleResponseMsg_NotSupported)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device2";
    std::string name = "ErrorInjectionSupported";
    std::string type = "NSM_ErrorInjectionSupported";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::NVLinkErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);

    NsmErrorInjectionSupported errorInjSupp(provider);

    uint8_t instanceId = 1;
    nsm_error_injection_types_mask data;
    memset(&data, 0, sizeof(data));
    // Set bit 0 only (MemoryErrors), NVLink is bit 2

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_types_mask_resp));
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_supported_error_injection_types_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsgPtr);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjSupp.handleResponseMsg(responseMsgPtr,
                                                 responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_FALSE(errorInjCapIntf->supported());
}

TEST(NsmErrorInjectionEnabled, GenRequestMsg)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device3";
    std::string name = "ErrorInjectionEnabled";
    std::string type = "NSM_ErrorInjectionEnabled";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::ThermalErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);

    NsmErrorInjectionEnabled errorInjEnabled(provider);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = errorInjEnabled.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

TEST(NsmErrorInjectionEnabled, HandleResponseMsg_Enabled)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device4";
    std::string name = "ErrorInjectionEnabled";
    std::string type = "NSM_ErrorInjectionEnabled";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::PCIeErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);

    NsmErrorInjectionEnabled errorInjEnabled(provider);

    uint8_t instanceId = 1;
    nsm_error_injection_types_mask data;
    memset(&data, 0, sizeof(data));
    // Set bit 1 (EI_PCI_ERRORS)
    data.mask[0] = 0x02;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_types_mask_resp));
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_current_error_injection_types_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsgPtr);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjEnabled.handleResponseMsg(responseMsgPtr,
                                                    responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_TRUE(errorInjCapIntf->enabled());
}

TEST(NsmErrorInjectionEnabled, HandleResponseMsg_Disabled)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device5";
    std::string name = "ErrorInjectionEnabled";
    std::string type = "NSM_ErrorInjectionEnabled";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::ThermalErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);

    NsmErrorInjectionEnabled errorInjEnabled(provider);

    uint8_t instanceId = 1;
    nsm_error_injection_types_mask data;
    memset(&data, 0, sizeof(data));
    // Set bit 1 only (PCI), Thermal is bit 3

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_types_mask_resp));
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_current_error_injection_types_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsgPtr);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjEnabled.handleResponseMsg(responseMsgPtr,
                                                    responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_FALSE(errorInjCapIntf->enabled());
}

TEST(NsmErrorInjectionPayload, Constructor)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device6";
    std::string name = "ErrorInjectionPayload";
    std::string type = "NSM_ErrorInjectionPayload";

    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());

    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        name, type, path, errorInjPayloadIntf);

    uint16_t errorType = EI_DEVICE_ERRORS;
    uint16_t errorSubtype = EI_DEVICE_ERRORS_SUBTYPE_FATAL;

    EXPECT_NO_THROW(NsmErrorInjectionPayload errorInjPayload(
        provider, errorType, errorSubtype));
}

TEST(NsmErrorInjectionPayload, GenRequestMsg)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device7";
    std::string name = "ErrorInjectionPayload";
    std::string type = "NSM_ErrorInjectionPayload";

    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());

    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        name, type, path, errorInjPayloadIntf);

    uint16_t errorType = EI_GPIO_SPOOFING;
    uint16_t errorSubtype = 0;

    NsmErrorInjectionPayload errorInjPayload(provider, errorType, errorSubtype);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = errorInjPayload.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) +
                                   sizeof(nsm_get_error_injection_payload_req));
}

TEST(NsmErrorInjectionPayload, DISABLED_HandleResponseMsg_Success)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device8";
    std::string name = "ErrorInjectionPayload";
    std::string type = "NSM_ErrorInjectionPayload";

    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());

    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        name, type, path, errorInjPayloadIntf);

    uint16_t errorType = EI_DEVICE_ERRORS;
    uint16_t errorSubtype = EI_DEVICE_ERRORS_SUBTYPE_FATAL;

    NsmErrorInjectionPayload errorInjPayload(provider, errorType, errorSubtype);

    uint8_t instanceId = 1;
    std::vector<uint8_t> payloadData = {0x01, 0x02, 0x03, 0x04};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_resp) - 1 +
        payloadData.size());
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_error_injection_payload_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, errorType, errorSubtype,
        payloadData.data(), payloadData.size(), responseMsgPtr);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjPayload.handleResponseMsg(responseMsgPtr,
                                                    responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto storedPayload = errorInjPayloadIntf->payload();
    EXPECT_EQ(storedPayload, payloadData);
}

TEST(NsmErrorInjectionPayload, DISABLED_HandleResponseMsg_SmallPayload)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device9";
    std::string name = "ErrorInjectionPayload";
    std::string type = "NSM_ErrorInjectionPayload";

    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());

    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        name, type, path, errorInjPayloadIntf);

    uint16_t errorType = EI_DEVICE_ERRORS;
    uint16_t errorSubtype = EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY;

    NsmErrorInjectionPayload errorInjPayload(provider, errorType, errorSubtype);

    uint8_t instanceId = 1;
    std::vector<uint8_t> payloadData = {0x00};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_resp) - 1 +
        payloadData.size());
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_error_injection_payload_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, errorType, errorSubtype,
        payloadData.data(), payloadData.size(), responseMsgPtr);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjPayload.handleResponseMsg(responseMsgPtr,
                                                    responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto storedPayload = errorInjPayloadIntf->payload();
    EXPECT_EQ(storedPayload.size(), 1);
}

TEST(NsmErrorInjectionPayload, HandleResponseMsg_Error)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device10";
    std::string name = "ErrorInjectionPayload";
    std::string type = "NSM_ErrorInjectionPayload";

    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());

    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        name, type, path, errorInjPayloadIntf);

    uint16_t errorType = EI_THERMAL_ERRORS;
    uint16_t errorSubtype = 0;

    NsmErrorInjectionPayload errorInjPayload(provider, errorType, errorSubtype);

    uint8_t instanceId = 1;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_resp));
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_error_injection_payload_resp(
        instanceId, NSM_ERROR, ERR_NULL, errorType, errorSubtype, nullptr, 0,
        responseMsgPtr);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjPayload.handleResponseMsg(responseMsgPtr,
                                                    responseMsg.size());

    EXPECT_EQ(result, NSM_ERROR);
}
