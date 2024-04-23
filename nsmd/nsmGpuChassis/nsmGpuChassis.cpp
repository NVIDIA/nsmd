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

    if (type == "NSM_GPU_Chassis")
    {
        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());
        auto chassisUuid = std::make_shared<NsmGpuChassis<UuidIntf>>(name);
        chassisUuid->uuid(uuid);
        auto device = getNsmDevice(manager, objPath, baseInterface);
        addSensor(device, chassisUuid);
    }
    else if (type == "NSM_Asset")
    {
        auto chassisAsset = std::make_shared<NsmGpuChassis<AssetIntf>>(name);
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        chassisAsset->manufacturer(manufacturer);
        auto device = getNsmDevice(manager, objPath, baseInterface);
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
        chassis->ChassisIntf::type(
            ChassisIntf::convertChassisTypeFromString(chassisType));
        auto device = getNsmDevice(manager, objPath, baseInterface);
        addSensor(device, chassis);
    }
    else if (type == "NSM_Dimension")
    {
        auto chassisDimension =
            std::make_shared<NsmGpuChassis<DimensionIntf>>(name);

        auto device = getNsmDevice(manager, objPath, baseInterface);
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
        chassisHealth->health(HealthIntf::convertHealthTypeFromString(health));
        auto device = getNsmDevice(manager, objPath, baseInterface);
        addSensor(device, chassisHealth);
    }
    else if (type == "NSM_Location")
    {
        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto chassisLocation =
            std::make_shared<NsmGpuChassis<LocationIntf>>(name);
        chassisLocation->locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        auto device = getNsmDevice(manager, objPath, baseInterface);
        addSensor(device, chassisLocation);
    }
    else if (type == "NSM_LocationCode")
    {
        auto locationCode = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationCode", interface.c_str());
        auto chassisLocationCode =
            std::make_shared<NsmGpuChassis<LocationCodeIntf>>(name);
        chassisLocationCode->locationCode(locationCode);
        auto device = getNsmDevice(manager, objPath, baseInterface);
        addSensor(device, chassisLocationCode);
    }
    else if (type == "NSM_PowerLimit")
    {
        auto chassisPowerLimit =
            std::make_shared<NsmGpuChassis<PowerLimitIntf>>(name);
        auto device = getNsmDevice(manager, objPath, interface);
        addSensor(device,
                  std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                      chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT),
                  objPath, interface);
        addSensor(device,
                  std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                      chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT),
                  objPath, interface);
    }
    else if (type == "NSM_OperationalStatus")
    {
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", interface.c_str());
        auto chassisOperationalStatus =
            std::make_shared<NsmGpuChassis<OperationalStatusIntf>>(name);
        auto device = getNsmDevice(manager, objPath, interface);
        addSensor(device,
                  std::make_shared<NsmGpuPresenceAndPowerStatus>(
                      chassisOperationalStatus, instanceId),
                  objPath, interface);
    }
    else if (type == "NSM_PowerState")
    {
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", interface.c_str());
        auto chassisPowerState =
            std::make_shared<NsmGpuChassis<PowerStateIntf>>(name);
        auto device = getNsmDevice(manager, objPath, interface);
        addSensor(device,
                  std::make_shared<NsmPowerSupplyStatus>(chassisPowerState,
                                                         instanceId),
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
