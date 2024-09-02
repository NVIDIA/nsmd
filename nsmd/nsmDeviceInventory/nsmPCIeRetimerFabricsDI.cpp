#include "nsmPCIeRetimerFabricsDI.hpp"

#include "dBusAsyncUtils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmPCIeRetimerFabricDI::NsmPCIeRetimerFabricDI(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& fabricsType) :
    NsmObject(name, type)
{
    auto inventoryObjPath = std::string(fabricsInventoryBasePath) + "/" + name;
    lg2::info("NsmPCIeRetimerFabricDI: {NAME}", "NAME", name.c_str());

    associationDefIntf = std::make_unique<AssociationDefinitionsInft>(
        bus, inventoryObjPath.c_str());
    uuidIntf = std::make_unique<UuidIntf>(bus, inventoryObjPath.c_str());
    fabricIntf = std::make_unique<FabricIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    associationDefIntf->associations(associationsList);

    fabricIntf->FabricIntf::type(
        FabricIntf::convertFabricTypeFromString(fabricsType));
}

static requester::Coroutine
    createNSMPCIeRetimerFabrics(SensorManager& manager,
                                const std::string& interface,
                                const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto fabricType = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "FabricType", interface.c_str());
    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_PCIeRetimer_Fabrics PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto retimerFabricsDi = std::make_shared<NsmPCIeRetimerFabricDI>(
        bus, name, associations, type, fabricType);
    nsmDevice->deviceSensors.emplace_back(retimerFabricsDi);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNSMPCIeRetimerFabrics,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_Fabrics")

} // namespace nsm
