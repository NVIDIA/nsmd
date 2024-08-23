#include "nsmNetworkAdapter.hpp"

#include "dBusAsyncUtils.hpp"
#include "nsmDebugInfo.hpp"
#include "nsmDebugToken.hpp"
#include "nsmEraseTrace.hpp"
#include "nsmLogInfo.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmNetworkAdapterDI::NsmNetworkAdapterDI(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    auto objPath = inventoryObjPath + name;
    lg2::info("NsmNetworkAdapterDI: {NAME}", "NAME", name.c_str());

    associationDefIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, objPath.c_str());
    pcieDeviceIntf = std::make_unique<PCIeDeviceIntf>(bus, objPath.c_str());
    networkInterfaceIntf =
        std::make_unique<NetworkInterfaceIntf>(bus, objPath.c_str());

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

static requester::Coroutine
    createNSMNetworkAdapter(SensorManager& manager,
                            const std::string& interface,
                            const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", interface.c_str());
    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath,
                                               interface + ".Associations", associations);
    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_NetworkAdapter PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        co_return NSM_ERROR;
    }

    auto networkAdapterDI = std::make_shared<NsmNetworkAdapterDI>(
        bus, name, associations, type, inventoryObjPath);
    nsmDevice->deviceSensors.emplace_back(networkAdapterDI);

    auto debugTokenObject = std::make_shared<NsmDebugTokenObject>(
        bus, name, associations, type, uuid);
    nsmDevice->addStaticSensor(debugTokenObject);

    auto networkAdapterDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterDebugInfoObject);

    auto networkAdapterEraseTraceObject = std::make_shared<NsmEraseTraceObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterEraseTraceObject);

    auto networkAdapterLogInfoObject = std::make_shared<NsmLogInfoObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterLogInfoObject);

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNSMNetworkAdapter,
    "xyz.openbmc_project.Configuration.NSM_NetworkAdapter")

} // namespace nsm
