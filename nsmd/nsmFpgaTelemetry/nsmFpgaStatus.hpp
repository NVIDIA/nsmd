// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include "nsmObject.hpp"
#include "types.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <memory>
#include <string>

namespace nsm
{

using FpgaOperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;
using FpgaHealthIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Health>;

// FPGA n inventory object: the NSM_FpgaProcessor path shown as a Processor
inline std::string fpgaProcessorPath(size_t instance)
{
    return "/xyz/openbmc_project/inventory/system/processors/FPGA_" +
           std::to_string(instance);
}

// Per-FPGA status: online flag, discovery check results and write fault.
class NsmFpgaStatus : public NsmObject
{
  public:
    NsmFpgaStatus(const std::string& name, const std::string& inventoryPath);

    // Sensors poll only while this is true.
    bool enabled() const;

    // Static refresh at discovery: takes the device's online state
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

    void setOnline(bool value);
    void setTypeCheck(bool passed);
    void setDigestCheck(bool passed);
    // The controller was reset or a core was loaded: both checks run again
    void requestRecheck()
    {
        setTypeCheck(false);
        setDigestCheck(false);
    }
    void setWriteFault(bool fault);

    bool isOnline() const
    {
        return online;
    }
    bool typeCheckPassed() const
    {
        return typeOk;
    }
    bool digestCheckPassed() const
    {
        return digestOk;
    }
    bool hasWriteFault() const
    {
        return writeFault;
    }
    const std::string& getInventoryPath() const
    {
        return inventoryPath;
    }

    void onDeviceOnline() override
    {
        setOnline(true);
    }
    void handleOfflineState() override
    {
        setOnline(false);
    }

    // One status object per FPGA device, looked up by its STATIC UUID.
    static std::shared_ptr<NsmFpgaStatus> get(const uuid_t& uuid);
    static std::shared_ptr<NsmFpgaStatus>
        getOrCreate(const uuid_t& uuid, const std::string& name,
                    const std::string& inventoryPath, bool& created);
    static void clear();

  private:
    void publish();

    std::string inventoryPath;
    bool online = false;
    bool typeOk = false;
    bool digestOk = false;
    bool writeFault = false;
    std::unique_ptr<FpgaOperationalStatusIntf> operationalStatusIntf;
    std::unique_ptr<FpgaHealthIntf> healthIntf;
};

} // namespace nsm
