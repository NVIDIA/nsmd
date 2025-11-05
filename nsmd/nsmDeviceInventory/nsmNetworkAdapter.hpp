#pragma once

#include "libnsm/diagnostics.h"

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
#include "nsmDbusIfaceOverride/nsmResetIface.hpp"
#endif
#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <com/nvidia/DeviceProtection/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Control/Reset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/NetworkInterface/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;
using NetworkInterfaceIntf =
    object_t<Inventory::Item::server::NetworkInterface>;
using ResetDeviceIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::Reset>;
using ProtectionIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::DeviceProtection>;
using ProtectionOption =
    sdbusplus::server::com::nvidia::DeviceProtection::ProtectionOption;

class NsmDeviceProtectionOptions : public NsmSensor
{
  public:
    NsmDeviceProtectionOptions(sdbusplus::bus::bus& bus, const char* path,
                               const std::string& name,
                               const std::string& type);
    NsmDeviceProtectionOptions() = delete;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    requester::Coroutine
        setProtectionOptions(const AsyncSetOperationValueType& value,
                             [[maybe_unused]] AsyncOperationStatusType* status,
                             std::shared_ptr<NsmDevice> device);

  private:
    ProtectionOption convertNsmdToDbusProtectionMode(uint8_t protectionMode);
    std::shared_ptr<ProtectionIntf> protectionIntf;
    bool asyncPatchInProgress{false};
    std::string objPath;
};

class NsmNetworkAdapterDI : public NsmObject
{
  public:
    NsmNetworkAdapterDI(sdbusplus::bus::bus& bus, const std::string& name,
                        const std::vector<utils::Association>& associations,
                        const std::string& type,
                        const std::string& inventoryObjPath);

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefIntf = nullptr;
    std::unique_ptr<PCIeDeviceIntf> pcieDeviceIntf = nullptr;
    std::unique_ptr<NetworkInterfaceIntf> networkInterfaceIntf = nullptr;
};

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
class NsmNetworkAdapterDIReset : public NsmObject
{
  public:
    NsmNetworkAdapterDIReset(sdbusplus::bus::bus& bus, const std::string& name,
                             const std::string& type,
                             std::string& inventoryObjPath,
                             std::shared_ptr<NsmDevice> device);

  private:
    std::shared_ptr<ResetDeviceIntf> resetIntf = nullptr;
    std::shared_ptr<NsmNetworkDeviceResetAsyncIntf> resetAsyncIntf = nullptr;
    std::string objPath;
};
#endif
} // namespace nsm
