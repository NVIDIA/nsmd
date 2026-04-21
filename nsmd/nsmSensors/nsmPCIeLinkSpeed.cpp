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

#include "nsmPCIeLinkSpeed.hpp"

#include "nsmAsioInterface/nsmAsioPCIeDeviceInterface.hpp"
#include "nsmDevice.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeLinkSpeedBase::NsmPCIeLinkSpeedBase(
    const NsmObject& provider, uint8_t deviceIndexOrUpstreamPortCount,
    bool isMultiPciePortEnabled) : NsmSensor(provider)
{
    this->isMultiPciePortEnabled = isMultiPciePortEnabled;
    (isMultiPciePortEnabled)
        ? this->upstreamPortCount = deviceIndexOrUpstreamPortCount
        : this->deviceIndex = deviceIndexOrUpstreamPortCount;
}

std::optional<Request>
    NsmPCIeLinkSpeedBase::genSinglePCIeRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, GROUP_ID_1, requestPtr);
    if (rc)
    {
        lg2::debug(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

std::optional<Request> NsmPCIeLinkSpeedBase::genMultiPCIeRequestMsg(
    eid_t eid, uint8_t instanceId, uint8_t multiPortType,
    uint8_t multiPortIndex, uint8_t multiPortUpstreamPortNumber)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    const nsm_multiport_query_scalar_group_telemetry_v2_req_data data{
        .upstream_port_index = multiPortUpstreamPortNumber,
        .type = multiPortType,
        .index = multiPortIndex,
        .group_index = GROUP_ID_1};

    auto rc = encode_multiport_query_scalar_group_telemetry_v2_req(
        instanceId, &data, requestPtr);
    if (rc)
    {
        lg2::debug(
            "encode_multi_query_scalar_group_telemetry_v2_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

std::optional<Request> NsmPCIeLinkSpeedBase::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
{
    return genSinglePCIeRequestMsg(eid, instanceId);
}

requester::Coroutine
    NsmPCIeLinkSpeedBase::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    if (!isMultiPciePortEnabled)
    {
        co_return co_await NsmSensor::update(nsmDevice);
    }

    // Incase of multi PCIe port, get the max from all the upstream ports
    uint8_t multiPortType = 0;
    uint8_t multiPortIndex = 0;
    uint8_t multiPortUpstreamPortNumber = 0;

    nsm_query_scalar_group_telemetry_group_1 max_data{
        .negotiated_link_speed = 1, // Gen1
        .negotiated_link_width = 0,
        .target_link_speed = 0,
        .max_link_speed = 1, // Gen1
        .max_link_width = 0,
        .max_read_request_size_bytes = 0,
        .max_payload_size_bytes = 0,
        .clock_mode = 0,
    };

    while (multiPortUpstreamPortNumber < upstreamPortCount)
    {
        auto requestMsg = genMultiPCIeRequestMsg(nsmDevice->getEid(), 0,
                                                 multiPortType, multiPortIndex,
                                                 multiPortUpstreamPortNumber);
        if (!requestMsg.has_value())
        {
            lg2::error(
                "NsmSensor::update: genRequestMsg failed, name={NAME}, eid={EID}",
                "NAME", getName(), "EID", nsmDevice->getEid());
            co_return NSM_SW_ERROR;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), *requestMsg,
                                               responseMsg, responseLen);
        if (rc)
        {
            co_return rc;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        nsm_query_scalar_group_telemetry_group_1 upPortData{};
        uint16_t size = 0;

        rc = decode_query_scalar_group_telemetry_v1_group1_resp(
            responseMsg.get(), responseLen, &cc, &size, &reasonCode,
            &upPortData);

        LG2_ERROR_FLT(
            "NsmPCIeLinkSpeedBase multi PCIe port decode_query_scalar_group_telemetry_v1_group1_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);

        if (rc == NSM_SUCCESS && cc == NSM_SUCCESS)
        {
            if (upPortData.negotiated_link_speed >
                max_data.negotiated_link_speed)
                max_data.negotiated_link_speed =
                    upPortData.negotiated_link_speed;
            if (upPortData.negotiated_link_width >
                max_data.negotiated_link_width)
                max_data.negotiated_link_width =
                    upPortData.negotiated_link_width;
            if (upPortData.target_link_speed > max_data.target_link_speed)
                max_data.target_link_speed = upPortData.target_link_speed;
            if (upPortData.max_link_speed > max_data.max_link_speed)
                max_data.max_link_speed = upPortData.max_link_speed;
            if (upPortData.max_link_width > max_data.max_link_width)
                max_data.max_link_width = upPortData.max_link_width;
        }
        multiPortUpstreamPortNumber++;
    }
    handleResponse(max_data);

    co_return NSM_SUCCESS;
}

uint8_t
    NsmPCIeLinkSpeedBase::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_1 data{};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);

    LG2_ERROR_FLT(
        "NsmPCIeLinkSpeedBase decode_query_scalar_group_telemetry_v1_group1_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    if (rc == NSM_SUCCESS && cc == NSM_SUCCESS)
    {
        handleResponse(data);
    }
    updateMetricOnSharedMemory();

    return cc ? cc : rc;
}

PCIeSlotIntf::Generations NsmPCIeLinkSpeedBase::generation(uint32_t value)
{
    return (value == 0) || (value > 6) ? PCIeSlotIntf::Generations::Unknown
                                       : PCIeSlotIntf::Generations(value - 1);
}
PCIeDeviceIntf::PCIeTypes NsmPCIeLinkSpeedBase::pcieType(uint32_t value)
{
    return (value == 0) || (value > 6) ? PCIeDeviceIntf::PCIeTypes::Unknown
                                       : PCIeDeviceIntf::PCIeTypes(value - 1);
};
uint32_t NsmPCIeLinkSpeedBase::linkWidth(uint32_t value)
{
    return (value > 0) ? (uint32_t)pow(2, value - 1) : 0;
}

NsmPCIeDeviceLinkSpeedAsio::NsmPCIeDeviceLinkSpeedAsio(
    const std::string& name, const std::string& type,
    std::shared_ptr<NsmAsioPCIeDeviceInterface> pcieDeviceIntf,
    uint8_t deviceIndex, bool isMultiPciePort) :
    NsmPCIeLinkSpeedBase(NsmObject(name, type), deviceIndex, isMultiPciePort),
    pcieDeviceIntf(std::move(pcieDeviceIntf))
{}

void NsmPCIeDeviceLinkSpeedAsio::handleResponse(
    const nsm_query_scalar_group_telemetry_group_1& data)
{
    if (!pcieDeviceIntf)
    {
        return;
    }
    pcieDeviceIntf->updateLinkSpeed(
        PCIeDeviceIntf::convertPCIeTypesToString(
            pcieType(data.negotiated_link_speed)),
        PCIeSlotIntf::convertGenerationsToString(
            generation(data.negotiated_link_speed)),
        PCIeDeviceIntf::convertPCIeTypesToString(pcieType(data.max_link_speed)),
        static_cast<size_t>(linkWidth(data.negotiated_link_width)),
        static_cast<size_t>(linkWidth(data.max_link_width)));
}
} // namespace nsm
