#include "nsmSwitch.hpp"

#include "deviceManager.hpp"
#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

namespace nsm
{

template <typename IntfType>
void NsmSwitchDI<IntfType>::fetchSpeedAndCalcBandwidth(double& currBandwidth,
                                                       double& maxBandwidth)
{
    currBandwidth = 0.0;
    maxBandwidth = 0.0;

    /*try
    {
        auto getAllPortOnSwitch =
            utils::DBusHandler().getAssociatedObjects(objPath, "all_states");
        auto portObjOnSwitch =
            std::get<std::vector<std::string>>(getAllPortOnSwitch);
        for (const auto& object : portObjOnSwitch)
        {
            auto getAllAssociatedObjects =
                utils::DBusHandler().getAssociatedObjects(object,
                                                          "associated_port");
            auto associatedPortObjects =
                std::get<std::vector<std::string>>(getAllAssociatedObjects);
            for (const auto& associatedObject : associatedPortObjects)
            {
                std::string portInfoIntf =
                    "xyz.openbmc_project.Inventory.Decorator.PortInfo";
                auto properties = utils::DBusHandler().getDbusProperties(
                    associatedObject.c_str(), portInfoIntf.c_str());

                for (const auto& pair : properties)
                {
                    if (pair.first == "CurrentSpeed")
                    {
                        currBandwidth += std::get<double>(pair.second);
                    }
                    else if (pair.first == "MaxSpeed")
                    {
                        maxBandwidth += std::get<double>(pair.second);
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to update bandwidth on NVSwitch, {ERROR}.", "ERROR",
                   e);
    }*/
}

template <typename IntfType>
void NsmSwitchDI<IntfType>::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(this->pdi().interface);
    std::vector<uint8_t> rawSmbpbiData = {};

    if constexpr (std::is_same_v<IntfType, SwitchIntf>)
    {
        nv::sensor_aggregation::DbusVariantType variant{
            this->pdi().currentBandwidth()};
        std::string propName = "CurrentBandwidth";
        nsm_shmem_utils::updateSharedMemoryOnSuccess(
            objPath, ifaceName, propName, rawSmbpbiData, variant);
    }
#endif
}

template <typename IntfType>
requester::Coroutine NsmSwitchDI<IntfType>::update(SensorManager& manager,
                                                   eid_t eid)
{
    DeviceManager& deviceManager = DeviceManager::getInstance();
    auto uuid = utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
    if (uuid)
    {
        if constexpr (std::is_same_v<IntfType, UuidIntf>)
        {
            auto nsmDevice = manager.getNsmDevice(*uuid);
            if (nsmDevice)
            {
                this->pdi().uuid(nsmDevice->deviceUuid);
            }
        }
    }

    // currentBandwidth & maxBandwidth on NVSwitch
    if constexpr (std::is_same_v<IntfType, SwitchIntf>)
    {
        double currentBandwidthGbps = 0.0;
        double maxBandwidth = 0.0;

        fetchSpeedAndCalcBandwidth(currentBandwidthGbps, maxBandwidth);

        this->pdi().currentBandwidth(currentBandwidthGbps);
        this->pdi().maxBandwidth(maxBandwidth);
        updateMetricOnSharedMemory();
    }
    co_return NSM_SUCCESS;
}

void createNsmSwitchDI(SensorManager& manager, const std::string& interface,
                       const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch";

    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto inventoryObjPath = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_NVSwitch")
    {
        auto nvSwitchUuid =
            std::make_shared<NsmSwitchDI<UuidIntf>>(name, inventoryObjPath);
        auto nvSwitchAssociation =
            std::make_shared<NsmSwitchDI<AssociationDefinitionsInft>>(
                name, inventoryObjPath);

        auto associations = utils::getAssociations(objPath,
                                                   interface + ".Associations");

        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        for (const auto& association : associations)
        {
            associations_list.emplace_back(association.forward,
                                           association.backward,
                                           association.absolutePath);
        }
        nvSwitchAssociation->pdi().associations(associations_list);
        nvSwitchUuid->pdi().uuid(uuid);

        device->addStaticSensor(nvSwitchUuid);
        device->addStaticSensor(nvSwitchAssociation);
    }
    else if (type == "NSM_Switch")
    {
        auto nvSwitchObject =
            std::make_shared<NsmSwitchDI<SwitchIntf>>(name, inventoryObjPath);
        auto switchType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "SwitchType", interface.c_str());
        auto switchProtocols =
            utils::DBusHandler().getDbusProperty<std::vector<std::string>>(
                objPath.c_str(), "SwitchSupportedProtocols", interface.c_str());

        std::vector<SwitchIntf::SwitchType> supported_protocols;
        for (const auto& protocol : switchProtocols)
        {
            supported_protocols.emplace_back(
                SwitchIntf::convertSwitchTypeFromString(protocol));
        }
        nvSwitchObject->pdi().type(
            SwitchIntf::convertSwitchTypeFromString(switchType));
        nvSwitchObject->pdi().supportedProtocols(supported_protocols);

        // maxSpeed and currentSpeed from PLDM T2

        device->addSensor(nvSwitchObject, false);
    }
    else if (type == "NSM_Asset")
    {
        auto nvSwitchAsset =
            std::make_shared<NsmSwitchDI<AssetIntf>>(name, inventoryObjPath);
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());

        nvSwitchAsset->pdi().manufacturer(manufacturer);
        device->addStaticSensor(nvSwitchAsset);
    }
}

std::vector<std::string> nvSwitchInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVSwitch",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.Switch"};

REGISTER_NSM_CREATION_FUNCTION(createNsmSwitchDI, nvSwitchInterfaces)
} // namespace nsm
