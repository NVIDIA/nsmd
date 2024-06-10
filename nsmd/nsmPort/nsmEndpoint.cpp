#include "nsmEndpoint.hpp"

#include "common/types.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmEndpoint::NsmEndpoint(sdbusplus::bus::bus& bus, const std::string& name,
                         const std::string& type,
                         const std::vector<utils::Association>& associations,
                         const std::string& fabricObjPath) :
    NsmObject(name, type)
{
    lg2::info("NsmFabricEndpoint: create sensor:{NAME}", "NAME", name.c_str());
    auto inventoryObjPath = fabricObjPath + "/Endpoints/" + name;

    endpointIntf = std::make_unique<EndpointIntf>(bus,
                                                  inventoryObjPath.c_str());
    associationDefIntf =
        std::make_unique<AssociationDefIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefIntf->associations(associations_list);
}

static void createNsmEndpoints(SensorManager& manager,
                               const std::string& interface,
                               const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto fabricsObjPath = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "FabricsObjPath", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto associations = utils::getAssociations(objPath,
                                               interface + ".Associations");

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_FabricsEndpoint PDI matches no NsmDevice : UUID={UUID}, Fabric={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", fabricsObjPath, "TYPE", type);
        return;
    }

    // create Endpoint on fabric
    auto fabricsEndpointSensor = std::make_shared<NsmEndpoint>(
        bus, name, type, associations, fabricsObjPath);

    if (!fabricsEndpointSensor)
    {
        lg2::error(
            "Failed to create NSM Fabrics Endpoint : UUID={UUID}, Type={TYPE}, Fabrics_Path={OBJPATH}",
            "UUID", uuid, "TYPE", type, "OBJPATH", fabricsObjPath);
        return;
    }

    nsmDevice->deviceSensors.push_back(fabricsEndpointSensor);
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmEndpoints, "xyz.openbmc_project.Configuration.NSM_FabricsEndpoint")

} // namespace nsm
