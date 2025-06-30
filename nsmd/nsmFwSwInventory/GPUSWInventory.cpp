#include "GPUSWInventory.hpp"

#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmCommon/sharedMemCommon.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <optional>
#include <vector>
namespace nsm
{
NsmGPUSWInventoryDriverVersionAndStatus::
    NsmGPUSWInventoryDriverVersionAndStatus(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::vector<utils::Association>& associations,
        const std::string& type, const std::string& manufacturer) :
    NsmObject(name, type)
{
    auto GPUFWInvBasePath = std::string(sotwareInventoryBasePath) + "/" + name;

    lg2::info("NsmGPUSWInventoryDriverVersionAndStatus: create sensor:{NAME}",
              "NAME", name.c_str());
    softwareVer = std::make_unique<SoftwareIntf>(bus, GPUFWInvBasePath.c_str());
    operationalStatus =
        std::make_unique<OperationalStatusIntf>(bus, GPUFWInvBasePath.c_str());
    // add all interfaces
    associationDef = std::make_unique<AssociationDefinitionsInft>(
        bus, GPUFWInvBasePath.c_str());
    // handle associations
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDef->associations(associations_list);
    asset = std::make_unique<NsmAssetIntf>(bus, GPUFWInvBasePath.c_str());
    asset->manufacturer(manufacturer);
    objPath = GPUFWInvBasePath.c_str();
    updateValue(DriverStateUnknown, "");
}

void NsmGPUSWInventoryDriverVersionAndStatus::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(operationalStatus->interface);
    std::vector<uint8_t> rawSmbpbiData = {};
    std::string propName = "Functional";

    nv::sensor_aggregation::DbusVariantType variantFun{
        operationalStatus->functional()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(objPath, ifaceName, propName,
                                                 rawSmbpbiData, variantFun);
#endif
}

void NsmGPUSWInventoryDriverVersionAndStatus::updateValue(
    enum8 driverState, std::string driverVersion)
{
    softwareVer->version(driverVersion);
    switch (static_cast<DriverStateEnum>(driverState))
    {
        case DriverLoaded:
            operationalStatus->functional(true);
            break;
        default:
            operationalStatus->functional(false);
            break;
    }

    // to be consumed by unit tests
    this->driverState = driverState;
    this->driverVersion = driverVersion;
    updateMetricOnSharedMemory();
}

requester::Coroutine
    NsmGPUSWInventoryDriverVersionAndStatus::update(SensorManager& manager,
                                                    eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_driver_info_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_driverVersion_req failed for GPU eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    enum8 driverState = 0;
    std::string driverVersion("", MAX_VERSION_STRING_SIZE);

    rc = decode_get_driver_info_resp(responseMsg.get(), responseLen, &cc,
                                     &reasonCode, &driverState,
                                     (char*)driverVersion.data());

    LG2_ERROR_FLT(
        "decode_get_driver_info_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        // Set previous version on error;
        driverVersion = this->driverVersion;
    }

    // Check if the values have changed
    bool stateChanged = (this->driverState != driverState);
    updateValue(driverState, driverVersion);
    if (stateChanged)
    {
        lg2::info(
            "NsmGPUSWInventoryDriverVersionAndStatus: state changed eid={EID}",
            "EID", eid);
        DeviceManager& deviceManager = DeviceManager::getInstance();
        co_await deviceManager.updateNsmDevice(nsmDeviceFound, eid);
        co_await DeviceManager::getInstance().refreshCapabilitySensor(
            nsmDeviceFound);
    }

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

static requester::Coroutine createGPUDriverSensor(SensorManager& manager,
                                                  const std::string& interface,
                                                  const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string manufacturer{};
    if (allCurrentIfaceProperties.count("Manufacturer"))
    {
        manufacturer =
            std::get<std::string>(allCurrentIfaceProperties.at("Manufacturer"));
    }

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_GPUSWInventory PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto sensor = std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
        bus, name, associations, type, manufacturer);
    nsmDevice->gpudriverSensor = sensor;
    // update sensor
    nsmDevice->addSensor(sensor, false);
    sensor->nsmDeviceFound = nsmDevice;
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createGPUDriverSensor,
    "xyz.openbmc_project.Configuration.NSM_GPU_SWInventory")

} // namespace nsm
