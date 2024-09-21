#include "nsmCommon.hpp"

#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmMemoryCapacity::NsmMemoryCapacity(const std::string& name,
                                     const std::string& type) :
    NsmSensor(name, type)

{}

std::optional<std::vector<uint8_t>>
    NsmMemoryCapacity::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, MAXIMUM_MEMORY_CAPACITY, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_inventory_information_req failed for property Maximum Memory Capacity "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmMemoryCapacity::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    std::vector<uint8_t> data(65535, 0);
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_inventory_information_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        uint32_t* maximumMemoryCapacityMiB =
            reinterpret_cast<uint32_t*>(data.data());
        updateReading(maximumMemoryCapacityMiB);
        clearErrorBitMap(
            "decode_get_inventory_information_resp for Maximum Memory Capacity");
    }
    else
    {
        logHandleResponseMsg(
            "decode_get_inventory_information_resp for Maximum Memory Capacity",
            reason_code, cc, rc);
        updateReading(NULL);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmTotalMemory::NsmTotalMemory(const std::string& name,
                               const std::string& type) :
    NsmMemoryCapacity(name, type)
{
    lg2::info("NsmTotalMemory : create sensor:{NAME}", "NAME", name.c_str());
}

void NsmTotalMemory::updateReading(uint32_t* maximumMemoryCapacity)
{
    if (maximumMemoryCapacity)
    {
        totalMemoryCapacity = new uint32_t(*maximumMemoryCapacity);
    }
    else
    {
        totalMemoryCapacity = nullptr;
    }
}

const uint32_t* NsmTotalMemory::getReading()
{
    return totalMemoryCapacity;
}

NsmMemoryCapacityUtil::NsmMemoryCapacityUtil(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    std::string& inventoryObjPath, std::shared_ptr<NsmTotalMemory> totalMemory,
    bool isLongRunning) :
    NsmSensor(name, type, isLongRunning),
    totalMemory(totalMemory), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmMemoryCapacityUtil: create sensor:{NAME}", "NAME",
              name.c_str());
    dimmMemoryMetricsIntf =
        std::make_unique<DimmMemoryMetricsIntf>(bus, inventoryObjPath.c_str());
    updateMetricOnSharedMemory();
}

void NsmMemoryCapacityUtil::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(dimmMemoryMetricsIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "CapacityUtilizationPercent";
    nv::sensor_aggregation::DbusVariantType capacityUtilizationPercent{
        static_cast<uint16_t>(
            dimmMemoryMetricsIntf->capacityUtilizationPercent())};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 capacityUtilizationPercent);

#endif
}

void NsmMemoryCapacityUtil::updateReading(
    const struct nsm_memory_capacity_utilization& data)
{
    const uint32_t* totalMemoryCapacity = totalMemory->getReading();
    if (totalMemoryCapacity == NULL)
    {
        lg2::error(
            "NsmMemoryCapacityUtil::updateReading unable to fetch total Memory Capacity data");
        return;
    }
    else if ((*totalMemoryCapacity) == 0)
    {
        lg2::error(
            "NsmMemoryCapacityUtil::updateReading total Memory Capacity value is {A}",
            "A", (*totalMemoryCapacity));
        return;
    }

    uint8_t usedMemoryPercent = (data.used_memory + data.reserved_memory) *
                                100 / (*totalMemoryCapacity);
    dimmMemoryMetricsIntf->capacityUtilizationPercent(usedMemoryPercent);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmMemoryCapacityUtil::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_memory_capacity_util_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_memory_capacity_util_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmMemoryCapacityUtil::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    struct nsm_memory_capacity_utilization data;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_memory_capacity_util_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(data);
        clearErrorBitMap("decode_get_memory_capacity_util_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_memory_capacity_util_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmMinGraphicsClockLimit::NsmMinGraphicsClockLimit(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmObject(name, type),
    cpuOperatingConfigIntf(cpuConfigIntf), inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmMinGraphicsClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());

    updateMetricOnSharedMemory();
}

requester::Coroutine NsmMinGraphicsClockLimit::update(SensorManager& manager,
                                                      eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MINIMUM_GRAPHICS_CLOCK_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmMinGraphicsClockLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
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
        lg2::error(
            "NsmMinGraphicsClockLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
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
        cpuOperatingConfigIntf->minSpeed(value);
        updateMetricOnSharedMemory();
    }
    else
    {
        lg2::error(
            "NsmMinGraphicsClockLimit decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

void NsmMinGraphicsClockLimit::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "MinSpeed";
    nv::sensor_aggregation::DbusVariantType minSpeedVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::minSpeed()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, minSpeedVal);
#endif
}

NsmMaxGraphicsClockLimit::NsmMaxGraphicsClockLimit(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmObject(name, type),
    cpuOperatingConfigIntf(cpuConfigIntf), inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmMaxGraphicsClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());

    updateMetricOnSharedMemory();
}

requester::Coroutine NsmMaxGraphicsClockLimit::update(SensorManager& manager,
                                                      eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_GRAPHICS_CLOCK_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmMaxGraphicsClockLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
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
        lg2::error(
            "NsmMaxGraphicsClockLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
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
        cpuOperatingConfigIntf->maxSpeed(value);
        updateMetricOnSharedMemory();
    }
    else
    {
        lg2::error(
            "NsmMaxGraphicsClockLimit decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

void NsmMaxGraphicsClockLimit::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "MaxSpeed";
    nv::sensor_aggregation::DbusVariantType maxSpeedVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::maxSpeed()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, maxSpeedVal);
#endif
}

} // namespace nsm
