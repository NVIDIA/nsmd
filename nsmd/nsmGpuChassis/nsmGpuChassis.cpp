#include "nsmGpuChassis.hpp"

#include "nsmDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPowerSupplyStatus.hpp"
#include "nsmSensorHelper.hpp"
#include "utils.hpp"

namespace nsm
{

void nsmGpuChassisCreateSensors(SensorManager& manager,
                                const std::string& interface,
                                const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_GPU_Chassis";

    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto device = getNsmDevice(manager, objPath, baseInterface);

    if (type == "NSM_GPU_Chassis")
    {
        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());
        auto chassisUuid = std::make_shared<NsmGpuChassis<UuidIntf>>(name);
        chassisUuid->pdi().uuid(uuid);
        addSensor(device, chassisUuid);
    }
    else if (type == "NSM_Asset")
    {
        auto chassisAsset = NsmGpuChassis<AssetIntf>(name);
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        chassisAsset.pdi().manufacturer(manufacturer);
        // create sensor
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      chassisAsset, BOARD_PART_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      chassisAsset, SERIAL_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      chassisAsset, MARKETING_NAME));
    }
    else if (type == "NSM_Chassis")
    {
        auto chassisType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "ChassisType", interface.c_str());
        auto chassis = std::make_shared<NsmGpuChassis<ChassisIntf>>(name);
        chassis->pdi().type(
            ChassisIntf::convertChassisTypeFromString(chassisType));
        addSensor(device, chassis);
    }
    else if (type == "NSM_Dimension")
    {
        auto chassisDimension = NsmGpuChassis<DimensionIntf>(name);
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<DimensionIntf>>(
                      chassisDimension, PRODUCT_LENGTH));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<DimensionIntf>>(
                      chassisDimension, PRODUCT_WIDTH));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<DimensionIntf>>(
                      chassisDimension, PRODUCT_HEIGHT));
    }
    else if (type == "NSM_Health")
    {
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto chassisHealth = std::make_shared<NsmGpuChassis<HealthIntf>>(name);
        chassisHealth->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        addSensor(device, chassisHealth);
    }
    else if (type == "NSM_Location")
    {
        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto chassisLocation =
            std::make_shared<NsmGpuChassis<LocationIntf>>(name);
        chassisLocation->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        addSensor(device, chassisLocation);
    }
    else if (type == "NSM_LocationCode")
    {
        auto locationCode = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationCode", interface.c_str());
        auto chassisLocationCode =
            std::make_shared<NsmGpuChassis<LocationCodeIntf>>(name);
        chassisLocationCode->pdi().locationCode(locationCode);
        addSensor(device, chassisLocationCode);
    }
    else if (type == "NSM_PowerLimit")
    {
        auto chassisPowerLimit = NsmGpuChassis<PowerLimitIntf>(name);
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        addSensor(device,
                  std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                      chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT),
                  priority);
        addSensor(device,
                  std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                      chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT),
                  priority);
    }
    else if (type == "NSM_OperationalStatus")
    {
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", baseInterface.c_str());
        auto inventoryObjPaths =
            utils::DBusHandler().getDbusProperty<dbus::Interfaces>(
                objPath.c_str(), "InventoryObjPaths", interface.c_str());
        auto gpuOperationalStatus = NsmInterfaceProvider<OperationalStatusIntf>(
            name, type, inventoryObjPaths);
        addSensor(device,
                  std::make_shared<NsmGpuPresenceAndPowerStatus>(
                      gpuOperationalStatus, instanceId),
                  objPath, interface);
    }
    else if (type == "NSM_PowerState")
    {
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", baseInterface.c_str());
        auto inventoryObjPaths =
            utils::DBusHandler().getDbusProperty<dbus::Interfaces>(
                objPath.c_str(), "InventoryObjPaths", interface.c_str());
        auto gpuPowerState =
            NsmInterfaceProvider<PowerStateIntf>(name, type, inventoryObjPaths);
        addSensor(
            device,
            std::make_shared<NsmPowerSupplyStatus>(gpuPowerState, instanceId),
            objPath, interface);
    }
}

std::vector<std::string> gpuChassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.Asset",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.Dimension",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.Health",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.Location",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.LocationCode",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.PowerLimit",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.OperationalStatus",
    "xyz.openbmc_project.Configuration.NSM_GPU_Chassis.PowerState"};

REGISTER_NSM_CREATION_FUNCTION(nsmGpuChassisCreateSensors, gpuChassisInterfaces)

} // namespace nsm
