/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "nsmOemResetStatistics.hpp"

#include "sharedMemCommon.hpp"

#include <phosphor-logging/lg2.hpp>

#include <stdexcept>

namespace nsm
{

// Static member initialization
const std::unordered_map<uint8_t, std::string>
    ResetStatisticsAggregator::tagToPropertyMap = {
        {0, "PF_FLR_ResetEntryCount"},      {1, "PF_FLR_ResetExitCount"},
        {2, "ConventionalResetEntryCount"}, {3, "ConventionalResetExitCount"},
        {4, "FundamentalResetEntryCount"},  {5, "FundamentalResetExitCount"},
        {6, "IRoTResetExitCount"},          {7, "LastResetType"}};

ResetStatisticsAggregator::ResetStatisticsAggregator(
    const std::string& name, const std::string& type,
    const std::string& inventoryObjPath,
    std::shared_ptr<ResetCountersIntf> resetCountersIntf,
    std::unique_ptr<AssociationDefinitionsIntf> associationDef) :
    NsmSensorAggregator(name, type),
    inventoryObjPath(inventoryObjPath), resetCountersIntf(resetCountersIntf),
    associationDef(std::move(associationDef))
{
#ifdef NVIDIA_SHMEM
    updateMetricOnSharedMemory();
#endif
}

std::optional<std::vector<uint8_t>>
    ResetStatisticsAggregator::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request;

    request.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_get_device_reset_statistics_req(instanceId, requestPtr);

    if (rc)
    {
        lg2::debug("encode_query_reset_statistics_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int ResetStatisticsAggregator::handleSamples(
    const std::vector<TelemetrySample>& samples)
{
    uint8_t returnValue = NSM_SW_SUCCESS;

    for (const auto& sample : samples)
    {
        // Ensure tag is within valid range
        if (sample.tag >
            static_cast<uint8_t>(NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE))
        {
            // No need to handle special tags like timestamp and uuid for now
            continue;
        }

        // Map tag to corresponding property
        auto it = tagToPropertyMap.find(sample.tag);
        if (it == tagToPropertyMap.end())
        {
            lg2::debug("Unknown tag {TAG} in reset statistics response.", "TAG",
                       sample.tag);
            continue;
        }

        const auto& property = it->second;

        // Handle LastResetType (enum8) differently
        if (property == "LastResetType")
        {
            // Validate data length for enum8
            if (sample.data_len != sizeof(enum8))
            {
                lg2::error(
                    "Invalid data length {LEN} for LastResetType. Expected 1 byte.",
                    "LEN", sample.data_len);
                returnValue = NSM_SW_ERROR_LENGTH;
                continue;
            }

            // Extract the enum8 value
            uint8_t value = sample.data[0];

            // Update property with enum8 value
            updateProperty(property, value);
        }
        else
        {
            // Validate data length for uint16
            if (sample.data_len != sizeof(uint16_t))
            {
                lg2::error(
                    "Invalid data length {LEN} for tag {TAG}. Expected 2 bytes.",
                    "LEN", sample.data_len, "TAG", sample.tag);
                returnValue = NSM_SW_ERROR_LENGTH;
                continue;
            }

            // Extract uint16 value
            uint16_t value =
                le16toh(*reinterpret_cast<const uint16_t*>(sample.data));

            // Update property with uint16 value
            updateProperty(property, value);
        }
    }

    return returnValue;
}

void ResetStatisticsAggregator::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(resetCountersIntf->interface);
    std::vector<uint8_t> smbusData = {};

    // Update LastResetType
    {
        nv::sensor_aggregation::DbusVariantType lastResetTypeValue{
            static_cast<uint8_t>(resetCountersIntf->lastResetType())};
        std::string lastResetTypeProp = "LastResetType";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(
            inventoryObjPath, ifaceName, lastResetTypeProp, smbusData,
            lastResetTypeValue);
    }

    // Update PF_FLR_ResetEntryCount
    {
        nv::sensor_aggregation::DbusVariantType pfFlrEntryValue{
            resetCountersIntf->pfflrResetEntryCount()};
        std::string pfFlrEntryProp = "PF_FLR_ResetEntryCount";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(
            inventoryObjPath, ifaceName, pfFlrEntryProp, smbusData,
            pfFlrEntryValue);
    }

    // Update PF_FLR_ResetExitCount
    {
        nv::sensor_aggregation::DbusVariantType pfFlrExitValue{
            resetCountersIntf->pfflrResetExitCount()};
        std::string pfFlrExitProp = "PF_FLR_ResetExitCount";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath,
                                                     ifaceName, pfFlrExitProp,
                                                     smbusData, pfFlrExitValue);
    }

    // Update ConventionalResetEntryCount
    {
        nv::sensor_aggregation::DbusVariantType conventionalEntryValue{
            resetCountersIntf->conventionalResetEntryCount()};
        std::string conventionalEntryProp = "ConventionalResetEntryCount";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(
            inventoryObjPath, ifaceName, conventionalEntryProp, smbusData,
            conventionalEntryValue);
    }

    // Update ConventionalResetExitCount
    {
        nv::sensor_aggregation::DbusVariantType conventionalExitValue{
            resetCountersIntf->conventionalResetExitCount()};
        std::string conventionalExitProp = "ConventionalResetExitCount";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(
            inventoryObjPath, ifaceName, conventionalExitProp, smbusData,
            conventionalExitValue);
    }

    // Update FundamentalResetEntryCount
    {
        nv::sensor_aggregation::DbusVariantType fundamentalEntryValue{
            resetCountersIntf->fundamentalResetEntryCount()};
        std::string fundamentalEntryProp = "FundamentalResetEntryCount";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(
            inventoryObjPath, ifaceName, fundamentalEntryProp, smbusData,
            fundamentalEntryValue);
    }

    // Update FundamentalResetExitCount
    {
        nv::sensor_aggregation::DbusVariantType fundamentalExitValue{
            resetCountersIntf->fundamentalResetExitCount()};
        std::string fundamentalExitProp = "FundamentalResetExitCount";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(
            inventoryObjPath, ifaceName, fundamentalExitProp, smbusData,
            fundamentalExitValue);
    }

    // Update IRoTResetExitCount
    {
        nv::sensor_aggregation::DbusVariantType iRotExitValue{
            resetCountersIntf->iRoTResetExitCount()};
        std::string iRotExitProp = "IRoTResetExitCount";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath,
                                                     ifaceName, iRotExitProp,
                                                     smbusData, iRotExitValue);
    }
#endif
}

void ResetStatisticsAggregator::updateProperty(const std::string& property,
                                               uint64_t value)
{
    try
    {
        if (property == "LastResetType")
        {
            // Convert and update enum property
            auto resetType = static_cast<ResetCountersIntf::ResetTypes>(value);
            resetCountersIntf->lastResetType(resetType);
        }
        else
        {
            // Update numeric properties as double
            resetCountersIntf->setPropertyByName(property,
                                                 static_cast<double>(value));
        }
        updateMetricOnSharedMemory();
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to update property {PROPERTY} with value {VALUE}: {ERROR}",
            "PROPERTY", property, "VALUE", value, "ERROR", e.what());
    }
}
} // namespace nsm
