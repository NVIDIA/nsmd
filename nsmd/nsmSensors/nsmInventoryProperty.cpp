#include "nsmInventoryProperty.hpp"

#include "nsmInterface.hpp"

namespace nsm
{

NsmInventoryProperty::NsmInventoryProperty(
    std::shared_ptr<NsmInterfaceProvider<AssetIntf>> pdi,
    nsm_inventory_property_identifiers property) :
    NsmSensor(*pdi), NsmInterfaceContainer(pdi), property(property)
{}
NsmInventoryProperty::NsmInventoryProperty(
    std::shared_ptr<NsmInterfaceProvider<DimensionIntf>> pdi,
    nsm_inventory_property_identifiers property) :
    NsmSensor(*pdi), NsmInterfaceContainer(pdi), property(property)
{}
NsmInventoryProperty::NsmInventoryProperty(
    std::shared_ptr<NsmInterfaceProvider<PowerLimitIntf>> pdi,
    nsm_inventory_property_identifiers property) :
    NsmSensor(*pdi), NsmInterfaceContainer(pdi), property(property)
{}

std::optional<Request> NsmInventoryProperty::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, uint8_t(property), requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmInventoryProperty::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    Response data(65535, 0);

    auto rc = decode_get_inventory_information_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize, data.data());
    if (rc)
    {
        lg2::error(
            "responseHandler: decode_get_inventory_information_resp failed. property={NUM} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NUM", int(property), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return rc;
    }

    if (cc == NSM_SUCCESS)
    {
        switch (property)
        {
            case BOARD_PART_NUMBER:
                cast<AssetIntf>().partNumber(
                    std::string((char*)data.data(), dataSize));
                break;
            case SERIAL_NUMBER:
                cast<AssetIntf>().serialNumber(
                    std::string((char*)data.data(), dataSize));
                break;
            case MARKETING_NAME:
                cast<AssetIntf>().model(
                    std::string((char*)data.data(), dataSize));
                break;
            case PRODUCT_LENGTH:
                cast<DimensionIntf>().depth(
                    decode_inventory_information_as_uint(data.data()));
                break;
            case PRODUCT_HEIGHT:
                cast<DimensionIntf>().height(
                    decode_inventory_information_as_uint(data.data()));
                break;
            case PRODUCT_WIDTH:
                cast<DimensionIntf>().width(
                    decode_inventory_information_as_uint(data.data()));
                break;
            case MINIMUM_DEVICE_POWER_LIMIT:
                cast<PowerLimitIntf>().minPowerWatts(
                    decode_inventory_information_as_uint(data.data()));
                break;
            case MAXIMUM_DEVICE_POWER_LIMIT:
                cast<PowerLimitIntf>().maxPowerWatts(
                    decode_inventory_information_as_uint(data.data()));
                break;
            default:
                throw std::runtime_error("Not implemented PDI");
                break;
        }
    }
    else
    {
        lg2::error(
            "responseHandler: decode_get_inventory_information_resp is not success CC. property={NUM} rc={RC}",
            "NUM", int(property), "RC", rc);
        return rc;
    }

    return cc;
}

} // namespace nsm