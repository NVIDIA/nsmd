#include "test/mockDBusHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "debugTokenUtils.hpp"
#include "nsmDebugTokenNIC.hpp"
#include "nsmDebugTokenUnified.hpp"

class NsmDebugTokenTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    std::string name = "GPU0";
    uuid_t uuid = "550e8400-e29b-41d4-a716-446655440000";
    std::string debugTokenDeviceType = "GPU";
};

// NsmDebugTokenNICObject Constructor Tests
TEST_F(NsmDebugTokenTest, NICObject_ConstructorBasic)
{
    nsm::NsmDebugTokenNICObject debugTokenNIC(bus, name, uuid);

    EXPECT_EQ(debugTokenNIC.getName(), name);
    EXPECT_EQ(debugTokenNIC.uuid, uuid);
}

// NsmDebugTokenUnifiedObject Constructor Tests
TEST_F(NsmDebugTokenTest, UnifiedObject_ConstructorWithGPUDeviceType)
{
    std::string deviceType = "GPU";
    nsm::NsmDebugTokenUnifiedObject debugTokenUnified(bus, name, uuid,
                                                      deviceType);

    EXPECT_EQ(debugTokenUnified.getName(), name);
}

TEST_F(NsmDebugTokenTest, UnifiedObject_ConstructorWithNICDeviceType)
{
    std::string deviceType = "NIC";
    nsm::NsmDebugTokenUnifiedObject debugTokenUnified(bus, name, uuid,
                                                      deviceType);

    EXPECT_EQ(debugTokenUnified.getName(), name);
}

// Debug Token Utils Tests
TEST_F(NsmDebugTokenTest, TokenUtils_TokenTypeToUint32CPU)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;

    // Test with CPU device type - None should map to 0
    std::string deviceType = "CPU";
    uint32_t result = nsm::token_utils::tokenTypeToUint32(TokenType::None,
                                                          deviceType);

    EXPECT_EQ(result, 0);
}

TEST_F(NsmDebugTokenTest, TokenUtils_TokenTypeToEnumCPU)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;

    // Test with CPU device type - convert enum to uint32 and back
    std::string deviceType = "CPU";
    uint32_t tokenValue = nsm::token_utils::tokenTypeToUint32(
        TokenType::DebugFirmwareUnlock, deviceType);
    TokenType resultEnum = nsm::token_utils::tokenTypeToEnum(tokenValue,
                                                             deviceType);

    // Round-trip conversion should work
    EXPECT_EQ(resultEnum, TokenType::DebugFirmwareUnlock);
}

// ========== Comprehensive Branch Coverage Tests for debugTokenUtils ==========

// Test all device types for getTokenTypeMappingForDevice branches
TEST_F(NsmDebugTokenTest, TokenUtils_GetMappingERoT)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "ERoT";

    uint32_t result = nsm::token_utils::tokenTypeToUint32(
        TokenType::DebugFirmwareUnlock, deviceType);
    EXPECT_EQ(result, 1);

    auto enumResult = nsm::token_utils::tokenTypeToEnum(1, deviceType);
    EXPECT_EQ(enumResult, TokenType::DebugFirmwareUnlock);
}

TEST_F(NsmDebugTokenTest, TokenUtils_GetMappingGPU)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "GPU";

    uint32_t result = nsm::token_utils::tokenTypeToUint32(TokenType::None,
                                                          deviceType);
    EXPECT_EQ(result, 0);
}

TEST_F(NsmDebugTokenTest, TokenUtils_GetMappingSMA)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "SMA";

    uint32_t result = nsm::token_utils::tokenTypeToUint32(
        TokenType::DebugFirmwareUnlock, deviceType);
    EXPECT_EQ(result, 1);
}

TEST_F(NsmDebugTokenTest, TokenUtils_GetMappingDefault)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "UnknownDevice";

    uint32_t result = nsm::token_utils::tokenTypeToUint32(
        TokenType::DebugFirmwareUnlock, deviceType);
    EXPECT_EQ(result, 1);
}

// Test tokenTypeToUint32 not found case
TEST_F(NsmDebugTokenTest, TokenUtils_TokenTypeToUint32NotFound)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "CPU";

    // Use a token type not in CPU mapping
    uint32_t result = nsm::token_utils::tokenTypeToUint32(
        static_cast<TokenType>(999), deviceType);
    EXPECT_EQ(result, 0);
}

// Test tokenTypeToEnum not found case
TEST_F(NsmDebugTokenTest, TokenUtils_TokenTypeToEnumNotFound)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "CPU";

    auto result = nsm::token_utils::tokenTypeToEnum(999, deviceType);
    EXPECT_EQ(result, TokenType::None);
}

// Test all CPU token types
TEST_F(NsmDebugTokenTest, TokenUtils_CPUTokenTypes)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "CPU";

    // Test all CPU token types
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::None, deviceType),
              0);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(
                  TokenType::DebugFirmwareUnlock, deviceType),
              1);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(
                  TokenType::CcplexArmJtagDebugCont, deviceType),
              2);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::NVJtagControl,
                                                  deviceType),
              3);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::DiagnosticBoot,
                                                  deviceType),
              4);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::FwDebugKnobs,
                                                  deviceType),
              5);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::FirewallLifting,
                                                  deviceType),
              6);
    EXPECT_EQ(
        nsm::token_utils::tokenTypeToUint32(TokenType::Verbosity, deviceType),
        7);
}

// Test tokenSubtypeToEnum with nullptr mapping
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeToEnumNullMapping)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "UnknownDevice";

    auto result = nsm::token_utils::tokenSubtypeToEnum(1, 1, deviceType);
    EXPECT_EQ(result, TokenSubtype::None);
}

// Test tokenSubtypeToEnum map not found
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeToEnumMapNotFound)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "CPU";

    auto result = nsm::token_utils::tokenSubtypeToEnum(999, 1, deviceType);
    EXPECT_EQ(result, TokenSubtype::None);
}

// Test tokenSubtypeToEnum subtype not found in vector
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeToEnumSubtypeNotFound)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "CPU";

    auto result = nsm::token_utils::tokenSubtypeToEnum(1, 0x99999999,
                                                       deviceType);
    EXPECT_EQ(result, TokenSubtype::None);
}

// Test tokenSubtypeToEnum found case
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeToEnumFound)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "CPU";

    // Test DebugFirmwareUnlock (type 1) subtype OOBHub (0x01)
    auto result = nsm::token_utils::tokenSubtypeToEnum(1, 0x00000001,
                                                       deviceType);
    EXPECT_EQ(result, TokenSubtype::OOBHub);
}

// Test tokenSubtypeBitmapToEnumArray with zero bitmap
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeBitmapToArrayZero)
{
    std::string deviceType = "CPU";

    auto result = nsm::token_utils::tokenSubtypeBitmapToEnumArray(1, 0,
                                                                  deviceType);
    EXPECT_TRUE(result.empty());
}

// Test tokenSubtypeBitmapToEnumArray with single bit
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeBitmapToArraySingleBit)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "CPU";

    // Single bit set (OOBHub = 0x01)
    auto result = nsm::token_utils::tokenSubtypeBitmapToEnumArray(1, 0x00000001,
                                                                  deviceType);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], TokenSubtype::OOBHub);
}

// Test tokenSubtypeBitmapToEnumArray with multiple bits
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeBitmapToArrayMultipleBits)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "CPU";

    // Multiple bits set (OOBHub | RAS = 0x01 | 0x02)
    auto result = nsm::token_utils::tokenSubtypeBitmapToEnumArray(1, 0x00000003,
                                                                  deviceType);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], TokenSubtype::OOBHub);
    EXPECT_EQ(result[1], TokenSubtype::RAS);
}

// Test tokenSubtypeBitmapToEnumArray with high bit
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeBitmapToArrayHighBit)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "CPU";

    // CcplexArmJtagDebugCont (type 2) with EnableCswp (0x80000000)
    auto result = nsm::token_utils::tokenSubtypeBitmapToEnumArray(2, 0x80000000,
                                                                  deviceType);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], TokenSubtype::EnableCswp);
}

// Test GPU token types
TEST_F(NsmDebugTokenTest, TokenUtils_GPUTokenTypes)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "GPU";

    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::None, deviceType),
              0);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(
                  TokenType::DebugFirmwareUnlock, deviceType),
              1);
}

// Test SMA token types
TEST_F(NsmDebugTokenTest, TokenUtils_SMATokenTypes)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "SMA";

    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::OTPDumpEnable,
                                                  deviceType),
              2);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::RAS, deviceType),
              4);
}

// Test ERoT token types
TEST_F(NsmDebugTokenTest, TokenUtils_ERoTTokenTypes)
{
    using TokenType = nsm::token_utils::TokenTypeEnum;
    std::string deviceType = "ERoT";

    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(TokenType::None, deviceType),
              0);
    EXPECT_EQ(nsm::token_utils::tokenTypeToUint32(
                  TokenType::DebugFirmwareUnlock, deviceType),
              1);
}

// Test tokenSubtypeToEnum for different device types
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeToEnumERoT)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "ERoT";

    auto result = nsm::token_utils::tokenSubtypeToEnum(0, 0, deviceType);
    EXPECT_EQ(result, TokenSubtype::None);
}

TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeToEnumGPU)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "GPU";

    auto result = nsm::token_utils::tokenSubtypeToEnum(1, 0, deviceType);
    EXPECT_EQ(result, TokenSubtype::None);
}

TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeToEnumSMA)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "SMA";

    // SMA DebugFirmwareUnlock with MCU subtype (0x01)
    auto result = nsm::token_utils::tokenSubtypeToEnum(1, 0x00000001,
                                                       deviceType);
    EXPECT_EQ(result, TokenSubtype::MCU);
}

// Test CPU subtypes for different token types
TEST_F(NsmDebugTokenTest, TokenUtils_CPUSubtypesDebugFirmwareUnlock)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "CPU";

    // Test several DebugFirmwareUnlock subtypes
    EXPECT_EQ(nsm::token_utils::tokenSubtypeToEnum(1, 0x00000001, deviceType),
              TokenSubtype::OOBHub);
    EXPECT_EQ(nsm::token_utils::tokenSubtypeToEnum(1, 0x00000002, deviceType),
              TokenSubtype::RAS);
    EXPECT_EQ(nsm::token_utils::tokenSubtypeToEnum(1, 0x00000004, deviceType),
              TokenSubtype::Pcore);
    EXPECT_EQ(nsm::token_utils::tokenSubtypeToEnum(1, 0x00000800, deviceType),
              TokenSubtype::ATF);
}

TEST_F(NsmDebugTokenTest, TokenUtils_CPUSubtypesCcplexArm)
{
    using TokenSubtype = nsm::token_utils::TokenSubtypeEnum;
    std::string deviceType = "CPU";

    // Test CcplexArmJtagDebugCont (type 2) subtypes
    EXPECT_EQ(nsm::token_utils::tokenSubtypeToEnum(2, 0x00000001, deviceType),
              TokenSubtype::Dbgen);
    EXPECT_EQ(nsm::token_utils::tokenSubtypeToEnum(2, 0x00000002, deviceType),
              TokenSubtype::Niden);
    EXPECT_EQ(nsm::token_utils::tokenSubtypeToEnum(2, 0x00000010, deviceType),
              TokenSubtype::JtagEnable);
}

// Test tokenSubtypeBitmapToEnumArray with all 32 bits iteration
TEST_F(NsmDebugTokenTest, TokenUtils_SubtypeBitmapFullRange)
{
    std::string deviceType = "CPU";

    // Set multiple bits across the range
    uint32_t bitmap = 0x00000001 | 0x00000002 | 0x00000004 | 0x00000800;
    auto result = nsm::token_utils::tokenSubtypeBitmapToEnumArray(1, bitmap,
                                                                  deviceType);

    // Should find 4 valid subtypes
    EXPECT_EQ(result.size(), 4);
}
