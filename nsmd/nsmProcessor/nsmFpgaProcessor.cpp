#include "nsmFpgaProcessor.hpp"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmInterface.hpp"

#include <unordered_map>

#define FPGA_PROCESSOR_INTERFACE                                               \
    "xyz.openbmc_project.Configuration.NSM_FpgaProcessor"
namespace nsm
{

NsmFpgaProcessor::NsmFpgaProcessor(
    sdbusplus::bus::bus& bus, std::string& name, std::string& type,
    std::string& inventoryObjPath,
    const std::vector<utils::Association>& associations,
    const std::string& fpgaType, const std::string& locationType,
    std::string& health) : NsmObject(name, type)
{
    acceleratorIntf =
        std::make_unique<AcceleratorIntf>(bus, inventoryObjPath.c_str());
    acceleratorIntf->type(accelaratorType::FPGA);
    assetIntf = std::make_unique<NsmAssetIntf>(bus, inventoryObjPath.c_str());
    assetIntf->manufacturer("NVIDIA");
    associationDefIntf =
        std::make_unique<AssociationDefIntf>(bus, inventoryObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefIntf->associations(associations_list);

    locationIntf = std::make_unique<LocationIntf>(bus,
                                                  inventoryObjPath.c_str());
    locationIntf->locationType(
        LocationIntf::convertLocationTypesFromString(locationType));
    fpgaTypeIntf = std::make_unique<FpgaTypeIntf>(bus,
                                                  inventoryObjPath.c_str());
    fpgaTypeIntf->fpgaType(FpgaTypeIntf::convertFPGATypeFromString(fpgaType));
    if (!health.empty())
    {
        healthIntf = std::make_unique<HealthIntf>(bus,
                                                  inventoryObjPath.c_str());
        healthIntf->health(HealthIntf::convertHealthTypeFromString(health));
    }
}

requester::Coroutine createNsmFpgaProcessorSensor(SensorManager& manager,
                                                  const std::string& interface,
                                                  const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();

        dbus::PropertyMap allBaseIfaceProperties;
        auto rc = co_await utils::coGetCachedBaseProperties(
            objPath, FPGA_PROCESSOR_INTERFACE, allBaseIfaceProperties);
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
                "The UUID of NSM_Processor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            // coverity[missing_return]
            co_return NSM_ERROR;
        }
        if (type == "NSM_FpgaProcessor")
        {
            std::string locationType{};
            if (allCurrentIfaceProperties.count("LocationType"))
            {
                locationType = std::get<std::string>(
                    allCurrentIfaceProperties.at("LocationType"));
            }
            std::string fpgaType{};
            if (allCurrentIfaceProperties.count("FpgaType"))
            {
                fpgaType = std::get<std::string>(
                    allCurrentIfaceProperties.at("FpgaType"));
            }

            std::vector<utils::Association> associations{};
            co_await utils::coGetAssociations(
                objPath, interface + ".Associations", associations);
            // NsmManagedStatus: the live NsmFpgaStatus from the
            // NSM_FpgaDiscoveryCheck owns Health and OperationalStatus
            bool nsmManagedStatus = false;
            if (allCurrentIfaceProperties.count("NsmManagedStatus"))
            {
                nsmManagedStatus = std::get<bool>(
                    allCurrentIfaceProperties.at("NsmManagedStatus"));
            }
            std::string health{};
            if (!nsmManagedStatus && allCurrentIfaceProperties.count("Health"))
            {
                health = std::get<std::string>(
                    allCurrentIfaceProperties.at("Health"));
            }

            auto processorSensor = std::make_shared<NsmFpgaProcessor>(
                bus, name, type, inventoryObjPath, associations, fpgaType,
                locationType, health);
            nsmDevice->addDeviceSensors(processorSensor);
            if (nsmManagedStatus)
            {
                // Location decorators as standalone NsmInterfaceProvider
                // objects, the same primitive nsmChassis uses for these
                const dbus::Interfaces objectPaths{inventoryObjPath};
                if (allCurrentIfaceProperties.count("LocationCode"))
                {
                    auto code = std::make_shared<
                        NsmInterfaceProvider<FpgaLocationCodeIntf>>(
                        name, type, objectPaths);
                    code->invoke(
                        pdiMethod(locationCode),
                        std::get<std::string>(
                            allCurrentIfaceProperties.at("LocationCode")));
                    nsmDevice->addStaticSensor(code);
                }
                if (allCurrentIfaceProperties.count("LocationContext"))
                {
                    auto ctx = std::make_shared<
                        NsmInterfaceProvider<FpgaLocationContextIntf>>(
                        name, type, objectPaths);
                    ctx->invoke(
                        pdiMethod(locationContext),
                        std::get<std::string>(
                            allCurrentIfaceProperties.at("LocationContext")));
                    nsmDevice->addStaticSensor(ctx);
                }
                if (allCurrentIfaceProperties.count("FieldReplaceable"))
                {
                    auto fru = std::make_shared<
                        NsmInterfaceProvider<FpgaReplaceableIntf>>(name, type,
                                                                   objectPaths);
                    fru->invoke(pdiMethod(fieldReplaceable),
                                std::get<bool>(allCurrentIfaceProperties.at(
                                    "FieldReplaceable")));
                    nsmDevice->addStaticSensor(fru);
                }
            }
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmFpgaProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_FpgaProcessor")
} // namespace nsm
