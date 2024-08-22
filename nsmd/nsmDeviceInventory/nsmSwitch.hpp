#pragma once

#include "libnsm/diagnostics.h"
#include "libnsm/network-ports.h"

#include "asyncOperationManager.hpp"
#include "nsmDbusIfaceOverride/nsmResetIface.hpp"
#include "nsmInterface.hpp"
#include "utils.hpp"
#include "nsmAssetIntf.hpp"
#include <com/nvidia/PowerMode/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <tal.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Control/Processor/Reset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/NvSwitch/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Switch/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using UuidIntf = object_t<Common::server::UUID>;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using SwitchIntf = object_t<Inventory::Item::server::Switch>;
using NvSwitchIntf = object_t<Inventory::Item::server::NvSwitch>;
using ResetIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::processor::Reset>;
using L1PowerModeIntf = object_t<sdbusplus::com::nvidia::server::PowerMode>;

template <typename IntfType>
class NsmSwitchDI : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmSwitchDI() = delete;
    NsmSwitchDI(const std::string& name, const std::string& inventoryObjPath) :
        NsmInterfaceProvider<IntfType>(name, "NSM_NVSwitch", inventoryObjPath),
        objPath(inventoryObjPath + name)
    {}

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::string objPath;
};

class NsmSwitchDIReset : public NsmObject
{
  public:
    NsmSwitchDIReset(sdbusplus::bus::bus& bus, const std::string& name,
                     const std::string& type, std::string& inventoryObjPath,
                     std::shared_ptr<NsmDevice> device);

  private:
    std::shared_ptr<ResetIntf> resetIntf = nullptr;
    std::shared_ptr<NsmSwitchResetAsyncIntf> resetAsyncIntf = nullptr;
    std::string objPath;
};

class NsmSwitchDIPowerMode : public NsmInterfaceProvider<L1PowerModeIntf>
{
  public:
    NsmSwitchDIPowerMode() = delete;
    NsmSwitchDIPowerMode(const std::string& name,
                         const std::string& inventoryObjPath) :
        NsmInterfaceProvider<L1PowerModeIntf>(name, "NSM_NVSwitch",
                                              inventoryObjPath),
        objPath(inventoryObjPath + name)
    {}

    std::string getInventoryObjectPath()
    {
        return objPath;
    }

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

    struct nsm_power_mode_data getPowerModeData();

    requester::Coroutine setL1PowerDevice(struct nsm_power_mode_data data,
                                          AsyncOperationStatusType* status,
                                          std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setL1HWModeControl(const AsyncSetOperationValueType& value,
                           AsyncOperationStatusType* status,
                           std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setL1FWThrottlingMode(const AsyncSetOperationValueType& value,
                              AsyncOperationStatusType* status,
                              std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setL1PredictionMode(const AsyncSetOperationValueType& value,
                            AsyncOperationStatusType* status,
                            std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setL1HWThreshold(const AsyncSetOperationValueType& value,
                         AsyncOperationStatusType* status,
                         std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setL1HWActiveTime(const AsyncSetOperationValueType& value,
                          AsyncOperationStatusType* status,
                          std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setL1HWInactiveTime(const AsyncSetOperationValueType& value,
                            AsyncOperationStatusType* status,
                            std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setL1HWPredictionInactiveTime(const AsyncSetOperationValueType& value,
                                      AsyncOperationStatusType* status,
                                      std::shared_ptr<NsmDevice> device);

  private:
    std::string objPath;
    bool asyncPatchInProgress{false};
};

} // namespace nsm
