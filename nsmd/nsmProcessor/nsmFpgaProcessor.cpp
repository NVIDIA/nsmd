#include "nsmFpgaProcessor.hpp"

#include "dBusAsyncUtils.hpp"

#define FPGA_PROCESSOR_INTERFACE                                               \
    "xyz.openbmc_project.Configuration.NSM_FpgaProcessor"
namespace nsm
{

NsmFpgaProcessor::NsmFpgaProcessor(
    sdbusplus::bus::bus& bus, std::string& name, std::string& type,
    std::string& inventoryObjPath,
    const std::vector<utils::Association>& associations,
    const std::string& fpgaType, const std::string& locationType,
    std::string& health) :
    NsmObject(name, type)
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
    healthIntf = std::make_unique<HealthIntf>(bus, inventoryObjPath.c_str());
    healthIntf->health(HealthIntf::convertHealthTypeFromString(health));
}

static requester::Coroutine
    createNsmFpgaProcessorSensor(SensorManager& manager,
                                 const std::string& interface,
                                 const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", FPGA_PROCESSOR_INTERFACE);

        auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", FPGA_PROCESSOR_INTERFACE);

        auto type = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "InventoryObjPath", FPGA_PROCESSOR_INTERFACE);

        auto nsmDevice = manager.getNsmDevice(uuid);
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
            auto locationType = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "LocationType", interface.c_str());
            auto fpgaType = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "FpgaType", interface.c_str());
            std::vector<utils::Association> associations{};
            co_await utils::coGetAssociations(
                objPath, interface + ".Associations", associations);
            auto health = co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "Health", interface.c_str());

            auto processorSensor = std::make_shared<NsmFpgaProcessor>(
                bus, name, type, inventoryObjPath, associations, fpgaType,
                locationType, health);
            nsmDevice->deviceSensors.emplace_back(processorSensor);
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
