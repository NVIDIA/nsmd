/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Coverage targets:
 *   - 44 utils::typeName<T>() template instantiations (common/utils.hpp)
 *   - utils::DBusHandler::instance (common/utils.hpp)
 *   - IDBusHandler::getDbusProperty<bool/double/unsigned long/vector<string>>
 *   - IDBusHandler::tryGetDbusProperty<bool>
 */

#include "mockDBusHandler.hpp"
#include "nsmInterface.hpp"
#include "utils.hpp"

#include <gtest/gtest.h>

// NSM headers that provide type aliases used in coverage XML
#include "nsmChassis/nsmChassis.hpp"
#include "nsmChassis/nsmClockOutputEnableState.hpp"
#include "nsmChassis/nsmPCIeFunction.hpp"
#include "nsmChassis/nsmPCIeLTSSMState.hpp"
#include "nsmChassis/nsmPowerSupplyStatus.hpp"
#include "nsmChassis/nsmWriteProtectedJumper.hpp"
#include "nsmErrorInjection/nsmErrorInjection.hpp"
#include "nsmNumericSensor/nsmNumericSensor.hpp"
#include "nsmPort/nsmPCIeErrors.hpp"
#include "nsmPort/nsmPort.hpp"
#include "nsmPort/nsmPortInfo.hpp"
#include "nsmSensors/nsmInventoryProperty.hpp"
#include "nsmSensors/nsmPCIeLinkSpeed.hpp"

// sdbusplus header for L1PowerModeIntf (com::nvidia::PowerMode)
// avoids pulling in nsmDeviceInventory/nsmSwitch.hpp (complex deps)
#include <com/nvidia/PowerMode/server.hpp>

using namespace nsm;
using namespace utils;

// Local type alias for L1PowerModeIntf from nsmSwitch.hpp
// (same underlying type:
// sdbusplus::server::object::object<com::nvidia::PowerMode>)
using L1PowerModeIntf_t =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::PowerMode>;

class NsmTypeNameCoverageTest : public ::testing::Test
{};

// ============================================================================
// NsmClockOutputEnableState typeName instantiations
// ============================================================================

TEST_F(NsmTypeNameCoverageTest, TypeNameClockOutputEnableState)
{
    EXPECT_FALSE(
        typeName<NsmClockOutputEnableState<NVLinkRefClockIntf>>().empty());
    EXPECT_FALSE(
        typeName<NsmClockOutputEnableState<PCIeRefClockIntf>>().empty());
}

// ============================================================================
// NsmErrorInjection typeName instantiations
// ============================================================================

TEST_F(NsmTypeNameCoverageTest, TypeNameErrorInjectionTypes)
{
    EXPECT_FALSE(typeName<NsmErrorInjection>().empty());
    EXPECT_FALSE(typeName<NsmErrorInjectionPayload>().empty());
}

// ============================================================================
// NsmInterfaces<T> typeName instantiations
// ============================================================================

TEST_F(NsmTypeNameCoverageTest, TypeNameNsmInterfacesCustomTypes)
{
    // NsmAssetIntf (from nsmDbusIfaceOverride/nsmAssetIntf.hpp via
    // nsmSensors/nsmInventoryProperty.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<NsmAssetIntf>>().empty());
    // NsmPortInfoIntf (from nsmPort/nsmPortInfo.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<NsmPortInfoIntf>>().empty());
    // L1PowerModeIntf (com::nvidia::PowerMode from nsmSwitch.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<L1PowerModeIntf_t>>().empty());
    // ErrorInjectionIntf (from nsmErrorInjection/nsmErrorInjection.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<ErrorInjectionIntf>>().empty());
}

TEST_F(NsmTypeNameCoverageTest, TypeNameNsmInterfacesNVLinkAndAssociation)
{
    // NVLinkRefClockIntf (from nsmChassis/nsmClockOutputEnableState.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<NVLinkRefClockIntf>>().empty());
    // AssociationDefinitionsInft (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<AssociationDefinitionsInft>>().empty());
    // UuidIntf (Common::UUID from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<UuidIntf>>().empty());
    // ItemIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<ItemIntf>>().empty());
}

TEST_F(NsmTypeNameCoverageTest, TypeNameNsmInterfacesDecoratorTypes)
{
    // DecoratorAreaIntf (from nsmNumericSensor/nsmNumericSensor.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<DecoratorAreaIntf>>().empty());
    // DimensionIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<DimensionIntf>>().empty());
    // LocationIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<LocationIntf>>().empty());
    // LocationCodeIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<LocationCodeIntf>>().empty());
    // LocationContextIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<LocationContextIntf>>().empty());
    // PCIeRefClockIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<PCIeRefClockIntf>>().empty());
    // PortStateIntf (from nsmPort/nsmPort.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<PortStateIntf>>().empty());
    // PowerLimitIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<PowerLimitIntf>>().empty());
    // ReplaceableIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<ReplaceableIntf>>().empty());
}

TEST_F(NsmTypeNameCoverageTest, TypeNameNsmInterfacesItemAndStateTypes)
{
    // ChassisIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<ChassisIntf>>().empty());
    // PCIeDeviceIntf (from nsmSensors/nsmPCIeLinkSpeed.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<PCIeDeviceIntf>>().empty());
    // PCIeSlotIntf (from nsmSensors/nsmPCIeLinkSpeed.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<PCIeSlotIntf>>().empty());
    // MctpUuidIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<MctpUuidIntf>>().empty());
    // LTSSMStateIntf (from nsmChassis/nsmPCIeLTSSMState.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<LTSSMStateIntf>>().empty());
    // SettingsIntf (from nsmChassis/nsmWriteProtectedJumper.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<SettingsIntf>>().empty());
    // PowerStateIntf (State::Chassis, from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<PowerStateIntf>>().empty());
    // HealthIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<HealthIntf>>().empty());
    // OperationalStatusIntf (from nsmChassis/nsmChassis.hpp)
    EXPECT_FALSE(typeName<NsmInterfaces<OperationalStatusIntf>>().empty());
}

// ============================================================================
// NsmInventoryProperty<T> typeName instantiations
// ============================================================================

TEST_F(NsmTypeNameCoverageTest, TypeNameNsmInventoryProperty)
{
    // NsmMNNVLinkTopologyIntf (from nsmDbusIfaceOverride via
    // nsmSensors/nsmInventoryProperty.hpp)
    EXPECT_FALSE(
        typeName<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>().empty());
    // AssetTagIntf (from nsmSensors/nsmInventoryProperty.hpp)
    EXPECT_FALSE(typeName<NsmInventoryProperty<AssetTagIntf>>().empty());
    // DimensionIntf
    EXPECT_FALSE(typeName<NsmInventoryProperty<DimensionIntf>>().empty());
    // PowerLimitIntf
    EXPECT_FALSE(typeName<NsmInventoryProperty<PowerLimitIntf>>().empty());
    // VersionIntf (from nsmSensors/nsmInventoryProperty.hpp)
    EXPECT_FALSE(typeName<NsmInventoryProperty<VersionIntf>>().empty());
}

// ============================================================================
// NsmPCIe* and other sensor typeName instantiations
// ============================================================================

TEST_F(NsmTypeNameCoverageTest, TypeNamePCIeAndRelatedSensors)
{
    EXPECT_FALSE(typeName<NsmPCIeErrors>().empty());
    EXPECT_FALSE(typeName<NsmPCIeFunction>().empty());
    EXPECT_FALSE(typeName<NsmPCIeLTSSMState>().empty());
    EXPECT_FALSE(typeName<NsmPCIeLinkSpeed<NsmPortInfoIntf>>().empty());
    EXPECT_FALSE(typeName<NsmPCIeLinkSpeed<PCIeDeviceIntf>>().empty());
    EXPECT_FALSE(typeName<NsmPCIeLinkSpeed<PCIeSlotIntf>>().empty());
    EXPECT_FALSE(typeName<NsmPCIeLinkSpeed<PCIeEccIntf>>().empty());
    EXPECT_FALSE(typeName<NsmPowerSupplyStatus>().empty());
    EXPECT_FALSE(typeName<NsmWriteProtectedJumper>().empty());
}

// ============================================================================
// IDBusHandler::getDbusProperty<T> and tryGetDbusProperty<T> coverage
// (via MockDBusHandler which implements IDBusHandler)
// ============================================================================

TEST_F(NsmTypeNameCoverageTest, GetDbusPropertyBool)
{
    auto& propMap = MockDbusAsync::propertyMap("/test/typename/path",
                                               "test.TypeName.Iface");
    propMap["BoolProp"] = dbus::Value(bool(true));

    auto& handler = utils::DBusHandler();
    auto result = handler.getDbusProperty<bool>(
        "/test/typename/path", "BoolProp", "test.TypeName.Iface");
    EXPECT_EQ(true, result);
}

TEST_F(NsmTypeNameCoverageTest, GetDbusPropertyDouble)
{
    auto& propMap = MockDbusAsync::propertyMap("/test/typename/path",
                                               "test.TypeName.Iface");
    propMap["DoubleProp"] = dbus::Value(double(2.71828));

    auto& handler = utils::DBusHandler();
    auto result = handler.getDbusProperty<double>(
        "/test/typename/path", "DoubleProp", "test.TypeName.Iface");
    EXPECT_DOUBLE_EQ(2.71828, result);
}

TEST_F(NsmTypeNameCoverageTest, GetDbusPropertyUnsignedLong)
{
    auto& propMap = MockDbusAsync::propertyMap("/test/typename/path",
                                               "test.TypeName.Iface");
    propMap["ULongProp"] = dbus::Value(uint64_t(12345678ULL));

    auto& handler = utils::DBusHandler();
    auto result = handler.getDbusProperty<uint64_t>(
        "/test/typename/path", "ULongProp", "test.TypeName.Iface");
    EXPECT_EQ(uint64_t(12345678ULL), result);
}

TEST_F(NsmTypeNameCoverageTest, GetDbusPropertyStringVector)
{
    auto& propMap = MockDbusAsync::propertyMap("/test/typename/path",
                                               "test.TypeName.Iface");
    propMap["StrVecProp"] =
        dbus::Value(std::vector<std::string>{"hello", "world"});

    auto& handler = utils::DBusHandler();
    auto result = handler.getDbusProperty<std::vector<std::string>>(
        "/test/typename/path", "StrVecProp", "test.TypeName.Iface");
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ("hello", result[0]);
    EXPECT_EQ("world", result[1]);
}

TEST_F(NsmTypeNameCoverageTest, TryGetDbusPropertyBool)
{
    auto& propMap = MockDbusAsync::propertyMap("/test/typename/path",
                                               "test.TypeName.Iface");
    propMap["TryBoolProp"] = dbus::Value(bool(false));

    auto& handler = utils::DBusHandler();
    auto result = handler.tryGetDbusProperty<bool>(
        "/test/typename/path", "TryBoolProp", "test.TypeName.Iface");
    EXPECT_EQ(false, result);
}
