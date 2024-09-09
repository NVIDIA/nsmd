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
                 const std::string& zoneType) :
    NsmObject(name, type)
{
    lg2::info("NsmFabricZone: create sensor:{NAME}", "NAME", name.c_str());
    auto inventoryObjPath = fabricObjPath + "/zones/0";

    zoneIntf = std::make_unique<ZoneIntf>(bus, inventoryObjPath.c_str());

    zoneIntf->type(ZoneIntf::convertZoneTypeFromString(zoneType));
    zoneIntf->routingEnabled(bool(true));
}

static requester::Coroutine createNsmZones(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto zoneType = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "ZoneType", interface.c_str());
    auto fabricsObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "FabricsObjPath", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDevice(uuid);
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

    nsmDevice->deviceSensors.push_back(fabricsZoneSensor);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmZones, "xyz.openbmc_project.Configuration.NSM_FabricsZone")

} // namespace nsm
