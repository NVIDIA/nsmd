#include "nsmZone.hpp"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "globals.hpp"
#include "nsmPortFeatures.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmZone::NsmZone(sdbusplus::bus_t& bus, const std::string& name,
                 const std::string& type, const std::string& fabricObjPath,
                 const std::string& zoneType) : NsmObject(name, type)
{
    lg2::info("NsmFabricZone: create sensor:{NAME}", "NAME", name.c_str());
    auto inventoryObjPath = fabricObjPath + "/zones/0";

    zoneIntf = std::make_unique<ZoneIntf>(bus, inventoryObjPath.c_str());

    zoneIntf->type(ZoneIntf::convertZoneTypeFromString(zoneType));
    zoneIntf->routingEnabled(bool(true));
}

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createFabricsZoneFeature([[maybe_unused]] SensorManager& manager,
                             const DeviceContext& dev,
                             const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("FabricsZone feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }
    auto& bus = utils::DBusHandler::getBus();
    const std::string type = "NSM_FabricsZone";
    auto name = param<std::string>(params, "Name").value_or("");
    auto zoneType = param<std::string>(params, "ZoneType").value_or("");
    auto fabricName = param<std::string>(params, "FabricName").value_or("");
    const auto fabricsObjPath =
        (fabricsInventoryBasePath / fabricName).string();

    auto fabricsZoneSensor =
        std::make_shared<NsmZone>(bus, name, type, fabricsObjPath, zoneType);
    dev.device->addDeviceSensors(fabricsZoneSensor);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
