#pragma once

#include "types.hpp"

#include <sdbusplus/asio/object_server.hpp>

namespace nsm
{

class NsmDevice
{
  public:
    NsmDevice(eid_t eid) : eid(eid)
    {}

    std::shared_ptr<sdbusplus::asio::dbus_interface> fruDeviceIntf;

  private:
    eid_t eid;
    std::string uuid;
};

} // namespace nsm
