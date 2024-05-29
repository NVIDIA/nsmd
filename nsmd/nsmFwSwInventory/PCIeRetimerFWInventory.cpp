#include "PCIeRetimerFWInventory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmPCIeRetimerFirmwareVersion::NsmPCIeRetimerFirmwareVersion(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& manufacturer,
    const uint8_t instanceNumber) :
    NsmObject(name, type), instanceNumber(instanceNumber)
{
    auto pcieRetimerFWInvBasePath = std::string(firmwareInventoryBasePath) +
                                    "/" + name;
    lg2::info("NsmPCIeRetimerFirmwareVersion: create sensor:{NAME}", "NAME",
              name.c_str());

    // add all interfaces
    associationDef_ = std::make_unique<AssociationDefinitionsInft>(
        bus, pcieRetimerFWInvBasePath.c_str());
    // handle associations
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDef_->associations(associations_list);
    asset_ = std::make_unique<AssetIntf>(bus, pcieRetimerFWInvBasePath.c_str());
    asset_->manufacturer(manufacturer);
    softwareVer_ =
        std::make_unique<SoftwareIntf>(bus, pcieRetimerFWInvBasePath.c_str());
}

void NsmPCIeRetimerFirmwareVersion::updateValue(std::string firmwareVersion)
{
    softwareVer_->version(firmwareVersion);
}

requester::Coroutine
    NsmPCIeRetimerFirmwareVersion::update(SensorManager& manager, eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = PCIeRetimerEEPROMIdentifier + instanceNumber;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, &responseMsg,
                                         &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    std::vector<uint8_t> data(8, 0);

    rc = decode_get_inventory_information_resp(
        responseMsg, responseLen, &cc, &reason_code, &dataSize, data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        std::stringstream iss;
        iss << int(data[0]) << '.' << int(data[2]);
        iss << '.';
        iss << int(((data[4] << 8) | data[6]));

        std::string firmwareVersion = iss.str();
        updateValue(firmwareVersion);
    }
    else
    {
        lg2::error(
            "decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return cc;
}

static void CreatePCIeRetimerFWInventory(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());

    auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Manufacturer", interface.c_str());

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto instanceNumber = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "INSTANCE_NUMBER", interface.c_str());

    auto associations = utils::getAssociations(objPath,
                                               interface + ".Associations");

    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of CreatePCIeRetimerFWInventory PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    auto retimer_fwVersion = std::make_shared<NsmPCIeRetimerFirmwareVersion>(
        bus, name, associations, type, manufacturer, instanceNumber);
    nsmDevice->deviceSensors.emplace_back(retimer_fwVersion);

    // update sensor
    retimer_fwVersion->update(manager, manager.getEid(nsmDevice)).detach();
}

REGISTER_NSM_CREATION_FUNCTION(
    CreatePCIeRetimerFWInventory,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_FWInventory")
} // namespace nsm
