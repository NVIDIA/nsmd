#pragma once

#include "base.h"

#include "types.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/server.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
    std::string propertyType; //!< D-Bus property type
};

using PropertyValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string,
                 std::vector<sdbusplus::message::object_path>,
                 std::vector<std::string>>;
using DbusProp = std::string;
using DbusChangedProps = std::map<DbusProp, PropertyValue>;
using ObjectPath = std::string;
using ServiceName = std::string;
using Interfaces = std::vector<std::string>;
using MapperServiceMap = std::vector<std::pair<ServiceName, Interfaces>>;
using GetSubTreeResponse = std::vector<std::pair<ObjectPath, MapperServiceMap>>;

#define UUID_LEN 36
namespace utils
{
constexpr bool Tx = true;
constexpr bool Rx = false;

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";
constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";

/** @struct CustomFD
 *
 *  RAII wrapper for file descriptor.
 */
struct CustomFD
{
    CustomFD(const CustomFD&) = delete;
    CustomFD& operator=(const CustomFD&) = delete;
    CustomFD(CustomFD&&) = delete;
    CustomFD& operator=(CustomFD&&) = delete;

    CustomFD(int fd) : fd(fd)
    {}

    ~CustomFD()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    int operator()() const
    {
        return fd;
    }

  private:
    int fd = -1;
};

/**
 * @brief The interface for DBusHandler
 */
class DBusHandlerInterface
{
  public:
    virtual ~DBusHandlerInterface() = default;

    virtual std::string getService(const char* path,
                                   const char* interface) const = 0;
    virtual GetSubTreeResponse
        getSubtree(const std::string& path, int depth,
                   const std::vector<std::string>& ifaceList) const = 0;

    virtual void setDbusProperty(const DBusMapping& dBusMap,
                                 const PropertyValue& value) const = 0;

    virtual PropertyValue
        getDbusPropertyVariant(const char* objPath, const char* dbusProp,
                               const char* dbusInterface) const = 0;
};

/**
 *  @class DBusHandler
 *
 *  Wrapper class to handle the D-Bus calls
 *
 *  This class contains the APIs to handle the D-Bus calls
 *  to cater the request from nsm requester.
 *  A class is created to mock the apis in the test cases
 */
class DBusHandler : public DBusHandlerInterface
{
  public:
    /** @brief Get the bus connection. */
    static auto& getBus()
    {
        static auto bus = sdbusplus::bus::new_default();
        return bus;
    }

    /** @brief Get the asio connection. */
    static auto& getAsioConnection()
    {
        static boost::asio::io_context io;
        static auto conn = std::make_shared<sdbusplus::asio::connection>(io);
        return conn;
    }

    /**
     *  @brief Get the DBUS Service name for the input dbus path
     *
     *  @param[in] path - DBUS object path
     *  @param[in] interface - DBUS Interface
     *
     *  @return std::string - the dbus service name
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    std::string getService(const char* path,
                           const char* interface) const override;

    /**
     *  @brief Get the Subtree response from the mapper
     *
     *  @param[in] path - DBUS object path
     *  @param[in] depth - Search depth
     *  @param[in] ifaceList - list of the interface that are being
     *                         queried from the mapper
     *
     *  @return GetSubTreeResponse - the mapper subtree response
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    GetSubTreeResponse
        getSubtree(const std::string& path, int depth,
                   const std::vector<std::string>& ifaceList) const override;

    /** @brief Get property(type: variant) from the requested dbus
     *
     *  @param[in] objPath - The Dbus object path
     *  @param[in] dbusProp - The property name to get
     *  @param[in] dbusInterface - The Dbus interface
     *
     *  @return The value of the property(type: variant)
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    PropertyValue
        getDbusPropertyVariant(const char* objPath, const char* dbusProp,
                               const char* dbusInterface) const override;

    /** @brief The template function to get property from the requested dbus
     *         path
     *
     *  @tparam Property - Excepted type of the property on dbus
     *
     *  @param[in] objPath - The Dbus object path
     *  @param[in] dbusProp - The property name to get
     *  @param[in] dbusInterface - The Dbus interface
     *
     *  @return The value of the property
     *
     *  @throw sdbusplus::exception::exception when dbus request fails
     *         std::bad_variant_access when \p Property and property on dbus do
     *         not match
     */
    template <typename Property>
    auto getDbusProperty(const char* objPath, const char* dbusProp,
                         const char* dbusInterface)
    {
        auto VariantValue =
            getDbusPropertyVariant(objPath, dbusProp, dbusInterface);
        return std::get<Property>(VariantValue);
    }

    /** @brief Set Dbus property
     *
     *  @param[in] dBusMap - Object path, property name, interface and property
     *                       type for the D-Bus object
     *  @param[in] value - The value to be set
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    void setDbusProperty(const DBusMapping& dBusMap,
                         const PropertyValue& value) const override;
};

/** @brief Print the buffer
 *
 *  @param[in]  isTx - True if the buffer is an outgoing NSM message, false if
                       the buffer is an incoming NSM message
 *  @param[in]  buffer - Buffer to print
 *
 *  @return - None
 */
void printBuffer(bool isTx, const std::vector<uint8_t>& buffer);

/** @brief Print the buffer
 *
 *  @param[in]  isTx - True if the buffer is an outgoing NSM message, false if
                       the buffer is an incoming NSM message
 *  @param[in] buffer - NSM message buffer to log
 *  @param[in] bufferLen - NSM message buffer length
 *
 *  @return - None
 */
void printBuffer(bool isTx, const nsm_msg* buffer, size_t bufferLen);

/** @brief Split strings according to special identifiers
 *
 *  We can split the string according to the custom identifier(';', ',', '&' or
 *  others) and store it to vector.
 *
 *  @param[in] srcStr       - The string to be split
 *  @param[in] delim        - The custom identifier
 *  @param[in] trimStr      - The first and last string to be trimmed
 *
 *  @return std::vector<std::string> Vectors are used to store strings
 */
std::vector<std::string> split(std::string_view srcStr, std::string_view delim,
                               std::string_view trimStr = "");
/** @brief Get the current system time in readable format
 *
 *  @return - std::string equivalent of the system time
 */
std::string getCurrentSystemTime();

/** @brief Get eid from the UUID
 *
 *  @return - eid_t eid for corresponding UUID
 */
eid_t getEidFromUUID(
    std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable, uuid_t uuid);

/** @brief UUID conversion from integer array to std::string.
 *
 *  @param[in] uuidIntArr   - The integer array to be converted to string
 *  @return - uuid_t
 */
uuid_t convertUUIDToString(const uint8_t* uuidIntArr);
} // namespace utils
