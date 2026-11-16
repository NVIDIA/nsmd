#include "nsmPCIeRetimerFabricsDI.hpp"

#include "dBusAsyncUtils.hpp"
#include "features/associations.hpp"
#include "globals.hpp"
#include "nsmDeviceInventoryFeatures.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmPCIeRetimerFabricDI::NsmPCIeRetimerFabricDI(
    sdbusplus::bus_t& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& fabricsType) :
    NsmObject(name, type)
{
    auto inventoryObjPath = std::string(fabricsInventoryBasePath) + "/" + name;
    lg2::info("NsmPCIeRetimerFabricDI: {NAME}", "NAME", name.c_str());

    associationDefIntf = std::make_unique<AssociationDefinitionsInft>(
        bus, inventoryObjPath.c_str());
    uuidIntf = std::make_unique<UuidIntf>(bus, inventoryObjPath.c_str());
    fabricIntf = std::make_unique<FabricIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    associationDefIntf->associations(associationsList);

    fabricIntf->FabricIntf::type(
        FabricIntf::convertFabricTypeFromString(fabricsType));
}

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createPCIeRetimerFabricsFeature([[maybe_unused]] SensorManager& manager,
                                    const DeviceContext& dev,
                                    const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("PCIeRetimerFabrics feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }
    auto& bus = utils::DBusHandler::getBus();
    const std::string type = "NSM_PCIeRetimer_Fabrics";
    auto name = param<std::string>(params, "Name").value_or("");
    auto fabricType = param<std::string>(params, "FabricType").value_or("");

    std::vector<utils::Association> associations{};
    addAssociations(associations, params, "ChassisAssociationPaths", "chassis",
                    "fabrics");

    auto retimerFabricsDi = std::make_shared<NsmPCIeRetimerFabricDI>(
        bus, name, associations, type, fabricType);
    dev.device->addDeviceSensors(retimerFabricsDi);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
