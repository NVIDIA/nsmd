#include "nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"

#include "nsmDevice.hpp"
#include "nsmGpuChassis/nsmInventoryProperty.hpp"
#include "nsmGpuChassis/nsmSensorHelper.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <utils.hpp>

namespace nsm
{

void createNsmChassisAssembly(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath,
                              const std::string baseType)
{
    std::string baseInterface = "xyz.openbmc_project.Configuration." + baseType;

    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto chassisName = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto device = getNsmDevice(manager, objPath, baseInterface);

    if (type == baseType)
    {
        lg2::debug("createNsmChassis: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", baseType.c_str());
        auto assemblyObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
                chassisName, name, baseType);
        addSensor(device, assemblyObject);
    }
    else if (type == "NSM_Asset")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto assetObject = NsmNVSwitchAndNicChassisAssembly<AssetIntf>(
            chassisName, name, baseType);

        auto assemblyName = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());
        auto model = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Model", interface.c_str());
        auto vendor = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Vendor", interface.c_str());
        auto sku = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "SKU", interface.c_str());
        auto partNumber = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "PartNumber", interface.c_str());
        auto serialNumber = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "SerialNumber", interface.c_str());
        auto productionDate = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "ProductionDate", interface.c_str());

        // initial value update
        assetObject.pdi().name(assemblyName);
        assetObject.pdi().model(model);
        assetObject.pdi().manufacturer(vendor);
        assetObject.pdi().sku(sku);
        assetObject.pdi().serialNumber(serialNumber);
        assetObject.pdi().partNumber(partNumber);
        assetObject.pdi().buildDate(productionDate);

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
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto healthObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<HealthIntf>>(
                chassisName, name, baseType);
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());

        healthObject->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        addSensor(device, healthObject);
    }
    else if (type == "NSM_Location")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto locationObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<LocationIntf>>(
                chassisName, name, baseType);

        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());

        locationObject->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        addSensor(device, locationObject);
    }
}

void createNsmNVSwitchChassisAssembly(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath)
{
    createNsmChassisAssembly(manager, interface, objPath,
                             "NSM_NVSwitch_ChassisAssembly");
}

void createNsmNVLinkMgmtNicChassisAssembly(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath)
{
    createNsmChassisAssembly(manager, interface, objPath,
                             "NSM_NVLinkMgmtNic_ChassisAssembly");
}

std::vector<std::string> nvSwitchChassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Health",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Location"};

std::vector<std::string> nvLinkMgmtNicChassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Health",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Location"};

REGISTER_NSM_CREATION_FUNCTION(createNsmNVSwitchChassisAssembly,
                               nvSwitchChassisAssemblyInterfaces)
REGISTER_NSM_CREATION_FUNCTION(createNsmNVLinkMgmtNicChassisAssembly,
                               nvLinkMgmtNicChassisAssemblyInterfaces)
} // namespace nsm
