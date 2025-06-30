#include "nsmEndpoint.hpp"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
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
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    std::string fabricsObjPath{};
    if (allCurrentIfaceProperties.count("FabricsObjPath"))
    {
        fabricsObjPath = std::get<std::string>(
            allCurrentIfaceProperties.at("FabricsObjPath"));
    }
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_FabricsEndpoint PDI matches no NsmDevice : UUID={UUID}, Fabric={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", fabricsObjPath, "TYPE", type);
        // coverity[missing_return]
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
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    nsmDevice->deviceSensors.push_back(fabricsEndpointSensor);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmEndpoints, "xyz.openbmc_project.Configuration.NSM_FabricsEndpoint")

} // namespace nsm
