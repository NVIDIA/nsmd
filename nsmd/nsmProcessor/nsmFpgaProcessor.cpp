#include "nsmFpgaProcessor.hpp"
#define FPGA_PROCESSOR_INTERFACE                                               \
    "xyz.openbmc_project.Configuration.NSM_FpgaProcessor"
namespace nsm
{

NsmFpgaProcessor::NsmFpgaProcessor(
    sdbusplus::bus::bus& bus, std::string& name, std::string& type,
    std::string& inventoryObjPath,
    const std::vector<utils::Association>& associations,
    const std::string& fpgaType, const std::string& locationType, std::string& health) :
    NsmObject(name, type)
{
    acceleratorIntf =
        std::make_unique<AcceleratorIntf>(bus, inventoryObjPath.c_str());
    acceleratorIntf->type(accelaratorType::FPGA);
    assetIntf = std::make_unique<AssetIntf>(bus, inventoryObjPath.c_str());
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

    locationIntf =
        std::make_unique<LocationIntf>(bus, inventoryObjPath.c_str());
    locationIntf->locationType(
        LocationIntf::convertLocationTypesFromString(locationType));
    fpgaTypeIntf =
        std::make_unique<FpgaTypeIntf>(bus, inventoryObjPath.c_str());
    fpgaTypeIntf->fpgaType(FpgaTypeIntf::convertFPGATypeFromString(fpgaType));
    healthIntf = std::make_unique<HealthIntf>(bus, inventoryObjPath.c_str());
    healthIntf->health(HealthIntf::convertHealthTypeFromString(health));
}

static void createNsmFpgaProcessorSensor(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", FPGA_PROCESSOR_INTERFACE);

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", FPGA_PROCESSOR_INTERFACE);

        auto type = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", FPGA_PROCESSOR_INTERFACE);

        auto nsmDevice = manager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_Processor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            return;
        }
        if (type == "NSM_FpgaProcessor")
        {
            auto locationType =
                utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "LocationType", interface.c_str());
            auto fpgaType = utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "FpgaType", interface.c_str());
            auto associations =
                utils::getAssociations(objPath, interface + ".Associations");
            auto health = utils::DBusHandler().getDbusProperty<std::string>(
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
        return;
    }
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmFpgaProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_FpgaProcessor")
} // namespace nsm