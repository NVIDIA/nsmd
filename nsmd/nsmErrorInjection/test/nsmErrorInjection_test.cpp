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

TEST(NsmErrorInjection, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device_bad";
    std::string name = "ErrorInjection_Bad";
    std::string type = "NSM_ErrorInjection";
    auto errorInjIntf = std::make_shared<ErrorInjectionIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionIntf> provider(name, type, path,
                                                      errorInjIntf);
    NsmErrorInjection errorInj(provider);
    auto request = errorInj.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

TEST(NsmErrorInjectionSupported, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device_supbad";
    std::string name = "ErrorInjectionSupported_Bad";
    std::string type = "NSM_ErrorInjectionSupported";
    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    // Must set a non-Unknown type or constructor throws
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::FatalErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);
    NsmErrorInjectionSupported errorInjSupp(provider);
    auto request = errorInjSupp.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

TEST(NsmErrorInjectionEnabled, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device3_bad";
    std::string name = "ErrorInjectionEnabled_Bad";
    std::string type = "NSM_ErrorInjectionEnabled";
    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    // Must set a non-Unknown type or constructor throws
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::PCIeErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);
    NsmErrorInjectionEnabled errorInjEnabled(provider);
    auto request = errorInjEnabled.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

TEST(NsmErrorInjectionPayload, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device8_bad";
    std::string name = "ErrorInjectionPayload_Bad";
    std::string type = "NSM_ErrorInjectionPayload";
    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        name, type, path, errorInjPayloadIntf);
    NsmErrorInjectionPayload errorInjPayload(provider, EI_GPIO_SPOOFING, 0);
    auto request = errorInjPayload.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// handleResponseMsg EI_DEVICE_ERRORS FATAL subtype: strips first 2 bytes
// (error_injection_subtype) from decoded payload → stores payload_bitmap only
TEST(NsmErrorInjectionPayload,
     HandleResponseMsg_DeviceErrors_FatalPayload_StripsSubtype)
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
    // Build a proper nsm_error_injection_fatal_payload (6 bytes packed):
    //   error_injection_subtype (2 bytes) + payload_bitmap (4 bytes)
    // encode_fatal_error_injection_payload requires data_size ==
    // sizeof(nsm_error_injection_fatal_payload)
    nsm_error_injection_fatal_payload fatalPayload{};
    fatalPayload.error_injection_subtype = EI_DEVICE_ERRORS_SUBTYPE_FATAL;
    fatalPayload.payload_bitmap = 0x01020304;

    std::vector<uint8_t> payloadData(sizeof(fatalPayload));
    std::memcpy(payloadData.data(), &fatalPayload, sizeof(fatalPayload));

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_resp) - 1 +
        payloadData.size());
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_error_injection_payload_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, errorType, errorSubtype,
        payloadData.data(), payloadData.size(), responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjPayload.handleResponseMsg(responseMsgPtr,
                                                    responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    // EI_DEVICE_ERRORS: handleResponseMsg strips first sizeof(uint16_t)=2 bytes
    // (error_injection_subtype). Remaining: payload_bitmap (4 bytes).
    auto storedPayload = errorInjPayloadIntf->payload();
    ASSERT_EQ(storedPayload.size(), 4u);
    uint32_t storedBitmap = 0;
    std::memcpy(&storedBitmap, storedPayload.data(), 4);
    EXPECT_EQ(le32toh(storedBitmap), fatalPayload.payload_bitmap);
}

// handleResponseMsg EI_DEVICE_ERRORS PORT_RECOVERY subtype: also strips
// first 2 bytes (error_injection_subtype) → stores l1/l2/l3/reserved (4 bytes)
TEST(NsmErrorInjectionPayload,
     HandleResponseMsg_DeviceErrors_PortRecoveryPayload_StripsSubtype)
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
    // Build proper port_recovery payload (6 bytes packed):
    //   error_injection_subtype(2) + l1(1) + l2(1) + l3(1) + reserved(1)
    nsm_error_injection_port_recovery_payload prPayload{};
    prPayload.error_injection_subtype = EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY;
    prPayload.l1_payload = 0xAB;
    prPayload.l2_payload = 0xCD;
    prPayload.l3_payload = 0xEF;
    prPayload.reserved = 0x00;

    std::vector<uint8_t> payloadData(sizeof(prPayload));
    std::memcpy(payloadData.data(), &prPayload, sizeof(prPayload));

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_resp) - 1 +
        payloadData.size());
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_error_injection_payload_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, errorType, errorSubtype,
        payloadData.data(), payloadData.size(), responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = errorInjPayload.handleResponseMsg(responseMsgPtr,
                                                    responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    // EI_DEVICE_ERRORS: first 2 bytes (subtype) stripped
    // Remaining: {l1_payload, l2_payload, l3_payload, reserved} = 4 bytes
    auto storedPayload = errorInjPayloadIntf->payload();
    ASSERT_EQ(storedPayload.size(), 4u);
    EXPECT_EQ(storedPayload[0], prPayload.l1_payload);
    EXPECT_EQ(storedPayload[1], prPayload.l2_payload);
    EXPECT_EQ(storedPayload[2], prPayload.l3_payload);
    EXPECT_EQ(storedPayload[3], prPayload.reserved);
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

// Decode fail tests: 7-byte buffer triggers NSM_SW_ERROR_LENGTH from
// the underlying decode function (decode requires 11 bytes minimum)

TEST(NsmErrorInjection, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device11";
    std::string name = "ErrorInjection";
    std::string type = "NSM_ErrorInjection";

    auto errorInjIntf = std::make_shared<ErrorInjectionIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionIntf> provider(name, type, path,
                                                      errorInjIntf);
    NsmErrorInjection errorInj(provider);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = errorInj.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmErrorInjectionSupported, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device12";
    std::string name = "ErrorInjectionSupported";
    std::string type = "NSM_ErrorInjectionSupported";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::FatalErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);
    NsmErrorInjectionSupported errorInjSupp(provider);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = errorInjSupp.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmErrorInjectionEnabled, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device13";
    std::string name = "ErrorInjectionEnabled";
    std::string type = "NSM_ErrorInjectionEnabled";

    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::PCIeErrors);

    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        name, type, path, errorInjCapIntf);
    NsmErrorInjectionEnabled errorInjEnabled(provider);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = errorInjEnabled.handleResponseMsg(response,
                                                   responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmErrorInjectionPayload, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device14";
    std::string name = "ErrorInjectionPayload";
    std::string type = "NSM_ErrorInjectionPayload";

    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());

    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        name, type, path, errorInjPayloadIntf);
    NsmErrorInjectionPayload errorInjPayload(provider, EI_DEVICE_ERRORS,
                                             EI_DEVICE_ERRORS_SUBTYPE_FATAL);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = errorInjPayload.handleResponseMsg(response,
                                                   responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// Error-CC branch coverage: TRUE branch of return cc ? cc : rc
//
// cc is initialised to NSM_ERROR (non-zero) in each handleResponseMsg.
//   TRUE  branch (cc!=0): decode succeeds but cc = NSM_ERROR from response
//   FALSE branch (cc==0): decode succeeds with cc = NSM_SUCCESS (covered above)
// =============================================================================

TEST(NsmErrorInjection, HandleResponseMsg_ErrorCC_CoversTrueBranch)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device_ei_errcc";
    auto errorInjIntf = std::make_shared<ErrorInjectionIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionIntf> provider(
        "ErrorInjection", "NSM_ErrorInjection", path, errorInjIntf);
    NsmErrorInjection errorInj(provider);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_mode_v1_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    nsm_error_injection_mode_v1 data = {};
    ASSERT_EQ(encode_get_error_injection_mode_v1_resp(0, NSM_ERROR, ERR_NULL,
                                                      &data, response),
              NSM_SW_SUCCESS);

    auto rc = errorInj.handleResponseMsg(response, responseMsg.size());

    // cc = NSM_ERROR → true branch of return cc ? cc : rc
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmErrorInjectionSupported, HandleResponseMsg_ErrorCC_CoversTrueBranch)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device_eis_errcc";
    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::FatalErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "ErrorInjectionSupported", "NSM_ErrorInjectionSupported", path,
        errorInjCapIntf);
    NsmErrorInjectionSupported errorInjSupp(provider);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_types_mask_resp));
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    nsm_error_injection_types_mask data;
    memset(&data, 0, sizeof(data));
    ASSERT_EQ(encode_get_current_error_injection_types_v1_resp(
                  0, NSM_ERROR, ERR_NULL, &data, response),
              NSM_SW_SUCCESS);

    auto rc = errorInjSupp.handleResponseMsg(response, responseMsg.size());

    // cc = NSM_ERROR → true branch of return cc ? cc : rc
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmErrorInjectionEnabled, HandleResponseMsg_ErrorCC_CoversTrueBranch)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device_eie_errcc";
    auto errorInjCapIntf =
        std::make_shared<ErrorInjectionCapabilityIntf>(bus, path.c_str());
    errorInjCapIntf->type(ErrorInjectionCapabilityIntf::Type::PCIeErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "ErrorInjectionEnabled", "NSM_ErrorInjectionEnabled", path,
        errorInjCapIntf);
    NsmErrorInjectionEnabled errorInjEnabled(provider);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_types_mask_resp));
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    nsm_error_injection_types_mask data;
    memset(&data, 0, sizeof(data));
    ASSERT_EQ(encode_get_current_error_injection_types_v1_resp(
                  0, NSM_ERROR, ERR_NULL, &data, response),
              NSM_SW_SUCCESS);

    auto rc = errorInjEnabled.handleResponseMsg(response, responseMsg.size());

    // cc = NSM_ERROR → true branch of return cc ? cc : rc
    EXPECT_EQ(rc, NSM_ERROR);
}

// NsmErrorInjectionPayload – success response → FALSE branch of
// return cc ? cc : rc (cc = NSM_SUCCESS = 0 after decode).
// Use EI_GPIO_SPOOFING with count_of_gpio=0 (2-byte payload, no gpio entries).
TEST(NsmErrorInjectionPayload, HandleResponseMsg_Success_CoversFalseBranch)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/device_eip_succ";
    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        "ErrorInjectionPayload", "NSM_ErrorInjectionPayload", path,
        errorInjPayloadIntf);
    NsmErrorInjectionPayload errorInjPayload(provider, EI_GPIO_SPOOFING, 0);

    // EI_GPIO_SPOOFING with count_of_gpio=0 needs 2 bytes (uint16_t for count)
    uint8_t gpioData[2] = {0, 0}; // count_of_gpio = 0
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_resp) - 1 +
            sizeof(gpioData),
        0);
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_error_injection_payload_resp(
                  1, NSM_SUCCESS, ERR_NULL, EI_GPIO_SPOOFING, 0, gpioData,
                  sizeof(gpioData), responseMsgPtr),
              NSM_SW_SUCCESS);

    auto rc = errorInjPayload.handleResponseMsg(responseMsgPtr,
                                                responseMsg.size());

    // cc = NSM_SUCCESS → false branch of return cc ? cc : rc
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// getErrorInjectionBitPosition – lines 40-46 in nsmErrorInjection.cpp
//
// The function is called inside the invoke() lambda in
// NsmErrorInjectionEnabled::handleResponseMsg() only when the decode
// succeeds (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS).
//
// Existing tests cover MemoryErrors(0), PCIeErrors(1), NVLinkErrors(2),
// ThermalErrors(3).  These tests cover:
//   FatalErrors           → EI_DEVICE_ERRORS (bit 4)  lines 40, 44
//   PortRecoveryErrors    → EI_DEVICE_ERRORS (bit 4)  line 41
//   USBBridgeEmulationErrors → EI_DEVICE_ERRORS(bit4) line 42
//   LeakDetectionErrors   → EI_DEVICE_ERRORS (bit 4)  line 43
//   GPIOSpoofingErrors    → EI_GPIO_SPOOFING (bit 5)  lines 45, 46
// =============================================================================

// Helper: build a current-types-mask success response with one mask byte.
static std::vector<uint8_t> makeCurrentTypesResp(uint8_t maskByte0)
{
    nsm_error_injection_types_mask data;
    memset(&data, 0, sizeof(data));
    data.mask[0] = maskByte0;
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_error_injection_types_mask_resp));
    auto* ptr = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_current_error_injection_types_v1_resp(1, NSM_SUCCESS, ERR_NULL,
                                                     &data, ptr);
    return resp;
}

// FatalErrors → EI_DEVICE_ERRORS (bit 4) – lines 40, 44
TEST(NsmErrorInjectionEnabled, HandleResponse_FatalErrors_CoversBitPos44)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/fatal_ei";
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               path.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::FatalErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "FatalEI", "NSM_EIC", path, intf);
    NsmErrorInjectionEnabled sensor(provider);

    auto resp = makeCurrentTypesResp(1u << EI_DEVICE_ERRORS); // 0x10
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(resp.data()),
                                       resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(intf->enabled());
}

// PortRecoveryErrors → EI_DEVICE_ERRORS (bit 4) – line 41
TEST(NsmErrorInjectionEnabled, HandleResponse_PortRecoveryErrors_CoversBitPos44)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/portrecovery_ei";
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               path.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::PortRecoveryErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "PortRecoveryEI", "NSM_EIC", path, intf);
    NsmErrorInjectionEnabled sensor(provider);

    auto resp = makeCurrentTypesResp(1u << EI_DEVICE_ERRORS); // 0x10
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(resp.data()),
                                       resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(intf->enabled());
}

// USBBridgeEmulationErrors → EI_DEVICE_ERRORS (bit 4) – line 42
TEST(NsmErrorInjectionEnabled, HandleResponse_USBBridgeErrors_CoversBitPos44)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/usberr_ei";
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               path.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::USBBridgeEmulationErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "USBBridgeEI", "NSM_EIC", path, intf);
    NsmErrorInjectionEnabled sensor(provider);

    auto resp = makeCurrentTypesResp(1u << EI_DEVICE_ERRORS); // 0x10
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(resp.data()),
                                       resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(intf->enabled());
}

// LeakDetectionErrors → EI_DEVICE_ERRORS (bit 4) – line 43
TEST(NsmErrorInjectionEnabled,
     HandleResponse_LeakDetectionErrors_CoversBitPos44)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/leakdetect_ei";
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               path.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::LeakDetectionErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "LeakDetectEI", "NSM_EIC", path, intf);
    NsmErrorInjectionEnabled sensor(provider);

    auto resp = makeCurrentTypesResp(1u << EI_DEVICE_ERRORS); // 0x10
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(resp.data()),
                                       resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(intf->enabled());
}

// GPIOSpoofingErrors → EI_GPIO_SPOOFING (bit 5) – lines 45, 46
TEST(NsmErrorInjectionEnabled, HandleResponse_GPIOSpoofingErrors_CoversBitPos46)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/gpiospoofing_ei";
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               path.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::GPIOSpoofingErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "GPIOSpoofingEI", "NSM_EIC", path, intf);
    NsmErrorInjectionEnabled sensor(provider);

    auto resp = makeCurrentTypesResp(1u << EI_GPIO_SPOOFING); // 0x20
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(resp.data()),
                                       resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(intf->enabled());
}

// =============================================================================
// NsmErrorInjectionSupported constructor: Unknown type throws – line 107
// =============================================================================

TEST(NsmErrorInjectionSupported, Constructor_UnknownType_Throws)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/unknown_ei";
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               path.c_str());
    // Default type is Unknown → constructor should throw std::invalid_argument
    //
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "UnknownEI", "NSM_EIS", path, intf);
    EXPECT_THROW(NsmErrorInjectionSupported sensor(provider),
                 std::invalid_argument);
}

// ─── Branch coverage: rc==NSM_SW_SUCCESS && cc!=NSM_SUCCESS → false branch ──

// NsmErrorInjection::handleResponseMsg: non-success CC →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch
TEST(NsmErrorInjection, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/ei_cc_branch";
    auto intf = std::make_shared<ErrorInjectionIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionIntf> provider("EI_ccbranch", "NSM_EI",
                                                      path, intf);
    NsmErrorInjection sensor(provider);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                       buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmErrorInjectionSupported::handleResponseMsg: non-success CC →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch
TEST(NsmErrorInjectionSupported,
     HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/eis_cc_branch";
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               path.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::PCIeErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "EIS_ccbranch", "NSM_EIS", path, intf);
    NsmErrorInjectionSupported sensor(provider);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                       buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmErrorInjectionPayload::handleResponseMsg: rc==NSM_SW_SUCCESS,
// cc==NSM_ERROR → L239 FALSE via cc!=NSM_SUCCESS (existing
// HandleResponseMsg_Error uses a full-size encoded buffer →
// decode_reason_code_and_cc returns NSM_SW_ERROR_LENGTH)
TEST(NsmErrorInjectionPayload,
     HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/eip_cc_branch";
    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        "EIP_ccbranch", "NSM_EIP", path, errorInjPayloadIntf);
    NsmErrorInjectionPayload sensor(provider, EI_THERMAL_ERRORS, 0);

    // Exactly-sized non-success buffer: decode_reason_code_and_cc returns
    // NSM_SW_SUCCESS with cc=NSM_ERROR → L239 FALSE via cc!=NSM_SUCCESS
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                       buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmErrorInjectionEnabled::handleResponseMsg: non-success CC →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch
TEST(NsmErrorInjectionEnabled,
     HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/eie_cc_branch";
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               path.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::PCIeErrors);
    NsmInterfaceProvider<ErrorInjectionCapabilityIntf> provider(
        "EIE_ccbranch", "NSM_EIC", path, intf);
    NsmErrorInjectionEnabled sensor(provider);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(buf.data()),
                                       buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmErrorInjectionPayload::handleResponseMsg: EI_GPIO_SPOOFING (not
// EI_DEVICE_ERRORS) with NSM_SUCCESS → FALSE branch of the inner
// `if (errorInjectionType == EI_DEVICE_ERRORS && dataSize > sizeof(uint16_t))`
// (line 241). No 2-byte stripping; decoded payload stored as-is.
TEST(NsmErrorInjectionPayload,
     HandleResponseMsg_GpioSpoofing_Success_NoStripping)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/test/eip_gpio_noerase";
    auto errorInjPayloadIntf =
        std::make_shared<ErrorInjectionPayloadIntf>(bus, path.c_str());
    NsmInterfaceProvider<ErrorInjectionPayloadIntf> provider(
        "EIP_gpio_noerase", "NSM_EIP", path, errorInjPayloadIntf);

    // EI_GPIO_SPOOFING is not EI_DEVICE_ERRORS → inner if FALSE → no erase
    uint16_t errorType = EI_GPIO_SPOOFING;
    uint16_t errorSubtype = 0;
    NsmErrorInjectionPayload sensor(provider, errorType, errorSubtype);

    // Minimal GPIO spoofing payload: count_of_gpio=0, no gpio_data entries
    nsm_error_injection_gpio_spoofing_payload gpioPayload{};
    gpioPayload.count_of_gpio = 0;
    // payload size = sizeof - sizeof(gpio_data[0]) since count=0
    size_t payloadSize = sizeof(nsm_error_injection_gpio_spoofing_payload) -
                         sizeof(uint16_t);
    std::vector<uint8_t> payloadData(payloadSize);
    std::memcpy(payloadData.data(), &gpioPayload, payloadSize);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_resp) - 1 +
        payloadData.size());
    auto responseMsgPtr = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t instanceId = 1;
    auto rc = encode_get_error_injection_payload_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, errorType, errorSubtype,
        payloadData.data(), payloadData.size(), responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(responseMsgPtr, responseMsg.size());
    EXPECT_EQ(result, NSM_SUCCESS);

    // EI_GPIO_SPOOFING: no stripping, decoded size == payloadSize
    auto stored = errorInjPayloadIntf->payload();
    EXPECT_EQ(stored.size(), payloadSize);
}
