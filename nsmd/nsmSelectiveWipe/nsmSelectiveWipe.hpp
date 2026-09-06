// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include "nsmObjectFactory.hpp"

#include <com/nvidia/SelectiveDataWipe/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <memory>
#include <string>
#include <vector>

namespace nsm
{

using SelectiveDataWipeIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::SelectiveDataWipe>;
using WipeOperationStatus =
    sdbusplus::common::com::nvidia::SelectiveDataWipe::OperationStatus;
using WipeDataClass =
    sdbusplus::common::com::nvidia::SelectiveDataWipe::DataClass;

/** @brief D-Bus front end for NSM Type 4 Selective Data Wipe (cmd 0x08). */
class NsmSelectiveWipeObject : public NsmObject
{
  public:
    NsmSelectiveWipeObject(sdbusplus::bus_t& bus, const std::string& name,
                           const std::string& type,
                           const std::string& inventoryObjPath,
                           const uuid_t& uuid);

    /** Unit-test constructor: skips D-Bus vtable registration. */
    NsmSelectiveWipeObject(const std::string& name, const std::string& type,
                           const uuid_t& uuid);

    void wipe(std::vector<WipeDataClass> targets);
    requester::Coroutine wipeOnDevice(uint16_t targetMask);

  private:
    class DbusInterface : public SelectiveDataWipeIntf
    {
      public:
        DbusInterface(sdbusplus::bus_t& bus, const char* path,
                      NsmSelectiveWipeObject& owner);
        void wipe(std::vector<WipeDataClass> targets) override;

      private:
        NsmSelectiveWipeObject& owner;
    };

    void setWipeStatus(WipeOperationStatus status);

    uuid_t uuid;
    WipeOperationStatus wipeStatus{WipeOperationStatus::Unavailable};
    std::unique_ptr<DbusInterface> dbusInterface;
};

} // namespace nsm
