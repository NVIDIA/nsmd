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

#include "nsmHistogramInfo.hpp"

#include "dBusAsyncUtils.hpp"
#include "interfaceWrapper.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <string>
#include <vector>

namespace nsm
{

static double getMaxValueForType(uint8_t data_type)
{
    switch (data_type)
    {
        case NvU8:
            return std::numeric_limits<uint8_t>::max();
        case NvS8:
            return std::numeric_limits<int8_t>::max();
        case NvU16:
            return std::numeric_limits<uint16_t>::max();
        case NvS16:
            return std::numeric_limits<int16_t>::max();
        case NvU32:
            return std::numeric_limits<uint32_t>::max();
        case NvS32:
            return std::numeric_limits<int32_t>::max();
        case NvS24_8:
            return std::numeric_limits<float>::max();
        case NvU64:
            return std::numeric_limits<uint64_t>::max();
        case NvS64:
            return std::numeric_limits<int64_t>::max();
        default:
            return std::numeric_limits<double>::quiet_NaN();
    }
}

static bool checkSizeOfBucketArrayIsValid(uint32_t total_size,
                                          uint16_t num_of_buckets,
                                          uint8_t bucket_data_type)
{
    uint32_t calculated_size = 0;
    switch (bucket_data_type)
    {
        case NvU8:
            calculated_size = sizeof(uint8_t) * num_of_buckets;
            break;
        case NvS8:
            calculated_size = sizeof(int8_t) * num_of_buckets;
            break;
        case NvU16:
            calculated_size = sizeof(uint16_t) * num_of_buckets;
            break;
        case NvS16:
            calculated_size = sizeof(int16_t) * num_of_buckets;
            break;
        case NvU32:
            calculated_size = sizeof(uint32_t) * num_of_buckets;
            break;
        case NvS32:
            calculated_size = sizeof(int32_t) * num_of_buckets;
            break;
        case NvU64:
            calculated_size = sizeof(uint64_t) * num_of_buckets;
            break;
        case NvS64:
            calculated_size = sizeof(int64_t) * num_of_buckets;
            break;
        case NvS24_8:
            calculated_size = sizeof(float) * num_of_buckets;
            break;
        default:
            calculated_size = 0;
            break;
    }
    if (calculated_size != total_size)
    {
        lg2::error(
            "checkSizeOfBucketArrayIsValid: number of buckets and actual content received is not aligned. expectedSize = NumOfBucket*DataType = {ESOB} receivedSize = {RSOB}",
            "ESOB", calculated_size, "RSOB", total_size);
        return false;
    }
    return true;
}

static double getValueFromBucketArray(uint8_t* data, uint8_t data_type,
                                      size_t index)
{
    switch (data_type)
    {
        case NvU8:
            return static_cast<double>(data[index]);
        case NvS8:
        {
            int8_t tmp = 0;
            memcpy(&tmp, data + (index * sizeof(int8_t)), sizeof(int8_t));
            return static_cast<double>(tmp);
        }
        case NvU16:
        {
            uint16_t tmp = 0;
            memcpy(&tmp, data + (index * sizeof(uint16_t)), sizeof(uint16_t));
            return static_cast<double>(tmp);
        }
        case NvS16:
        {
            int16_t tmp = 0;
            memcpy(&tmp, data + (index * sizeof(int16_t)), sizeof(int16_t));
            return static_cast<double>(tmp);
        }
        case NvU32:
        {
            uint32_t tmp = 0;
            memcpy(&tmp, data + (index * sizeof(uint32_t)), sizeof(uint32_t));
            return static_cast<double>(tmp);
        }
        case NvS32:
        {
            int32_t tmp = 0;
            memcpy(&tmp, data + (index * sizeof(int32_t)), sizeof(int32_t));
            return static_cast<double>(tmp);
        }
        case NvS24_8:
        {
            float tmp = 0;
            memcpy(&tmp, data + (index * sizeof(float)), sizeof(float));
            return static_cast<double>(tmp);
        }
        case NvU64:
        {
            uint64_t tmp = 0;
            memcpy(&tmp, data + (index * sizeof(uint64_t)), sizeof(uint64_t));
            return utils::uint64ToDoubleSafeConvert(tmp);
        }
        case NvS64:
        {
            int64_t tmp = 0;
            memcpy(&tmp, data + (index * sizeof(int64_t)), sizeof(int64_t));
            return utils::int64ToDoubleSafeConvert(tmp);
        }
        default:
            // No operation for 8-bit types
            return 0.0;
    }
}

NsmHistogramFormat::NsmHistogramFormat(
    sdbusplus::bus::bus& bus, std::string& name, const std::string& type,
    std::shared_ptr<FormatIntf>& formatIntf, std::string& parentObjPath,
    std::string& deviceObjPath, uint32_t histogramId, uint16_t parameter) :
    NsmSensor(name, type),
    formatIntf(formatIntf), histogramId(histogramId), parameter(parameter)
{
    lg2::error("NsmHistogramFormat: {NAME}", "NAME", name.c_str());
    histogramName = name;
    deviceType = type;
    objPath = parentObjPath + "/Histograms/" + histogramName;
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent_device", "histograms", deviceObjPath);
    associationDefIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, objPath.c_str());

    associationDefIntf->associations(associationsList);
    formatIntf->unitOfMeasure(BucketUnits::Others);
}

std::optional<std::vector<uint8_t>>
    NsmHistogramFormat::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_histogram_format_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_histogram_format_req(instanceId, histogramId,
                                              parameter, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_histogram_format_req failed. {TYPE} - {NAME} eid={EID} rc={RC}",
            "TYPE", deviceType, "NAME", histogramName, "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmHistogramFormat::handleResponseMsg(const struct nsm_msg* responseMsg,
                                              size_t responseLen)
{
    auto& bus = utils::DBusHandler::getBus();
    SensorManager& sensorManager = SensorManager::getInstance();
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_histogram_format_metadata metaData;
    // Data size field on nsm response is 2 bytes means max value could be
    // 0xFFFF (65535). Possible size for buckets = 65535 -
    // sizeof(nsm_histogram_format_metadata) = 65535 - 16 = 65519.
    std::vector<uint8_t> bucket_offsets(65520, 0);
    uint32_t total_bucket_offset_size;

    auto rc = decode_get_histogram_format_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize, &metaData,
        bucket_offsets.data(), &total_bucket_offset_size);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        clearErrorBitMap("decode_get_histogram_format_resp");
        formatIntf->numOfBuckets(
            static_cast<uint64_t>(metaData.num_of_buckets));
        formatIntf->minSamplingTime(
            static_cast<uint64_t>(metaData.min_sampling_time));
        formatIntf->accumulationCycle(
            static_cast<uint64_t>(metaData.accumulation_cycle));
        formatIntf->incrementDuration(
            static_cast<uint64_t>(metaData.increment_duration));
        formatIntf->bucketDataType(
            static_cast<uint8_t>(metaData.bucket_data_type));
        switch (metaData.bucket_unit_of_measure)
        {
            case NSM_BUCKET_UNIT_WATTS:
                formatIntf->unitOfMeasure(BucketUnits::Watts);
                break;
            case NSM_BUCKET_UNIT_PERCENT:
                formatIntf->unitOfMeasure(BucketUnits::Percent);
                break;
            case NSM_BUCKET_UNIT_COUNTS:
                formatIntf->unitOfMeasure(BucketUnits::Count);
                break;
            default:
                formatIntf->unitOfMeasure(BucketUnits::Others);
                break;
        }

        if (!checkSizeOfBucketArrayIsValid(total_bucket_offset_size,
                                           metaData.num_of_buckets,
                                           metaData.bucket_data_type))
        {
            return NSM_SW_ERROR_COMMAND_FAIL;
        }

        std::vector<std::tuple<std::string, std::string, std::string>>
            associationsList;
        associationsList = associationDefIntf->associations();
        bucket_offsets.resize(total_bucket_offset_size);
        for (size_t i = 0; i < metaData.num_of_buckets; i++)
        {
            auto bucketObjPath = objPath + "/Buckets/" + std::to_string(i);
            auto bucketSensorObjectPath = bucketObjPath +
                                          "/com.nvidia.Histogram.BucketInfo";

            auto bucketInfoIntf = getInterfaceOnObjectPath<BucketInfoIntf>(
                bucketSensorObjectPath, sensorManager, bus,
                bucketObjPath.c_str());

            associationsList.emplace_back("histogram_buckets",
                                          "parent_histogram", bucketObjPath);
            bucketInfoIntf->start(getValueFromBucketArray(
                bucket_offsets.data(), metaData.bucket_data_type, i));

            if ((i + 1) < metaData.num_of_buckets)
            {
                bucketInfoIntf->end(getValueFromBucketArray(
                    bucket_offsets.data(), metaData.bucket_data_type, i + 1));
            }
            else
            {
                bucketInfoIntf->end(
                    getMaxValueForType(metaData.bucket_data_type));
            }

            switch (metaData.bucket_unit_of_measure)
            {
                case NSM_BUCKET_UNIT_WATTS:
                    bucketInfoIntf->unit(BucketUnits::Watts);
                    break;
                case NSM_BUCKET_UNIT_PERCENT:
                    bucketInfoIntf->unit(BucketUnits::Percent);
                    break;
                case NSM_BUCKET_UNIT_COUNTS:
                    bucketInfoIntf->unit(BucketUnits::Count);
                    break;
                default:
                    bucketInfoIntf->unit(BucketUnits::Others);
                    break;
            }
        }
        associationDefIntf->associations(associationsList);
    }
    else
    {
        if (shouldLogError(cc, rc))
        {
            lg2::error(
                "responseHandler: decode_get_histogram_format_resp unsuccessfull. {TYPE} - {NAM} reasonCode={RSNCOD} cc={CC} rc={RC}",
                "TYPE", deviceType, "NAM", histogramName, "RSNCOD", reasonCode,
                "CC", cc, "RC", rc);
        }
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmHistogramData::NsmHistogramData(std::string& name, const std::string& type,
                                   std::shared_ptr<FormatIntf>& formatIntf,
                                   std::string& inventoryObjPath,
                                   uint32_t histogramId, uint16_t parameter) :
    NsmSensor(name, type),
    formatIntf(formatIntf), objPath(inventoryObjPath), histogramId(histogramId),
    parameter(parameter)
{
    lg2::debug("NsmHistogramData: {NAME}", "NAME", name.c_str());
    histogramName = name;
    deviceType = type;
}

std::optional<std::vector<uint8_t>>
    NsmHistogramData::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_histogram_data_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_histogram_data_req(instanceId, histogramId, parameter,
                                            requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_histogram_data_req failed. {TYPE} - {NAME} eid={EID} rc={RC}",
            "TYPE", deviceType, "NAME", histogramName, "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmHistogramData::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    auto& bus = utils::DBusHandler::getBus();
    SensorManager& sensorManager = SensorManager::getInstance();
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint16_t number_of_buckets;
    // Data size field on nsm response is 2 bytes means max value could be
    // 0xFFFF (65535). Possible size for buckets = 65535 -
    // sizeof(nsm_histogram_format_metadata) = 65535 - 16 = 65519.
    std::vector<uint8_t> bucket_data(65520, 0);
    uint32_t total_bucket_data_size;
    uint8_t dataTypeOfBucket;

    auto rc = decode_get_histogram_data_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize,
        &dataTypeOfBucket, &number_of_buckets, bucket_data.data(),
        &total_bucket_data_size);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        clearErrorBitMap("decode_get_histogram_data_resp");

        if ((formatIntf->numOfBuckets() != number_of_buckets) ||
            (formatIntf->bucketDataType() != dataTypeOfBucket) ||
            !checkSizeOfBucketArrayIsValid(total_bucket_data_size,
                                           number_of_buckets, dataTypeOfBucket))
        {
            return NSM_SW_ERROR_COMMAND_FAIL;
        }

        bucket_data.resize(total_bucket_data_size);
        for (size_t i = 0; i < number_of_buckets; i++)
        {
            auto bucketObjPath = objPath + "/Buckets/" + std::to_string(i);
            auto bucketSensorObjectPath = bucketObjPath +
                                          "/com.nvidia.Histogram.BucketInfo";

            auto bucketInfoIntf = getInterfaceOnObjectPath<BucketInfoIntf>(
                bucketSensorObjectPath, sensorManager, bus,
                bucketObjPath.c_str());

            bucketInfoIntf->value(getValueFromBucketArray(
                bucket_data.data(), formatIntf->bucketDataType(), i));
        }
    }
    else
    {
        if (shouldLogError(cc, rc))
        {
            lg2::error(
                "responseHandler: decode_get_histogram_data_resp unsuccessfull. {TYPE} - {NAM} reasonCode={RSNCOD} cc={CC} rc={RC}",
                "TYPE", deviceType, "NAM", histogramName, "RSNCOD", reasonCode,
                "CC", cc, "RC", rc);
        }
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

} // namespace nsm
