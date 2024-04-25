#include "nsmGpuChassisAssembly.hpp"

#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensorHelper.hpp"
#include "utils.hpp"

namespace nsm
{

void nsmGpuChassisAssemblyCreateSensors(SensorManager& manager,
                                        const std::string& interface,
                                        const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_GPU_ChassisAssembly";

    auto chassisName = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto device = getNsmDevice(manager, objPath, baseInterface);

    if (type == "NSM_GPU_ChassisAssembly")
    {
        auto assemblyObject =
            std::make_shared<NsmGpuChassisAssembly<AssemblyIntf>>(chassisName,
                                                                  name);
        addSensor(device, assemblyObject);
    }
    else if (type == "NSM_Area")
    {
        auto physicalContext =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "PhysicalContext", interface.c_str());
        auto chassisArea = std::make_shared<NsmGpuChassisAssembly<AreaIntf>>(
            chassisName, name);
        chassisArea->pdi().physicalContext(
            AreaIntf::convertPhysicalContextTypeFromString(physicalContext));
        addSensor(device, chassisArea);
    }
    else if (type == "NSM_Asset")
    {
        auto vendor = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Vendor", interface.c_str());
        auto assetsName = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());
        auto assetObject = NsmGpuChassisAssembly<AssetIntf>(chassisName, name);
        assetObject.pdi().manufacturer(vendor);
        assetObject.pdi().name(assetsName);
        // create sensor
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, BOARD_PART_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, SERIAL_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, MARKETING_NAME));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, BUILD_DATE));
    }
    else if (type == "NSM_Health")
    {
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto healthObject = std::make_shared<NsmGpuChassisAssembly<HealthIntf>>(
            chassisName, name);
        healthObject->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        addSensor(device, healthObject);
    }
    else if (type == "NSM_Location")
    {
        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto locationObject =
            std::make_shared<NsmGpuChassisAssembly<LocationIntf>>(chassisName,
                                                                  name);
        locationObject->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        addSensor(device, locationObject);
    }
}
std::vector<std::string> gpuChassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisAssembly.Area",
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisAssembly.Asset",
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisAssembly.Health",
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisAssembly.Location"};
REGISTER_NSM_CREATION_FUNCTION(nsmGpuChassisAssemblyCreateSensors,
                               gpuChassisAssemblyInterfaces)

} // namespace nsm
