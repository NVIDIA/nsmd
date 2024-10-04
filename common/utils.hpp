/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "base.h"

#include "coroutine.hpp"
#include "types.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Software/SecurityCommon/common.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <queue>
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

using PropertyValue = std::variant<
    bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t,
    double, std::string, std::vector<sdbusplus::message::object_path>,
    std::vector<std::string>, std::vector<uint64_t>,
    std::vector<std::tuple<std::string, std::string, std::string>>>;
using DbusProp = std::string;
using DbusChangedProps = std::map<DbusProp, PropertyValue>;
using ObjectPath = std::string;
using ServiceName = std::string;
using MapperServiceMap = std::vector<std::pair<ServiceName, dbus::Interfaces>>;
using GetSubTreeResponse = std::vector<std::pair<ObjectPath, MapperServiceMap>>;
using GetAssociatedObjectsResponse = std::variant<std::vector<ObjectPath>>;
using PropertyValuesCollection =
    std::vector<std::pair<std::string, PropertyValue>>;

#define UUID_INT_SIZE 16
#define UUID_LEN 36
namespace utils
{
constexpr bool Tx = true;
constexpr bool Rx = false;

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";
constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";

struct Association
{
    std::string forward;
    std::string backward;
    std::string absolutePath;
};
using Associations =
    std::vector<std::tuple<std::string, std::string, std::string>>;

struct bitfield256_err_code
{
    bitfield256_t bitMap;
    bool isAnyBitSet; // Flag indicating if any bits are set

    bitfield256_err_code()
    {
        // Initialize cc_map and rc_map with all bits set to zero
        for (int i = 0; i < 8; i++)
        {
            bitMap.fields[i].byte = 0;
        }
        isAnyBitSet = false;
    }

    bool isBitSet(const int& errCode);
    std::string getSetBits() const;
};

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

    CustomFD(int fd) : fd(fd) {}

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
class IDBusHandler
{
  public:
    virtual ~IDBusHandler() = default;

    virtual std::string getService(const char* path,
                                   const char* interface) const = 0;

    virtual MapperServiceMap getServiceMap(
        const char* path,
        const dbus::Interfaces& ifaceList = dbus::Interfaces()) const = 0;

    virtual GetSubTreeResponse
        getSubtree(const std::string& path, int depth,
                   const dbus::Interfaces& ifaceList) const = 0;

    virtual void setDbusProperty(const DBusMapping& dBusMap,
                                 const PropertyValue& value) const = 0;

    virtual PropertyValue
        getDbusPropertyVariant(const char* objPath, const char* dbusProp,
                               const char* dbusInterface) const = 0;

    virtual PropertyValuesCollection
        getDbusProperties(const char* objPath,
                          const char* dbusInterface) const = 0;

    virtual GetAssociatedObjectsResponse
        getAssociatedObjects(const std::string& path,
                             const std::string& association) const = 0;

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
        auto VariantValue = getDbusPropertyVariant(objPath, dbusProp,
                                                   dbusInterface);
        return std::get<Property>(VariantValue);
    }
    /** @brief The template function to get optional property from the requested
     * dbus path without throwing sdbusplus::exception::exception on fail
     *
     *  @tparam Property - Excepted type of the property on dbus
     *
     *  @param[in] objPath - The Dbus object path
     *  @param[in] dbusProp - The property name to get
     *  @param[in] dbusInterface - The Dbus interface
     *
     *  @return The value of the property
     *
     *  @throw std::bad_variant_access when \p Property and property on dbus do
     *         not match
     */
    template <typename Property>
    auto tryGetDbusProperty(const char* objPath, const char* dbusProp,
                            const char* dbusInterface)
    {
        try
        {
            return getDbusProperty<Property>(objPath, dbusProp, dbusInterface);
        }
        catch (const sdbusplus::exception::exception&)
        {
            return Property();
        }
    }
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
class DBusHandler : public IDBusHandler
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

    /** @brief Get the DBusHandler instance. */
    static DBusHandler& instance()
    {
        static DBusHandler dBusHandler;
        return dBusHandler;
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
     *  @brief Get the DBUS ServiceMap for the input dbus path
     *
     *  @param[in] path - DBUS object path
     *  @param[in] ifaceList - list of the interface that are being
     *                         queried from the mapper
     *
     *  @return MapperServiceMap - the dbus services map
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    MapperServiceMap
        getServiceMap(const char* path,
                      const dbus::Interfaces& ifaceList) const override;

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
                   const dbus::Interfaces& ifaceList) const override;

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

    /** @brief Get properties(type: variant) from the requested dbus
     *
     *  @param[in] objPath - The Dbus object path
     *  @param[in] dbusInterface - The Dbus interface
     *
     *  @return The collection of the properties (type: variant)
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    PropertyValuesCollection
        getDbusProperties(const char* objPath,
                          const char* dbusInterface) const override;

    /**
     *  @brief Get the associated object response from the mapper
     *
     *  @param[in] path - DBUS object path
     *  @param[in] association - forward / reverse association
     *
     *  @return GetAssociatedObjectsResponse - the mapper get associated object
     * response
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    GetAssociatedObjectsResponse
        getAssociatedObjects(const std::string& path,
                             const std::string& association) const override;
};

IDBusHandler& DBusHandler();

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

/** @brief Get UUID from the eid
 *
 *  @return - uuid_t uuid for corresponding eid
 */
std::optional<std::string> getUUIDFromEID(
    const std::multimap<std::string,
                        std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable,
    eid_t eid);

/** @brief Get eid from the UUID
 *
 *  @return - eid_t eid for corresponding UUID
 */
eid_t getEidFromUUID(
    const std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
        eidTable,
    uuid_t uuid);

/** @brief UUID conversion from integer array to std::string.
 *
 *  @param[in] uuidIntArr   - The integer array to be converted to string
 *  @return - uuid_t
 */
uuid_t convertUUIDToString(const std::vector<uint8_t>& uuidIntArr);

/** @brief Make valid D-Bus Object Path of Interface Name by replacing unwanted
 * characters with underscore ('_')
 *
 *  @param[in] name - potentially invalid D-Bus Name
 *  @return - valid D-Bus Name
 */
std::string makeDBusNameValid(const std::string& name);

/** @brief Get associations of a configuration PDI
 *
 *  @param[in] objPath - D-Bus Object Path of configuration PDI
 *  @param[in] interfaceSubStr - Sub string to identify association interfaces
 * at objPath
 *  @return - Associations
 */
std::vector<Association> getAssociations(const std::string& objPath,
                                         const std::string& interfaceSubStr);

/**
 * @brief Converts vector of Association struct to dbus tuples collection
 *
 * @param associations vector of Association struct
 * @return Associations Dbus tuples collection
 */
Associations getAssociations(const std::vector<Association>& associations);

/** @brief Parse bitfield response for nsm command
 *
 * @param[in] data - std::vector<uint8_t>
 * @param[in] const bitfield8_t* value - pointer to bitfield data
 * @param[in] size - uint_8 varuable to hold bitfield data
 */
void convertBitMaskToVector(std::vector<uint8_t>& data,
                            const bitfield8_t* value, uint8_t size);

/**
 * @brief Verify allowed device type and its instance number
 *
 * @param deviceType NSM device type identification
 * @param instanceNumber NSM device instance number
 * @param retimer Verify Retimer subdevice under Baseboard device
 * @throws std::invalid_argument Throws exception when wrong device type or
 * instance number is given
 */
void verifyDeviceAndInstanceNumber(NsmDeviceIdentification deviceType,
                                   uint8_t instanceNumber, bool retimer);
/**
 * @brief Get Device Name associated to Device Type
 *
 * @param deviceType NSM device type number
 */
std::string getDeviceNameFromDeviceType(const uint8_t deviceType);

/**
 * @brief Get Device Instance Name = deviceName_deviceInstanceNumber
 *
 * @param deviceType NSM device type number
 * @param instanceNumber NSM device instance number
 */
std::string getDeviceInstanceName(const uint8_t deviceType,
                                  const uint8_t instanceNumber);

/** @brief Get associations of a configuration PDI by coroutine
 *
 *  @param[in] objPath - D-Bus Object Path of configuration PDI
 *  @param[in] interfaceSubStr - Sub string to identify association interfaces
 * at objPath
 *  @param[out] - associations
 */

requester::Coroutine coGetAssociations(const std::string& objPath,
                                       const std::string& interfaceSubStr,
                                       std::vector<Association>& associations);

// Function to convert bitfield256_t to a bitmap
/**
 * Converts a bitfield256_t structure to a bitmap.
 *
 * @param bf Pointer to the bitfield256_t structure.
 **/
std::vector<uint8_t> bitfield256_tToBitMap(bitfield256_t bf);

/**
 * @brief Converts a bitmap into two vectors containing the indices of bits set
 * to 0 and 1.
 *
 * @param[in] bitmap - A vector of bytes representing the bitmap.
 * @return A pair of vectors, where the first vector contains the indices of
 * bits that are 0, and the second vector contains the indices of bits that
 * are 1.
 *
 * @note The indices are 0-based, meaning the first bit in the bitmap is index
 * 0.
 */
std::pair<std::vector<uint8_t>, std::vector<uint8_t>>
    bitmapToIndices(const std::vector<uint8_t>& bitmap);

/**
 * @brief Converts a list of indices into a bitmap.
 *
 * @param[in] indices - A vector of 1-based indices where bits should be set
 * to 1.
 * @param[in] size - (optional) size of the output bitmap.
 * @return A vector of bytes representing the bitmap.
 *
 * @note Indices that are not provided in the list will be set to 0 in the
 * bitmap.
 */
std::vector<uint8_t> indicesToBitmap(const std::vector<uint8_t>& indices,
                                     const size_t size = 0);

/**
 * @brief Converts a bitfield representing update methods into a list of update
 * method enums.
 *
 * @param[in] updateMethodBitfield - A bitfield where each bit represents a
 * different update method.
 * @return A vector of SecurityCommon::UpdateMethods enums corresponding to the
 * set bits in the bitfield.
 *
 * @note The function checks specific bits in the bitfield and adds the
 * corresponding update method enum to the returned vector. Only the bits that
 * are set in the bitfield will have their corresponding enums included in the
 * list.
 */
std::vector<sdbusplus::common::xyz::openbmc_project::software::SecurityCommon::
                UpdateMethods>
    updateMethodsBitfieldToList(bitfield32_t updateMethodBitfield);

// Function to convert bitmap to a bitfield256_t
/**
 * Converts a bitmap structure to a bitfield256_t.
 *
 * @param bitmap Pointer to the bitmap structure.
 **/
bitfield256_t bitMapToBitfield256_t(const std::vector<uint8_t>& bitmap);
} // namespace utils
