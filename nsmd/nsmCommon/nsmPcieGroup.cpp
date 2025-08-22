/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "nsmCommon/nsmPcieGroup.hpp"

#include "libnsm/pci-links.h"

#include "nsmCommon/sharedMemCommon.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmPcieGroup::NsmPcieGroup(const std::string& name, const std::string& type,
                           uint8_t deviceId, uint8_t groupId) :
    NsmSensor(name, type), isMultiPciePortEnabled(false), deviceId(deviceId),
    groupId(groupId)
{}

NsmPcieGroup::NsmPcieGroup(const std::string& name, const std::string& type,
                           uint8_t groupId, uint8_t multiPortType,
                           uint8_t multiPortIndex,
                           uint8_t multiPortUpstreamPortNumber) :
    NsmSensor(name, type), isMultiPciePortEnabled(true),
    multiPortType(multiPortType), multiPortIndex(multiPortIndex),
    multiPortUpstreamPortNumber(multiPortUpstreamPortNumber), groupId(groupId)
{}

std::optional<std::vector<uint8_t>>
    NsmPcieGroup::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    if (isMultiPciePortEnabled)
    {
        return genMultiPortRequestMsg(eid, instanceId);
    }
    else
    {
        return genSinglePortRequestMsg(eid, instanceId);
    }
}

std::optional<std::vector<uint8_t>>
    NsmPcieGroup::genSinglePortRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, deviceId,
                                                         groupId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmPciGroup :: encode_query_scalar_group_telemetry_v1_req failed for"
            "group = {GROUPID} eid={EID} rc={RC}",
            "GROUPID", groupId, "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

std::optional<std::vector<uint8_t>>
    NsmPcieGroup::genMultiPortRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    const nsm_multiport_query_scalar_group_telemetry_v2_req_data data{
        .upstream_port_index = multiPortUpstreamPortNumber,
        .type = multiPortType,
        .index = multiPortIndex,
        .group_index = groupId};

    auto rc = encode_multiport_query_scalar_group_telemetry_v2_req(
        instanceId, &data, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmPciGroup :: encode_multiport_query_scalar_group_telemetry_v2_req failed for"
            "group = {GROUPID} eid={EID} rc={RC}",
            "GROUPID", groupId, "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}
} // namespace nsm

namespace nsm
{
NsmPciGroup2::NsmPciGroup2(const std::string& name, const std::string& type,
                           std::shared_ptr<PCieEccIntf> pcieECCIntf,
                           std::shared_ptr<PCieEccIntf> pciePortIntf,
                           uint8_t deviceId, std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_2), pciePortIntf(pciePortIntf),
    pCieEccIntf(pcieECCIntf), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmPciGroup2: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmPciGroup2::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(pCieEccIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "nonfeCount";
    nv::sensor_aggregation::DbusVariantType nonfeCountVal{
        pCieEccIntf->nonfeCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nonfeCountVal);

    propName = "feCount";
    nv::sensor_aggregation::DbusVariantType feCountVal{pCieEccIntf->feCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, feCountVal);

    propName = "ceCount";
    nv::sensor_aggregation::DbusVariantType ceCountVal{pCieEccIntf->ceCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ceCountVal);

    propName = "UnsupportedRequestCount";
    nv::sensor_aggregation::DbusVariantType unsupportedRequestCount{
        pCieEccIntf->ceCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 unsupportedRequestCount);

    // pcie port metrics
    ifaceName = std::string(pciePortIntf->interface);
    propName = "nonfeCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nonfeCountVal);

    ifaceName = std::string(pciePortIntf->interface);
    propName = "feCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, feCountVal);

    ifaceName = std::string(pciePortIntf->interface);
    propName = "ceCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ceCountVal);

    ifaceName = std::string(pciePortIntf->interface);
    propName = "UnsupportedRequestCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 unsupportedRequestCount);

#endif
}

void NsmPciGroup2::updateReading(
    const nsm_query_scalar_group_telemetry_group_2& data)
{
    pCieEccIntf->nonfeCount(data.non_fatal_errors);
    pCieEccIntf->feCount(data.fatal_errors);
    pCieEccIntf->ceCount(data.correctable_errors);
    pCieEccIntf->unsupportedRequestCount(data.unsupported_request_count);
    // pcie port metrics
    pciePortIntf->nonfeCount(data.non_fatal_errors);
    pciePortIntf->feCount(data.fatal_errors);
    pciePortIntf->ceCount(data.correctable_errors);
    pciePortIntf->unsupportedRequestCount(data.unsupported_request_count);
    updateMetricOnSharedMemory();
}

uint8_t NsmPciGroup2::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_2 data;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &data);

    LG2_ERROR_FLT(
        "NsmPciGroup2 decode_query_scalar_group_telemetry_v1_group2_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(data);
    }
    return cc ? cc : rc;
}

NsmPciGroup3::NsmPciGroup3(const std::string& name, const std::string& type,
                           std::shared_ptr<PCieEccIntf> pcieECCIntf,
                           std::shared_ptr<PCieEccIntf> pciePortIntf,
                           uint8_t deviceId, std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_3), pciePortIntf(pciePortIntf),
    pCieEccIntf(pcieECCIntf), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmPciGroup2: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmPciGroup3::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(pCieEccIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "L0ToRecoveryCount";
    nv::sensor_aggregation::DbusVariantType l0ToRecoveryCountVal{
        pCieEccIntf->l0ToRecoveryCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, l0ToRecoveryCountVal);

    // pcie port metrics
    ifaceName = std::string(pciePortIntf->interface);
    propName = "L0ToRecoveryCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, l0ToRecoveryCountVal);
#endif
}

void NsmPciGroup3::updateReading(
    const ::nsm_query_scalar_group_telemetry_group_3& data)
{
    pCieEccIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
    // pcie port metrics
    pciePortIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
    updateMetricOnSharedMemory();
}

uint8_t NsmPciGroup3::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_3 data;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &data);

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup3 decode_query_scalar_group_telemetry_v1_group3_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(data);
    }

    return cc ? cc : rc;
}

NsmPciGroup4::NsmPciGroup4(const std::string& name, const std::string& type,
                           std::shared_ptr<PCieEccIntf> pcieECCIntf,
                           std::shared_ptr<PCieEccIntf> pciePortIntf,
                           uint8_t deviceId, std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_4), pciePortIntf(pciePortIntf),
    pCieEccIntf(pcieECCIntf), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmPciGroup4: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmPciGroup4::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(pCieEccIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "ReplayCount";
    nv::sensor_aggregation::DbusVariantType replayCountVal{
        pCieEccIntf->replayCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, replayCountVal);

    propName = "ReplayRolloverCount";
    nv::sensor_aggregation::DbusVariantType replayRolloverCountVal{
        pCieEccIntf->replayRolloverCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 replayRolloverCountVal);

    propName = "NAKSentCount";
    nv::sensor_aggregation::DbusVariantType nakSentCountVal{
        pCieEccIntf->nakSentCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nakSentCountVal);

    propName = "NAKReceivedCount";
    nv::sensor_aggregation::DbusVariantType nakReceivedCountVal{
        pCieEccIntf->nakReceivedCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nakReceivedCountVal);

    // pcie port metrics
    propName = "ReplayCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, replayCountVal);

    propName = "ReplayRolloverCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 replayRolloverCountVal);

    propName = "NAKSentCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nakSentCountVal);

    propName = "NAKReceivedCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nakReceivedCountVal);
#endif
}

void NsmPciGroup4::updateReading(
    const ::nsm_query_scalar_group_telemetry_group_4& data)
{
    pCieEccIntf->replayCount(data.replay_cnt);
    pCieEccIntf->replayRolloverCount(data.replay_rollover_cnt);
    pCieEccIntf->nakSentCount(data.NAK_sent_cnt);
    pCieEccIntf->nakReceivedCount(data.NAK_recv_cnt);
    // pcie port metrics
    pciePortIntf->replayCount(data.replay_cnt);
    pciePortIntf->replayRolloverCount(data.replay_rollover_cnt);
    pciePortIntf->nakSentCount(data.NAK_sent_cnt);
    pciePortIntf->nakReceivedCount(data.NAK_recv_cnt);
    updateMetricOnSharedMemory();
}

uint8_t NsmPciGroup4::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_4 data;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &data);

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup4 decode_query_scalar_group_telemetry_v1_group4_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(data);
    }

    return cc ? cc : rc;
}
} // namespace nsm
