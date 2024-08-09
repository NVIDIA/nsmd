#include "nsmCommon.hpp"

#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm{

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
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_inventory_information_resp for Maximum Memory Capacity"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
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
    std::string& inventoryObjPath,
    std::shared_ptr<NsmTotalMemory> totalMemory) :
    NsmSensor(name, type),
    totalMemory(totalMemory),inventoryObjPath(inventoryObjPath)

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
        dimmMemoryMetricsIntf->capacityUtilizationPercent()};
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
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_memory_capacity_util_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

} // namespace nsm