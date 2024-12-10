#include "nsmReady.hpp"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmZone.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
static requester::Coroutine
    createNsmReadySensor(SensorManager& /*manager*/,
                         const std::string& /*interface*/,
                         const std::string& /*objPath*/)
{
    // dbus timeout seen
    nsm::SensorManagerImpl::isEMReady();
    nsm::SensorManagerImpl::isMCTPReady();

    // marking here directly was working
    //  markEMReady();
    lg2::info("createNsmReadySensor completed");
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmReadySensor, "xyz.openbmc_project.Configuration.NSM_Poll_Ready")

} // namespace nsm
