#include "nsmMemory.hpp"

#include "platform-environmental.h"

#include "../../common/coroutine.hpp"
#include "../../common/telemetryTombstone.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "interfaceWrapper.hpp"
#ifdef NVIDIA_SHMEM
#include "nsmCommon/sharedMemCommon.hpp"
#endif
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <unordered_map>
#include <vector>
#define MEMORY_INTERFACE "xyz.openbmc_project.Configuration.NSM_Memory"

namespace nsm
{

using RowRemapFailureState = MemoryRowRemappingIntf::RowRemappingFailureStates;
using RowRemapPendingState = MemoryRowRemappingIntf::RowRemappingPendingStates;

NsmMemoryErrorCorrection::NsmMemoryErrorCorrection(
    std::string& name, std::string& type, std::shared_ptr<DimmIntf> dimmIntf,
    std::string& correctionType, std::string& inventoryObjPath) :
    NsmObject(name, type), dimmIntf(dimmIntf),
    inventoryObjPath(inventoryObjPath)
{
    dimmIntf->ecc(DimmIntf::convertEccFromString(correctionType));
    updateMetricOnSharedMemory();
}

void NsmMemoryErrorCorrection::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(dimmIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType eccValue{
        static_cast<uint16_t>(dimmIntf->ecc())};
    std::string propName = "ECC";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, eccValue);
#endif
}

NsmMemoryDeviceType::NsmMemoryDeviceType(std::string& name, std::string& type,
                                         std::shared_ptr<DimmIntf> dimmIntf,
                                         std::string& memoryType,
                                         std::string& inventoryObjPath) :
    NsmObject(name, type), dimmIntf(dimmIntf),
    inventoryObjPath(inventoryObjPath)
{
    dimmIntf->memoryType(DimmIntf::convertDeviceTypeFromString(memoryType));
    updateMetricOnSharedMemory();
}

void NsmMemoryDeviceType::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(dimmIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType memoryTypeValue{
        dimmIntf->convertDeviceTypeToString(dimmIntf->memoryType())};
    std::string propName = "MemoryType";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, memoryTypeValue);
#endif
}

NsmLocationIntfMemory::NsmLocationIntfMemory(sdbusplus::bus_t& bus,
                                             std::string& name,
                                             std::string& type,
                                             std::string& inventoryObjPath) :
    NsmObject(name, type), inventoryObjPath(inventoryObjPath)
{
    locationIntf =
        std::make_unique<LocationIntfMemory>(bus, inventoryObjPath.c_str());

    locationIntf->locationType(LocationTypesMemory::Embedded);
}

NsmMemoryHealth::NsmMemoryHealth(sdbusplus::bus_t& bus, std::string& name,
                                 std::string& type,
                                 std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    healthIntf = std::make_unique<MemoryHealthIntf>(bus,
                                                    inventoryObjPath.c_str());
    healthIntf->health(MemoryHealthType::OK);
}

NsmMemoryAssociation::NsmMemoryAssociation(
    sdbusplus::bus_t& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath,
    const std::vector<utils::Association>& associations) : NsmObject(name, type)
{
    associationDef = std::make_unique<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDef->associations(associations_list);
}

NsmRowRemapState::NsmRowRemapState(
    std::string& name, std::string& type,
    std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmRowRemapIntf: create sensor:{NAME}", "NAME", name.c_str());
    memoryRowRemappingStateIntf = memoryRowRemappingIntf;
    updateMetricOnSharedMemory();
}

void NsmRowRemapState::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(memoryRowRemappingStateIntf->interface);
    std::vector<uint8_t> smbusData = {};

    // Publish the enum as its D-Bus string; nv-shmem maps True/False/Unknown
    // to the true/false/null MetricValue. Keeps shmem the same shape as D-Bus.
    nv::sensor_aggregation::DbusVariantType rowRemappingFailed{
        memoryRowRemappingStateIntf->convertRowRemappingFailureStatesToString(
            memoryRowRemappingStateIntf->rowRemappingFailureState())};
    std::string propName = "RowRemappingFailureState";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, rowRemappingFailed);

    nv::sensor_aggregation::DbusVariantType rowRemappingPending{
        memoryRowRemappingStateIntf->convertRowRemappingPendingStatesToString(
            memoryRowRemappingStateIntf->rowRemappingPendingState())};
    propName = "RowRemappingPendingState";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, rowRemappingPending);
#endif
}

void NsmRowRemapState::updateReading(bitfield8_t flags)
{
    memoryRowRemappingStateIntf->rowRemappingFailureState(
        flags.bits.bit0 ? RowRemapFailureState::True
                        : RowRemapFailureState::False);
    memoryRowRemappingStateIntf->rowRemappingPendingState(
        flags.bits.bit1 ? RowRemapPendingState::True
                        : RowRemapPendingState::False);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmRowRemapState::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_state_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_row_remap_state_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmRowRemapState::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_get_row_remap_state_resp(responseMsg, responseLen, &cc,
                                              &data_size, &reasonCode, &flags);

    LG2_ERROR_FLT(
        "decode_get_row_remap_state_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(flags);
    }

    return cc ? cc : rc;
}

NsmRowRemappingCounts::NsmRowRemappingCounts(
    std::string& name, std::string& type,
    std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmRowRemappingCount: create sensor:{NAME}", "NAME",
              name.c_str());
    memoryRowRemappingCountsIntf = memoryRowRemappingIntf;
    // Seed the markers so never-polled counts are not published as 0.
    telemetryNotAvailable();
}

void NsmRowRemappingCounts::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(memoryRowRemappingCountsIntf->interface);
    std::vector<uint8_t> smbusData = {};

    auto ce = memoryRowRemappingCountsIntf->ceRowRemappingCount();
    nv::sensor_aggregation::DbusVariantType ceRowRemappingCount{ce};
    std::string propName = "ceRowRemappingCount";
    uint8_t ceRc = (ce == Sentinel<uint32_t>::notAvailable)
                       ? TELEMETRY_NOT_AVAILABLE
                       : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, ceRowRemappingCount,
        "", ceRc);

    auto ue = memoryRowRemappingCountsIntf->ueRowRemappingCount();
    propName = "ueRowRemappingCount";
    nv::sensor_aggregation::DbusVariantType ueRowRemappingCount{ue};
    uint8_t ueRc = (ue == Sentinel<uint32_t>::notAvailable)
                       ? TELEMETRY_NOT_AVAILABLE
                       : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, ueRowRemappingCount,
        "", ueRc);
#endif
}

void NsmRowRemappingCounts::updateReading(uint32_t correctable_error,
                                          uint32_t uncorrectable_error)
{
    memoryRowRemappingCountsIntf->ceRowRemappingCount(correctable_error);
    memoryRowRemappingCountsIntf->ueRowRemappingCount(uncorrectable_error);
    updateMetricOnSharedMemory();
}

void NsmRowRemappingCounts::telemetryNotAvailable()
{
    memoryRowRemappingCountsIntf->ceRowRemappingCount(
        Sentinel<uint32_t>::notAvailable);
    memoryRowRemappingCountsIntf->ueRowRemappingCount(
        Sentinel<uint32_t>::notAvailable);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmRowRemappingCounts::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_row_remapping_counts_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_row_remapping_counts_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmRowRemappingCounts::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    uint32_t correctableError;
    uint32_t uncorrectableError;
    uint16_t dataSize;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_get_row_remapping_counts_resp(
        responseMsg, responseLen, &cc, &dataSize, &reasonCode,
        &correctableError, &uncorrectableError);

    LG2_ERROR_FLT(
        "decode_get_row_remapping_counts_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(correctableError, uncorrectableError);
    }
    return cc ? cc : rc;
}

NsmRemappingAvailabilityBankCount::NsmRemappingAvailabilityBankCount(
    const std::string& name, const std::string& type,
    std::shared_ptr<MemoryRowRemappingIntf> rowRemapIntf,
    const std::string& inventoryObjPath) :
    NsmSensor(name, type), rowRemapIntf(rowRemapIntf),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmRemappingAvailabilityBankCount: create sensor:{NAME}", "NAME",
              name.c_str());
    // Seed the markers so never-polled counts are not published as 0.
    telemetryNotAvailable();
}

void NsmRemappingAvailabilityBankCount::updateReading(
    const nsm_row_remap_availability& data)
{
    rowRemapIntf->highRemappingAvailablityBankCount(data.high_remapping);
    rowRemapIntf->maxRemappingAvailablityBankCount(data.max_remapping);
    rowRemapIntf->lowRemappingAvailablityBankCount(data.low_remapping);
    rowRemapIntf->noRemappingAvailablityBankCount(data.no_remapping);
    rowRemapIntf->partialRemappingAvailablityBankCount(data.partial_remapping);
    updateMetricOnSharedMemory();
}

void NsmRemappingAvailabilityBankCount::telemetryNotAvailable()
{
    constexpr auto na = Sentinel<uint16_t>::notAvailable;
    rowRemapIntf->highRemappingAvailablityBankCount(na);
    rowRemapIntf->maxRemappingAvailablityBankCount(na);
    rowRemapIntf->lowRemappingAvailablityBankCount(na);
    rowRemapIntf->noRemappingAvailablityBankCount(na);
    rowRemapIntf->partialRemappingAvailablityBankCount(na);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmRemappingAvailabilityBankCount::genRequestMsg(eid_t eid,
                                                     uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_availability_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_row_remap_availability_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmRemappingAvailabilityBankCount::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_row_remap_availability data;
    uint16_t dataSize;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_get_row_remap_availability_resp(
        responseMsg, responseLen, &cc, &dataSize, &reasonCode, &data);

    LG2_ERROR_FLT(
        "decode_get_row_remap_availability_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(data);
    }
    return cc ? cc : rc;
}

void NsmRemappingAvailabilityBankCount::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(rowRemapIntf->interface);
    std::vector<uint8_t> smbusData = {};
    constexpr auto na = Sentinel<uint16_t>::notAvailable;

    auto maxVal = rowRemapIntf->maxRemappingAvailablityBankCount();
    nv::sensor_aggregation::DbusVariantType maxRemappingAvailablityBankCount{
        maxVal};
    std::string propName = "MaxRemappingAvailablityBankCount";
    uint8_t maxRc = (maxVal == na) ? TELEMETRY_NOT_AVAILABLE
                                   : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        maxRemappingAvailablityBankCount, "", maxRc);

    auto highVal = rowRemapIntf->highRemappingAvailablityBankCount();
    propName = "HighRemappingAvailablityBankCount";
    nv::sensor_aggregation::DbusVariantType highRemappingAvailablityBankCount{
        highVal};
    uint8_t highRc = (highVal == na) ? TELEMETRY_NOT_AVAILABLE
                                     : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        highRemappingAvailablityBankCount, "", highRc);

    auto lowVal = rowRemapIntf->lowRemappingAvailablityBankCount();
    propName = "LowRemappingAvailablityBankCount";
    nv::sensor_aggregation::DbusVariantType lowRemappingAvailablityBankCount{
        lowVal};
    uint8_t lowRc = (lowVal == na) ? TELEMETRY_NOT_AVAILABLE
                                   : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        lowRemappingAvailablityBankCount, "", lowRc);

    auto partialVal = rowRemapIntf->partialRemappingAvailablityBankCount();
    propName = "PartialRemappingAvailablityBankCount";
    nv::sensor_aggregation::DbusVariantType
        partialRemappingAvailablityBankCount{partialVal};
    uint8_t partialRc = (partialVal == na) ? TELEMETRY_NOT_AVAILABLE
                                           : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        partialRemappingAvailablityBankCount, "", partialRc);

    auto noVal = rowRemapIntf->noRemappingAvailablityBankCount();
    propName = "NoRemappingAvailablityBankCount";
    nv::sensor_aggregation::DbusVariantType noRemappingAvailablityBankCount{
        noVal};
    uint8_t noRc = (noVal == na) ? TELEMETRY_NOT_AVAILABLE
                                 : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        noRemappingAvailablityBankCount, "", noRc);

#endif
}

NsmEccErrorCountsDram::NsmEccErrorCountsDram(
    std::string& name, std::string& type,
    std::shared_ptr<EccModeIntfDram> eccIntf, std::string& inventoryObjPath) :
    NsmSensor(name, type), eccIntf(eccIntf), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmEccErrorCounts: create sensor:{NAME}", "NAME", name.c_str());
    // Seed the markers so never-polled counts are not published as 0.
    telemetryNotAvailable();
}

void NsmEccErrorCountsDram ::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(eccIntf->interface);
    std::vector<uint8_t> smbusData = {};

    auto ce = static_cast<int64_t>(eccIntf->ceCount());
    std::string propName = "ceCount";
    nv::sensor_aggregation::DbusVariantType ceCount{ce};
    uint8_t ceRc = (ce == Sentinel<int64_t>::notAvailable)
                       ? TELEMETRY_NOT_AVAILABLE
                       : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, ceCount, "", ceRc);

    auto ue = static_cast<int64_t>(eccIntf->ueCount());
    propName = "ueCount";
    nv::sensor_aggregation::DbusVariantType ueCount{ue};
    uint8_t ueRc = (ue == Sentinel<int64_t>::notAvailable)
                       ? TELEMETRY_NOT_AVAILABLE
                       : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, ueCount, "", ueRc);
#endif
}

void NsmEccErrorCountsDram::updateReading(
    struct nsm_ECC_error_counts errorCounts)
{
    eccIntf->ceCount(errorCounts.dram_corrected);
    eccIntf->ueCount(errorCounts.dram_uncorrected);
    updateMetricOnSharedMemory();
}

void NsmEccErrorCountsDram::telemetryNotAvailable()
{
    // Stamp the int64 marker on the int64 D-Bus properties directly; the wire
    // struct's uint32 fields cannot represent the int64 marker.
    eccIntf->ceCount(Sentinel<int64_t>::notAvailable);
    eccIntf->ueCount(Sentinel<int64_t>::notAvailable);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmEccErrorCountsDram::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_ECC_error_counts_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_ECC_error_counts_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmEccErrorCountsDram::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_ECC_error_counts errorCounts;
    uint16_t dataSize;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_get_ECC_error_counts_resp(
        responseMsg, responseLen, &cc, &dataSize, &reasonCode, &errorCounts);

    LG2_ERROR_FLT(
        "decode_get_ECC_error_counts_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(errorCounts);
    }
    return cc ? cc : rc;
}

NsmMinMemoryClockLimit::NsmMinMemoryClockLimit(
    std::string& name, std::string& type, std::shared_ptr<DimmIntf> dimmIntf) :
    NsmObject(name, type), dimmIntf(dimmIntf)
{
    lg2::info("NsmMinMemoryClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine
    NsmMinMemoryClockLimit::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MINIMUM_MEMORY_CLOCK_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmMinMemoryClockLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmMinMemoryClockLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog(
            "NsmMinMemoryClockLimit decode_get_inventory_information_resp",
            reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        std::vector<uint16_t> allowedSpeedMT = dimmIntf->allowedSpeedsMT();
        allowedSpeedMT[0] = static_cast<uint16_t>(value);
        dimmIntf->allowedSpeedsMT(allowedSpeedMT);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmMaxMemoryClockLimit::NsmMaxMemoryClockLimit(
    std::string& name, std::string& type, std::shared_ptr<DimmIntf> dimmIntf) :
    NsmObject(name, type), dimmIntf(dimmIntf)
{
    lg2::info("NsmMaxMemoryClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine
    NsmMaxMemoryClockLimit::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_MEMORY_CLOCK_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmMaxMemoryClockLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmMaxMemoryClockLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog(
            "NsmMaxMemoryClockLimit decode_get_inventory_information_resp",
            reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        std::vector<uint16_t> allowedSpeedMT = dimmIntf->allowedSpeedsMT();
        allowedSpeedMT[1] = static_cast<uint16_t>(value);
        dimmIntf->allowedSpeedsMT(allowedSpeedMT);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmMemCurrClockFreq::NsmMemCurrClockFreq(const std::string& name,
                                         const std::string& type,
                                         std::shared_ptr<DimmIntf> dimmIntf,
                                         std::string inventoryObjPath) :
    NsmSensor(name, type), dimmIntf(dimmIntf),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmMemCurrClockFreq: create sensor:{NAME}", "NAME",
              name.c_str());
    updateReading(Sentinel<uint16_t>::notAvailable);
}

void NsmMemCurrClockFreq::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(dimmIntf->interface);
    auto speed = dimmIntf->memoryConfiguredSpeedInMhz();
    nv::sensor_aggregation::DbusVariantType valueVariant{speed};
    std::vector<uint8_t> smbusData = {};
    std::string propName = "MemoryConfiguredSpeedInMhz";
    uint8_t rc = (speed == Sentinel<uint16_t>::notAvailable)
                     ? TELEMETRY_NOT_AVAILABLE
                     : TELEMETRY_AVAILABLE;
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, valueVariant, "", rc);
#endif
}

void NsmMemCurrClockFreq::updateReading(const uint32_t& clockFreq)
{
    dimmIntf->memoryConfiguredSpeedInMhz(clockFreq);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmMemCurrClockFreq::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_curr_clock_freq_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint8_t clock_id = MEMORY_CLOCK;
    auto rc = encode_get_curr_clock_freq_req(instanceId, clock_id, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_curr_clock_freq_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmMemCurrClockFreq::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    uint32_t clockFreq = 1;
    uint16_t dataSize;
    uint16_t reasonCode = 0;

    auto rc = decode_get_curr_clock_freq_resp(
        responseMsg, responseLen, &cc, &dataSize, &reasonCode, &clockFreq);

    LG2_ERROR_FLT(
        "decode_get_curr_clock_freq_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(clockFreq);
    }
    return cc ? cc : rc;
}

NsmMemCapacity::NsmMemCapacity(const std::string& name, const std::string& type,
                               std::shared_ptr<DimmIntf> dimmIntf) :
    NsmMemoryCapacity(name, type), dimmIntf(dimmIntf)

{
    lg2::info("NsmMemCapacity: create sensor:{NAME}", "NAME", name.c_str());
}

void NsmMemCapacity::updateReading(
    std::optional<uint32_t> maximumMemoryCapacity)
{
    if (!maximumMemoryCapacity)
    {
        lg2::debug(
            "NsmMemCapacity::updateReading unable to fetch Maximum Memory Capacity");
        return;
    }
    dimmIntf->memorySizeInKB(*maximumMemoryCapacity * 1024);
}

void createNSMMemory(std::shared_ptr<NsmDevice> nsmDevice,
                     SensorManager& manager, sdbusplus::bus_t& bus,
                     std::string& name, std::string& type,
                     std::string& inventoryObjPath,
                     const dbus::PropertyMap& allCurrentIfaceProperties,
                     const std::vector<utils::Association>& associations)
{
    auto sensorObjectPath = inventoryObjPath +
                            "/xyz.openbmc_project.Inventory.Item.Dimm";

    std::shared_ptr<DimmIntf> dimmIntf = getInterfaceOnObjectPath<DimmIntf>(
        sensorObjectPath, manager, bus, inventoryObjPath.c_str());

    std::string correctionType{};
    if (allCurrentIfaceProperties.count("ErrorCorrection"))
    {
        correctionType = std::get<std::string>(
            allCurrentIfaceProperties.at("ErrorCorrection"));
    }

    auto sensorErrorCorrection = std::make_shared<NsmMemoryErrorCorrection>(
        name, type, dimmIntf, correctionType, inventoryObjPath);
    nsmDevice->addDeviceSensors(sensorErrorCorrection);
    std::string deviceType{};
    if (allCurrentIfaceProperties.count("DeviceType"))
    {
        deviceType =
            std::get<std::string>(allCurrentIfaceProperties.at("DeviceType"));
    }

    auto sensorDeviceType = std::make_shared<NsmMemoryDeviceType>(
        name, type, dimmIntf, deviceType, inventoryObjPath);
    nsmDevice->addDeviceSensors(sensorDeviceType);
    auto sensorHealth = std::make_shared<NsmMemoryHealth>(bus, name, type,
                                                          inventoryObjPath);
    nsmDevice->addDeviceSensors(sensorHealth);
    auto sensorMemoryLocation = std::make_shared<NsmLocationIntfMemory>(
        bus, name, type, inventoryObjPath);
    nsmDevice->addDeviceSensors(sensorMemoryLocation);
    auto associationSensor = std::make_shared<NsmMemoryAssociation>(
        bus, name, type, inventoryObjPath, associations);
    nsmDevice->addDeviceSensors(associationSensor);

    bool priority = false;
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }

    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    auto minMemoryClockSensor =
        std::make_shared<NsmMinMemoryClockLimit>(name, type, dimmIntf);
    nsmDevice->addStaticSensor(minMemoryClockSensor);
    auto maxMemoryClockSensor =
        std::make_shared<NsmMaxMemoryClockLimit>(name, type, dimmIntf);
    nsmDevice->addStaticSensor(maxMemoryClockSensor);

    auto currClockFreqSensor = std::make_shared<NsmMemCurrClockFreq>(
        name, type, dimmIntf, inventoryObjPath);

    nsmDevice->addSensor(currClockFreqSensor, priority);
    auto memCapacitySensor = std::make_shared<NsmMemCapacity>(name, type,
                                                              dimmIntf);
    nsmDevice->addStaticSensor(memCapacitySensor);
}

void createMemoryRowRemapping(std::shared_ptr<NsmDevice> nsmDevice,
                              sdbusplus::bus_t& bus, std::string& name,
                              std::string& type, std::string& inventoryObjPath)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    bool priority = false;

    auto sensorRowRemapState = std::make_shared<NsmRowRemapState>(
        name, type, rowRemapIntf, inventoryObjPath);
    auto sensorRowRemappingCounts = std::make_shared<NsmRowRemappingCounts>(
        name, type, rowRemapIntf, inventoryObjPath);
    auto remappingAvailabilitySensor =
        std::make_shared<NsmRemappingAvailabilityBankCount>(
            name, type, rowRemapIntf, inventoryObjPath);

    nsmDevice->addSensor(sensorRowRemapState, priority);
    nsmDevice->addSensor(sensorRowRemappingCounts, priority);
    nsmDevice->addSensor(remappingAvailabilitySensor, priority);
}

void createMemoryECCMode(std::shared_ptr<NsmDevice> nsmDevice,
                         sdbusplus::bus_t& bus, std::string& name,
                         std::string& type, std::string& inventoryObjPath)
{
    bool priority = false;

    auto eccModeIntf =
        std::make_shared<EccModeIntfDram>(bus, inventoryObjPath.c_str());
    auto sensor = std::make_shared<NsmEccErrorCountsDram>(
        name, type, eccModeIntf, inventoryObjPath);

    nsmDevice->addSensor(sensor, priority);
}

void createMemoryMemCapacityUtil(std::shared_ptr<NsmDevice> nsmDevice,
                                 std::string& name, std::string& type,
                                 std::string& inventoryObjPath)
{
    bool priority = false;

    auto isLongRunning = true;

    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(name, type);
    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        name, type, dbus::Interfaces{inventoryObjPath});
    auto sensor = std::make_shared<NsmMemoryCapacityUtil>(
        provider, totalMemorySensor, isLongRunning, nsmDevice);
    nsmDevice->addSensor(sensor, priority, isLongRunning);
}

requester::Coroutine createNsmMemorySensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath)
{
    {
        auto& bus = utils::DBusHandler::getBus();
        dbus::PropertyMap allBaseIfaceProperties;
        auto rc = co_await utils::coGetCachedBaseProperties(
            objPath, MEMORY_INTERFACE, allBaseIfaceProperties);
        if (rc != NSM_SUCCESS)
        {
            co_return rc;
        }
        auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
            utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

        std::string name{};
        if (allBaseIfaceProperties.count("Name"))
        {
            name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
        }
        uuid_t uuid{};
        if (allBaseIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
        }
        std::string type{};
        if (allCurrentIfaceProperties.count("Type"))
        {
            type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
        }
        std::string inventoryObjPath{};
        if (allBaseIfaceProperties.count("InventoryObjPath"))
        {
            inventoryObjPath = std::get<std::string>(
                allBaseIfaceProperties.at("InventoryObjPath"));
        }

        inventoryObjPath = inventoryObjPath + "_DRAM_0";
        auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_Processor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            // coverity[missing_return]
            co_return NSM_ERROR;
        }

        if (type == "NSM_Memory")
        {
            std::vector<utils::Association> associations{};
            co_await utils::coGetAssociations(
                objPath, interface + ".Associations", associations);
            createNSMMemory(nsmDevice, manager, bus, name, type,
                            inventoryObjPath, allCurrentIfaceProperties,
                            associations);
        }
        else if (type == "NSM_Memory_Attributes")
        {
            createMemoryRowRemapping(nsmDevice, bus, name, type,
                                     inventoryObjPath);
            createMemoryECCMode(nsmDevice, bus, name, type, inventoryObjPath);
            createMemoryMemCapacityUtil(nsmDevice, name, type,
                                        inventoryObjPath);
        }
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

dbus::Interfaces memoryInterfaces{
    "xyz.openbmc_project.Configuration.NSM_Memory",
    "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes"};

REGISTER_NSM_CREATION_FUNCTION(createNsmMemorySensor, memoryInterfaces)

} // namespace nsm
