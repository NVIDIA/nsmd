// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include "fpga/fpga-tables.h"

#include "nsmDbusIfaceOverride/nsmAssetIntf.hpp"
#include "nsmFpgaController.hpp"
#include "nsmFpgaPort.hpp"
#include "nsmFpgaStatus.hpp"
#include "nsmNumericSensor/nsmNumericSensor.hpp"
#include "nsmObject.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPort/nsmPort.hpp"
#include "utils.hpp"

#include <com/nvidia/OpticalLaneStatus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp>
#include <xyz/openbmc_project/State/Decorator/PowerState/server.hpp>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nsm
{

// Interfaces nsmd has no alias for yet, the rest come from the nsmd headers
using FpgaRevisionIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Revision>;
using FpgaLaneStatusIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::OpticalLaneStatus>;
using FpgaPowerStateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::PowerState>;

// Identity of one NSM_FpgaTelemetry entry; the rule decides the output
struct FpgaSensorInfo
{
    std::string name;
    std::string type;
    std::string objectPath;
    std::string chassisAssociation;
    std::string physicalContext;
    const nsm_fpga_rule* rule = nullptr;
    std::vector<utils::Association> associations;
    // How often the group holding this item is read (RefreshIntervalMs)
    uint64_t refreshIntervalUsec = RR_REFRESH_LIMIT_IN_USEC;
    // Also publish the state as a PowerState word (PowerState EM flag)
    bool powerState = false;
    // A link item: the Port property it feeds and the port's type and
    // protocol (PortProperty, PortType and PortProtocol EM entries)
    std::string portProperty;
    std::string portType;
    std::string portProtocol;
};

// One generic FPGA telemetry sensor; the rule picks the output: a state object
// (OperationalStatus and Health), a numeric Sensor.Value, identity text on
// Asset, a per-lane vector, or one property of a Port object. Items may share
// one object path, so the interfaces several items publish on are shared by
// path.
class NsmFpgaSensor : public NsmObject
{
  public:
    NsmFpgaSensor(sdbusplus::bus_t& bus, const FpgaSensorInfo& info,
                  std::shared_ptr<NsmFpgaStatus> status,
                  std::shared_ptr<NsmFpgaController> controller);

    // Standalone read: one episode for this item alone
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;
    // Read inside the group's round that started at roundStartUsec
    requester::Coroutine updateInRound(std::shared_ptr<NsmDevice> nsmDevice,
                                       uint64_t roundStartUsec);

    // Decode a state word through the rule's state map and publish it
    void publish(uint32_t value);
    // OperationalStatus and Health from the map's state and health codes
    void publishState(uint8_t state, uint8_t health);
    // The group's setup check ended the round: publish its state
    void publishGroupState(uint8_t state, uint8_t health);
    // No valid reading this round: numeric NaN, state UnavailableOffline,
    // identity text and vectors empty
    void markUnavailable();

    const nsm_fpga_rule* getRule() const
    {
        return rule;
    }
    bool hasReading() const
    {
        return haveReading;
    }
    uint32_t lastRaw() const
    {
        return raw;
    }
    uint64_t refreshIntervalUsec() const
    {
        return refreshInterval;
    }
    // Due inside its group's round: never read, or its own interval ago
    bool isDue(uint64_t nowUsec) const
    {
        return lastReadUsec == 0 || nowUsec - lastReadUsec > refreshInterval;
    }

    // Sensor.Value unit for a sensor type; nullopt if unsupported
    static std::optional<SensorUnit>
        unitForSensorType(const std::string& sensorType);
    // Value of the captured bytes under the rule's numeric spec, nullopt
    // when the field does not fit, is not finite or is out of range
    static std::optional<double> decode(const nsm_fpga_rule& rule,
                                        std::span<const uint8_t> capture);

  private:
    // Publish the outcome of a read and log once per change
    int finish(int rc, uint32_t value);
    // Decode the capture for the rule's kind and publish, false when it
    // does not decode
    bool publishCapture(uint32_t value);
    // Publish the elements of an ARRAY rule on its vector property
    void publishArray(const std::vector<double>& elements);
    // Blank every identity field the rule names
    void clearIdentity();

    const nsm_fpga_rule* rule;
    std::string objectPath;
    std::shared_ptr<NsmFpgaStatus> status;
    std::shared_ptr<NsmFpgaController> controller;
    bool haveReading = false;
    uint32_t raw = 0;
    // Outcome of the last read; unset until the first one
    std::optional<bool> lastReadOk;
    // Start of the last round that read the item; zero until the first one
    uint64_t lastReadUsec = 0;
    const uint64_t refreshInterval;
    // State output, built for a state rule
    std::unique_ptr<FpgaOperationalStatusIntf> operationalStatusIntf;
    std::unique_ptr<FpgaHealthIntf> healthIntf;
    // Available only while the last read decoded a state; false on failure
    std::unique_ptr<AvailabilityIntf> availabilityIntf;
    // Power word of a module state object: On, PoweringOn, Off or Unknown
    std::unique_ptr<FpgaPowerStateIntf> powerStateIntf;
    // Port output of a link item: the state feeds one property of the port
    std::shared_ptr<NsmFpgaPortObject> port;
    NsmFpgaPortObject::Role portRole = NsmFpgaPortObject::Role::LinkStatus;
    std::shared_ptr<AssociationDefinitionsInft> associationIntf;
    // Numeric output, built for a numeric rule
    std::shared_ptr<NsmNumericSensorValueAggregate> sensorValue;
    // Identifier output, built for an identity rule
    std::shared_ptr<AssetIntf> assetIntf;
    std::shared_ptr<FpgaRevisionIntf> revisionIntf;
    // Vector output, built for an array rule; the rule's vector picks one
    std::shared_ptr<PortOpticalModuleMetricsIntf> metricsIntf;
    std::shared_ptr<FpgaLaneStatusIntf> laneStatusIntf;
    // Set the Asset property an identity field names
    void publishIdentity(const nsm_fpga_identity_field& field,
                         const std::string& text);
};

// One telemetry group of an FPGA: the scheduled unit. update() reads its
// items in series inside one controller session; once the episode has used
// its slice the remaining items go to the next round, which is due at once.
// An identity item runs until it has a reading and again once cleared.
class NsmFpgaGroup : public NsmObject
{
  public:
    explicit NsmFpgaGroup(const std::string& name);

    // The controller that runs the round and the map's group record, if any
    void bind(std::shared_ptr<NsmFpgaController> controller,
              const nsm_fpga_group* definition);
    void addItem(std::shared_ptr<NsmFpgaSensor> item);
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;
    // Due once the shortest item interval has passed since the last round
    bool needsUpdate(const uint64_t& nowUsec) const override;

    size_t itemCount() const
    {
        return items.size();
    }
    uint64_t refreshIntervalUsec() const
    {
        return refreshInterval;
    }

    // One group object per FPGA device and group id, looked up by STATIC UUID.
    static std::shared_ptr<NsmFpgaGroup> getOrCreate(const uuid_t& uuid,
                                                     const std::string& group,
                                                     bool& created);
    static void clear();

  private:
    std::vector<std::shared_ptr<NsmFpgaSensor>> items;
    std::shared_ptr<NsmFpgaController> controller;
    const nsm_fpga_group* definition = nullptr;
    // The shortest interval of the items, whatever the round robin default
    uint64_t refreshInterval = RR_REFRESH_LIMIT_IN_USEC;
    uint64_t lastRoundUsec = 0;
    // Items the last round left for the next one, starting at nextItem
    bool pending = false;
    size_t nextItem = 0;
};

requester::Coroutine createNsmFpgaTelemetry(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath);

} // namespace nsm
