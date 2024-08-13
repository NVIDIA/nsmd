#include "nsmEndpoint.hpp"

#include "dBusAsyncUtils.hpp"
#include "common/types.hpp"
#include "utils.hpp"

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

static requester::Coroutine createNsmEndpoints(SensorManager& manager,
                                               const std::string& interface,
                                               const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto fabricsObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "FabricsObjPath", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                    associations);

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_FabricsEndpoint PDI matches no NsmDevice : UUID={UUID}, Fabric={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", fabricsObjPath, "TYPE", type);
        co_return NSM_ERROR;
    }

    // create Endpoint on fabric
    auto fabricsEndpointSensor = std::make_shared<NsmEndpoint>(
        bus, name, type, associations, fabricsObjPath);

    if (!fabricsEndpointSensor)
    {
        lg2::error(
            "Failed to create NSM Fabrics Endpoint : UUID={UUID}, Type={TYPE}, Fabrics_Path={OBJPATH}",
            "UUID", uuid, "TYPE", type, "OBJPATH", fabricsObjPath);
        co_return NSM_ERROR;
    }

    nsmDevice->deviceSensors.push_back(fabricsEndpointSensor);
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmEndpoints, "xyz.openbmc_project.Configuration.NSM_FabricsEndpoint")

} // namespace nsm
