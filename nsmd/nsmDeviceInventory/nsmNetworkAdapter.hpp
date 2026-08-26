#pragma once

#include "device-configuration.h"
#include "libnsm/diagnostics.h"

#include <functional>

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
#include "nsmDbusIfaceOverride/nsmResetIface.hpp"
#endif
#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <com/nvidia/DeviceMode/DPUOperationMode/server.hpp>
#include <com/nvidia/DeviceMode/PCIeBifurcation/server.hpp>
#include <com/nvidia/DeviceMode/PCIeControlledEWTraffic/server.hpp>
#include <com/nvidia/DeviceMode/PCIeMultiSocket/server.hpp>
#include <com/nvidia/DeviceProtection/server.hpp>
#include <com/nvidia/Software/ProtectionOptionsMode/server.hpp>
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
using DPUOperationModeIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::DeviceMode::server::DPUOperationMode,
    Association::server::Definitions>;
using DPUOperationModeServer =
    sdbusplus::com::nvidia::DeviceMode::server::DPUOperationMode;
using OperationMode = DPUOperationModeServer::OperationMode;

using PCIeDeviceModeIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::DeviceMode::server::PCIeMultiSocket,
    sdbusplus::com::nvidia::DeviceMode::server::PCIeControlledEWTraffic,
    sdbusplus::com::nvidia::DeviceMode::server::PCIeBifurcation,
    Association::server::Definitions>;

using ProtectionOptionsModeServer =
    sdbusplus::com::nvidia::Software::server::ProtectionOptionsMode;

// ProtectionOptionsModeServer has a pure-virtual setProtectionOptions() because
// the PDI YAML declares a SetProtectionOptions method. bmcweb uses individual
// bool property writes via AsyncOperationManager instead of the method, so the
// concrete class below implements the pure virtual by throwing NotAllowed.
class ProtectionOptionsModeIntf : public object_t<ProtectionOptionsModeServer>
{
  public:
    using object_t<ProtectionOptionsModeServer>::object_t;

    void setProtectionOptions(bool, bool, bool, bool) override
    {
        throw sdbusplus::error::xyz::openbmc_project::common::NotAllowed{};
    }
};
using PCIeMultiSocketServer =
    sdbusplus::com::nvidia::DeviceMode::server::PCIeMultiSocket;
using PCIeControlledEWTrafficServer =
    sdbusplus::com::nvidia::DeviceMode::server::PCIeControlledEWTraffic;
using EWTrafficMode = PCIeControlledEWTrafficServer::EWTrafficMode;
using PCIeBifurcationServer =
    sdbusplus::com::nvidia::DeviceMode::server::PCIeBifurcation;

class NsmDeviceProtectionOptions : public NsmSensor
{
  public:
    NsmDeviceProtectionOptions(sdbusplus::bus_t& bus, const char* path,
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
    NsmNetworkAdapterDI(sdbusplus::bus_t& bus, const std::string& name,
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
    NsmNetworkAdapterDIReset(sdbusplus::bus_t& bus, const std::string& name,
                             const std::string& type,
                             std::string& inventoryObjPath,
                             std::shared_ptr<NsmDevice> device);

  private:
    std::shared_ptr<ResetDeviceIntf> resetIntf = nullptr;
    std::shared_ptr<NsmNetworkDeviceResetAsyncIntf> resetAsyncIntf = nullptr;
    std::string objPath;
};
#endif

// ---- Device Mode Settings V2 sensor classes ----

class NsmDeviceModeSettingsV2Base : public NsmSensor
{
  public:
    NsmDeviceModeSettingsV2Base(const std::string& name,
                                const std::string& type,
                                enum device_mode_index deviceModeIndex,
                                uint8_t patchabilityBitmap);

    static bool isModeBitSet(uint8_t bitmap, uint8_t subModeOffset);

  protected:
    bool isSubDeviceModePatchable(uint8_t subModeOffset) const;

    enum device_mode_index deviceModeIndex;
    uint8_t patchabilityBitmap;
    static constexpr uint8_t deviceModeSettingsNoChange = 0xFF;
};

class NsmDeviceModeSettingsV2GetBase : public NsmDeviceModeSettingsV2Base
{
  public:
    NsmDeviceModeSettingsV2GetBase(const std::string& name,
                                   const std::string& type,
                                   enum device_mode_index deviceModeIndex,
                                   uint8_t patchabilityBitmap);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  protected:
    virtual uint8_t handleDeviceModeGetPayload(const uint8_t* currentData,
                                               uint16_t currentLength,
                                               const uint8_t* pendingData,
                                               uint16_t pendingLength) = 0;
};

class NsmDeviceModeSettingsV2SetBase : public NsmDeviceModeSettingsV2Base
{
  public:
    NsmDeviceModeSettingsV2SetBase(const std::string& name,
                                   const std::string& type,
                                   enum device_mode_index deviceModeIndex,
                                   uint8_t patchabilityBitmap);

    std::optional<std::vector<uint8_t>>
        genRequestMsg([[maybe_unused]] eid_t eid,
                      [[maybe_unused]] uint8_t instanceId) override;
    uint8_t
        handleResponseMsg([[maybe_unused]] const struct nsm_msg* responseMsg,
                          [[maybe_unused]] size_t responseLen) override;

    // Set sensors are PATCH dispatchers, not polling sensors: registered
    // as static sensors for device-level lifecycle tracking,
    // update() is a no-op that returns success
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

  protected:
    std::optional<std::vector<uint8_t>>
        createSetRequestMsg(uint8_t instanceId,
                            const std::vector<uint8_t>& modeData) const;
    bool asyncPatchInProgress{false};
};

class NsmDPUOperationModeDeviceModeSettingsV2Get :
    public NsmDeviceModeSettingsV2GetBase
{
  public:
    static constexpr enum device_mode_index modeIndex =
        DEVICE_MODE_DPU_OPERATION_MODE;

    NsmDPUOperationModeDeviceModeSettingsV2Get(
        const std::string& name, const std::string& type,
        uint8_t patchabilityBitmap,
        std::shared_ptr<DPUOperationModeIntf> deviceModeIntf);

  protected:
    uint8_t handleDeviceModeGetPayload(const uint8_t* currentData,
                                       uint16_t currentLength,
                                       const uint8_t* pendingData,
                                       uint16_t pendingLength) override;

  private:
    std::shared_ptr<DPUOperationModeIntf> deviceModeIntf;
};

class NsmDPUOperationModeDeviceModeSettingsV2Set :
    public NsmDeviceModeSettingsV2SetBase
{
  public:
    static constexpr enum device_mode_index modeIndex =
        DEVICE_MODE_DPU_OPERATION_MODE;

    NsmDPUOperationModeDeviceModeSettingsV2Set(
        const std::string& name, const std::string& type,
        uint8_t patchabilityBitmap,
        std::shared_ptr<DPUOperationModeIntf> deviceModeIntf);

    requester::Coroutine setPendingMode(const AsyncSetOperationValueType& value,
                                        AsyncOperationStatusType* status,
                                        std::shared_ptr<NsmDevice> nsmDevice);

  private:
    static std::vector<uint8_t> buildDpuOperationModeData(OperationMode mode);

    std::shared_ptr<DPUOperationModeIntf> deviceModeIntf;
};

class NsmPCIeDeviceModeDeviceModeSettingsV2Get :
    public NsmDeviceModeSettingsV2GetBase
{
  public:
    static constexpr enum device_mode_index modeIndex =
        DEVICE_MODE_PCIE_DEVICE_MODE;

    NsmPCIeDeviceModeDeviceModeSettingsV2Get(
        const std::string& name, const std::string& type,
        uint8_t patchabilityBitmap,
        std::shared_ptr<PCIeDeviceModeIntf> pcieDeviceModeIntf);

  protected:
    uint8_t handleDeviceModeGetPayload(const uint8_t* currentData,
                                       uint16_t currentLength,
                                       const uint8_t* pendingData,
                                       uint16_t pendingLength) override;

  private:
    std::shared_ptr<PCIeDeviceModeIntf> pcieDeviceModeIntf;
};

class NsmPCIeDeviceModeDeviceModeSettingsV2Set :
    public NsmDeviceModeSettingsV2SetBase
{
  public:
    static constexpr enum device_mode_index modeIndex =
        DEVICE_MODE_PCIE_DEVICE_MODE;

    NsmPCIeDeviceModeDeviceModeSettingsV2Set(
        const std::string& name, const std::string& type,
        uint8_t patchabilityBitmap,
        std::shared_ptr<PCIeDeviceModeIntf> pcieDeviceModeIntf);

    requester::Coroutine
        setPendingModes(const AsyncSetOperationValueType& value,
                        AsyncOperationStatusType* status,
                        std::shared_ptr<NsmDevice> nsmDevice);

  private:
    static std::vector<uint8_t>
        buildPcieDeviceModeData(uint8_t multiSocketsMode,
                                uint8_t controlledEWMode,
                                uint8_t bifurcationRawMode);

    std::shared_ptr<PCIeDeviceModeIntf> pcieDeviceModeIntf;
};

// ---- Protection Options Mode V2 (NSM Type 5, Device Mode Index 26) ----

class NsmNetworkAdapterProtectionOptionsMode : public NsmSensor
{
  public:
    NsmNetworkAdapterProtectionOptionsMode(
        const std::string& name, const std::string& type,
        std::shared_ptr<ProtectionOptionsModeIntf> protectionOptionsModeIntf,
        std::shared_ptr<AssociationDefinitionsInft> associationDefIntf);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    // AsyncSetOperationHandler for one boolean property write (bit index:
    // 0=HostFirmwareUpdateRestrictionEnabled,
    // 1=HostConfigurationChangeRestrictionEnabled,
    // 2=HostTransceiverFirmwareUpdateRestrictionEnabled,
    // 3=HostTransceiverConfigurationChangeRestrictionEnabled).
    // Reads the other three current values, substitutes the new value, packs
    // the 16-bit bitmask, and sends NSM Set Device Mode Settings V2 (idx 26).
    requester::Coroutine setFlag(const AsyncSetOperationValueType& value,
                                 AsyncOperationStatusType* status,
                                 std::shared_ptr<NsmDevice> device,
                                 uint8_t bit);

  private:
    std::shared_ptr<ProtectionOptionsModeIntf> protectionOptionsModeIntf;
    std::shared_ptr<AssociationDefinitionsInft> associationDefIntf;
    bool asyncPatchInProgress{false};
};

} // namespace nsm
