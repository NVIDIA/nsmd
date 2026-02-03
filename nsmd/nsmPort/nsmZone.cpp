#include "nsmZone.hpp"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmZone::NsmZone(sdbusplus::bus::bus& bus, const std::string& name,
                 const std::string& type, const std::string& fabricObjPath,
                 const std::string& zoneType) : NsmObject(name, type)
{
    lg2::info("NsmFabricZone: create sensor:{NAME}", "NAME", name.c_str());
    auto inventoryObjPath = fabricObjPath + "/zones/0";

    zoneIntf = std::make_unique<ZoneIntf>(bus, inventoryObjPath.c_str());

    zoneIntf->type(ZoneIntf::convertZoneTypeFromString(zoneType));
    zoneIntf->routingEnabled(bool(true));
}

requester::Coroutine createNsmZones(SensorManager& manager,
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
    std::string zoneType{};
    if (allCurrentIfaceProperties.count("ZoneType"))
    {
        zoneType =
            std::get<std::string>(allCurrentIfaceProperties.at("ZoneType"));
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

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_FabricsZone PDI matches no NsmDevice : UUID={UUID}, Fabric={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", fabricsObjPath, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    // create zone on fabric
    auto fabricsZoneSensor =
        std::make_shared<NsmZone>(bus, name, type, fabricsObjPath, zoneType);

    if (!fabricsZoneSensor)
    {
        lg2::error(
            "Failed to create NSM Fabrics Zone : UUID={UUID}, Type={TYPE}, Fabrics_Path={OBJPATH}",
            "UUID", uuid, "TYPE", type, "OBJPATH", fabricsObjPath);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    nsmDevice->addDeviceSensors(fabricsZoneSensor);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmZones, "xyz.openbmc_project.Configuration.NSM_FabricsZone")

} // namespace nsm
