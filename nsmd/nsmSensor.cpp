#include "nsmSensor.hpp"

#include "sensorManager.hpp"

namespace nsm
{
requester::Coroutine NsmSensor::update(SensorManager& manager, eid_t eid)
{
    auto requestMsg = genRequestMsg(eid, 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmSensor::update: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", eid);
        co_return NSM_SW_ERROR;
    }

    const struct nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    auto rc = co_await manager.SendRecvNsmMsg(eid, *requestMsg, &responseMsg,
                                              &responseLen);
    if (rc)
    {
        lg2::error(
            "NsmSensor::update: manager.SendRecvNsmMsg failed, name={NAME} eid={EID}",
            "NAME", getName(), "EID", eid);
    }

    rc = handleResponseMsg(responseMsg, responseLen);
    if (rc)
    {
        lg2::error(
            "NsmSensor::update: handleResponseMsg failed, name={NAME} eid={EID}",
            "NAME", getName(), "EID", eid);
    }

    co_return rc;
}
} // namespace nsm