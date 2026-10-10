// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <endian.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "device-configuration.h"
#include "fpga/fpga-maps.h"

#include "nsmFpgaDiscoveryCheck.hpp"
#include "nsmFpgaStatus.hpp"

#include <cstdlib>
#include <cstring>
#include <functional>

using namespace nsm;
using namespace ::testing;

namespace
{
const nsm_fpga_map_identity& lp30Map()
{
    return *nsm_fpga_map_identity_by_platform("LP30");
}
const std::array<uint8_t, 32> knownDigest = fpgaMapDigest(lp30Map());
static std::vector<uint8_t> makeReadResponse(const std::vector<uint8_t>& data,
                                             uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_device_config_v2_resp) - 1 +
                                  data.size());
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_device_config_v2_resp(
        0, cc, ERR_NULL, data.data(), static_cast<uint16_t>(data.size()),
        nullptr, 0, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    return response;
}

static std::vector<uint8_t>
    makeSupportedConfigsResponse(const std::vector<uint32_t>& types,
                                 uint32_t nextHandle = 0,
                                 uint32_t totalCount = 0)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_configs_resp) -
        sizeof(uint32_t) + types.size() * sizeof(uint32_t));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_supported_device_configs_resp(
        0, NSM_SUCCESS, ERR_NULL, nextHandle,
        totalCount ? totalCount : static_cast<uint32_t>(types.size()),
        types.data(), static_cast<uint16_t>(types.size()), msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    return response;
}
} // namespace

struct NsmFpgaDiscoveryCheckTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmFpgaDiscoveryCheckTest() : SensorManagerTest(devices) {}

    ~NsmFpgaDiscoveryCheckTest()
    {
        NsmFpgaStatus::clear();
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmFpgaStatus> makeStatus()
    {
        return std::make_shared<NsmFpgaStatus>(
            "HGX_FPGA_0",
            "/xyz/openbmc_project/inventory/system/processors/FPGA_0");
    }

    NsmFpgaDiscoveryCheck makeTypeCheck(std::shared_ptr<NsmFpgaStatus> status)
    {
        return NsmFpgaDiscoveryCheck(
            "HGX_FPGA_0_DiscoveryCheck_ConfigurationType",
            "NSM_FpgaDiscoveryCheck",
            NsmFpgaDiscoveryCheck::CheckKind::ConfigurationType, 2, 0x1080, 8,
            status, lp30Map());
    }

    NsmFpgaDiscoveryCheck makeDigestCheck(std::shared_ptr<NsmFpgaStatus> status)
    {
        return NsmFpgaDiscoveryCheck(
            "HGX_FPGA_0_DiscoveryCheck_MapDigest", "NSM_FpgaDiscoveryCheck",
            NsmFpgaDiscoveryCheck::CheckKind::MapDigest, 2, 0x1080, 8, status,
            lp30Map());
    }
};

TEST_F(NsmFpgaDiscoveryCheckTest, StatusGatesEnabled)
{
    auto status = makeStatus();
    EXPECT_FALSE(status->enabled());
    status->setOnline(true);
    EXPECT_FALSE(status->enabled());
    status->setTypeCheck(true);
    status->setDigestCheck(true);
    EXPECT_TRUE(status->enabled());
    EXPECT_EQ(status->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
    status->setWriteFault(true);
    EXPECT_FALSE(status->enabled());
    EXPECT_EQ(status->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Fault);
    EXPECT_EQ(status->healthIntf->health(),
              FpgaHealthIntf::HealthType::Critical);
}

TEST_F(NsmFpgaDiscoveryCheckTest, StatusOfflineIsUnavailable)
{
    auto status = makeStatus();
    status->setTypeCheck(true);
    status->setDigestCheck(true);
    status->setOnline(false);
    EXPECT_FALSE(status->enabled());
    EXPECT_EQ(status->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
}

TEST_F(NsmFpgaDiscoveryCheckTest, RegistryReturnsOneStatusPerUuid)
{
    bool created = false;
    auto a = NsmFpgaStatus::getOrCreate(
        "STATIC:3:0:MCTP_EID:17", "HGX_FPGA_0",
        "/xyz/openbmc_project/inventory/system/processors/FPGA_0", created);
    EXPECT_TRUE(created);
    auto b = NsmFpgaStatus::getOrCreate(
        "STATIC:3:0:MCTP_EID:17", "HGX_FPGA_0",
        "/xyz/openbmc_project/inventory/system/processors/FPGA_0", created);
    EXPECT_FALSE(created);
    EXPECT_EQ(a, b);
    EXPECT_EQ(NsmFpgaStatus::get("STATIC:3:1:MCTP_EID:19"), nullptr);
    // Default status path is the FPGA processor object bmcweb reads
    EXPECT_EQ(fpgaProcessorPath(1),
              "/xyz/openbmc_project/inventory/system/processors/FPGA_1");
}

TEST_F(NsmFpgaDiscoveryCheckTest, DigestRequestReadsDigestRegister)
{
    auto status = makeStatus();
    auto check = makeDigestCheck(status);

    auto request = check.genRequestMsg(17, 0);
    ASSERT_TRUE(request.has_value());
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto req =
        reinterpret_cast<const nsm_get_device_config_v2_req*>(msg->payload);
    EXPECT_EQ(req->hdr.command, NSM_GET_DEVICE_CONFIG_V2);
    EXPECT_EQ(le16toh(req->hdr.data_size), 4 + sizeof(nsm_mmio_access_query));
    EXPECT_EQ(le32toh(req->device_config_type), 2u);
    EXPECT_EQ(req->device_config_query[0], 0x80);
    EXPECT_EQ(req->device_config_query[1], 0x10);
    EXPECT_EQ(req->device_config_query[8], 0x08);
}

TEST_F(NsmFpgaDiscoveryCheckTest, DigestCheckPassesOnMatchingDigest)
{
    auto status = makeStatus();
    status->setOnline(true);
    status->setTypeCheck(true);
    auto check = makeDigestCheck(status);

    std::vector<uint8_t> data(knownDigest.begin(), knownDigest.end());
    auto response = makeReadResponse(data);
    auto rc = check.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(check.passed());
    EXPECT_TRUE(status->enabled());
    EXPECT_FALSE(check.needsUpdate(UINT64_MAX));
    // a freshly activated bank asks for the check again, polling pauses
    status->requestRecheck();
    EXPECT_FALSE(status->enabled());
    EXPECT_TRUE(check.needsUpdate(UINT64_MAX));
}

TEST_F(NsmFpgaDiscoveryCheckTest, DigestCheckFailsOnMismatch)
{
    auto status = makeStatus();
    status->setOnline(true);
    status->setTypeCheck(true);
    auto check = makeDigestCheck(status);

    std::vector<uint8_t> data(32, 0xaa);
    auto response = makeReadResponse(data);
    check.handleResponseMsg(reinterpret_cast<const nsm_msg*>(response.data()),
                            response.size());
    EXPECT_FALSE(check.passed());
    EXPECT_FALSE(status->enabled());
    EXPECT_TRUE(check.needsUpdate(UINT64_MAX));
}

TEST_F(NsmFpgaDiscoveryCheckTest, DigestCheckAcceptsOnlyThePlatformDigest)
{
    auto status = makeStatus();
    status->setOnline(true);
    status->setTypeCheck(true);
    // Another platform's row: same window, its own digest
    nsm_fpga_map_identity other = lp30Map();
    other.platform = "Test";
    other.name = "test build";
    std::array<uint8_t, 32> expected{};
    for (size_t i = 0; i < expected.size(); ++i)
    {
        expected[i] = static_cast<uint8_t>(0x80 + i);
        other.digest[i] = expected[i];
    }
    NsmFpgaDiscoveryCheck check("HGX_FPGA_0_DiscoveryCheck_MapDigest",
                                "NSM_FpgaDiscoveryCheck",
                                NsmFpgaDiscoveryCheck::CheckKind::MapDigest, 2,
                                0x1080, 8, status, other);

    auto request = check.genRequestMsg(17, 0);
    ASSERT_TRUE(request.has_value());
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto req =
        reinterpret_cast<const nsm_get_device_config_v2_req*>(msg->payload);
    EXPECT_EQ(req->device_config_query[0], 0x80);
    EXPECT_EQ(req->device_config_query[1], 0x10);

    std::vector<uint8_t> data(expected.begin(), expected.end());
    auto response = makeReadResponse(data);
    EXPECT_EQ(
        check.handleResponseMsg(
            reinterpret_cast<const nsm_msg*>(response.data()), response.size()),
        NSM_SUCCESS);
    EXPECT_TRUE(check.passed());

    // The LP30 digest is another platform's build here, so it fails
    std::vector<uint8_t> known(knownDigest.begin(), knownDigest.end());
    response = makeReadResponse(known);
    check.handleResponseMsg(reinterpret_cast<const nsm_msg*>(response.data()),
                            response.size());
    EXPECT_FALSE(check.passed());
    EXPECT_EQ(check.getExpectedDigest(), expected);
    EXPECT_STREQ(lp30Map().name, "P0 ibc1 schema 13");
}

TEST_F(NsmFpgaDiscoveryCheckTest, DigestCheckFailsOnShortReply)
{
    auto status = makeStatus();
    status->setOnline(true);
    status->setTypeCheck(true);
    auto check = makeDigestCheck(status);

    std::vector<uint8_t> data(16, 0);
    auto response = makeReadResponse(data);
    auto rc = check.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_FALSE(check.passed());
}

TEST_F(NsmFpgaDiscoveryCheckTest, TypeCheckRequestsSupportedConfigs)
{
    auto status = makeStatus();
    auto check = makeTypeCheck(status);
    auto request = check.genRequestMsg(17, 0);
    ASSERT_TRUE(request.has_value());
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto req = reinterpret_cast<const nsm_get_supported_device_configs_req*>(
        msg->payload);
    EXPECT_EQ(req->hdr.command, NSM_GET_SUPPORTED_DEVICE_CONFIGS);
    EXPECT_EQ(le16toh(req->hdr.data_size), sizeof(uint32_t));
    EXPECT_EQ(le32toh(req->handle), 0u);
    EXPECT_FALSE(check.passed());
}

TEST_F(NsmFpgaDiscoveryCheckTest, TypeCheckPassesWhenTypeAdvertised)
{
    auto status = makeStatus();
    status->setOnline(true);
    status->setDigestCheck(true);
    auto check = makeTypeCheck(status);

    auto response = makeSupportedConfigsResponse({1, 2});
    auto rc = check.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(check.passed());
    EXPECT_TRUE(status->enabled());
    EXPECT_FALSE(check.needsUpdate(UINT64_MAX));
}

TEST_F(NsmFpgaDiscoveryCheckTest, TypeCheckFollowsTheChunks)
{
    auto status = makeStatus();
    status->setOnline(true);
    status->setDigestCheck(true);
    auto check = makeTypeCheck(status);

    // First chunk lists type 1 only and points at handle 7
    auto first = makeSupportedConfigsResponse({1}, 7, 3);
    EXPECT_EQ(check.handleResponseMsg(
                  reinterpret_cast<const nsm_msg*>(first.data()), first.size()),
              NSM_SUCCESS);
    EXPECT_FALSE(check.passed());
    EXPECT_TRUE(check.moreTypeChunks);
    // The next request carries handle 7
    auto request = check.genRequestMsg(17, 0);
    ASSERT_TRUE(request.has_value());
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    const uint8_t* handle = msg->payload + sizeof(nsm_common_req_v2);
    EXPECT_EQ(handle[0], 7);
    EXPECT_EQ(handle[1], 0);

    // The last chunk carries the type
    auto second = makeSupportedConfigsResponse({2, 3}, 0, 3);
    EXPECT_EQ(
        check.handleResponseMsg(reinterpret_cast<const nsm_msg*>(second.data()),
                                second.size()),
        NSM_SUCCESS);
    EXPECT_TRUE(check.passed());
    EXPECT_FALSE(check.moreTypeChunks);
    EXPECT_TRUE(status->enabled());
}

TEST_F(NsmFpgaDiscoveryCheckTest, TypeCheckFailsWhenTypeMissing)
{
    auto status = makeStatus();
    status->setOnline(true);
    status->setDigestCheck(true);
    auto check = makeTypeCheck(status);

    auto response = makeSupportedConfigsResponse({1});
    check.handleResponseMsg(reinterpret_cast<const nsm_msg*>(response.data()),
                            response.size());
    EXPECT_FALSE(check.passed());
    EXPECT_FALSE(status->enabled());
    EXPECT_TRUE(check.needsUpdate(UINT64_MAX));
}

TEST_F(NsmFpgaDiscoveryCheckTest, StatusFollowsDeviceCallbacks)
{
    auto status = makeStatus();
    status->onDeviceOnline();
    EXPECT_TRUE(status->isOnline());
    status->handleOfflineState();
    EXPECT_FALSE(status->isOnline());
    status->setWriteFault(true);
    EXPECT_TRUE(status->hasWriteFault());
    status->setWriteFault(false);
    EXPECT_FALSE(status->hasWriteFault());
    EXPECT_EQ(status->getInventoryPath(), fpgaProcessorPath(0));
}

TEST(FpgaProperties, OptionalReadersAcceptOnlyMatchingTypes)
{
    dbus::PropertyMap props = {{"S", std::string("x")}, {"U", uint64_t{7}},
                               {"D", double{8.0}},      {"F", double{8.5}},
                               {"N", int64_t{-1}},      {"B", true}};
    EXPECT_EQ(utils::getOptionalString(props, "S"), "x");
    EXPECT_EQ(utils::getOptionalString(props, "U"), std::nullopt);
    EXPECT_EQ(utils::getOptionalString(props, "missing"), std::nullopt);
    EXPECT_EQ(utils::getOptionalUnsigned(props, "U"), 7u);
    EXPECT_EQ(utils::getOptionalUnsigned(props, "D"), 8u);
    EXPECT_EQ(utils::getOptionalUnsigned(props, "F"), std::nullopt);
    EXPECT_EQ(utils::getOptionalUnsigned(props, "N"), std::nullopt);
    EXPECT_EQ(utils::getOptionalUnsigned(props, "B"), std::nullopt);
    EXPECT_EQ(utils::getOptionalUnsigned(props, "S"), std::nullopt);
    EXPECT_EQ(utils::getOptionalUnsigned(props, "missing"), std::nullopt);
    EXPECT_EQ(utils::vectorTo256BitHexString(
                  std::vector<uint8_t>(knownDigest.begin(), knownDigest.end()))
                  .substr(0, 10),
              "0x3072ef68");
}

namespace
{
// Answers device I/O from a test-supplied function; an empty reply is an error
struct FakeReplier
{
    std::function<Response(const Request&)> reply;
    size_t calls = 0;

    requester::Coroutine io(eid_t, Request& request,
                            std::shared_ptr<const nsm_msg>& responseMsg,
                            size_t& responseLen, bool)
    {
        ++calls;
        Response response = reply ? reply(request) : Response{};
        responseLen = response.size();
        if (responseLen == 0)
        {
            // coverity[missing_return]
            co_return NSM_ERROR;
        }
        auto raw = reinterpret_cast<nsm_msg*>(malloc(responseLen));
        memcpy(raw, response.data(), responseLen);
        responseMsg = std::shared_ptr<const nsm_msg>(
            raw, [](const nsm_msg* ptr) { free((void*)ptr); });
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    }
};
} // namespace

struct NsmFpgaDiscoveryFactoryTest : public NsmFpgaDiscoveryCheckTest
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_FpgaDiscoveryCheck";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::shared_ptr<MockNsmDevice> fpga;
    FakeReplier replier;

    NsmFpgaDiscoveryFactoryTest()
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        fpga->isDeviceActive = false;
        ON_CALL(*fpga, sensorIO(_, _, _, _, _))
            .WillByDefault(Invoke(&replier, &FakeReplier::io));
    }

    dbus::PropertyMap entry(const std::string& name, const uuid_t& uuid)
    {
        return {{"Name", name},
                {"Type", std::string("NSM_FpgaDiscoveryCheck")},
                {"UUID", uuid},
                {"Platform", std::string("LP30")}};
    }

    // Runs the factory and returns how many objects it added to the FPGA
    size_t create(SensorManager& manager, const std::string& path,
                  const dbus::PropertyMap& props)
    {
        utils::MockDbusAsync::propertyMap(path, intf) = props;
        const size_t before = fpga->deviceSensors.size();
        createNsmFpgaDiscoveryCheck(manager, intf, path);
        return fpga->deviceSensors.size() - before;
    }

    static std::shared_ptr<NsmFpgaDiscoveryCheck>
        findCheck(const std::shared_ptr<NsmDevice>& device,
                  NsmFpgaDiscoveryCheck::CheckKind kind)
    {
        for (auto& object : device->deviceSensors)
        {
            auto check =
                std::dynamic_pointer_cast<NsmFpgaDiscoveryCheck>(object);
            if (check && check->getCheckKind() == kind)
            {
                return check;
            }
        }
        return nullptr;
    }

    static const nsm_get_device_config_v2_req*
        readRequest(const std::vector<uint8_t>& request)
    {
        auto msg = reinterpret_cast<const nsm_msg*>(request.data());
        return reinterpret_cast<const nsm_get_device_config_v2_req*>(
            msg->payload);
    }

    std::shared_ptr<NsmFpgaDiscoveryCheck>
        makeCheck(NsmFpgaDiscoveryCheck::CheckKind kind,
                  std::shared_ptr<NsmFpgaStatus> status)
    {
        return std::make_shared<NsmFpgaDiscoveryCheck>(
            "HGX_FPGA_0_DiscoveryCheck", "NSM_FpgaDiscoveryCheck", kind, 2,
            0x1080, 8, status, lp30Map());
    }
};

TEST_F(NsmFpgaDiscoveryFactoryTest, FactoryCreatesStatusAndBothChecks)
{
    EXPECT_EQ(create(mockManager, "/xyz/test/fpga/check0",
                     entry("HGX_FPGA_0_DiscoveryCheck", fpgaUuid)),
              3u);
    EXPECT_EQ(fpga->capabilityRefreshSensors.size(), 2u);
    auto status = NsmFpgaStatus::get(fpgaUuid);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->getInventoryPath(), fpgaProcessorPath(0));
    EXPECT_FALSE(status->isOnline());
    EXPECT_NE(
        findCheck(fpga, NsmFpgaDiscoveryCheck::CheckKind::ConfigurationType),
        nullptr);
    auto digest = findCheck(fpga, NsmFpgaDiscoveryCheck::CheckKind::MapDigest);
    ASSERT_NE(digest, nullptr);
    // Register, length and digest come from the platform's libnsm map row
    auto request = digest->genRequestMsg(17, 0);
    ASSERT_TRUE(request.has_value());
    auto req = readRequest(*request);
    EXPECT_EQ(le32toh(req->device_config_type), 2u);
    EXPECT_EQ(req->device_config_query[0], 0x80);
    EXPECT_EQ(req->device_config_query[1], 0x10);
    EXPECT_EQ(req->device_config_query[8], 0x08);
    EXPECT_EQ(digest->getExpectedDigest(), knownDigest);
    // A second entry for the same FPGA reuses its status object
    EXPECT_EQ(create(mockManager, "/xyz/test/fpga/check0b",
                     entry("HGX_FPGA_0_DiscoveryCheck_b", fpgaUuid)),
              2u);
    EXPECT_EQ(NsmFpgaStatus::get(fpgaUuid), status);
}

TEST_F(NsmFpgaDiscoveryFactoryTest, FactoryHonoursEntryOverrides)
{
    const uuid_t other = "STATIC:3:1:NSM_DEVICE_INSTANCE_NUMBER:1";
    auto device = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(other));
    ASSERT_NE(device, nullptr);
    device->isDeviceActive = false;
    auto props = entry("HGX_FPGA_1_DiscoveryCheck", other);
    props["ConfigurationType"] = uint64_t{3};
    props["DigestAddress"] = double{8192};
    props["DigestLength"] = int64_t{16};
    props["InventoryObjPath"] = std::string("/xyz/test/fpga/FPGA_1");
    utils::MockDbusAsync::propertyMap("/xyz/test/fpga/check1", intf) = props;
    const size_t before = device->deviceSensors.size();
    createNsmFpgaDiscoveryCheck(mockManager, intf, "/xyz/test/fpga/check1");
    EXPECT_EQ(device->deviceSensors.size(), before + 3);
    auto status = NsmFpgaStatus::get(other);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->getInventoryPath(), "/xyz/test/fpga/FPGA_1");
    auto digest = findCheck(device,
                            NsmFpgaDiscoveryCheck::CheckKind::MapDigest);
    ASSERT_NE(digest, nullptr);
    auto request = digest->genRequestMsg(19, 0);
    ASSERT_TRUE(request.has_value());
    auto req = readRequest(*request);
    EXPECT_EQ(le32toh(req->device_config_type), 3u);
    EXPECT_EQ(req->device_config_query[0], 0x00);
    EXPECT_EQ(req->device_config_query[1], 0x20);
    EXPECT_EQ(req->device_config_query[8], 0x10);
}

TEST_F(NsmFpgaDiscoveryFactoryTest, FactoryRejectsBadEntries)
{
    auto with = [&](const std::string& path, const std::string& key,
                    dbus::Value value) {
        auto props = entry("HGX_FPGA_0_DiscoveryCheck", fpgaUuid);
        props[key] = value;
        return create(mockManager, path, props);
    };
    EXPECT_EQ(with("/xyz/test/fpga/r1", "DigestAddress", uint64_t{4225}), 0u);
    EXPECT_EQ(with("/xyz/test/fpga/r2", "DigestLength", uint64_t{0}), 0u);
    EXPECT_EQ(with("/xyz/test/fpga/r3", "DigestLength", uint64_t{4}), 0u);
    EXPECT_EQ(with("/xyz/test/fpga/r4", "DigestLength", uint64_t{1024}), 0u);
    EXPECT_EQ(with("/xyz/test/fpga/r5", "ConfigurationType", uint64_t{1} << 32),
              0u);
    // The platform must name a row of the libnsm map table
    EXPECT_EQ(with("/xyz/test/fpga/r9", "Platform", std::string("Unknown")),
              0u);
    EXPECT_EQ(with("/xyz/test/fpga/r10", "Platform", uint64_t{30}), 0u);
    EXPECT_EQ(NsmFpgaStatus::get(fpgaUuid), nullptr);
    // Values of the wrong shape fall back to the defaults
    EXPECT_EQ(with("/xyz/test/fpga/r6", "DigestAddress", int64_t{-4}), 3u);
    EXPECT_EQ(with("/xyz/test/fpga/r7", "DigestLength", true), 2u);
    EXPECT_EQ(with("/xyz/test/fpga/r8", "DigestAddress", std::string("0x1080")),
              2u);
}

TEST_F(NsmFpgaDiscoveryFactoryTest, FactoryRejectsUnknownDevice)
{
    NullReturnMockSensorManager none(devices);
    EXPECT_EQ(create(none, "/xyz/test/fpga/u2",
                     entry("HGX_FPGA_0_DiscoveryCheck", fpgaUuid)),
              0u);
    EXPECT_EQ(NsmFpgaStatus::get(fpgaUuid), nullptr);
}

TEST_F(NsmFpgaDiscoveryFactoryTest, ChecksRunFromThePollingLoopNotAtCreation)
{
    fpga->isDeviceActive = true;
    replier.reply = [](const Request& request) -> Response {
        auto msg = reinterpret_cast<const nsm_msg*>(request.data());
        if (msg->payload[0] == NSM_GET_SUPPORTED_DEVICE_CONFIGS)
        {
            return makeSupportedConfigsResponse({1, 2});
        }
        std::vector<uint8_t> data(knownDigest.begin(), knownDigest.end());
        return makeReadResponse(data);
    };
    EXPECT_EQ(create(mockManager, "/xyz/test/fpga/on",
                     entry("HGX_FPGA_0_DiscoveryCheck", fpgaUuid)),
              3u);
    // Creation sends nothing, the status is offline until its refresh
    EXPECT_EQ(replier.calls, 0u);
    auto status = NsmFpgaStatus::get(fpgaUuid);
    ASSERT_NE(status, nullptr);
    EXPECT_FALSE(status->isOnline());
    auto statusRefresh = status->update(fpga);
    EXPECT_TRUE(status->isOnline());
    EXPECT_FALSE(status->enabled());
    // The polling loop runs each check once and the FPGA becomes enabled
    auto digest = findCheck(fpga, NsmFpgaDiscoveryCheck::CheckKind::MapDigest);
    auto type = findCheck(fpga,
                          NsmFpgaDiscoveryCheck::CheckKind::ConfigurationType);
    auto first = digest->update(fpga);
    auto second = type->update(fpga);
    EXPECT_EQ(replier.calls, 2u);
    EXPECT_TRUE(digest->passed());
    EXPECT_TRUE(type->passed());
    EXPECT_TRUE(status->enabled());
    // An offline device leaves the status offline on its refresh
    fpga->isDeviceActive = false;
    auto offline = status->update(fpga);
    EXPECT_FALSE(status->isOnline());
}

TEST_F(NsmFpgaDiscoveryFactoryTest, UpdateWithoutReplyFailsTheCheck)
{
    fpga->isDeviceActive = true;
    auto status = makeStatus();
    status->setOnline(true);
    status->setTypeCheck(true);
    status->setDigestCheck(true);
    auto digest = makeCheck(NsmFpgaDiscoveryCheck::CheckKind::MapDigest,
                            status);
    auto first = digest->update(fpga);
    EXPECT_EQ(replier.calls, 1u);
    EXPECT_FALSE(digest->passed());
    EXPECT_FALSE(status->digestCheckPassed());
    EXPECT_TRUE(status->typeCheckPassed());
    auto type = makeCheck(NsmFpgaDiscoveryCheck::CheckKind::ConfigurationType,
                          status);
    auto second = type->update(fpga);
    EXPECT_EQ(replier.calls, 2u);
    EXPECT_FALSE(type->passed());
    EXPECT_FALSE(status->typeCheckPassed());
    // The same failure is not logged twice
    auto third = type->update(fpga);
    EXPECT_EQ(replier.calls, 3u);
    EXPECT_FALSE(status->enabled());
}

TEST_F(NsmFpgaDiscoveryFactoryTest, TypeCheckReadsOneChunkPerPollUpToTheLimit)
{
    fpga->isDeviceActive = true;
    uint32_t handle = 0;
    replier.reply = [&handle](const Request&) {
        return makeSupportedConfigsResponse({1}, ++handle, 100);
    };
    auto status = makeStatus();
    status->setOnline(true);
    status->setDigestCheck(true);
    auto type = makeCheck(NsmFpgaDiscoveryCheck::CheckKind::ConfigurationType,
                          status);
    // Every poll is one request, the type stays unproven and the check due
    for (size_t poll = 1; poll < 16; ++poll)
    {
        auto coro = type->update(fpga);
        EXPECT_EQ(replier.calls, poll);
        EXPECT_FALSE(type->passed());
        EXPECT_TRUE(type->needsUpdate(UINT64_MAX));
    }
    // The sixteenth chunk ends the search and starts again from the first
    auto last = type->update(fpga);
    EXPECT_EQ(replier.calls, 16u);
    EXPECT_FALSE(type->passed());
    EXPECT_FALSE(status->typeCheckPassed());
    EXPECT_TRUE(type->needsUpdate(UINT64_MAX));
    auto request = type->genRequestMsg(17, 0);
    ASSERT_TRUE(request.has_value());
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto req = reinterpret_cast<const nsm_get_supported_device_configs_req*>(
        msg->payload);
    EXPECT_EQ(le32toh(req->handle), 0u);
}

TEST_F(NsmFpgaDiscoveryFactoryTest, TypeCheckPassesThroughUpdate)
{
    fpga->isDeviceActive = true;
    replier.reply = [](const Request&) {
        return makeSupportedConfigsResponse({2});
    };
    auto status = makeStatus();
    status->setOnline(true);
    status->setDigestCheck(true);
    auto type = makeCheck(NsmFpgaDiscoveryCheck::CheckKind::ConfigurationType,
                          status);
    auto coro = type->update(fpga);
    EXPECT_EQ(replier.calls, 1u);
    EXPECT_TRUE(type->passed());
    EXPECT_TRUE(status->enabled());
    EXPECT_FALSE(type->needsUpdate(UINT64_MAX));
}
