// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "nsmFpgaStatus.hpp"

#include "nsmDevice.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <map>

namespace nsm
{

namespace
{
static std::map<uuid_t, std::shared_ptr<NsmFpgaStatus>>& registry()
{
    static std::map<uuid_t, std::shared_ptr<NsmFpgaStatus>> statuses;
    return statuses;
}
} // namespace

NsmFpgaStatus::NsmFpgaStatus(const std::string& name,
                             const std::string& inventoryPath) :
    NsmObject(name, "NSM_FpgaStatus"), inventoryPath(inventoryPath)
{
    auto& bus = utils::DBusHandler::getBus();
    operationalStatusIntf =
        std::make_unique<FpgaOperationalStatusIntf>(bus, inventoryPath.c_str());
    healthIntf = std::make_unique<FpgaHealthIntf>(bus, inventoryPath.c_str());
    publish();
}

bool NsmFpgaStatus::enabled() const
{
    return online && typeOk && digestOk && !writeFault;
}

requester::Coroutine NsmFpgaStatus::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    setOnline(nsmDevice && nsmDevice->isOnline());
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

void NsmFpgaStatus::setOnline(bool value)
{
    if (online != value)
    {
        online = value;
        lg2::info("FPGA status online flag changed", "NAME", getName(),
                  "ONLINE", value);
        publish();
    }
}

void NsmFpgaStatus::setTypeCheck(bool passed)
{
    if (typeOk != passed)
    {
        typeOk = passed;
        lg2::info("FPGA status configuration type check changed", "NAME",
                  getName(), "OK", passed);
        publish();
    }
}

void NsmFpgaStatus::setDigestCheck(bool passed)
{
    if (digestOk != passed)
    {
        digestOk = passed;
        lg2::info("FPGA status map digest check changed", "NAME", getName(),
                  "OK", passed);
        publish();
    }
}

void NsmFpgaStatus::setWriteFault(bool fault)
{
    if (writeFault != fault)
    {
        writeFault = fault;
        lg2::info("FPGA status write fault changed", "NAME", getName(), "FAULT",
                  fault);
        publish();
    }
}

void NsmFpgaStatus::publish()
{
    using StateType = FpgaOperationalStatusIntf::StateType;
    using HealthType = FpgaHealthIntf::HealthType;

    StateType state = StateType::UnavailableOffline;
    HealthType health = HealthType::OK;
    if (writeFault)
    {
        state = StateType::Fault;
        health = HealthType::Critical;
    }
    else if (online && typeOk && digestOk)
    {
        state = StateType::Enabled;
    }
    else if (online)
    {
        health = HealthType::Critical;
    }

    operationalStatusIntf->state(state);
    operationalStatusIntf->functional(enabled());
    healthIntf->health(health);
}

std::shared_ptr<NsmFpgaStatus> NsmFpgaStatus::get(const uuid_t& uuid)
{
    auto it = registry().find(uuid);
    return it == registry().end() ? nullptr : it->second;
}

std::shared_ptr<NsmFpgaStatus>
    NsmFpgaStatus::getOrCreate(const uuid_t& uuid, const std::string& name,
                               const std::string& inventoryPath, bool& created)
{
    created = false;
    if (auto existing = get(uuid))
    {
        return existing;
    }
    auto status = std::make_shared<NsmFpgaStatus>(name, inventoryPath);
    registry()[uuid] = status;
    created = true;
    return status;
}

void NsmFpgaStatus::clear()
{
    registry().clear();
}

} // namespace nsm
