#pragma once
#include "base.h"
#include "platform-environmental.h"

#include "nsmSensor.hpp"
#include "nsmCommon/nsmCommon.hpp"
#include <com/nvidia/MemoryRowRemapping/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Dimm/server.hpp>
#include <xyz/openbmc_project/Memory/MemoryECC/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{
using DimmIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm>;

using EccType =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm::Ecc;
class NsmMemoryErrorCorrection : public NsmObject
{
  public:
    NsmMemoryErrorCorrection(std::string& name, std::string& type,
                             std::shared_ptr<DimmIntf> dimmIntf,
                             std::string& correctionType);

  private:
    std::shared_ptr<DimmIntf> dimmIntf;
};

using MemoryDeviceType =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm::DeviceType;
class NsmMemoryDeviceType : public NsmObject
{
  public:
    NsmMemoryDeviceType(std::string& name, std::string& type,
                        std::shared_ptr<DimmIntf> dimmIntf,
                        std::string& memoryType);

  private:
    std::shared_ptr<DimmIntf> dimmIntf;
};

using MemoryHealthIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Health>;
using MemoryHealthType = sdbusplus::xyz::openbmc_project::State::Decorator::
    server::Health::HealthType;
class NsmMemoryHealth : public NsmObject
{
  public:
    NsmMemoryHealth(sdbusplus::bus::bus& bus, std::string& name,
                    std::string& type, std::string& inventoryObjPath);

  private:
    std::shared_ptr<MemoryHealthIntf> healthIntf;
};

using LocationTypesMemory = sdbusplus::xyz::openbmc_project::Inventory::
    Decorator::server::Location::LocationTypes;
using LocationIntfMemory = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Location>;
class NsmLocationIntfMemory : public NsmObject
{
  public:
    NsmLocationIntfMemory(sdbusplus::bus::bus& bus, std::string& name,
                          std::string& type, std::string& inventoryObjPath);

  private:
    std::unique_ptr<LocationIntfMemory> locationIntf;
};

using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

class NsmMemoryAssociation : public NsmObject
{
  public:
    NsmMemoryAssociation(sdbusplus::bus::bus& bus, const std::string& name,
                         const std::string& type,
                         const std::string& inventoryObjPath,
                         const std::vector<utils::Association>& associations);

  private:
    std::unique_ptr<AssociationDefinitionsIntf> associationDef = nullptr;
};

using MemoryRowRemappingIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::server::MemoryRowRemapping>;

class NsmRowRemapState : public NsmSensor
{
  public:
    NsmRowRemapState(
        std::string& name, std::string& type,
        std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingIntf);
    NsmRowRemapState() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(bitfield8_t flags);

    std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingStateIntf =
        nullptr;
};

class NsmRowRemappingCounts : public NsmSensor
{
  public:
    NsmRowRemappingCounts(
        std::string& name, std::string& type,
        std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingIntf);
    NsmRowRemappingCounts() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(uint32_t correctable_error,
                       uint32_t uncorrectabale_error);

    std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingCountsIntf =
        nullptr;
};

using EccModeIntfDram = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Memory::server::MemoryECC>;

class NsmEccErrorCountsDram : public NsmSensor
{
  public:
    NsmEccErrorCountsDram(std::string& name, std::string& type,
                          std::shared_ptr<EccModeIntfDram> eccIntf);
    NsmEccErrorCountsDram() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(struct nsm_ECC_error_counts);

    std::shared_ptr<EccModeIntfDram> eccIntf = nullptr;
};

class NsmClockLimitMemory : public NsmSensor
{
  public:
    NsmClockLimitMemory(const std::string& name, const std::string& type,
                        std::shared_ptr<DimmIntf> dimmIntf);
    NsmClockLimitMemory() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(const struct nsm_clock_limit&);
    std::shared_ptr<DimmIntf> dimmIntf;
};
class NsmMemCurrClockFreq : public NsmSensor
{
  public:
    NsmMemCurrClockFreq(const std::string& name, const std::string& type,
                        std::shared_ptr<DimmIntf> dimmIntf);
    NsmMemCurrClockFreq() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(const uint32_t& clockFreq);

    std::shared_ptr<DimmIntf> dimmIntf;
};

class NsmMemCapacity : public NsmMemoryCapacity
{
  public:
    NsmMemCapacity(const std::string& name, const std::string& type,
                   std::shared_ptr<DimmIntf> dimmIntf);
    NsmMemCapacity() = default;

  private:
    void updateReading(uint32_t* maximumMemoryCapacity);
    std::shared_ptr<DimmIntf> dimmIntf;
};
} // namespace nsm
