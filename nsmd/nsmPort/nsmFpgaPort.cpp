#include "nsmFpgaPort.hpp"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"

#include <unordered_map>

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

        dbus::PropertyMap allBaseIfaceProperties;
        auto rc = co_await utils::coGetCachedBaseProperties(
            objPath, FPGA_PORT_INTERFACE, allBaseIfaceProperties);
        if (rc != NSM_SUCCESS)
        {
            co_return rc;
        }
        auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
            utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

        std::string name{};
        if (allBaseIfaceProperties.count("Name"))
        {
            name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
        }
        uuid_t uuid{};
        if (allBaseIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
        }
        std::string type{};
        if (allCurrentIfaceProperties.count("Type"))
        {
            type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
        }
        std::string inventoryObjPath{};
        if (allBaseIfaceProperties.count("InventoryObjPath"))
        {
            inventoryObjPath = std::get<std::string>(
                allBaseIfaceProperties.at("InventoryObjPath"));
        }

        auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
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
            std::string health{};
            if (allCurrentIfaceProperties.count("Health"))
            {
                health = std::get<std::string>(
                    allCurrentIfaceProperties.at("Health"));
            }
            std::string chasisState{};
            if (allCurrentIfaceProperties.count("ChasisPowerState"))
            {
                chasisState = std::get<std::string>(
                    allCurrentIfaceProperties.at("ChasisPowerState"));
            }

            auto sensor = std::make_shared<NsmFpgaPort>(
                bus, name, type, health, chasisState, associations,
                inventoryObjPath);
            nsmDevice->deviceSensors.emplace_back(sensor);
        }
        else if (type == "NSM_PortInfo")
        {
            std::string portType{};
            if (allCurrentIfaceProperties.count("PortType"))
            {
                portType = std::get<std::string>(
                    allCurrentIfaceProperties.at("PortType"));
            }
            std::string portProtocol{};
            if (allCurrentIfaceProperties.count("PortProtocol"))
            {
                portProtocol = std::get<std::string>(
                    allCurrentIfaceProperties.at("PortProtocol"));
            }
            bool priority{};
            if (allCurrentIfaceProperties.count("Priority"))
            {
                priority =
                    std::get<bool>(allCurrentIfaceProperties.at("Priority"));
            }
            uint64_t deviceIndex{};
            if (allBaseIfaceProperties.count("DeviceIndex"))
            {
                deviceIndex = std::get<uint64_t>(
                    allBaseIfaceProperties.at("DeviceIndex"));
            }

            auto portInfoIntf =
                std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
            auto portWidthIntf =
                std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
            auto portInfoSensor = std::make_shared<NsmFpgaPortInfo>(
                name, type, portType, portProtocol, portInfoIntf);
            nsmDevice->deviceSensors.emplace_back(portInfoSensor);
            auto pcieECCIntfSensorGroup1 = std::make_shared<NsmPCIeECCGroup1>(
                name, type, inventoryObjPath, portInfoIntf, portWidthIntf,
                deviceIndex);

            nsmDevice->addSensor(pcieECCIntfSensorGroup1, priority);
        }
        else if (type == "NSM_PortState")
        {
            std::string linkStatus{};
            if (allCurrentIfaceProperties.count("LinkStatus"))
            {
                linkStatus = std::get<std::string>(
                    allCurrentIfaceProperties.at("LinkStatus"));
            }

            auto portStateSensor = std::make_shared<NsmFpgaPortState>(
                bus, name, type, linkStatus, inventoryObjPath);
            nsmDevice->deviceSensors.emplace_back(portStateSensor);
        }
        else if (type == "NSM_PCIe")
        {
            bool priority{};
            if (allCurrentIfaceProperties.count("Priority"))
            {
                priority =
                    std::get<bool>(allCurrentIfaceProperties.at("Priority"));
            }
            uint64_t deviceIndex{};
            if (allBaseIfaceProperties.count("DeviceIndex"))
            {
                deviceIndex = std::get<uint64_t>(
                    allBaseIfaceProperties.at("DeviceIndex"));
            }

            auto pcieECCIntf =
                std::make_shared<PCIeEccIntf>(bus, inventoryObjPath.c_str());

            auto pcieECCIntfSensorGroup2 = std::make_shared<NsmPCIeECCGroup2>(
                name, type, inventoryObjPath, pcieECCIntf, deviceIndex);
            auto pcieECCIntfSensorGroup3 = std::make_shared<NsmPCIeECCGroup3>(
                name, type, inventoryObjPath, pcieECCIntf, deviceIndex);
            auto pcieECCIntfSensorGroup4 = std::make_shared<NsmPCIeECCGroup4>(
                name, type, inventoryObjPath, pcieECCIntf, deviceIndex);

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

            nsmDevice->addSensor(pcieECCIntfSensorGroup2, priority);

            nsmDevice->addSensor(pcieECCIntfSensorGroup3, priority);

            nsmDevice->addSensor(pcieECCIntfSensorGroup4, priority);
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
