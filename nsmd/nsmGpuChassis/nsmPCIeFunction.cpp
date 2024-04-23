#include "nsmPCIeFunction.hpp"

#include "libnsm/pci-links.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeFunction::NsmPCIeFunction(
    std::shared_ptr<NsmInterfaceProvider<PCIeDeviceIntf>> pdi, uint8_t deviceId,
    uint8_t functionId) :
    NsmSensor(*pdi), NsmInterfaceContainer(pdi), deviceId(deviceId),
    functionId(functionId)
{}

std::optional<Request> NsmPCIeFunction::genRequestMsg(eid_t eid,
                                                      uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, deviceId,
                                                         0, requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeFunction::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_0 data = {};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);
    if (rc)
    {
        lg2::error(
            "responseHandler: decode_query_scalar_group_telemetry_v1_group0_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return rc;
    }

    if (cc != NSM_SUCCESS)
    {
        memset(&data, 0, sizeof(data));
        lg2::error(
            "responseHandler: decode_query_scalar_group_telemetry_v1_group0_resp is not success CC. rc={RC}",
            "RC", rc);
    }

#define pcieFunction(X)                                                        \
    pdi->function##X##VendorId(                              \
        std::to_string(data.pci_vendor_id));                                   \
    pdi->function##X##DeviceId(                              \
        std::to_string(data.pci_device_id));                                   \
    pdi->function##X##SubsystemVendorId(                     \
        std::to_string(data.pci_subsystem_vendor_id));                         \
    pdi->function##X##SubsystemId(                           \
        std::to_string(data.pci_subsystem_device_id));

    switch (functionId)
    {
        case 0:
            pcieFunction(0);
            break;
        case 1:
            pcieFunction(1);
            break;
        case 2:
            pcieFunction(2);
            break;
        case 3:
            pcieFunction(3);
            break;
        case 4:
            pcieFunction(4);
            break;
        case 5:
            pcieFunction(5);
            break;
        case 6:
            pcieFunction(6);
            break;
        case 7:
            pcieFunction(7);
            break;
    }

    return cc == NSM_SUCCESS ? NSM_SUCCESS : rc;
}

} // namespace nsm