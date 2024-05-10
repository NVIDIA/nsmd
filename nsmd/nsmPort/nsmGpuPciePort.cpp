#include "nsmGpuPciePort.hpp"
#define GPU_PCIe_INTERFACE "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0"
namespace nsm
{

NsmGpuPciePort::NsmGpuPciePort(sdbusplus::bus::bus& bus, const std::string& name,
                         const std::string& type, const std::string& health, const std::string& chasisState,
                         const std::vector<utils::Association>& associations,
                         const std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    lg2::info("NsmGpuPciePort: create sensor:{NAME}", "NAME", name.c_str());
    associationDefIntf =
        std::make_unique<AssociationDefIntf>(bus, inventoryObjPath.c_str());
    
    chasisStateIntf = std::make_unique<ChasisStateIntf>(bus, inventoryObjPath.c_str());
    chasisStateIntf->currentPowerState(ChasisStateIntf::convertPowerStateFromString(chasisState));

    healthIntf = std::make_unique<HealthIntf>(bus, inventoryObjPath.c_str());
    healthIntf->health(HealthIntf::convertHealthTypeFromString(health));

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

NsmGpuPciePortInfo::NsmGpuPciePortInfo(const std::string& name,
                                 const std::string& type,
                                 const std::string& portType,
                                 const std::string& portProtocol,
                                 std::shared_ptr<PortInfoIntf> portInfoIntf) :
    NsmObject(name, type),
    portInfoIntf(portInfoIntf)
{
    lg2::info("NsmGpuPciePortInfo: create sensor:{NAME}", "NAME", name.c_str());
    portInfoIntf->type(PortInfoIntf::convertPortTypeFromString(portType));
    portInfoIntf->protocol(
        PortInfoIntf::convertPortProtocolFromString(portProtocol));
}

static void createNsmGpuPcieSensor(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", GPU_PCIe_INTERFACE);

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", GPU_PCIe_INTERFACE);

        auto type = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", GPU_PCIe_INTERFACE);
        inventoryObjPath = inventoryObjPath + "/Ports/PCIe_0";

        auto nsmDevice = manager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_GPU_PCIe_0 PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            return;
        }
        if (type == "NSM_GPU_PCIe_0")
        {
            auto associations =
                utils::getAssociations(objPath, interface + ".Associations");
            auto health = utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "Health", interface.c_str());
            auto chasisState = utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "ChasisPowerState", interface.c_str());
            auto sensor = std::make_shared<NsmGpuPciePort>(
                bus, name, type, health, chasisState, associations, inventoryObjPath);
            nsmDevice->deviceSensors.emplace_back(sensor);
        }
        else if (type == "NSM_PortInfo")
        {
            auto portType = utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "PortType", interface.c_str());
            auto portProtocol =
                utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "PortProtocol", interface.c_str());
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            auto deviceIndex = utils::DBusHandler().getDbusProperty<uint64_t>(
                objPath.c_str(), "DeviceIndex", GPU_PCIe_INTERFACE);
            auto portInfoIntf =
                std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
            auto portWidthIntf = std::make_shared<PortWidthIntf>(
                bus, inventoryObjPath.c_str());
            auto portInfoSensor = std::make_shared<NsmGpuPciePortInfo>(
                name, type, portType, portProtocol, portInfoIntf);
            nsmDevice->deviceSensors.emplace_back(portInfoSensor);
            auto pcieECCIntfSensorGroup1 = std::make_shared<NsmPCIeECCGroup1>(
                name, type, portInfoIntf, portWidthIntf, deviceIndex);

            if (priority)
            {
                nsmDevice->prioritySensors.emplace_back(
                    pcieECCIntfSensorGroup1);
            }
            else
            {
                nsmDevice->roundRobinSensors.emplace_back(
                    pcieECCIntfSensorGroup1);
            }
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
        return;
    }
}

REGISTER_NSM_CREATION_FUNCTION(createNsmGpuPcieSensor,
                               "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmGpuPcieSensor,
    "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0.PortInfo")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmGpuPcieSensor,
    "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0.PortState")
} // namespace nsm
