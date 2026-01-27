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

#include "nsmPortConfigurationInfo.hpp"

namespace nsm
{

// Static member initialization
const std::unordered_map<uint8_t, std::string>
    NsmPCIePortConfigurationInfo::tagToPropertyMap = {{0, "PresetGen3"},
                                                      {1, "PresetGen4"},
                                                      {2, "PresetGen5"},
                                                      {3, "PresetGen6"},
                                                      {4, "TxAmplitude"}};

NsmPCIePortConfigurationInfo::NsmPCIePortConfigurationInfo(
    const std::string& name, const std::string& type,
    std::shared_ptr<PCIePortConfigurationInfoIntf>
        pciePortConfigurationInfoIntf,
    uint8_t portNumber, uint8_t portType, uint8_t portIndex,
    const std::string& inventoryObjPath) :
    NsmSensorAggregator(name, type),
    pciePortConfigurationInfoIntf(pciePortConfigurationInfoIntf),
    portNumber(portNumber), portType(portType), portIndex(portIndex),
    inventoryObjPath(inventoryObjPath)
{
    pciePortConfigurationInfoIntf->preset0(
        std::vector<PCIePortConfigurationInfoIntf::PCIePresetIndex>(
            4, PCIePortConfigurationInfoIntf::PCIePresetIndex::DeviceDefault));
    pciePortConfigurationInfoIntf->preset1(
        std::vector<PCIePortConfigurationInfoIntf::PCIePresetIndex>(
            4, PCIePortConfigurationInfoIntf::PCIePresetIndex::DeviceDefault));
}

void NsmPCIePortConfigurationInfo::updatePresetProperty(uint8_t tag,
                                                        uint8_t preset0,
                                                        uint8_t preset1)
{
    // get current preset0 and modify it
    if (tag >= 4)
    {
        lg2::error("Invalid tag {TAG} for preset property update", "TAG", tag);
        return;
    }

    std::vector<PCIePortConfigurationInfoIntf::PCIePresetIndex> getPreset0;
    getPreset0 = pciePortConfigurationInfoIntf->preset0();
    if (getPreset0.size() < 4)
    {
        getPreset0.resize(4);
    }
    auto preset0type = PCIePortConfigurationInfoIntf::PCIePresetIndex(preset0);
    getPreset0[tag] = preset0type;
    pciePortConfigurationInfoIntf->preset0(getPreset0);

    // get current preset1 and modify it
    std::vector<PCIePortConfigurationInfoIntf::PCIePresetIndex> getPreset1;
    getPreset1 = pciePortConfigurationInfoIntf->preset1();
    if (getPreset1.size() < 4)
    {
        getPreset1.resize(4);
    }
    auto preset1type = PCIePortConfigurationInfoIntf::PCIePresetIndex(preset1);
    getPreset1[tag] = preset1type;
    pciePortConfigurationInfoIntf->preset1(getPreset1);
}

void NsmPCIePortConfigurationInfo::updateTxAmplitudeProperty(
    uint8_t txAmplitude)
{
    pciePortConfigurationInfoIntf->txAmplitude(
        static_cast<uint64_t>(txAmplitude));
}

std::optional<std::vector<uint8_t>>
    NsmPCIePortConfigurationInfo::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_pcie_port_config_req(instanceId, portNumber, portType,
                                              portIndex, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_pcie_port_config_req({PORTNUMBER}, {PORTTYPE}, {PORTINDEX}) failed. eid={EID} rc={RC}",
            "PORTNUMBER", portNumber, "PORTTYPE", portType, "PORTINDEX",
            portIndex, "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

int NsmPCIePortConfigurationInfo::handleSample(const TelemetrySample& sample)
{
    uint8_t returnValue = NSM_SW_SUCCESS;

    if (sample.tag >
        static_cast<uint8_t>(NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE))
    {
        // No need to handle special tags like timestamp and uuid for now
        return returnValue;
    }

    // Map tag to corresponding property
    auto it = tagToPropertyMap.find(sample.tag);
    if (it == tagToPropertyMap.end())
    {
        lg2::debug("Unknown tag {TAG} in port configuration response.", "TAG",
                   sample.tag);
        return returnValue;
    }

    if (sample.tag <= 3) // Tags 0-3 are preset properties
    {
        uint8_t preset0;
        uint8_t preset1;
        if (decode_preset_PCIe_data(sample.data, sample.data_len, &preset0,
                                    &preset1) != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode Preset data. Tag={TAG}, Data length={LEN}",
                "TAG", sample.tag, "LEN", sample.data_len);
            returnValue = NSM_SW_ERROR_LENGTH;
            return returnValue;
        }

        updatePresetProperty(sample.tag, preset0, preset1);
    }
    else if (sample.tag == 4) // Tag 4 is TxAmplitude
    {
        uint8_t txAmplitude;
        if (decode_PCIe_TxAmplitude_data(sample.data, sample.data_len,
                                         &txAmplitude) != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode TxAmplitude. Tag={TAG}, Data length={LEN}",
                "TAG", sample.tag, "LEN", sample.data_len);
            returnValue = NSM_SW_ERROR_LENGTH;
            return returnValue;
        }

        updateTxAmplitudeProperty(txAmplitude);
    }

    return returnValue;
}

requester::Coroutine NsmPCIePortConfigurationInfo::setPortConfiguration(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("set Port Configuration On Device for EID: {EID}", "EID", eid);
    const std::vector<std::tuple<std::string, uint32_t>>* pciePortConfigData =
        std::get_if<std::vector<std::tuple<std::string, uint32_t>>>(&value);

    if (pciePortConfigData == nullptr)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    // Initialize response vector
    std::vector<uint8_t> request{};
    request.reserve(256);

    uint16_t samplesCount{};

    for (const auto& [property, value] : *pciePortConfigData)
    {
        uint8_t tag = 0;
        uint8_t reading[64]{};
        size_t sample_len{};
        std::array<uint8_t, 256> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        ++samplesCount;

        if (property == "TxAmplitude")
        {
            uint8_t txAmplitude = static_cast<uint8_t>(value);
            encode_PCIe_TxAmplitude_data(txAmplitude, reading, &sample_len);
            tag = 4;
        }
        else if (property == "PresetGen3")
        {
            uint8_t preset = static_cast<uint8_t>(value);
            encode_preset_PCIe_data(preset, reading, &sample_len);
            tag = 0;
        }
        else if (property == "PresetGen4")
        {
            uint8_t preset = static_cast<uint8_t>(value);
            encode_preset_PCIe_data(preset, reading, &sample_len);
            tag = 1;
        }
        else if (property == "PresetGen5")
        {
            uint8_t preset = static_cast<uint8_t>(value);
            encode_preset_PCIe_data(preset, reading, &sample_len);
            tag = 2;
        }
        else if (property == "PresetGen6")
        {
            uint8_t preset = static_cast<uint8_t>(value);
            encode_preset_PCIe_data(preset, reading, &sample_len);
            tag = 3;
        }
        // Encode the sample into the request
        int ret = encode_aggregate_resp_sample(tag, true, reading, sample_len,
                                               nsm_sample, &sample_len);
        if (ret != NSM_SW_SUCCESS)
        {
            lg2::error(
                "encode_aggregate_resp_sample failed for {PROPERTY}: rc={RC}",
                "PROPERTY", property, "RC", ret);
            continue;
        }

        // Add the sample to the request vector
        request.insert(request.end(), sample.begin(),
                       std::next(sample.begin(), sample_len));
    }

    if (request.size() == 0)
    {
        lg2::error(
            "No samples to send for Port Configuration On Device for EID: {EID}",
            "EID", eid);
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_DATA;
    }

    // Initialize the request message with the minimum size
    std::vector<uint8_t> requestMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_config_aggregate_req) - 1 +
            request.size(),
        0);

    auto reqMsg = reinterpret_cast<nsm_msg*>(requestMsg.data());

    int ret = encode_set_port_config_aggregate_req(
        0, portNumber, portType, portIndex, samplesCount, request.data(),
        request.size(), reqMsg);
    if (ret != NSM_SW_SUCCESS)
    {
        lg2::error("encode_set_port_config_aggregate_req failed: rc={RC}", "RC",
                   ret);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await device->postPatchIO(eid, requestMsg, responseMsg,
                                            responseLen);
    if (rc_ != NSM_SW_SUCCESS)
    {
        lg2::error(
            "postPatchIO failed for Port Configuration On Device for EID: {EID}: rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc_));
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    int rc = decode_set_port_config_aggregate_resp(
        responseMsg.get(), responseLen, &cc, &reason_code);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        lg2::info(
            "NsmPortConfigurationInfo::setPortConfiguration for EID: {EID} completed",
            "EID", eid);
        *status = AsyncOperationStatusType::Success;
    }
    else
    {
        lg2::error(
            "NsmPortConfigurationInfo::setPortConfiguration decode_set_port_config_aggregate_resp failed. "
            "eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return NSM_SW_SUCCESS;
}

} // namespace nsm
