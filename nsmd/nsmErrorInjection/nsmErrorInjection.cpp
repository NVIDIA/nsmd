/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "nsmErrorInjection.hpp"

#include "device-configuration.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

/**
 * @brief Convert D-Bus ErrorInjectionCapability Type to NSM protocol bit
 * position
 *
 * The D-Bus Type enum has more values than the NSM protocol bits because
 * FatalErrors, PortRecoveryErrors, USBBridgeEmulationErrors, and
 * LeakDetectionErrors all map to the same EI_DEVICE_ERRORS bit (bit 4) with
 * different subtypes.
 *
 * @param type D-Bus ErrorInjectionCapability Type
 * @return NSM protocol bit position for the error injection type
 */
static uint8_t
    getErrorInjectionBitPosition(ErrorInjectionCapabilityIntf::Type type)
{
    switch (type)
    {
        case ErrorInjectionCapabilityIntf::Type::MemoryErrors:
            return EI_MEMORY_ERRORS;
        case ErrorInjectionCapabilityIntf::Type::PCIeErrors:
            return EI_PCI_ERRORS;
        case ErrorInjectionCapabilityIntf::Type::NVLinkErrors:
            return EI_NVLINK_ERRORS;
        case ErrorInjectionCapabilityIntf::Type::ThermalErrors:
            return EI_THERMAL_ERRORS;
        case ErrorInjectionCapabilityIntf::Type::FatalErrors:
        case ErrorInjectionCapabilityIntf::Type::PortRecoveryErrors:
        case ErrorInjectionCapabilityIntf::Type::USBBridgeEmulationErrors:
        case ErrorInjectionCapabilityIntf::Type::LeakDetectionErrors:
            return EI_DEVICE_ERRORS;
        case ErrorInjectionCapabilityIntf::Type::GPIOSpoofingErrors:
            return EI_GPIO_SPOOFING;
        case ErrorInjectionCapabilityIntf::Type::Unknown:
        default:
            return 0;
    }
}

NsmErrorInjection::NsmErrorInjection(
    const NsmInterfaceProvider<ErrorInjectionIntf>& provider) :
    NsmSensor(provider), NsmInterfaceContainer<ErrorInjectionIntf>(provider)
{}

std::optional<Request> NsmErrorInjection::genRequestMsg(eid_t eid,
                                                        uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_mode_v1_req(instanceNumber,
                                                     requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_error_injection_mode_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmErrorInjection::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_error_injection_mode_v1 data;

    auto rc = decode_get_error_injection_mode_v1_resp(responseMsg, responseLen,
                                                      &cc, &reasonCode, &data);

    LG2_ERROR_FLT(
        "decode_get_error_injection_mode_v1_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        invoke(pdiMethod(errorInjectionModeEnabled), data.mode);
        invoke(pdiMethod(persistentDataModified),
               uint8_t(data.flags.bits.bit0));
    }

    return cc ? cc : rc;
}

NsmErrorInjectionSupported::NsmErrorInjectionSupported(
    const NsmInterfaceProvider<ErrorInjectionCapabilityIntf>& provider) :
    NsmSensor(provider),
    NsmInterfaceContainer<ErrorInjectionCapabilityIntf>(provider)
{
    invoke([](const auto& pdi) {
        if (pdi.type() == ErrorInjectionCapabilityIntf::Type::Unknown)
        {
            throw std::invalid_argument(
                "NsmErrorInjectionSupported::ctor: PDI type cannot be Unknown");
        }
    });
}

std::optional<Request>
    NsmErrorInjectionSupported::genRequestMsg(eid_t eid, uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_supported_error_injection_types_v1_req(instanceNumber,
                                                                requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_supported_error_injection_types_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmErrorInjectionSupported::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_error_injection_types_mask data;

    auto rc = decode_get_error_injection_types_v1_resp(responseMsg, responseLen,
                                                       &cc, &reasonCode, &data);

    LG2_ERROR_FLT(
        "decode_get_error_injection_types_v1_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        invoke([data](auto& pdi) {
            auto type = pdi.type();
            auto bitPos = getErrorInjectionBitPosition(type);
            pdi.supported(data.mask[bitPos / 8] & (1 << (bitPos % 8)));
        });
    }

    return cc ? cc : rc;
}

std::optional<Request>
    NsmErrorInjectionEnabled::genRequestMsg(eid_t eid, uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_current_error_injection_types_v1_req(instanceNumber,
                                                              requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_current_error_injection_types_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmErrorInjectionEnabled::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_error_injection_types_mask data;

    auto rc = decode_get_error_injection_types_v1_resp(responseMsg, responseLen,
                                                       &cc, &reasonCode, &data);

    LG2_ERROR_FLT(
        "decode_get_error_injection_types_v1_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        invoke([data](auto& pdi) {
            auto type = pdi.type();
            auto bitPos = getErrorInjectionBitPosition(type);
            pdi.enabled(data.mask[bitPos / 8] & (1 << (bitPos % 8)));
        });
    }

    return cc ? cc : rc;
}

NsmErrorInjectionPayload::NsmErrorInjectionPayload(
    const NsmInterfaceProvider<ErrorInjectionPayloadIntf>& provider,
    uint16_t errorInjectionType, uint16_t errorInjectionSubtype) :
    NsmSensor(provider),
    NsmInterfaceContainer<ErrorInjectionPayloadIntf>(provider),
    errorInjectionType(errorInjectionType),
    errorInjectionSubtype(errorInjectionSubtype)
{}

std::optional<Request>
    NsmErrorInjectionPayload::genRequestMsg(eid_t eid, uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_error_injection_payload_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_payload_req(
        instanceNumber, errorInjectionType, errorInjectionSubtype, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_error_injection_payload_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmErrorInjectionPayload::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> data(responseLen, 0);
    size_t dataSize = 0;
    auto rc = decode_get_error_injection_payload_resp(
        responseMsg, responseLen, errorInjectionType, errorInjectionSubtype,
        &cc, &reasonCode, data.data(), &dataSize);

    LG2_ERROR_FLT(
        "decode_get_error_injection_payload_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        if (errorInjectionType == EI_DEVICE_ERRORS &&
            dataSize > sizeof(uint16_t))
        {
            if (data.size() >= dataSize)
            {
                data.erase(data.begin(), data.begin() + sizeof(uint16_t));
                dataSize -= sizeof(uint16_t);
            }
        }
        data.resize(dataSize);
        invoke([data](auto& pdi) { pdi.payload(data); });
    }

    return cc ? cc : rc;
}

} // namespace nsm
