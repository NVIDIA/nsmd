#include "nsmGpuChassisPCIeDevice.hpp"

#include "nsmDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeDevice.hpp"
#include "nsmPCIeFunction.hpp"
#include "nsmPCIeLTSSMState.hpp"
#include "nsmSensorHelper.hpp"
#include "utils.hpp"

namespace nsm
{

void nsmGpuChassisPCIeDeviceCreateSensors(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_GPU_ChassisPCIeDevice";

    auto chassisName = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto device = getNsmDevice(manager, objPath, baseInterface);

    if (type == "NSM_GPU_ChassisPCIeDevice")
    {
        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());
        auto uuidObject = std::make_shared<NsmGpuChassisPCIeDevice<UuidIntf>>(
            chassisName, name);
        uuidObject->pdi().uuid(uuid);
        addSensor(device, uuidObject);
    }
    else if (type == "NSM_Asset")
    {
        auto assetObject =
            NsmGpuChassisPCIeDevice<AssetIntf>(chassisName, name);
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        assetObject.pdi().manufacturer(manufacturer);
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
    }
    else if (type == "NSM_Health")
    {
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto healthObject =
            std::make_shared<NsmGpuChassisPCIeDevice<HealthIntf>>(chassisName,
                                                                  name);
        healthObject->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        auto device = getNsmDevice(manager, objPath, baseInterface);
        addSensor(device, healthObject);
    }
    else if (type == "NSM_PCIeDevice")
    {
        auto deviceType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "DeviceType", interface.c_str());
        auto deviceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", interface.c_str());
        auto functionIds =
            utils::DBusHandler().getDbusProperty<std::vector<uint64_t>>(
                objPath.c_str(), "Functions", interface.c_str());
        auto pcieDeviceObject =
            NsmGpuChassisPCIeDevice<PCIeDeviceIntf>(chassisName, name);
        pcieDeviceObject.pdi().deviceType(deviceType);
        addSensor(device,
                  std::make_shared<NsmPCIeDevice>(pcieDeviceObject, deviceId));
        for (auto& id : functionIds)
        {
            addSensor(manager, device,
                      std::make_shared<NsmPCIeFunction>(pcieDeviceObject,
                                                        deviceId, id));
        }
    }
    else if (type == "NSM_LTSSMState")
    {
        auto deviceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceId", interface.c_str());
        auto ltssmStateObject =
            NsmGpuChassisPCIeDevice<LTSSMStateIntf>(chassisName, name);
        auto device = getNsmDevice(manager, objPath, baseInterface);
        addSensor(device, std::make_shared<NsmPCIeLTSSMState>(ltssmStateObject,
                                                              deviceId));
    }
}

std::vector<std::string> gpuChassisPCIeDeviceInterfaces{
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisPCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisPCIeDevice.Asset",
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisPCIeDevice.Health",
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisPCIeDevice.PCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisPCIeDevice.LTSSMState"};

REGISTER_NSM_CREATION_FUNCTION(nsmGpuChassisPCIeDeviceCreateSensors,
                               gpuChassisPCIeDeviceInterfaces)

} // namespace nsm
