#include "nsmGpuChassisPCIeDevice.hpp"

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeFunction.hpp"
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

    if (type == "NSM_PCIeDevice")
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
        for (auto& id : functionIds)
        {
            addSensor(manager, device,
                      std::make_shared<NsmPCIeFunction>(pcieDeviceObject,
                                                        deviceId, id));
        }
    }
}

REGISTER_NSM_CREATION_FUNCTION(
    nsmGpuChassisPCIeDeviceCreateSensors,
    "xyz.openbmc_project.Configuration.NSM_GPU_ChassisPCIeDevice.PCIeDevice")

} // namespace nsm
