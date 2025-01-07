#include "nsmSensor.hpp"

#include "sensorManager.hpp"

namespace nsm
{
requester::Coroutine NsmSensor::update(SensorManager& manager, eid_t eid)
{
    if (isLongRunning)
    {
        // TODO: Temporarily disabling long running commands.
        // To be removed after backend starts support long running commands.

        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    auto requestMsg = genRequestMsg(eid, 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmSensor::update: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", eid);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await manager.SendRecvNsmMsg(eid, *requestMsg, responseMsg,
                                              responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    rc = handleResponseMsg(responseMsg.get(), responseLen);
    // coverity[missing_return]
    co_return rc;
}
} // namespace nsm
