#include "GPUSWInventory.hpp"

#include "dBusAsyncUtils.hpp"
#include "nsmAssetIntf.hpp"

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
        lg2::error(
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
    char driverVersion[MAX_VERSION_STRING_SIZE] = {0};

    rc = decode_get_driver_info_resp(responseMsg.get(), responseLen, &cc,
                                     &reasonCode, &driverState, driverVersion);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        std::string version(driverVersion);
        updateValue(driverState, version);
        clearErrorBitMap("decode_get_driver_info_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_driver_info_resp", reasonCode, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

static requester::Coroutine createGPUDriverSensor(SensorManager& manager,
                                                  const std::string& interface,
                                                  const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto manufacturer = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Manufacturer", interface.c_str());
    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDevice(uuid);
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
    nsmDevice->capabilityRefreshSensors.emplace_back(sensor);

    // update sensor
    nsmDevice->addStaticSensor(sensor);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createGPUDriverSensor,
    "xyz.openbmc_project.Configuration.NSM_GPU_SWInventory")

} // namespace nsm
