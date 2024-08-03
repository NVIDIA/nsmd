#include "nsmRetimerPort.hpp"
#include "nsmGpuPriorityMapping.h"

#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortState/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem1/server.hpp>
#include <xyz/openbmc_project/PCIe/ClearPCIeCounters/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>
#include "asyncOperationManager.hpp"

#define MAX_SCALAR_SOURCE_MASK_SIZE 4

namespace nsm
{
using AssociationDefIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using PortInfoIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortInfo>;
using PortStateIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortState>;
using PortWidthIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortWidth>;
using PortIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::item::Port>;
using PCIeEccIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;
using ChasisStateIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::state::Chassis>;
using HealthIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::state::decorator::Health>;
using ClearPCIeIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::pc_ie::ClearPCIeCounters>;

using PortType = sdbusplus::server::xyz::openbmc_project::inventory::decorator::
    PortInfo::PortType;
using PortProtocol = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortInfo::PortProtocol;

std::map<std::string, std::tuple<uint8_t, uint8_t>> counterToGroupIdMap = {
    {"CorrectableErrorCount", std::make_tuple(GROUP_ID_2, DS_ID_3)},
    {"NonFatalErrorCount", std::make_tuple(GROUP_ID_2, DS_ID_0)},
    {"FatalErrorCount", std::make_tuple(GROUP_ID_2, DS_ID_1)},
    {"L0ToRecoveryCount", std::make_tuple(GROUP_ID_3, DS_ID_0)},
    {"ReplayCount", std::make_tuple(GROUP_ID_4, DS_ID_6)},
    {"ReplayRolloverCount", std::make_tuple(GROUP_ID_4, DS_ID_4)},
    {"NAKSentCount", std::make_tuple(GROUP_ID_4, DS_ID_2)},
    {"NAKReceivedCount", std::make_tuple(GROUP_ID_4, DS_ID_1)},
    {"UnsupportedRequestCount", std::make_tuple(GROUP_ID_2, DS_ID_2)}};

class NsmGpuPciePort : public NsmObject
{
  public:
    NsmGpuPciePort(sdbusplus::bus::bus& bus, const std::string& name,
                   const std::string& type, const std::string& health,
                   const std::string& chasisState,
                   const std::vector<utils::Association>& associations,
                   const std::string& inventoryObjPath);

  private:
    std::unique_ptr<AssociationDefIntf> associationDefIntf = nullptr;
    std::unique_ptr<ChasisStateIntf> chasisStateIntf = nullptr;
    std::unique_ptr<HealthIntf> healthIntf = nullptr;
};

class NsmGpuPciePortInfo : public NsmObject
{
  public:
    NsmGpuPciePortInfo(const std::string& name, const std::string& type,
                       const std::string& portType,
                       const std::string& portProtocol,
                       std::shared_ptr<PortInfoIntf> portInfoIntf);

  private:
    std::shared_ptr<PortInfoIntf> portInfoIntf;
};

using CounterType = sdbusplus::common::xyz::openbmc_project::pc_ie::
    ClearPCIeCounters::CounterType;

class NsmClearPCIeIntf : public ClearPCIeIntf
{
  public:
    NsmClearPCIeIntf(sdbusplus::bus::bus& bus, const char* path,
                     const uint8_t deviceIndex,
                     std::shared_ptr<NsmDevice> device) :
        ClearPCIeIntf(bus, path), deviceIndex(deviceIndex), device(device)
    {}

    sdbusplus::message::object_path clearCounter(std::string Counter) override;
    requester::Coroutine
        clearPCIeErrorCounter(AsyncOperationStatusType* status,
                              const uint8_t deviceIndex, const uint8_t groupId,
                              const uint8_t dsId);
    requester::Coroutine doClearPCIeCountersOnDevice(
        std::shared_ptr<AsyncStatusIntf> statusInterface, const std::string& Counter);

    uint8_t deviceIndex;
    std::shared_ptr<NsmDevice> device;
};

class NsmClearPCIeCounters : public NsmObject
{
  public:
    NsmClearPCIeCounters(const std::string& name, const std::string& type,
                         const uint8_t groupId, uint8_t deviceIndex,
                         std::shared_ptr<ClearPCIeIntf> clearPCIeIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    void findAndUpdateCounter(CounterType counter,
                              std::vector<CounterType>& clearableCounters);
    void updateReading(const bitfield8_t clearableSource[]);
    uint8_t groupId;
    uint8_t deviceIndex;
    std::shared_ptr<ClearPCIeIntf> clearPCIeIntf = nullptr;
};

} // namespace nsm
