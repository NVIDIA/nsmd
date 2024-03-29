#include "NVLinkManagementNICSWInventory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmSWInventoryDriverVersionAndStatus::NsmSWInventoryDriverVersionAndStatus(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& manufacturer) :
    NsmSensor(name, type)
{
    auto NVLinkManagementNICFWInvBasePath =
        std::string(sotwareInventoryBasePath) + "/" + name;

    lg2::info("NsmSWInventoryDriverVersionAndStatus: create sensor:{NAME}",
              "NAME", name.c_str());
    softwareVer_ = std::make_unique<SoftwareIntf>(
        bus, NVLinkManagementNICFWInvBasePath.c_str());
    operationalStatus_ = std::make_unique<OperationalStatusIntf>(
        bus, NVLinkManagementNICFWInvBasePath.c_str());
    associationDef_ = std::make_unique<AssociationDefinitionsInft>(
        bus, NVLinkManagementNICFWInvBasePath.c_str());

    asset_ = std::make_unique<AssetIntf>(
        bus, NVLinkManagementNICFWInvBasePath.c_str());
    asset_->manufacturer(manufacturer);
}

void NsmSWInventoryDriverVersionAndStatus::updateValue(enum8 driverState,
                                                       char driverVersion[64])
{
    softwareVer_->version(std::string(driverVersion));
    switch (driverState)
    {
        case 2:
            operationalStatus_->functional(true);
            break;
        default:
            operationalStatus_->functional(false);
            break;
    }
}

std::optional<std::vector<uint8_t>>
    NsmSWInventoryDriverVersionAndStatus::genRequestMsg(eid_t eid,
                                                        uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_driver_info_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_driver_version_req failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmSWInventoryDriverVersionAndStatus::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    auto driver_info = reinterpret_cast<struct nsm_driver_info*>(
        malloc(sizeof(struct nsm_driver_info) + 256));

    auto rc = decode_get_driver_info_resp(responseMsg, responseLen, &cc,
                                          &reason_code, driver_info);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateValue(driver_info->driver_state, driver_info->driver_version);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_driver_info_resp sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

static void createNsmNVLinkManagerDriverSensor(const std::string& interface,
                                               const std::string& objPath,
                                               NsmDeviceTable& nsmDevices)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "UUID", interface.c_str());
    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = findNsmDeviceByUUID(nsmDevices, uuid);
    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_NVLinkManagementSWInventory PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    auto sensor = std::make_shared<NsmSWInventoryDriverVersionAndStatus>(
        bus, name, type, manufacturer);

    if (priority)
    {
        nsmDevice->prioritySensors.push_back(sensor);
    }
    else
    {
        nsmDevice->roundRobinSensors.push_back(sensor);
    }
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmNVLinkManagerDriverSensor,
    "xyz.openbmc_project.Configuration.NSM_NVLinkManagementSWInventory")

} // namespace nsm