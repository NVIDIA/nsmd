#pragma once
#include "pci-links.h"
#include "platform-environmental.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PCIeRefClock/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>
#include <xyz/openbmc_project/PCIe/LTSSMState/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using PCIeRefClockIntf = object_t<Inventory::Decorator::server::PCIeRefClock>;
using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;
using LTSSMStateIntf = object_t<PCIe::server::LTSSMState>;

/** @brief Convert to PCIe gen type
 *
 *  @param[in] link_speed - link speed
 *  @return string PCIe type
 */
std::string convertToPCIeTypeStr(uint32_t link_speed);

/** @brief Convert to lane count
 *
 *  @param[in] link_width - link width
 *  @return size_t lane count
 */
size_t convertToLaneCount(uint32_t link_width);

/** @brief Convert to lTSSM State
 *
 *  @param[in] ltssm_state - ltssm state
 *  @return string lTSSM State
 */
std::string convertToLTSSMStateStr(uint32_t ltssm_state);

class NsmPCIeDeviceQueryScalarTelemetry : public NsmSensor
{
  public:
    NsmPCIeDeviceQueryScalarTelemetry(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::vector<utils::Association>& associations,
        const std::string& type, const std::string& deviceType,
        const uint8_t deviceIndex, std::string& inventoryObjPath);
    NsmPCIeDeviceQueryScalarTelemetry() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefIntf = nullptr;
    std::unique_ptr<PCIeDeviceIntf> pcieDeviceIntf = nullptr;
    uint8_t deviceIndex;
};

class NsmPCIeDeviceGetClockOutput : public NsmSensor
{
  public:
    NsmPCIeDeviceGetClockOutput(sdbusplus::bus::bus& bus,
                                const std::string& name,
                                const std::string& type,
                                const uint64_t& deviceInstance,
                                std::string& inventoryObjPath);
    NsmPCIeDeviceGetClockOutput() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    bool getRetimerClockState(uint32_t clockBuffer);

    std::unique_ptr<PCIeRefClockIntf> pcieRefClockIntf = nullptr;
    uint8_t clkBufIndex;
    uint8_t deviceInstanceNumber;
};

class NsmPCIeDeviceQueryLTSSMState : public NsmSensor
{
  public:
    NsmPCIeDeviceQueryLTSSMState(sdbusplus::bus::bus& bus,
                                 const std::string& name,
                                 const std::string& type,
                                 const uint8_t& deviceIndex,
                                 std::string& inventoryObjPath);
    NsmPCIeDeviceQueryLTSSMState() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::unique_ptr<LTSSMStateIntf> ltssmStateIntf = nullptr;
    uint8_t deviceIndex;
};
} // namespace nsm
