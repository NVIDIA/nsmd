#include "nsmPCIeRetimer.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmPCIeRetimerChassis::NsmPCIeRetimerChassis(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type) :
    NsmObject(name, type)
{
    auto pcieRetimerChaasisBasePath =
        std::string(chassisInventoryBasePath) + "/" + name;
    lg2::info("NsmPCIeRetimerChassis: create sensor:{NAME}", "NAME",
              name.c_str());

    // attach all interfaces
    associationDef_ = std::make_unique<AssociationDefinitionsInft>(
        bus, pcieRetimerChaasisBasePath.c_str());

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

    asset_ =
        std::make_unique<AssetIntf>(bus, pcieRetimerChaasisBasePath.c_str());
    asset_->sku("");
    location_ =
        std::make_unique<LocationIntf>(bus, pcieRetimerChaasisBasePath.c_str());
    location_->locationType(LocationIntf::convertLocationTypesFromString(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"));
    chassis_ =
        std::make_unique<ChassisIntf>(bus, pcieRetimerChaasisBasePath.c_str());
    chassis_->ChassisIntf::type(ChassisIntf::convertChassisTypeFromString(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"));
}

static void CreatePCIeRetimerChassis(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto associations =
        utils::getAssociations(objPath, interface + ".Associations");

    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NsmPCIeRetimerChassis PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    auto retimer_chassis =
        std::make_shared<NsmPCIeRetimerChassis>(bus, name, associations, type);
    nsmDevice->deviceSensors.emplace_back(retimer_chassis);
}

REGISTER_NSM_CREATION_FUNCTION(
    CreatePCIeRetimerChassis,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer")

} // namespace nsm
