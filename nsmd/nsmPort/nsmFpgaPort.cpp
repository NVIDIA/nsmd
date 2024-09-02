#include "nsmFpgaPort.hpp"
#define FPGA_PORT_INTERFACE "xyz.openbmc_project.Configuration.NSM_FpgaPort"
namespace nsm
{

NsmFpgaPort::NsmFpgaPort(sdbusplus::bus::bus& bus, const std::string& name,
                         const std::string& type, const std::string& health,
                         const std::string& chasisState,
                         const std::vector<utils::Association>& associations,
                         const std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    lg2::info("NsmFpgaPort: create sensor:{NAME}", "NAME", name.c_str());
    portIntf = std::make_unique<PortIntf>(bus, inventoryObjPath.c_str());
    associationDefIntf =
        std::make_unique<AssociationDefIntf>(bus, inventoryObjPath.c_str());

    chasisStateIntf =
        std::make_unique<ChasisStateIntf>(bus, inventoryObjPath.c_str());
    chasisStateIntf->currentPowerState(
        ChasisStateIntf::convertPowerStateFromString(chasisState));

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

NsmFpgaPortInfo::NsmFpgaPortInfo(const std::string& name,
                                 const std::string& type,
                                 const std::string& portType,
                                 const std::string& portProtocol,
                                 std::shared_ptr<PortInfoIntf> portInfoIntf) :
    NsmObject(name, type), portInfoIntf(portInfoIntf)
{
    lg2::info("NsmFpgaPortInfo: create sensor:{NAME}", "NAME", name.c_str());
    portInfoIntf->type(PortInfoIntf::convertPortTypeFromString(portType));
    portInfoIntf->protocol(
        PortInfoIntf::convertPortProtocolFromString(portProtocol));
}

NsmFpgaPortState::NsmFpgaPortState(sdbusplus::bus::bus& bus,
                                   const std::string& name,
                                   const std::string& type,
                                   const std::string& linkStatus,
                                   const std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    lg2::info("NsmFpgaPortState: create sensor:{NAME}", "NAME", name.c_str());
    portStateIntf = std::make_shared<PortStateIntf>(bus,
                                                    inventoryObjPath.c_str());
    portStateIntf->linkStatus(
        PortStateIntf::convertLinkStatusTypeFromString(linkStatus));
}

static requester::Coroutine
    createNsmFpgaPortSensor(SensorManager& manager,
                            const std::string& interface,
                            const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", FPGA_PORT_INTERFACE);

        auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", FPGA_PORT_INTERFACE);

        auto type = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "InventoryObjPath", FPGA_PORT_INTERFACE);

        auto nsmDevice = manager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_FpgaPort PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            // coverity[missing_return]
            co_return NSM_ERROR;
        }
        if (type == "NSM_FpgaPort")
        {
            std::vector<utils::Association> associations{};
            co_await utils::coGetAssociations(
                objPath, interface + ".Associations", associations);
            auto health = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "Health", interface.c_str());
            auto chasisState = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "ChasisPowerState", interface.c_str());
            auto sensor = std::make_shared<NsmFpgaPort>(
                bus, name, type, health, chasisState, associations,
                inventoryObjPath);
            nsmDevice->deviceSensors.emplace_back(sensor);
        }
        else if (type == "NSM_PortInfo")
        {
            auto portType = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "PortType", interface.c_str());
            auto portProtocol = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "PortProtocol", interface.c_str());
            auto priority = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            auto deviceIndex = co_await utils::coGetDbusProperty<uint64_t>(
                objPath.c_str(), "DeviceIndex", FPGA_PORT_INTERFACE);
            auto portInfoIntf =
                std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
            auto portWidthIntf =
                std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
            auto portInfoSensor = std::make_shared<NsmFpgaPortInfo>(
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
        else if (type == "NSM_PortState")
        {
            auto linkStatus = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "LinkStatus", interface.c_str());
            auto portStateSensor = std::make_shared<NsmFpgaPortState>(
                bus, name, type, linkStatus, inventoryObjPath);
            nsmDevice->deviceSensors.emplace_back(portStateSensor);
        }
        else if (type == "NSM_PCIe")
        {
            auto priority = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto deviceIndex = co_await utils::coGetDbusProperty<uint64_t>(
                objPath.c_str(), "DeviceIndex", FPGA_PORT_INTERFACE);

            auto pcieECCIntf =
                std::make_shared<PCIeEccIntf>(bus, inventoryObjPath.c_str());

            auto pcieECCIntfSensorGroup2 = std::make_shared<NsmPCIeECCGroup2>(
                name, type, pcieECCIntf, deviceIndex);
            auto pcieECCIntfSensorGroup3 = std::make_shared<NsmPCIeECCGroup3>(
                name, type, pcieECCIntf, deviceIndex);
            auto pcieECCIntfSensorGroup4 = std::make_shared<NsmPCIeECCGroup4>(
                name, type, pcieECCIntf, deviceIndex);

            if (!pcieECCIntfSensorGroup2 || !pcieECCIntfSensorGroup3 ||
                !pcieECCIntfSensorGroup4)
            {
                lg2::error(
                    "Failed to create NSM PCIe ECC Port sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                    "UUID", uuid, "NAME", name, "TYPE", type, "OBJPATH",
                    objPath);
                // coverity[missing_return]
                co_return NSM_ERROR;
            }
            if (priority)
            {
                nsmDevice->prioritySensors.emplace_back(
                    pcieECCIntfSensorGroup2);
                nsmDevice->prioritySensors.emplace_back(
                    pcieECCIntfSensorGroup3);
                nsmDevice->prioritySensors.emplace_back(
                    pcieECCIntfSensorGroup4);
            }
            else
            {
                nsmDevice->roundRobinSensors.emplace_back(
                    pcieECCIntfSensorGroup2);
                nsmDevice->roundRobinSensors.emplace_back(
                    pcieECCIntfSensorGroup3);
                nsmDevice->roundRobinSensors.emplace_back(
                    pcieECCIntfSensorGroup4);
            }
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createNsmFpgaPortSensor,
                               "xyz.openbmc_project.Configuration.NSM_FpgaPort")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmFpgaPortSensor,
    "xyz.openbmc_project.Configuration.NSM_FpgaPort.PCIe")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmFpgaPortSensor,
    "xyz.openbmc_project.Configuration.NSM_FpgaPort.PortInfo")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmFpgaPortSensor,
    "xyz.openbmc_project.Configuration.NSM_FpgaPort.PortState")
} // namespace nsm
