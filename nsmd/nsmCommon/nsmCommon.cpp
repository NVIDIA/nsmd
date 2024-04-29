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

} // namespace nsm