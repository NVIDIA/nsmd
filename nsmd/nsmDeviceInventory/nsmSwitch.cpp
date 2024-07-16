#include "nsmSwitch.hpp"

#include "asyncOperationManager.hpp"
#include "deviceManager.hpp"
#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmDebugToken.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPort/nsmPortDisableFuture.hpp"
#include "utils.hpp"

namespace nsm
{

NsmSwitchDIReset::NsmSwitchDIReset(sdbusplus::bus::bus& bus,
                                   const std::string& name,
                                   const std::string& type,
                                   std::string& inventoryObjPath,
                                   std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type)
{
    lg2::info("NsmSwitchDIReset: create sensor:{NAME}", "NAME", name.c_str());

    objPath = inventoryObjPath + name;
    resetIntf = std::make_shared<NsmResetIntf>(bus, objPath.c_str());
    resetIntf->resetType(sdbusplus::common::xyz::openbmc_project::control::
                             processor::Reset::ResetTypes::ForceRestart);
    resetAsyncIntf =
        std::make_shared<NsmSwitchResetAsyncIntf>(bus, objPath.c_str(), device);
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
    co_return NSM_SUCCESS;
}

void createNsmSwitchDI(SensorManager& manager, const std::string& interface,
                       const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch";

    auto& bus = utils::DBusHandler::getBus();
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
        auto nvSwitchIntf =
            std::make_shared<NsmSwitchDI<NvSwitchIntf>>(name, inventoryObjPath);
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

        device->deviceSensors.emplace_back(nvSwitchIntf);
        device->addStaticSensor(nvSwitchUuid);
        device->addStaticSensor(nvSwitchAssociation);

        auto debugTokenObject = std::make_shared<NsmDebugTokenObject>(
            bus, name, associations, type, uuid);
        device->addStaticSensor(debugTokenObject);

        // Device Reset for NVSwitch
        auto nvSwitchResetSensor = std::make_shared<NsmSwitchDIReset>(
            bus, name, type, inventoryObjPath, device);
        device->deviceSensors.push_back(nvSwitchResetSensor);
    }
    else if (type == "NSM_PortDisableFuture")
    {
        // Port disable future status on NVSwitch
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto nvSwitchPortDisableFuture =
            std::make_shared<nsm::NsmDevicePortDisableFuture>(name, type,
                                                              inventoryObjPath);

        nvSwitchPortDisableFuture->pdi().portDisableFuture(
            std::vector<uint8_t>{});
        device->addSensor(nvSwitchPortDisableFuture, priority);

        nsm::AsyncSetOperationHandler setPortDisableFutureHandler =
            std::bind(&NsmDevicePortDisableFuture::setPortDisableFuture,
                      nvSwitchPortDisableFuture, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        AsyncOperationManager::getInstance()
            ->getDispatcher(nvSwitchPortDisableFuture->getInventoryObjectPath())
            ->addAsyncSetOperation(
                "com.nvidia.NVLink.NVLinkDisableFuture", "PortDisableFuture",
                AsyncSetOperationInfo{setPortDisableFutureHandler,
                                      nvSwitchPortDisableFuture, device});
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
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.PortDisableFuture",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.Asset"};

REGISTER_NSM_CREATION_FUNCTION(createNsmSwitchDI, nvSwitchInterfaces)
} // namespace nsm
