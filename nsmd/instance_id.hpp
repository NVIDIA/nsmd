#pragma once

#include "libnsm/instance-id.h"

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstdint>
#include <exception>
#include <string>
#include <system_error>

namespace nsm
{

/** @class InstanceId
 *  @brief Implementation of NSM instance id
 */
class InstanceIdDb
{
  public:
    InstanceIdDb()
    {
        int rc = instance_db_init_default(&instanceIdDb);
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }

    /** @brief Constructor
     *
     *  @param[in] path - instance ID database path
     */
    InstanceIdDb(const std::string& path)
    {
        int rc = instance_db_init(&instanceIdDb, path.c_str());
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }

    ~InstanceIdDb()
    {
        int rc = instance_db_destroy(instanceIdDb);
        if (rc)
        {
            lg2::error("instance_db_destroy failed, rc= {RC}", "RC", rc);
        }
    }

    /** @brief Allocate an instance ID for the given terminus
     *  @param[in] eid - the Endpoint ID the instance ID is associated with
     *  @return - instance id or -EAGAIN if there are no available instance
     *            IDs
     */
    uint8_t next(uint8_t eid)
    {
        uint8_t id;
        int rc = instance_id_alloc(instanceIdDb, eid, &id);

        if (rc == -EAGAIN)
        {
            throw std::runtime_error("No free instance ids");
        }

        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }

        return id;
    }

    /** @brief Mark an instance id as unused
     *  @param[in] eid - the terminus ID the instance ID is associated with
     *  @param[in] instanceId - NSM instance id to be freed
     */
    void free(uint8_t eid, uint8_t instanceId)
    {
        int rc = instance_id_free(instanceIdDb, eid, instanceId);
        if (rc == -EINVAL)
        {
            throw std::runtime_error(
                "Instance ID " + std::to_string(instanceId) + " for EID " +
                std::to_string(eid) + " was not previously allocated");
        }
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }

  private:
    instance_db* instanceIdDb = nullptr;
};

} // namespace nsm