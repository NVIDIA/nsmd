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

struct nsm_power_mode_data NsmSwitchDIPowerMode::getPowerModeData()
{
    struct nsm_power_mode_data powerModeData;
    powerModeData.l1_hw_mode_control = this->pdi().hwModeControl();
    powerModeData.l1_hw_mode_threshold =
        static_cast<uint32_t>(this->pdi().hwThreshold());
    powerModeData.l1_fw_throttling_mode = this->pdi().fwThrottlingMode();
    powerModeData.l1_prediction_mode = this->pdi().predictionMode();
    powerModeData.l1_hw_active_time =
        static_cast<uint16_t>(this->pdi().hwActiveTime());
    powerModeData.l1_hw_inactive_time =
        static_cast<uint16_t>(this->pdi().hwInactiveTime());
    powerModeData.l1_prediction_inactive_time =
        static_cast<uint16_t>(this->pdi().hwPredictionInactiveTime());

    return powerModeData;
}

requester::Coroutine NsmSwitchDIPowerMode::update(SensorManager& manager,
                                                  eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_power_mode_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_power_mode_req failed. eid={EID} rc={RC}", "EID",
                   eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_power_mode_data data;

    rc = decode_get_power_mode_resp(responseMsg.get(), responseLen, &cc,
                                    &dataSize, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // update values
        (data.l1_hw_mode_control == 1) ? this->pdi().hwModeControl(true)
                                       : this->pdi().hwModeControl(false);
        this->pdi().hwThreshold(
            static_cast<uint64_t>(data.l1_hw_mode_threshold));
        (data.l1_fw_throttling_mode == 1) ? this->pdi().fwThrottlingMode(true)
                                          : this->pdi().fwThrottlingMode(false);
        (data.l1_prediction_mode == 1) ? this->pdi().predictionMode(true)
                                       : this->pdi().predictionMode(false);
        this->pdi().hwActiveTime(static_cast<uint64_t>(data.l1_hw_active_time));
        this->pdi().hwInactiveTime(
            static_cast<uint64_t>(data.l1_hw_inactive_time));
        this->pdi().hwPredictionInactiveTime(
            static_cast<uint64_t>(data.l1_prediction_inactive_time));
    }
    else
    {
        lg2::error(
            "responseHandler: decode_get_power_mode_resp unsuccessfull. reasonCode={RSNCOD} cc={CC} rc={RC}",
            "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SUCCESS;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1PowerDevice(
    struct nsm_power_mode_data data,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("setL1PowerDevice for EID: {EID}", "EID", eid);

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_power_mode_req(0, requestMsg, data);

    if (rc)
    {
        lg2::error(
            "setL1PowerDevice encode_set_power_mode_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "setL1PowerDevice SendRecvNsmMsgSync failed for while setting PowerMode "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_power_mode_resp(responseMsg.get(), responseLen, &cc,
                                    &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info("setL1PowerDevice for EID: {EID} completed", "EID", eid);
    }
    else
    {
        lg2::error(
            "setL1PowerDevice decode_set_power_mode_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        lg2::error("throwing write failure exception");
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWModeControl(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* l1HWModeControl = std::get_if<bool>(&value);

    if (!l1HWModeControl)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    if (*l1HWModeControl)
    {
        l1PowerModeData.l1_hw_mode_control = 1;
    }
    else
    {
        l1PowerModeData.l1_hw_mode_control = 0;
    }

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;

    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1FWThrottlingMode(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* l1FWThrottlingMode = std::get_if<bool>(&value);

    if (!l1FWThrottlingMode)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    if (*l1FWThrottlingMode)
    {
        l1PowerModeData.l1_fw_throttling_mode = 1;
    }
    else
    {
        l1PowerModeData.l1_fw_throttling_mode = 0;
    }

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;

    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1PredictionMode(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* l1PredictionMode = std::get_if<bool>(&value);

    if (!l1PredictionMode)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    if (*l1PredictionMode)
    {
        l1PowerModeData.l1_prediction_mode = 1;
    }
    else
    {
        l1PowerModeData.l1_prediction_mode = 0;
    }

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;

    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWThreshold(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* l1HWThreshold = std::get_if<uint32_t>(&value);

    if (!l1HWThreshold)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    l1PowerModeData.l1_hw_mode_threshold =
        static_cast<uint64_t>(*l1HWThreshold);

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;

    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWActiveTime(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* l1HWActiveTime = std::get_if<uint32_t>(&value);

    if (!l1HWActiveTime)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    l1PowerModeData.l1_hw_active_time = static_cast<uint64_t>(*l1HWActiveTime);

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;

    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWInactiveTime(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* l1HWInactiveTime = std::get_if<uint32_t>(&value);

    if (!l1HWInactiveTime)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    l1PowerModeData.l1_hw_inactive_time =
        static_cast<uint64_t>(*l1HWInactiveTime);

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;

    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWPredictionInactiveTime(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* l1HWPredictionInactiveTime = std::get_if<uint32_t>(&value);

    if (!l1HWPredictionInactiveTime)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    l1PowerModeData.l1_prediction_inactive_time =
        static_cast<uint64_t>(*l1HWPredictionInactiveTime);

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;

    co_return rc;
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
    else if (type == "NSM_PowerMode")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto nvSwitchL1PowerMode =
            std::make_shared<NsmSwitchDIPowerMode>(name, inventoryObjPath);

        nvSwitchL1PowerMode->pdi().hwModeControl(false);
        nvSwitchL1PowerMode->pdi().hwThreshold(0);
        nvSwitchL1PowerMode->pdi().fwThrottlingMode(false);
        nvSwitchL1PowerMode->pdi().predictionMode(false);
        nvSwitchL1PowerMode->pdi().hwActiveTime(0);
        nvSwitchL1PowerMode->pdi().hwInactiveTime(0);
        nvSwitchL1PowerMode->pdi().hwPredictionInactiveTime(0);

        device->addSensor(nvSwitchL1PowerMode, priority);
        auto objectPath = nvSwitchL1PowerMode->getInventoryObjectPath();

        nsm::AsyncSetOperationHandler setL1HWModeControlHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWModeControl,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWModeControl",
                AsyncSetOperationInfo{setL1HWModeControlHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1FWThrottlingModeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1FWThrottlingMode,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "FWThrottlingMode",
                AsyncSetOperationInfo{setL1FWThrottlingModeHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1PredictionModeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1PredictionMode,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "PredictionMode",
                AsyncSetOperationInfo{setL1PredictionModeHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1HWThresholdHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWThreshold,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWThreshold",
                AsyncSetOperationInfo{setL1HWThresholdHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1HWActiveTimeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWActiveTime,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWActiveTime",
                AsyncSetOperationInfo{setL1HWActiveTimeHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1HWInactiveTimeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWInactiveTime,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWInactiveTime",
                AsyncSetOperationInfo{setL1HWInactiveTimeHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1HWPredictionInactiveTimeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWPredictionInactiveTime,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWPredictionInactiveTime",
                AsyncSetOperationInfo{setL1HWPredictionInactiveTimeHandler,
                                      nvSwitchL1PowerMode, device});
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
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.PowerMode",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.Asset"};

REGISTER_NSM_CREATION_FUNCTION(createNsmSwitchDI, nvSwitchInterfaces)
} // namespace nsm
