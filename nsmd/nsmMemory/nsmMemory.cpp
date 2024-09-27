#include "nsmMemory.hpp"

#include "platform-environmental.h"

#include "dBusAsyncUtils.hpp"
#include "interfaceWrapper.hpp"
#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>
#define MEMORY_INTERFACE "xyz.openbmc_project.Configuration.NSM_Memory"

namespace nsm
{

NsmMemoryErrorCorrection::NsmMemoryErrorCorrection(
    std::string& name, std::string& type, std::shared_ptr<DimmIntf> dimmIntf,
    std::string& correctionType, std::string& inventoryObjPath) :
    NsmObject(name, type),
    dimmIntf(dimmIntf), inventoryObjPath(inventoryObjPath)
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
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData, eccValue);
#endif
}

NsmMemoryDeviceType::NsmMemoryDeviceType(std::string& name, std::string& type,
                                         std::shared_ptr<DimmIntf> dimmIntf,
                                         std::string& memoryType,
                                         std::string& inventoryObjPath) :
    NsmObject(name, type),
    dimmIntf(dimmIntf), inventoryObjPath(inventoryObjPath)
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
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, memoryTypeValue);
#endif
}

NsmLocationIntfMemory::NsmLocationIntfMemory(sdbusplus::bus::bus& bus,
                                             std::string& name,
                                             std::string& type,
                                             std::string& inventoryObjPath) :
    NsmObject(name, type),
    inventoryObjPath(inventoryObjPath)
{
    locationIntf =
        std::make_unique<LocationIntfMemory>(bus, inventoryObjPath.c_str());

    locationIntf->locationType(LocationTypesMemory::Embedded);
}

NsmMemoryHealth::NsmMemoryHealth(sdbusplus::bus::bus& bus, std::string& name,
                                 std::string& type,
                                 std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    healthIntf = std::make_unique<MemoryHealthIntf>(bus,
                                                    inventoryObjPath.c_str());
    healthIntf->health(MemoryHealthType::OK);
}

NsmMemoryAssociation::NsmMemoryAssociation(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath,
    const std::vector<utils::Association>& associations) :
    NsmObject(name, type)
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
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

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

    nv::sensor_aggregation::DbusVariantType rowRemappingFailureStateVal{
        memoryRowRemappingStateIntf->rowRemappingFailureState()};
    std::string propName = "RowRemappingFailureState";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 rowRemappingFailureStateVal);

    nv::sensor_aggregation::DbusVariantType rowRemappingPendingStateVal{
        memoryRowRemappingStateIntf->rowRemappingPendingState()};
    propName = "RowRemappingPendingState";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 rowRemappingPendingStateVal);
#endif
}

void NsmRowRemapState::updateReading(bitfield8_t flags)
{
    memoryRowRemappingStateIntf->rowRemappingFailureState(flags.bits.bit0);
    memoryRowRemappingStateIntf->rowRemappingPendingState(flags.bits.bit1);
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
    uint8_t cc = NSM_ERROR;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_row_remap_state_resp(responseMsg, responseLen, &cc,
                                              &data_size, &reason_code, &flags);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(flags);
        clearErrorBitMap("decode_get_row_remap_state_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_row_remap_state_resp", reason_code, cc,
                             rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmRowRemappingCounts::NsmRowRemappingCounts(
    std::string& name, std::string& type,
    std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmRowRemappingCount: create sensor:{NAME}", "NAME",
              name.c_str());
    memoryRowRemappingCountsIntf = memoryRowRemappingIntf;
    updateMetricOnSharedMemory();
}

void NsmRowRemappingCounts::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(memoryRowRemappingCountsIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType ceRowRemappingCount{
        memoryRowRemappingCountsIntf->ceRowRemappingCount()};
    std::string propName = "ceRowRemappingCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ceRowRemappingCount);

    propName = "ueRowRemappingCount";
    nv::sensor_aggregation::DbusVariantType ueRowRemappingCount{
        memoryRowRemappingCountsIntf->ueRowRemappingCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ueRowRemappingCount);
#endif
}

void NsmRowRemappingCounts::updateReading(uint32_t correctable_error,
                                          uint32_t uncorrectable_error)
{
    memoryRowRemappingCountsIntf->ceRowRemappingCount(correctable_error);
    memoryRowRemappingCountsIntf->ueRowRemappingCount(uncorrectable_error);
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
    uint8_t cc = NSM_ERROR;
    uint32_t correctable_error;
    uint32_t uncorrectable_error;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_row_remapping_counts_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code,
        &correctable_error, &uncorrectable_error);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(correctable_error, uncorrectable_error);
        clearErrorBitMap("decode_get_row_remapping_counts_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_row_remapping_counts_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmRemappingAvailabilityBankCount::NsmRemappingAvailabilityBankCount(
    const std::string& name, const std::string& type,
    std::shared_ptr<MemoryRowRemappingIntf> rowRemapIntf,
    const std::string& inventoryObjPath) :
    NsmSensor(name, type),
    rowRemapIntf(rowRemapIntf), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmRemappingAvailabilityBankCount: create sensor:{NAME}", "NAME",
              name.c_str());
    updateMetricOnSharedMemory();
}

void NsmRemappingAvailabilityBankCount::updateReading(
    const nsm_row_remap_availability& data)
{
    rowRemapIntf->highRemappingAvailablityBankCount(data.high_remapping);
    rowRemapIntf->maxRemappingAvailablityBankCount(data.max_remapping);
    rowRemapIntf->lowRemappingAvailablityBankCount(data.low_remapping);
    rowRemapIntf->noRemappingAvailablityBankCount(data.no_remapping);
    rowRemapIntf->partialRemappingAvailablityBankCount(data.partial_remapping);
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
    uint8_t cc = NSM_ERROR;
    struct nsm_row_remap_availability data;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_row_remap_availability_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(data);
        updateMetricOnSharedMemory();
        clearErrorBitMap("decode_get_row_remap_availability_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_row_remap_availability_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

void NsmRemappingAvailabilityBankCount::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(rowRemapIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType maxRemappingAvailablityBankCount{
        rowRemapIntf->maxRemappingAvailablityBankCount()};
    std::string propName = "MaxRemappingAvailablityBankCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        maxRemappingAvailablityBankCount);

    propName = "HighRemappingAvailablityBankCount";
    nv::sensor_aggregation::DbusVariantType highRemappingAvailablityBankCount{
        rowRemapIntf->highRemappingAvailablityBankCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        highRemappingAvailablityBankCount);

    propName = "LowRemappingAvailablityBankCount";
    nv::sensor_aggregation::DbusVariantType lowRemappingAvailablityBankCount{
        rowRemapIntf->lowRemappingAvailablityBankCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        lowRemappingAvailablityBankCount);

    propName = "PartialRemappingAvailablityBankCount";
    nv::sensor_aggregation::DbusVariantType
        partialRemappingAvailablityBankCount{
            rowRemapIntf->partialRemappingAvailablityBankCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        partialRemappingAvailablityBankCount);

    propName = "NoRemappingAvailablityBankCount";
    nv::sensor_aggregation::DbusVariantType noRemappingAvailablityBankCount{
        rowRemapIntf->noRemappingAvailablityBankCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        noRemappingAvailablityBankCount);

#endif
}

NsmEccErrorCountsDram::NsmEccErrorCountsDram(
    std::string& name, std::string& type,
    std::shared_ptr<EccModeIntfDram> eccIntf, std::string& inventoryObjPath) :
    NsmSensor(name, type),
    eccIntf(eccIntf), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmEccErrorCounts: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmEccErrorCountsDram ::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(eccIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "ceCount";
    nv::sensor_aggregation::DbusVariantType ceCount{
        static_cast<int64_t>(eccIntf->ceCount())};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData, ceCount);
    propName = "ueCount";
    nv::sensor_aggregation::DbusVariantType ueCount{
        static_cast<int64_t>(eccIntf->ueCount())};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData, ueCount);
#endif
}

void NsmEccErrorCountsDram::updateReading(
    struct nsm_ECC_error_counts errorCounts)
{
    eccIntf->ceCount(errorCounts.dram_corrected);
    eccIntf->ueCount(errorCounts.dram_uncorrected);
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
    uint8_t cc = NSM_ERROR;
    struct nsm_ECC_error_counts errorCounts;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_ECC_error_counts_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &errorCounts);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(errorCounts);
        clearErrorBitMap("decode_get_ECC_error_counts_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_ECC_error_counts_resp", reason_code,
                             cc, rc);

        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmMinMemoryClockLimit::NsmMinMemoryClockLimit(
    std::string& name, std::string& type, std::shared_ptr<DimmIntf> dimmIntf) :
    NsmObject(name, type),
    dimmIntf(dimmIntf)
{
    lg2::info("NsmMinMemoryClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine NsmMinMemoryClockLimit::update(SensorManager& manager,
                                                    eid_t eid)
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
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmMinMemoryClockLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        std::vector<uint16_t> allowedSpeedMT = dimmIntf->allowedSpeedsMT();
        allowedSpeedMT[0] = static_cast<uint16_t>(value);
        dimmIntf->allowedSpeedsMT(allowedSpeedMT);
        clearErrorBitMap(
            "NsmMinMemoryClockLimit decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmMinMemoryClockLimit decode_get_inventory_information_resp",
            reason_code, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

NsmMaxMemoryClockLimit::NsmMaxMemoryClockLimit(
    std::string& name, std::string& type, std::shared_ptr<DimmIntf> dimmIntf) :
    NsmObject(name, type),
    dimmIntf(dimmIntf)
{
    lg2::info("NsmMaxMemoryClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine NsmMaxMemoryClockLimit::update(SensorManager& manager,
                                                    eid_t eid)
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
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmMaxMemoryClockLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        std::vector<uint16_t> allowedSpeedMT = dimmIntf->allowedSpeedsMT();
        allowedSpeedMT[1] = static_cast<uint16_t>(value);
        dimmIntf->allowedSpeedsMT(allowedSpeedMT);
        clearErrorBitMap(
            "NsmMaxMemoryClockLimit decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmMaxMemoryClockLimit decode_get_inventory_information_resp",
            reason_code, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

NsmMemCurrClockFreq::NsmMemCurrClockFreq(const std::string& name,
                                         const std::string& type,
                                         std::shared_ptr<DimmIntf> dimmIntf,
                                         std::string inventoryObjPath) :
    NsmSensor(name, type),
    dimmIntf(dimmIntf), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmMemCurrClockFreq: create sensor:{NAME}", "NAME",
              name.c_str());
    updateMetricOnSharedMemory();
}

void NsmMemCurrClockFreq::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(dimmIntf->interface);
    nv::sensor_aggregation::DbusVariantType valueVariant{
        dimmIntf->memoryConfiguredSpeedInMhz()};
    std::vector<uint8_t> smbusData = {};
    std::string propName = "MemoryConfiguredSpeedInMhz";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, valueVariant);
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
    uint8_t cc = NSM_ERROR;
    uint32_t clockFreq = 1;
    uint16_t data_size;
    uint16_t reason_code;

    auto rc = decode_get_curr_clock_freq_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &clockFreq);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(clockFreq);
        clearErrorBitMap("decode_get_curr_clock_freq_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_curr_clock_freq_resp", reason_code, cc,
                             rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmMemCapacity::NsmMemCapacity(const std::string& name, const std::string& type,
                               std::shared_ptr<DimmIntf> dimmIntf) :
    NsmMemoryCapacity(name, type),
    dimmIntf(dimmIntf)

{
    lg2::info("NsmMemCapacity: create sensor:{NAME}", "NAME", name.c_str());
}

void NsmMemCapacity::updateReading(uint32_t* maximumMemoryCapacity)
{
    if (maximumMemoryCapacity == NULL)
    {
        lg2::debug(
            "NsmMemCapacity::updateReading unable to fetch Maximum Memory Capacity");
        return;
    }
    dimmIntf->memorySizeInKB(*maximumMemoryCapacity * 1024);
}

static requester::Coroutine createNsmMemorySensor(SensorManager& manager,
                                                  const std::string& interface,
                                                  const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", MEMORY_INTERFACE);

        auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", MEMORY_INTERFACE);

        auto type = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "InventoryObjPath", MEMORY_INTERFACE);
        inventoryObjPath = inventoryObjPath + "_DRAM_0";
        auto nsmDevice = manager.getNsmDevice(uuid);
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
            auto sensorObjectPath = inventoryObjPath +
                                    "/xyz.openbmc_project.Inventory.Item.Dimm";

            std::shared_ptr<DimmIntf> dimmIntf =
                retrieveInterfaceFromSensorMap<DimmIntf>(
                    sensorObjectPath, manager, bus, inventoryObjPath.c_str());

            auto correctionType =
                co_await utils::coGetDbusProperty<std::string>(
                    objPath.c_str(), "ErrorCorrection", interface.c_str());
            auto sensorErrorCorrection =
                std::make_shared<NsmMemoryErrorCorrection>(
                    name, type, dimmIntf, correctionType, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensorErrorCorrection);
            auto deviceType = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "DeviceType", interface.c_str());
            auto sensorDeviceType = std::make_shared<NsmMemoryDeviceType>(
                name, type, dimmIntf, deviceType, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensorDeviceType);
            auto sensorHealth = std::make_shared<NsmMemoryHealth>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensorHealth);
            auto sensorMemoryLocation = std::make_shared<NsmLocationIntfMemory>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensorMemoryLocation);
            std::vector<utils::Association> associations{};
            co_await utils::coGetAssociations(
                objPath, interface + ".Associations", associations);
            auto associationSensor = std::make_shared<NsmMemoryAssociation>(
                bus, name, type, inventoryObjPath, associations);
            nsmDevice->deviceSensors.push_back(associationSensor);

            auto priority = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
            auto minMemoryClockSensor =
                std::make_shared<NsmMinMemoryClockLimit>(name, type, dimmIntf);
            nsmDevice->addStaticSensor(minMemoryClockSensor);
            auto maxMemoryClockSensor =
                std::make_shared<NsmMaxMemoryClockLimit>(name, type, dimmIntf);
            nsmDevice->addStaticSensor(maxMemoryClockSensor);

            auto currClockFreqSensor = std::make_shared<NsmMemCurrClockFreq>(
                name, type, dimmIntf, inventoryObjPath);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(currClockFreqSensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(currClockFreqSensor);
            }
            auto memCapacitySensor =
                std::make_shared<NsmMemCapacity>(name, type, dimmIntf);
            nsmDevice->addStaticSensor(memCapacitySensor);
        }
        else if (type == "NSM_RowRemapping")
        {
            auto rowRemapIntf = std::make_shared<MemoryRowRemappingIntf>(
                bus, inventoryObjPath.c_str());
            auto priority = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto sensorRowRemapState = std::make_shared<NsmRowRemapState>(
                name, type, rowRemapIntf, inventoryObjPath);
            auto sensorRowRemappingCounts =
                std::make_shared<NsmRowRemappingCounts>(
                    name, type, rowRemapIntf, inventoryObjPath);
            auto remappingAvailabilitySensor =
                std::make_shared<NsmRemappingAvailabilityBankCount>(
                    name, type, rowRemapIntf, inventoryObjPath);

            nsmDevice->addSensor(sensorRowRemapState, priority);
            nsmDevice->addSensor(sensorRowRemappingCounts, priority);
            nsmDevice->addSensor(remappingAvailabilitySensor, priority);
        }
        else if (type == "NSM_ECC")
        {
            auto priority = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto eccModeIntf = std::make_shared<EccModeIntfDram>(
                bus, inventoryObjPath.c_str());
            auto sensor = std::make_shared<NsmEccErrorCountsDram>(
                name, type, eccModeIntf, inventoryObjPath);
            if (priority)
            {
                nsmDevice->prioritySensors.push_back(sensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(sensor);
            }
        }
        else if (type == "NSM_MemCapacityUtil")
        {
            auto priority = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            bool isLongRunning{false};
            try
            {
                isLongRunning = co_await utils::coGetDbusProperty<bool>(
                    objPath.c_str(), "LongRunning", interface.c_str());
            }
            catch (...)
            {}

            auto totalMemorySensor = std::make_shared<NsmTotalMemory>(name,
                                                                      type);
            auto sensor = std::make_shared<NsmMemoryCapacityUtil>(
                bus, name, type, inventoryObjPath, totalMemorySensor,
                isLongRunning);

            nsmDevice->addSensor(sensor, priority, isLongRunning);
            nsmDevice->addSensor(totalMemorySensor, priority);
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createNsmMemorySensor,
                               "xyz.openbmc_project.Configuration.NSM_Memory")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmMemorySensor,
    "xyz.openbmc_project.Configuration.NSM_Memory.ECCMode")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmMemorySensor,
    "xyz.openbmc_project.Configuration.NSM_Memory.RowRemapping")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmMemorySensor,
    "xyz.openbmc_project.Configuration.NSM_Memory.MemCapacityUtil")

} // namespace nsm
