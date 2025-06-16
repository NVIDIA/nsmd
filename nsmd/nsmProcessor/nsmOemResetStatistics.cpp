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

#include "nsmOemResetStatistics.hpp"
#ifdef NVIDIA_SHMEM
#include "sharedMemCommon.hpp"
#endif
#include <phosphor-logging/lg2.hpp>

#include <stdexcept>

namespace nsm
{

// Static member initialization
const std::unordered_map<uint8_t, std::string>
    ResetStatisticsAggregator::tagToPropertyMap = {
        {0, "PF_FLR_ResetEntryCount"},
        {1, "PF_FLR_ResetExitCount"},
        {2, "ConventionalResetEntryCount"},
        {3, "ConventionalResetExitCount"},
        {4, "FundamentalResetEntryCount"},
        {5, "FundamentalResetExitCount"},
        {6, "IRoTResetExitCount"},
        {7, "LastResetType"},
        {8, "BootReason"}};

ResetStatisticsAggregator::ResetStatisticsAggregator(
    const std::string& name, const std::string& type,
    const std::string& inventoryObjPath,
    std::shared_ptr<ResetCountersIntf> resetCountersIntf,
    std::unique_ptr<AssociationDefinitionsIntf> associationDef) :
    NsmSensorAggregator(name, type), inventoryObjPath(inventoryObjPath),
    resetCountersIntf(resetCountersIntf),
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

int ResetStatisticsAggregator::handleSample(const TelemetrySample& sample)
{
    uint8_t returnValue = NSM_SW_SUCCESS;

    // Ensure tag is within valid range
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
        lg2::debug("Unknown tag {TAG} in reset statistics response.", "TAG",
                   sample.tag);
        return returnValue;
    }

    const auto& property = it->second;

    // Handle LastResetType (enum8) differently
    if (property == "LastResetType")
    {
        uint8_t resetType;
        if (decode_reset_enum_data(sample.data, sample.data_len, &resetType) !=
            NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode LastResetType. Tag={TAG}, Data length={LEN}",
                "TAG", sample.tag, "LEN", sample.data_len);
            returnValue = NSM_SW_ERROR_LENGTH;
            return returnValue;
        }

        // Update property with enum8 value
        updateProperty(property, resetType);
    }
    else if (property == "BootReason")
    {
        std::array<uint64_t, 4> counterVal;
        if (decode_reset_count_256data(sample.data, sample.data_len,
                                       counterVal.data(),
                                       counterVal.size()) != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode ResetCount 256 data. Tag={TAG}, Data length={LEN}",
                "TAG", sample.tag, "LEN", sample.data_len);
            returnValue = NSM_SW_ERROR_LENGTH;
            return returnValue;
        }
        // Update property with array of uint64 value
        updateBootReasonProperty(property, counterVal);
    }
    else
    {
        uint16_t count;
        if (decode_reset_count_data(sample.data, sample.data_len, &count) !=
            NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode ResetCount. Tag={TAG}, Data length={LEN}",
                "TAG", sample.tag, "LEN", sample.data_len);
            returnValue = NSM_SW_ERROR_LENGTH;
            return returnValue;
        }
        // Update property with uint16 value
        updateProperty(property, count);
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
            resetCountersIntf->convertResetTypesToString(
                resetCountersIntf->lastResetType())};
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

void ResetStatisticsAggregator::updateBootReasonProperty(
    const std::string& property, const std::array<uint64_t, 4>& value)
{
    try
    {
        // Convert and update enum array property
        nsm_boot_reason_type_breakdown bootReason;
        std::memcpy(&bootReason, &value, sizeof(bootReason));
        std::vector<BootReasonTypes> enabledReasons;

        if (bootReason.wake_up)
            enabledReasons.emplace_back(BootReasonTypes::WakeUp);
        if (bootReason.power_on)
            enabledReasons.emplace_back(BootReasonTypes::PowerOn);
        if (bootReason.voltage_detect)
            enabledReasons.emplace_back(BootReasonTypes::VoltageDetect);
        if (bootReason.warm_reset)
            enabledReasons.emplace_back(BootReasonTypes::WarmReset);
        if (bootReason.fatal_error)
            enabledReasons.emplace_back(BootReasonTypes::FatalError);
        if (bootReason.pin)
            enabledReasons.emplace_back(BootReasonTypes::Pin);
        if (bootReason.debug_access_port)
            enabledReasons.emplace_back(BootReasonTypes::DebugAccessPort);
        if (bootReason.reset_timeout)
            enabledReasons.emplace_back(BootReasonTypes::ResetTimeout);
        if (bootReason.low_power_acknowledge_timeout)
            enabledReasons.emplace_back(
                BootReasonTypes::LowPowerAcknowledgeTimeout);
        if (bootReason.system_clock_generator)
            enabledReasons.emplace_back(BootReasonTypes::SystemClockGenerator);
        if (bootReason.windowed_watchdog_0)
            enabledReasons.emplace_back(BootReasonTypes::WindowedWatchdog0);
        if (bootReason.software)
            enabledReasons.emplace_back(BootReasonTypes::Software);
        if (bootReason.lockup_reset)
            enabledReasons.emplace_back(BootReasonTypes::LockupReset);
        if (bootReason.cpu1)
            enabledReasons.emplace_back(BootReasonTypes::CPU1);
        if (bootReason.vbat)
            enabledReasons.emplace_back(BootReasonTypes::VBAT);
        if (bootReason.windowed_watchdog_1)
            enabledReasons.emplace_back(BootReasonTypes::WindowedWatchdog1);
        if (bootReason.code_watchdog_0)
            enabledReasons.emplace_back(BootReasonTypes::CodeWatchdog0);
        if (bootReason.code_watchdog_1)
            enabledReasons.emplace_back(BootReasonTypes::CodeWatchdog1);
        if (bootReason.jtag)
            enabledReasons.emplace_back(BootReasonTypes::JTAG);
        if (bootReason.security_violation)
            enabledReasons.emplace_back(BootReasonTypes::SecurityViolation);
        if (bootReason.tamper)
            enabledReasons.emplace_back(BootReasonTypes::Tamper);
        if (bootReason.iaccviol)
            enabledReasons.emplace_back(BootReasonTypes::IAccViol);
        if (bootReason.daccviol)
            enabledReasons.emplace_back(BootReasonTypes::DAccViol);
        if (bootReason.munstkerr)
            enabledReasons.emplace_back(BootReasonTypes::Munstkerr);
        if (bootReason.mstkerr)
            enabledReasons.emplace_back(BootReasonTypes::Mstkerr);
        if (bootReason.mmfarvalid)
            enabledReasons.emplace_back(BootReasonTypes::MMFarValid);
        if (bootReason.bfarvalid)
            enabledReasons.emplace_back(BootReasonTypes::BFarValid);
        if (bootReason.stkerr)
            enabledReasons.emplace_back(BootReasonTypes::Stkerr);
        if (bootReason.unstkerr)
            enabledReasons.emplace_back(BootReasonTypes::Unstkerr);
        if (bootReason.imprecise_error)
            enabledReasons.emplace_back(BootReasonTypes::ImpreciseError);
        if (bootReason.precise_error)
            enabledReasons.emplace_back(BootReasonTypes::PreciseError);
        if (bootReason.ibuserr)
            enabledReasons.emplace_back(BootReasonTypes::IBusErr);
        if (bootReason.undefinstr)
            enabledReasons.emplace_back(BootReasonTypes::UndefInstr);
        if (bootReason.invstate)
            enabledReasons.emplace_back(BootReasonTypes::InvState);
        if (bootReason.invpc)
            enabledReasons.emplace_back(BootReasonTypes::InvPC);
        if (bootReason.nocp)
            enabledReasons.emplace_back(BootReasonTypes::NoCP);
        if (bootReason.unaligned)
            enabledReasons.emplace_back(BootReasonTypes::Unaligned);
        if (bootReason.devbyzero)
            enabledReasons.emplace_back(BootReasonTypes::DevByZero);
        if (bootReason.vecttbl)
            enabledReasons.emplace_back(BootReasonTypes::VectTbl);
        if (bootReason.forced)
            enabledReasons.emplace_back(BootReasonTypes::Forced);
        if (bootReason.debugevt)
            enabledReasons.emplace_back(BootReasonTypes::DebugEvt);
        if (bootReason.mctp)
            enabledReasons.emplace_back(BootReasonTypes::MCTP);
        if (bootReason.i2c)
            enabledReasons.emplace_back(BootReasonTypes::I2C);
        if (bootReason.i3c)
            enabledReasons.emplace_back(BootReasonTypes::I3C);
        if (bootReason.pldm)
            enabledReasons.emplace_back(BootReasonTypes::PLDM);
        if (bootReason.usb)
            enabledReasons.emplace_back(BootReasonTypes::USB);
        if (bootReason.flash)
            enabledReasons.emplace_back(BootReasonTypes::Flash);
        if (bootReason.logger)
            enabledReasons.emplace_back(BootReasonTypes::Logger);
        if (bootReason.spdm)
            enabledReasons.emplace_back(BootReasonTypes::SPDM);

        resetCountersIntf->bootReason(enabledReasons);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to update property {PROPERTY}: {ERROR}", "PROPERTY",
                   property, "ERROR", e.what());
    }
}

} // namespace nsm
