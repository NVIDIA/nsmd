// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga/fpga-tables.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
const nsm_fpga_command wordRead = {
    .op = NSM_FPGA_OP_READ32,
    .station = 29,
    .u = {.csr = {0x05000400ULL, 0}},
};
} // namespace

TEST(fpgaTables, mapP0IsBoundToItsIdentity)
{
	const auto *p0 = nsm_fpga_map_p0();
	ASSERT_NE(p0, nullptr);
	EXPECT_EQ(p0->window.configuration_type, 2u);
	EXPECT_EQ(p0->window.control_write, 0x1000u);
	EXPECT_EQ(p0->window.status, 0x10a0u);
	EXPECT_EQ(p0->window.compact, 0x10e0u);
	EXPECT_EQ(p0->station_count, 38u);
}

TEST(fpgaTables, stationBySelectorFindsEachStationKind)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *ddr = nsm_fpga_station_by_selector(p0, 29);
	ASSERT_NE(ddr, nullptr);
	EXPECT_EQ(ddr->selector, 29);
	EXPECT_EQ(ddr->kind, NSM_FPGA_STATION_WORD);
	EXPECT_EQ(ddr->first_byte, 0x05000200u);
	EXPECT_EQ(ddr->last_byte, 0x0500080fu);
	const auto *osfp = nsm_fpga_station_by_selector(p0, 1);
	ASSERT_NE(osfp, nullptr);
	EXPECT_EQ(osfp->kind, NSM_FPGA_STATION_I2C_BYTE);
	EXPECT_EQ(nsm_fpga_station_by_selector(p0, 37)->kind,
		  NSM_FPGA_STATION_SDM);
	EXPECT_EQ(nsm_fpga_station_by_selector(p0, 38), nullptr);
	EXPECT_EQ(nsm_fpga_station_by_selector(nullptr, 28), nullptr);
}

TEST(fpgaTables, findRuleAndFindGroupRejectUnknownNames)
{
	const auto *p0 = nsm_fpga_map_p0();
	EXPECT_EQ(nsm_fpga_find_rule(p0, "HGX_FPGA_0_Anything"), nullptr);
	EXPECT_EQ(nsm_fpga_find_group(p0, "Anything"), nullptr);
	EXPECT_EQ(nsm_fpga_find_rule(p0, nullptr), nullptr);
}

TEST(fpgaTables, ruleValidAndWordLegalAcceptEveryRowOfEveryPart)
{
	const auto *p0 = nsm_fpga_map_p0();
	for (size_t p = 0; p < p0->part_count; ++p) {
		const auto *part = p0->parts[p];
		for (size_t g = 0; g < part->group_count; ++g) {
			const auto &group = part->groups[g];
			EXPECT_TRUE(nsm_fpga_commands_valid(p0, group.setup,
							    group.setup_count))
			    << group.name;
			EXPECT_TRUE(nsm_fpga_commands_valid(
			    p0, group.cleanup, group.cleanup_count))
			    << group.name;
			EXPECT_EQ(nsm_fpga_find_group(p0, group.name), &group);
		}
		for (size_t i = 0; i < part->rule_count; ++i) {
			const auto &rule = part->rules[i];
			EXPECT_TRUE(nsm_fpga_rule_valid(p0, &rule))
			    << rule.name;
			EXPECT_EQ(nsm_fpga_find_rule(p0, rule.name), &rule);
			for (size_t c = 0; c < rule.command_count; ++c) {
				const auto &cmd = rule.commands[c];
				if (cmd.op == NSM_FPGA_OP_READ32 ||
				    cmd.op == NSM_FPGA_OP_WRITE32) {
					EXPECT_TRUE(nsm_fpga_word_legal(
					    p0, cmd.station, cmd.op,
					    cmd.u.csr.address, cmd.u.csr.value,
					    0xf))
					    << rule.name << " row " << c;
				}
			}
		}
	}
}

/* A state map for test rules: bit 0 set is enabled, anything else absent */
const nsm_fpga_state_entry testStates[] = {
    {0x1, 0x1, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK},
    {0, 0, NSM_FPGA_STATE_ABSENT, NSM_FPGA_HEALTH_OK}};

TEST(fpgaTables, ruleValidChecksStationAndWindow)
{
	const auto *p0 = nsm_fpga_map_p0();
	nsm_fpga_rule rule = {
	    .name = "Test_Rule",
	    .commands = &wordRead,
	    .command_count = 1,
	    .kind = NSM_FPGA_RULE_STATE,
	    .group = "Test",
	    .states = testStates,
	    .state_count = 2,
	};
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, &rule));
	nsm_fpga_command misaligned = wordRead;
	misaligned.u.csr.address = 0x05000401;
	rule.commands = &misaligned;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &rule));
	nsm_fpga_command beyond = wordRead;
	beyond.u.csr.address = 0x05000900;
	rule.commands = &beyond;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &rule));
	nsm_fpga_command onI2c = wordRead;
	onI2c.station = 1;
	rule.commands = &onI2c;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &rule));
	nsm_fpga_command badPoll = wordRead;
	badPoll.poll = NSM_FPGA_POLL_ANY + 1;
	rule.commands = &badPoll;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &rule));
	rule.commands = &wordRead;
	rule.command_count = 0;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &rule));
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, nullptr));
}

TEST(fpgaTables, ruleWordReadsTheGroupBufferForRowlessRules)
{
	const auto *p0 = nsm_fpga_map_p0();
	nsm_fpga_rule rowless = {
	    .name = "Presence",
	    .kind = NSM_FPGA_RULE_STATE,
	    .numeric = {.buffer = NSM_FPGA_BUF_GROUP, .offset = 1},
	    .group = "Test",
	    .states = testStates,
	    .state_count = 2,
	};
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, &rowless));
	rowless.numeric.buffer = NSM_FPGA_BUF_ITEM;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &rowless));
	rowless.numeric.buffer = NSM_FPGA_BUF_GROUP;
	const std::array<uint8_t, 3> group = {0xaa, 0x12, 0x34};
	uint32_t word = 0;
	ASSERT_TRUE(nsm_fpga_rule_word(&rowless, nullptr, 0, group.data(),
				       group.size(), &word));
	EXPECT_EQ(word, 0x3412u);
	EXPECT_FALSE(
	    nsm_fpga_rule_word(&rowless, nullptr, 0, group.data(), 1, &word));
	/* a rule with rows reads its own first word */
	const nsm_fpga_rule withRows = {
	    .name = "Word", .commands = &wordRead, .command_count = 1};
	const std::array<uint8_t, 4> item = {0x01, 0x00, 0x00, 0x80};
	ASSERT_TRUE(nsm_fpga_rule_word(&withRows, item.data(), item.size(),
				       nullptr, 0, &word));
	EXPECT_EQ(word, 0x80000001u);
	EXPECT_FALSE(nsm_fpga_rule_word(&withRows, nullptr, 0, group.data(),
					group.size(), &word));
}

TEST(fpgaTables, ruleValidChecksCleanupRowsToo)
{
	const auto *p0 = nsm_fpga_map_p0();
	const nsm_fpga_command ack = {
	    .op = NSM_FPGA_OP_ACK_CLEAR,
	    .station = 29,
	    .poll_mask = 1,
	    .u = {.csr = {0x0500045cULL, 0}},
	};
	nsm_fpga_rule rule = {
	    .name = "Test_Rule",
	    .commands = &wordRead,
	    .command_count = 1,
	    .cleanup = &ack,
	    .cleanup_count = 1,
	    .kind = NSM_FPGA_RULE_NUMERIC,
	    .group = "Test",
	};
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, &rule));
	nsm_fpga_command badAck = ack;
	badAck.u.csr.address = 0x05000900;
	rule.cleanup = &badAck;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &rule));
}

TEST(fpgaTables, commandsValidChecksPopByteAndCapturedRows)
{
	const auto *p0 = nsm_fpga_map_p0();
	const nsm_fpga_command pop = {
	    .op = NSM_FPGA_OP_POP,
	    .station = 37,
	    .poll_mask = 0x3,
	    .u = {.pop = {24, 20}},
	};
	EXPECT_TRUE(nsm_fpga_commands_valid(p0, &pop, 1));
	nsm_fpga_command popNoMask = pop;
	popNoMask.poll_mask = 0;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &popNoMask, 1));
	nsm_fpga_command popBadOffset = pop;
	popBadOffset.u.pop.data_address = 12;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &popBadOffset, 1));
	const nsm_fpga_command byte = {
	    .op = NSM_FPGA_OP_BYTE,
	    .station = 1,
	    .u = {.i2c = {0x50, 0,
			  NSM_FPGA_I2C_READ | NSM_FPGA_I2C_STOP |
			      NSM_FPGA_I2C_READ_NACK}},
	};
	EXPECT_TRUE(nsm_fpga_commands_valid(p0, &byte, 1));
	nsm_fpga_command byteOnWord = byte;
	byteOnWord.station = 29;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &byteOnWord, 1));
	nsm_fpga_command readWithData = byte;
	readWithData.u.i2c.data = 1;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &readWithData, 1));
	const nsm_fpga_command captured = {
	    .op = NSM_FPGA_OP_WRITE_CAPTURED,
	    .station = 1,
	    .u = {.captured = {0, 0x71, NSM_FPGA_I2C_START | NSM_FPGA_I2C_STOP,
			       0}},
	};
	EXPECT_TRUE(nsm_fpga_commands_valid(p0, &captured, 1));
	EXPECT_TRUE(nsm_fpga_commands_valid(p0, nullptr, 0));
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, nullptr, 1));
}

TEST(fpgaTables, commandsValidAndXferEncodersHandleTransactionRows)
{
	const auto *p0 = nsm_fpga_map_p0();
	const nsm_fpga_command xfer = {
	    .op = NSM_FPGA_OP_I2C_XFER,
	    .station = 1,
	    .u = {.xfer = {0x50, {0x7e, 0x01, 0x11}, 3, 0, 0x6, 6}},
	};
	EXPECT_TRUE(nsm_fpga_commands_valid(p0, &xfer, 1));
	EXPECT_EQ(nsm_fpga_xfer_opcode(&xfer.u.xfer), 0x58);
	EXPECT_EQ(nsm_fpga_xfer_argument(0x50, xfer.u.xfer.write, 3),
		  0x11017e50u);
	EXPECT_EQ(nsm_fpga_xfer_argument(0x71, xfer.u.xfer.write, 0), 0x71u);
	nsm_fpga_command onWord = xfer;
	onWord.station = 29;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &onWord, 1));
	nsm_fpga_command tooMany = xfer;
	tooMany.u.xfer.write_count = 4;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &tooMany, 1));
	nsm_fpga_command longRead = xfer;
	longRead.u.xfer.read_count = 5;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &longRead, 1));
	nsm_fpga_command empty = xfer;
	empty.u.xfer.write_count = 0;
	empty.u.xfer.captured = 0;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &empty, 1));
	nsm_fpga_command capturedBeyond = xfer;
	capturedBeyond.u.xfer.write_count = 1;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &capturedBeyond, 1));
	nsm_fpga_command reserved = xfer;
	reserved.u.xfer.dev_addr = 0x07;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &reserved, 1));
	EXPECT_EQ(nsm_fpga_xfer_opcode(nullptr), 0);
}

TEST(fpgaTables, i2cArgumentPacksAddressDataAndFlags)
{
	const nsm_fpga_i2c_op write = {0x71, 0x02, NSM_FPGA_I2C_START};
	EXPECT_EQ(nsm_fpga_i2c_argument(&write), 0x00020271u);
	const nsm_fpga_i2c_op last = {0x50, 0,
				      NSM_FPGA_I2C_READ | NSM_FPGA_I2C_STOP |
					  NSM_FPGA_I2C_READ_NACK};
	EXPECT_EQ(nsm_fpga_i2c_argument(&last), 0x000d0050u);
	EXPECT_TRUE(nsm_fpga_i2c_argument_valid(nsm_fpga_i2c_argument(&last)));
	EXPECT_EQ(nsm_fpga_i2c_argument(nullptr), 0u);
}

TEST(fpgaTables, compareHoldsReadsBuffersAndAppliesTheOperator)
{
	const std::array<uint8_t, 4> item = {0x02, 0x00, 0x8e, 0x00};
	const std::array<uint8_t, 2> group = {0x01, 0x30};
	nsm_fpga_compare c = {.buffer = NSM_FPGA_BUF_ITEM,
			      .op = NSM_FPGA_CMP_EQ,
			      .width = 1,
			      .offset = 0,
			      .mask = 0x3,
			      .value = 2};
	EXPECT_TRUE(nsm_fpga_compare_holds(&c, item.data(), item.size(),
					   group.data(), group.size(), 0));
	c.op = NSM_FPGA_CMP_GE;
	c.value = 1;
	EXPECT_TRUE(nsm_fpga_compare_holds(&c, item.data(), item.size(),
					   group.data(), group.size(), 0));
	c.op = NSM_FPGA_CMP_LE;
	EXPECT_FALSE(nsm_fpga_compare_holds(&c, item.data(), item.size(),
					    group.data(), group.size(), 0));
	/* group buffer, two byte word */
	nsm_fpga_compare g = {.buffer = NSM_FPGA_BUF_GROUP,
			      .op = NSM_FPGA_CMP_EQ,
			      .width = 2,
			      .offset = 0,
			      .mask = 0xffff,
			      .value = 0x3001};
	EXPECT_TRUE(nsm_fpga_compare_holds(&g, item.data(), item.size(),
					   group.data(), group.size(), 0));
	/* out of range read is false, never true */
	g.offset = 1;
	EXPECT_FALSE(nsm_fpga_compare_holds(&g, item.data(), item.size(),
					    group.data(), group.size(), 0));
	/* no buffer: compare the value just read */
	const nsm_fpga_compare v = {.buffer = NSM_FPGA_BUF_NONE,
				    .op = NSM_FPGA_CMP_EQ,
				    .mask = 0x3,
				    .value = 0x1};
	EXPECT_TRUE(nsm_fpga_compare_holds(&v, nullptr, 0, nullptr, 0, 0x5));
	EXPECT_FALSE(nsm_fpga_compare_holds(&v, nullptr, 0, nullptr, 0, 0x6));
	EXPECT_FALSE(
	    nsm_fpga_compare_holds(nullptr, nullptr, 0, nullptr, 0, 0));
}

TEST(fpgaTables, compareHoldsOnTheWordTestsTheValueJustRead)
{
	nsm_fpga_compare w = {
	    .buffer = NSM_FPGA_BUF_WORD, .mask = 0x1, .value = 0x1};
	EXPECT_TRUE(nsm_fpga_compare_holds(&w, nullptr, 0, nullptr, 0, 0x75));
	EXPECT_FALSE(nsm_fpga_compare_holds(&w, nullptr, 0, nullptr, 0, 0x74));
	// Row conditions may use it, rule conditions may not
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .remember = 1,
	     .u = {.csr = {0x0500045cULL, 0}}},
	    {.op = NSM_FPGA_OP_ACK_CLEAR,
	     .station = 29,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}},
	     .run_if = w}};
	EXPECT_TRUE(nsm_fpga_commands_valid(nsm_fpga_map_p0(), rows, 2));
	nsm_fpga_rule rule = {.name = "Word",
			      .commands = rows,
			      .command_count = 2,
			      .kind = NSM_FPGA_RULE_STATE,
			      .states = testStates,
			      .state_count =
				  sizeof(testStates) / sizeof(testStates[0]),
			      .run_if = w};
	EXPECT_FALSE(nsm_fpga_rule_valid(nsm_fpga_map_p0(), &rule));
}

TEST(fpgaTables, pollSatisfiedChecksEachPollKind)
{
	EXPECT_TRUE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_NONE, 0x1, 0));
	EXPECT_TRUE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_SET, 0x3, 0x7));
	EXPECT_FALSE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_SET, 0x3, 0x1));
	EXPECT_TRUE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_ANY, 0xc, 0x4));
	EXPECT_FALSE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_ANY, 0xc, 0x3));
	EXPECT_FALSE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_ANY + 1, 0xc, 0x4));
}

TEST(fpgaTables, decodeNumericHandlesEveryFormat)
{
	const double nan = std::numeric_limits<double>::quiet_NaN();
	double value = 0.0;
	/* a whole word, scaled */
	const std::array<uint8_t, 4> word = {0x10, 0x00, 0x00, 0x00};
	nsm_fpga_numeric_spec spec = {.sensor_type = "temperature",
				      .format = NSM_FPGA_FORMAT_FIXED,
				      .buffer = NSM_FPGA_BUF_ITEM,
				      .bit_width = 32,
				      .scale = 0.5,
				      .min_value = nan,
				      .max_value = 100.0};
	ASSERT_TRUE(nsm_fpga_decode_numeric(&spec, word.data(), word.size(),
					    nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 8.0);
	/* above max is invalid */
	spec.max_value = 7.0;
	EXPECT_FALSE(nsm_fpga_decode_numeric(&spec, word.data(), word.size(),
					     nullptr, 0, &value));
	/* FIXED: 11-bit signed field at bit 2, quarter degrees (IOSSM) */
	const std::array<uint8_t, 4> iossm = {0x74, 0x02, 0x00, 0x00};
	const nsm_fpga_numeric_spec fixed = {.sensor_type = "temperature",
					     .format = NSM_FPGA_FORMAT_FIXED,
					     .buffer = NSM_FPGA_BUF_ITEM,
					     .bit_shift = 2,
					     .bit_width = 11,
					     .is_signed = true,
					     .scale = 0.25,
					     .min_value = -40.0,
					     .max_value = 125.0};
	ASSERT_TRUE(nsm_fpga_decode_numeric(&fixed, iossm.data(), iossm.size(),
					    nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 39.25);
	const std::array<uint8_t, 4> negative = {0xfc, 0x1f, 0x00, 0x00};
	ASSERT_TRUE(nsm_fpga_decode_numeric(
	    &fixed, negative.data(), negative.size(), nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, -0.25);
	/* SDM Q8.8 in the low 16 bits */
	const std::array<uint8_t, 4> sdm = {0x60, 0x1e, 0x00, 0x00};
	const nsm_fpga_numeric_spec q88 = {.sensor_type = "temperature",
					   .format = NSM_FPGA_FORMAT_FIXED,
					   .buffer = NSM_FPGA_BUF_ITEM,
					   .bit_shift = 0,
					   .bit_width = 16,
					   .is_signed = true,
					   .scale = 1.0 / 256.0,
					   .min_value = -40.0,
					   .max_value = 125.0};
	ASSERT_TRUE(nsm_fpga_decode_numeric(&q88, sdm.data(), sdm.size(),
					    nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 30.375);
	/* big endian 16-bit, CMIS style */
	const std::array<uint8_t, 2> be = {0x27, 0xc0};
	const nsm_fpga_numeric_spec s16be = {.sensor_type = "temperature",
					     .format = NSM_FPGA_FORMAT_S16BE,
					     .buffer = NSM_FPGA_BUF_ITEM,
					     .scale = 1.0 / 256.0,
					     .min_value = nan,
					     .max_value = nan};
	ASSERT_TRUE(nsm_fpga_decode_numeric(&s16be, be.data(), be.size(),
					    nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 39.75);
	/* a field that does not fit is invalid */
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&spec, word.data(), 2, nullptr, 0, &value));
	EXPECT_FALSE(nsm_fpga_decode_numeric(nullptr, word.data(), word.size(),
					     nullptr, 0, &value));
}

TEST(fpgaTables, decodeElementWalksTheStride)
{
	const double nan = std::numeric_limits<double>::quiet_NaN();
	const std::array<uint8_t, 6> lanes = {0x00, 0x10, 0x00,
					      0x20, 0x00, 0x30};
	const nsm_fpga_numeric_spec spec = {.sensor_type = "power",
					    .format = NSM_FPGA_FORMAT_U16BE,
					    .buffer = NSM_FPGA_BUF_ITEM,
					    .scale = 0.0001,
					    .min_value = nan,
					    .max_value = nan,
					    .element_count = 3,
					    .stride_bits = 16};
	double value = 0.0;
	ASSERT_TRUE(nsm_fpga_decode_element(&spec, 2, lanes.data(),
					    lanes.size(), nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 0x30 * 0.0001);
	EXPECT_FALSE(nsm_fpga_decode_element(&spec, 3, lanes.data(),
					     lanes.size(), nullptr, 0, &value));
}

TEST(fpgaTables, decodeElementWalksBitsOfAFixedField)
{
	/* lane flags: bit i of the first byte, then the second byte */
	const std::array<uint8_t, 2> flags = {0x05, 0x80};
	const nsm_fpga_numeric_spec spec = {.sensor_type = "tx_fault",
					    .format = NSM_FPGA_FORMAT_FIXED,
					    .buffer = NSM_FPGA_BUF_ITEM,
					    .bit_width = 1,
					    .scale = 1.0,
					    .min_value = 0.0,
					    .max_value = 1.0,
					    .element_count = 32,
					    .stride_bits = 1};
	double value = 0.0;
	ASSERT_TRUE(nsm_fpga_decode_element(&spec, 0, flags.data(),
					    flags.size(), nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 1.0);
	ASSERT_TRUE(nsm_fpga_decode_element(&spec, 1, flags.data(),
					    flags.size(), nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 0.0);
	ASSERT_TRUE(nsm_fpga_decode_element(&spec, 15, flags.data(),
					    flags.size(), nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 1.0);
	/* the vector ends where the capture does */
	EXPECT_FALSE(nsm_fpga_decode_element(&spec, 16, flags.data(),
					     flags.size(), nullptr, 0, &value));
	/* a bit stride needs a FIXED field */
	nsm_fpga_numeric_spec bytes = spec;
	bytes.format = NSM_FPGA_FORMAT_U16BE;
	EXPECT_FALSE(nsm_fpga_decode_element(&bytes, 0, flags.data(),
					     flags.size(), nullptr, 0, &value));
}

TEST(fpgaTables, commandsValidRequiresCheckRowsToReadABuffer)
{
	const auto *p0 = nsm_fpga_map_p0();
	const nsm_fpga_command check[] = {
	    {.op = NSM_FPGA_OP_CHECK,
	     .station = 1,
	     .expect = {.buffer = NSM_FPGA_BUF_GROUP,
			.op = NSM_FPGA_CMP_EQ,
			.width = 1,
			.offset = 1,
			.mask = 0x10,
			.value = 0},
	     .on_expect_fail = NSM_FPGA_END_GROUP,
	     .group_state = NSM_FPGA_STATE_ABSENT}};
	EXPECT_TRUE(nsm_fpga_commands_valid(p0, check, 1));
	/* the state an ended group publishes must exist */
	nsm_fpga_command odd = check[0];
	odd.group_state = 9;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &odd, 1));
	/* without a buffer a check has nothing to compare */
	nsm_fpga_command bare = check[0];
	bare.expect.buffer = NSM_FPGA_BUF_NONE;
	EXPECT_FALSE(nsm_fpga_commands_valid(p0, &bare, 1));
}

TEST(fpgaTables, ruleValidChecksRunIfAndArraySpec)
{
	const auto *p0 = nsm_fpga_map_p0();
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_BYTE,
	     .station = 1,
	     .capture = NSM_FPGA_BUF_ITEM,
	     .u = {.i2c = {0x50, 0,
			   NSM_FPGA_I2C_READ | NSM_FPGA_I2C_START |
			       NSM_FPGA_I2C_STOP | NSM_FPGA_I2C_READ_NACK}}}};
	nsm_fpga_rule array = {.name = "Lanes",
			       .commands = rows,
			       .command_count = 1,
			       .kind = NSM_FPGA_RULE_ARRAY,
			       .numeric = {.format = NSM_FPGA_FORMAT_FIXED,
					   .buffer = NSM_FPGA_BUF_ITEM,
					   .bit_width = 1,
					   .scale = 1.0,
					   .element_count = 8,
					   .stride_bits = 1,
					   .vector = NSM_FPGA_VECTOR_TX_FAULT},
			       .run_if = {.buffer = NSM_FPGA_BUF_GROUP,
					  .op = NSM_FPGA_CMP_EQ,
					  .width = 1,
					  .offset = 5,
					  .mask = 0x0e,
					  .value = 0x06}};
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, &array));
	array.numeric.element_count = 0;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &array));
	array.numeric.element_count = 8;
	array.numeric.format = NSM_FPGA_FORMAT_U16BE;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &array));
	array.numeric.format = NSM_FPGA_FORMAT_FIXED;
	/* run_if reads the group buffer only */
	array.run_if.buffer = NSM_FPGA_BUF_ITEM;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &array));
	array.run_if.buffer = NSM_FPGA_BUF_GROUP;
	/* an array names the vector property it publishes */
	array.numeric.vector = NSM_FPGA_VECTOR_NONE;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &array));
}

TEST(fpgaTables, decodeStateMapsTheWord)
{
	const auto *p0 = nsm_fpga_map_p0();
	/* a module state code under bits 1 to 3 */
	static const nsm_fpga_state_entry moduleStates[] = {
	    {0x0e, 0x06, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK},
	    {0x0e, 0x04, NSM_FPGA_STATE_STARTING, NSM_FPGA_HEALTH_OK},
	    {0x0e, 0x02, NSM_FPGA_STATE_STANDBY_OFFLINE, NSM_FPGA_HEALTH_OK},
	    {0x0e, 0x0a, NSM_FPGA_STATE_FAULT, NSM_FPGA_HEALTH_CRITICAL},
	    {0, 0, NSM_FPGA_STATE_UNAVAILABLE_OFFLINE,
	     NSM_FPGA_HEALTH_WARNING}};
	const nsm_fpga_rule module = {.name = "Module",
				      .kind = NSM_FPGA_RULE_STATE,
				      .numeric = {.buffer = NSM_FPGA_BUF_GROUP},
				      .states = moduleStates,
				      .state_count = 5};
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, &module));
	uint8_t state = 0;
	uint8_t health = 0;
	EXPECT_TRUE(nsm_fpga_decode_state(&module, 0x06, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_ENABLED);
	EXPECT_EQ(health, NSM_FPGA_HEALTH_OK);
	EXPECT_TRUE(nsm_fpga_decode_state(&module, 0x0a, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_FAULT);
	EXPECT_EQ(health, NSM_FPGA_HEALTH_CRITICAL);
	EXPECT_TRUE(nsm_fpga_decode_state(&module, 0x02, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_STANDBY_OFFLINE);
	/* an unknown code takes the map's default line */
	EXPECT_TRUE(nsm_fpga_decode_state(&module, 0x0e, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_UNAVAILABLE_OFFLINE);
	EXPECT_EQ(health, NSM_FPGA_HEALTH_WARNING);
	/* a map without a default line matches nothing for other words */
	static const nsm_fpga_state_entry ready[] = {
	    {0x8, 0x8, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK}};
	const nsm_fpga_rule link = {.name = "Link",
				    .commands = &wordRead,
				    .command_count = 1,
				    .kind = NSM_FPGA_RULE_STATE,
				    .states = ready,
				    .state_count = 1};
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, &link));
	EXPECT_TRUE(nsm_fpga_decode_state(&link, 0x18, &state, &health));
	EXPECT_FALSE(nsm_fpga_decode_state(&link, 0x10, &state, &health));
	/* a state rule needs a map, no other kind may carry one */
	nsm_fpga_rule bare = link;
	bare.state_count = 0;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &bare));
	nsm_fpga_rule numeric = link;
	numeric.kind = NSM_FPGA_RULE_NUMERIC;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &numeric));
	/* a bad state or health code is refused */
	static const nsm_fpga_state_entry odd[] = {
	    {0, 0, 9, NSM_FPGA_HEALTH_OK}};
	nsm_fpga_rule broken = link;
	broken.states = odd;
	broken.state_count = 1;
	EXPECT_FALSE(nsm_fpga_rule_valid(p0, &broken));
}

TEST(fpgaTables, decodeIdentityRendersHexAndText)
{
	const std::array<uint8_t, 12> bytes = {
	    0x70, 0xb6, 0xc6, 0x45, 0x99, 0xa3, 0x7b, 0x20, 'A', 'B', ' ', ' '};
	char text[NSM_FPGA_IDENTITY_TEXT_MAX];
	const nsm_fpga_identity_field le = {.asset = NSM_FPGA_ASSET_SERIAL,
					    .format = NSM_FPGA_IDENTITY_HEX_LE,
					    .buffer = NSM_FPGA_BUF_ITEM,
					    .offset = 0,
					    .length = 8};
	ASSERT_TRUE(nsm_fpga_decode_identity(&le, bytes.data(), bytes.size(),
					     nullptr, 0, text, sizeof(text)));
	EXPECT_STREQ(text, "0x207ba39945c6b670");
	const nsm_fpga_identity_field be = {.asset = NSM_FPGA_ASSET_SERIAL,
					    .format = NSM_FPGA_IDENTITY_HEX_BE,
					    .buffer = NSM_FPGA_BUF_ITEM,
					    .offset = 4,
					    .length = 4};
	ASSERT_TRUE(nsm_fpga_decode_identity(&be, bytes.data(), bytes.size(),
					     nullptr, 0, text, sizeof(text)));
	EXPECT_STREQ(text, "0x99a37b20");
	const nsm_fpga_identity_field ascii = {.asset = NSM_FPGA_ASSET_PART,
					       .format =
						   NSM_FPGA_IDENTITY_ASCII,
					       .buffer = NSM_FPGA_BUF_ITEM,
					       .offset = 8,
					       .length = 4};
	ASSERT_TRUE(nsm_fpga_decode_identity(&ascii, bytes.data(), bytes.size(),
					     nullptr, 0, text, sizeof(text)));
	EXPECT_STREQ(text, "AB");
	/* bytes beyond the buffer and a text that does not fit are refused */
	EXPECT_FALSE(nsm_fpga_decode_identity(&le, bytes.data(), 7, nullptr, 0,
					      text, sizeof(text)));
	EXPECT_FALSE(nsm_fpga_decode_identity(&be, bytes.data(), bytes.size(),
					      nullptr, 0, text, 10));
	/* a rule without fields publishes the default serial number */
	const nsm_fpga_rule rule = {.name = "Id",
				    .kind = NSM_FPGA_RULE_IDENTITY};
	size_t count = 0;
	const auto *fields = nsm_fpga_rule_identity(&rule, &count);
	ASSERT_NE(fields, nullptr);
	EXPECT_EQ(count, 1u);
	EXPECT_EQ(fields[0].asset, NSM_FPGA_ASSET_SERIAL);
	EXPECT_EQ(fields[0].length, 8);
	/* an identity field with a bad format makes the rule invalid */
	const nsm_fpga_identity_field bad = {.asset = NSM_FPGA_ASSET_SERIAL,
					     .format = 9,
					     .buffer = NSM_FPGA_BUF_ITEM,
					     .offset = 0,
					     .length = 4};
	const nsm_fpga_command read = {.op = NSM_FPGA_OP_READ32,
				       .station = 29,
				       .capture = NSM_FPGA_BUF_ITEM,
				       .u = {.csr = {0x05000400ULL, 0}}};
	const nsm_fpga_rule invalid = {.name = "Bad",
				       .commands = &read,
				       .command_count = 1,
				       .kind = NSM_FPGA_RULE_IDENTITY,
				       .identity = &bad,
				       .identity_count = 1};
	EXPECT_FALSE(nsm_fpga_rule_valid(nsm_fpga_map_p0(), &invalid));
}

TEST(fpgaTables, stationBySelectorGivesSdmTheStrictKind)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *sdm = nsm_fpga_station_by_selector(p0, 37);
	ASSERT_NE(sdm, nullptr);
	EXPECT_EQ(sdm->kind, NSM_FPGA_STATION_SDM);
}

TEST(fpgaTables, sdmWordLegalMirrorsThePolicy)
{
	/* reads only at offsets 8, 20, 24, 32 */
	EXPECT_TRUE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_READ32, 8, 0, 0xf));
	EXPECT_TRUE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_READ32, 20, 0, 0xf));
	EXPECT_TRUE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_READ32, 24, 0, 0xf));
	EXPECT_TRUE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_READ32, 32, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_READ32, 12, 0, 0xf));
	/* temperature and voltage take the two-word form: header then argument.
	   The client field (bits 24 to 26) is ignored by the policy, so the
	   sent form with client 7 and the bare form both pass. */
	EXPECT_TRUE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x07001019u, 0xf));
	EXPECT_TRUE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x07001018u, 0xf));
	EXPECT_TRUE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x00001019u, 0xf));
	EXPECT_TRUE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x00001018u, 0xf));
	EXPECT_TRUE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 0x00000001u, 0xf));
	/* the argument-less codes keep the single offset-4 header write */
	EXPECT_TRUE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 0x07000010u, 0xf));
	EXPECT_TRUE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 0x07000012u, 0xf));
	EXPECT_TRUE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 0x07000013u, 0xf));
	/* the argument-less temperature header at offset 4 faults: rejected */
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 0x07000019u, 0xf));
	/* a temperature header at the wrong offset, or wrong mask, is rejected
	 */
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x00001010u, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 0x00000001u, 0x3));
}

TEST(fpgaTables, wordLegalGuardsStationAndWindow)
{
	const auto *p0 = nsm_fpga_map_p0();
	/* SDM station: the old argument-less temperature write is rejected */
	EXPECT_FALSE(nsm_fpga_word_legal(p0, 37, NSM_FPGA_OP_WRITE32, 4,
					 0x07000019u, 0xf));
	/* the two-word temperature form is allowed */
	EXPECT_TRUE(nsm_fpga_word_legal(p0, 37, NSM_FPGA_OP_WRITE32, 0,
					0x00001019u, 0xf));
	EXPECT_TRUE(nsm_fpga_word_legal(p0, 37, NSM_FPGA_OP_WRITE32, 4,
					0x00000001u, 0xf));
	EXPECT_TRUE(
	    nsm_fpga_word_legal(p0, 37, NSM_FPGA_OP_READ32, 24, 0, 0xf));
	/* plain word station: alignment and window bounds enforced */
	EXPECT_TRUE(nsm_fpga_word_legal(p0, 29, NSM_FPGA_OP_READ32,
					0x05000400ULL, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_word_legal(p0, 29, NSM_FPGA_OP_READ32,
					 0x05000401ULL, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_word_legal(p0, 29, NSM_FPGA_OP_READ32,
					 0x05000900ULL, 0, 0xf));
	/* an unknown station is never legal */
	EXPECT_FALSE(
	    nsm_fpga_word_legal(p0, 38, NSM_FPGA_OP_READ32, 8, 0, 0xf));
}

namespace
{
/* The numeric decode model, computed independently of the library */
bool modelField(const nsm_fpga_numeric_spec &spec, const uint8_t *bytes,
		size_t length, size_t offset, unsigned shift, double *value)
{
	if (spec.buffer != NSM_FPGA_BUF_ITEM &&
	    spec.buffer != NSM_FPGA_BUF_GROUP) {
		return false;
	}
	if (spec.format == NSM_FPGA_FORMAT_FIXED &&
	    (spec.bit_width == 0 || spec.bit_width > 32 ||
	     shift + spec.bit_width > 32)) {
		return false;
	}
	const size_t width = spec.format == NSM_FPGA_FORMAT_FIXED
				 ? (shift + spec.bit_width + 7) / 8
				 : 2;
	if (offset > length || width > length - offset) {
		return false;
	}
	uint64_t raw = 0;
	for (size_t i = 0; i < width; ++i) {
		raw |= (uint64_t)bytes[offset + i] << (8 * i);
	}
	double decoded = 0.0;
	if (spec.format == NSM_FPGA_FORMAT_FIXED) {
		uint64_t field =
		    (raw >> shift) & (((uint64_t)1 << spec.bit_width) - 1);
		if (spec.is_signed &&
		    (field & ((uint64_t)1 << (spec.bit_width - 1))) != 0) {
			field -= (uint64_t)1 << spec.bit_width;
		}
		decoded =
		    spec.is_signed ? (double)(int64_t)field : (double)field;
	} else {
		const uint64_t swapped =
		    ((raw & 0xff) << 8) | ((raw >> 8) & 0xff);
		decoded = spec.format == NSM_FPGA_FORMAT_S16BE
			      ? (double)(int16_t)swapped
			      : (double)swapped;
	}
	decoded = decoded * spec.scale + spec.bias;
	if (!std::isfinite(decoded) ||
	    (!std::isnan(spec.min_value) && decoded < spec.min_value) ||
	    (!std::isnan(spec.max_value) && decoded > spec.max_value)) {
		return false;
	}
	*value = decoded;
	return true;
}

/* A deterministic byte pattern varied by seed */
std::vector<uint8_t> patternBytes(size_t size, uint8_t seed)
{
	std::vector<uint8_t> bytes(size);
	for (size_t i = 0; i < size; ++i) {
		bytes[i] = (uint8_t)(seed + i * 7);
	}
	return bytes;
}
} // namespace

TEST(fpgaTables, decodeNumericMatchesTheFormulaOnEveryPart)
{
	const auto *p0 = nsm_fpga_map_p0();
	const uint8_t seeds[] = {0x00, 0xff, 0x35};
	for (size_t p = 0; p < p0->part_count; ++p) {
		const auto *part = p0->parts[p];
		for (size_t i = 0; i < part->rule_count; ++i) {
			const auto &rule = part->rules[i];
			if (rule.kind != NSM_FPGA_RULE_NUMERIC) {
				continue;
			}
			const auto &spec = rule.numeric;
			EXPECT_NE(spec.sensor_type, nullptr) << rule.name;
			for (const uint8_t seed : seeds) {
				auto bytes =
				    patternBytes(spec.offset + 8, seed);
				double expected = 0.0;
				const bool held = modelField(
				    spec, bytes.data(), bytes.size(),
				    spec.offset, spec.bit_shift, &expected);
				double value = 0.0;
				const bool decoded = nsm_fpga_decode_numeric(
				    &spec, bytes.data(), bytes.size(),
				    bytes.data(), bytes.size(), &value);
				EXPECT_EQ(decoded, held) << rule.name;
				if (decoded && held) {
					EXPECT_DOUBLE_EQ(value, expected)
					    << rule.name;
				}
			}
		}
	}
}

TEST(fpgaTables, decodeStateFindsTheFirstMatchOnEveryPart)
{
	const auto *p0 = nsm_fpga_map_p0();
	for (size_t p = 0; p < p0->part_count; ++p) {
		const auto *part = p0->parts[p];
		for (size_t i = 0; i < part->rule_count; ++i) {
			const auto &rule = part->rules[i];
			if (rule.kind != NSM_FPGA_RULE_STATE) {
				continue;
			}
			for (size_t e = 0; e < rule.state_count; ++e) {
				const uint32_t word = rule.states[e].value;
				const nsm_fpga_state_entry *first = nullptr;
				for (size_t j = 0; j < rule.state_count; ++j) {
					if ((word & rule.states[j].mask) ==
					    rule.states[j].value) {
						first = &rule.states[j];
						break;
					}
				}
				uint8_t state = 0xff;
				uint8_t health = 0xff;
				const bool decoded = nsm_fpga_decode_state(
				    &rule, word, &state, &health);
				EXPECT_EQ(decoded, first != nullptr)
				    << rule.name;
				if (decoded && first != nullptr) {
					EXPECT_EQ(state, first->state)
					    << rule.name;
					EXPECT_EQ(health, first->health)
					    << rule.name;
				}
			}
		}
	}
}

TEST(fpgaTables, decodeElementWalksEveryArraySpecOfEveryPart)
{
	const auto *p0 = nsm_fpga_map_p0();
	for (size_t p = 0; p < p0->part_count; ++p) {
		const auto *part = p0->parts[p];
		for (size_t i = 0; i < part->rule_count; ++i) {
			const auto &rule = part->rules[i];
			if (rule.kind != NSM_FPGA_RULE_ARRAY) {
				continue;
			}
			const auto &spec = rule.numeric;
			const size_t bits =
			    spec.bit_shift +
			    spec.element_count * spec.stride_bits;
			auto bytes =
			    patternBytes(spec.offset + bits / 8 + 8, 0x5a);
			for (size_t e = 0; e < spec.element_count; ++e) {
				const size_t bit =
				    spec.bit_shift + e * spec.stride_bits;
				double expected = 0.0;
				const bool held =
				    modelField(spec, bytes.data(), bytes.size(),
					       spec.offset + bit / 8,
					       (unsigned)(bit % 8), &expected);
				double value = 0.0;
				const bool decoded = nsm_fpga_decode_element(
				    &spec, e, bytes.data(), bytes.size(),
				    bytes.data(), bytes.size(), &value);
				EXPECT_EQ(decoded, held) << rule.name;
				if (decoded && held) {
					EXPECT_DOUBLE_EQ(value, expected)
					    << rule.name;
				}
			}
			double value = 0.0;
			EXPECT_FALSE(nsm_fpga_decode_element(
			    &spec, spec.element_count, bytes.data(),
			    bytes.size(), bytes.data(), bytes.size(), &value))
			    << rule.name;
		}
	}
}

TEST(fpgaTables, decodeIdentityRendersEveryFieldOfEveryPart)
{
	const auto *p0 = nsm_fpga_map_p0();
	for (size_t p = 0; p < p0->part_count; ++p) {
		const auto *part = p0->parts[p];
		for (size_t i = 0; i < part->rule_count; ++i) {
			const auto &rule = part->rules[i];
			if (rule.kind != NSM_FPGA_RULE_IDENTITY) {
				continue;
			}
			size_t count = 0;
			const auto *fields =
			    nsm_fpga_rule_identity(&rule, &count);
			ASSERT_NE(fields, nullptr);
			for (size_t f = 0; f < count; ++f) {
				const auto &field = fields[f];
				std::vector<uint8_t> bytes(field.offset +
							   field.length);
				for (size_t b = 0; b < bytes.size(); ++b) {
					bytes[b] = (uint8_t)('A' + (b % 26));
				}
				const uint8_t *data =
				    bytes.data() + field.offset;
				std::string expected;
				if (field.format == NSM_FPGA_IDENTITY_ASCII) {
					expected.assign((const char *)data,
							field.length);
				} else {
					static const char digits[] =
					    "0123456789abcdef";
					expected = "0x";
					for (size_t b = 0; b < field.length;
					     ++b) {
						const uint8_t byte =
						    field.format ==
							    NSM_FPGA_IDENTITY_HEX_LE
							? data[field.length -
							       1 - b]
							: data[b];
						expected.push_back(
						    digits[byte >> 4]);
						expected.push_back(
						    digits[byte & 0xf]);
					}
				}
				char out[NSM_FPGA_IDENTITY_TEXT_MAX] = {};
				EXPECT_TRUE(nsm_fpga_decode_identity(
				    &field, bytes.data(), bytes.size(),
				    bytes.data(), bytes.size(), out,
				    sizeof(out)))
				    << rule.name;
				EXPECT_STREQ(out, expected.c_str());
			}
		}
	}
}

TEST(fpgaTables, i2cArgumentAndXferEncodersPackEveryWireRow)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto checkRows = [](const nsm_fpga_command *rows, size_t count,
				  const char *name) {
		for (size_t c = 0; c < count; ++c) {
			const auto &cmd = rows[c];
			if (cmd.op == NSM_FPGA_OP_BYTE) {
				const auto &op = cmd.u.i2c;
				const uint32_t argument =
				    (uint32_t)(op.dev_addr & 0x7f) |
				    ((uint32_t)op.data << 8) |
				    ((uint32_t)(op.flags & 0xf) << 16);
				EXPECT_EQ(nsm_fpga_i2c_argument(&op), argument)
				    << name;
				EXPECT_EQ(op.dev_addr & 0x80, 0) << name;
			} else if (cmd.op == NSM_FPGA_OP_I2C_XFER) {
				const auto &op = cmd.u.xfer;
				const uint8_t opcode =
				    (uint8_t)(0x40 |
					      ((op.write_count & 0x3) << 3) |
					      (op.read_count & 0x7));
				EXPECT_EQ(nsm_fpga_xfer_opcode(&op), opcode)
				    << name;
				uint32_t argument = op.dev_addr & 0x7fu;
				for (size_t w = 0; w < op.write_count && w < 3;
				     ++w) {
					argument |= (uint32_t)op.write[w]
						    << (8 * (w + 1));
				}
				EXPECT_EQ(
				    nsm_fpga_xfer_argument(
					op.dev_addr, op.write, op.write_count),
				    argument)
				    << name;
			}
		}
	};
	for (size_t p = 0; p < p0->part_count; ++p) {
		const auto *part = p0->parts[p];
		for (size_t g = 0; g < part->group_count; ++g) {
			const auto &group = part->groups[g];
			checkRows(group.setup, group.setup_count, group.name);
			checkRows(group.cleanup, group.cleanup_count,
				  group.name);
		}
		for (size_t i = 0; i < part->rule_count; ++i) {
			const auto &rule = part->rules[i];
			checkRows(rule.commands, rule.command_count, rule.name);
			checkRows(rule.cleanup, rule.cleanup_count, rule.name);
		}
	}
}
