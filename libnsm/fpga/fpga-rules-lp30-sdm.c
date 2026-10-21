// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
/* clang-format off */
/* Generated from fpga-rules-lp30-sdm.rows by gen-fpga-rules.py, do not edit. */
#include "fpga-tables.h"

#include <math.h>

static const struct nsm_fpga_command rows_Chip_Id[] = {
    {.op = NSM_FPGA_OP_POP, .station = 37, .clean = NSM_FPGA_UNLESS_CLEAN, .poll_mask = 0xfffffffc, .u = {.pop = {0x18, 0x14}}, .limit = 8},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xffffffff, .u = {.csr = {8, 0}}},
    {.op = NSM_FPGA_OP_WRITE32, .station = 37, .u = {.csr = {4, 0x7000012}}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xfffffffc, .u = {.csr = {0x18, 0}}, .expect = {.mask = 3, .value = 1}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .u = {.csr = {0x14, 0}}, .expect = {.mask = 0xffffffff, .value = 0x7002000}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xfffffffc, .u = {.csr = {0x18, 0}}, .expect = {.mask = 3, .value = 0}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .capture = NSM_FPGA_BUF_ITEM, .u = {.csr = {0x14, 0}}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xfffffffc, .u = {.csr = {0x18, 0}}, .expect = {.mask = 3, .value = 2}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .capture = NSM_FPGA_BUF_ITEM, .u = {.csr = {0x14, 0}}},
};

static const struct nsm_fpga_command end_Chip_Id[] = {
    {.op = NSM_FPGA_OP_POP, .station = 37, .clean = NSM_FPGA_LEAVES_CLEAN, .poll_mask = 0xfffffffc, .u = {.pop = {0x18, 0x14}}, .limit = 8},
};

static const struct nsm_fpga_command rows_SDM_Temp_0[] = {
    {.op = NSM_FPGA_OP_POP, .station = 37, .clean = NSM_FPGA_UNLESS_CLEAN, .poll_mask = 0xfffffffc, .u = {.pop = {0x18, 0x14}}, .limit = 8},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xffffffff, .u = {.csr = {8, 0}}},
    {.op = NSM_FPGA_OP_WRITE32, .station = 37, .u = {.csr = {0, 0x7001019}}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xffffffff, .u = {.csr = {8, 0}}},
    {.op = NSM_FPGA_OP_WRITE32, .station = 37, .u = {.csr = {4, 1}}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xfffffffc, .u = {.csr = {0x18, 0}}, .expect = {.mask = 3, .value = 1}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .u = {.csr = {0x14, 0}}, .expect = {.mask = 0xffffffff, .value = 0x7001000}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xfffffffc, .u = {.csr = {0x18, 0}}, .expect = {.mask = 3, .value = 2}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .capture = NSM_FPGA_BUF_ITEM, .u = {.csr = {0x14, 0}}},
};

static const struct nsm_fpga_command rows_Idcode[] = {
    {.op = NSM_FPGA_OP_POP, .station = 37, .clean = NSM_FPGA_UNLESS_CLEAN, .poll_mask = 0xfffffffc, .u = {.pop = {0x18, 0x14}}, .limit = 8},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xffffffff, .u = {.csr = {8, 0}}},
    {.op = NSM_FPGA_OP_WRITE32, .station = 37, .u = {.csr = {4, 0x7000010}}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xfffffffc, .u = {.csr = {0x18, 0}}, .expect = {.mask = 3, .value = 1}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .u = {.csr = {0x14, 0}}, .expect = {.mask = 0xffffffff, .value = 0x7001000}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .poll = NSM_FPGA_POLL_ANY, .poll_mask = 0xfffffffc, .u = {.csr = {0x18, 0}}, .expect = {.mask = 3, .value = 2}},
    {.op = NSM_FPGA_OP_READ32, .station = 37, .capture = NSM_FPGA_BUF_ITEM, .u = {.csr = {0x14, 0}}},
};

static const struct nsm_fpga_identity_field fields_Idcode[] = {
    {.asset = NSM_FPGA_ASSET_MODEL, .format = NSM_FPGA_IDENTITY_HEX_LE, .buffer = NSM_FPGA_BUF_ITEM, .offset = 0, .length = 4},
};

static const struct nsm_fpga_rule rules_sdm[] = {
    {.name = "Chip_Id",
     .commands = rows_Chip_Id,
     .command_count = sizeof(rows_Chip_Id) / sizeof(rows_Chip_Id[0]),
     .cleanup = end_Chip_Id,
     .cleanup_count = sizeof(end_Chip_Id) / sizeof(end_Chip_Id[0]),
     .kind = NSM_FPGA_RULE_IDENTITY,
     .group = "FPGA_Identity"},
    {.name = "SDM_Temp_0",
     .commands = rows_SDM_Temp_0,
     .command_count = sizeof(rows_SDM_Temp_0) / sizeof(rows_SDM_Temp_0[0]),
     .cleanup = end_Chip_Id,
     .cleanup_count = sizeof(end_Chip_Id) / sizeof(end_Chip_Id[0]),
     .kind = NSM_FPGA_RULE_NUMERIC,
     .numeric = {.sensor_type = "temperature", .format = NSM_FPGA_FORMAT_FIXED, .buffer = NSM_FPGA_BUF_ITEM, .bit_width = 16, .is_signed = true, .scale = 0.00390625, .min_value = -40.0, .max_value = 125.0},
     .group = "FPGA_Sdm"},
    {.name = "Idcode",
     .commands = rows_Idcode,
     .command_count = sizeof(rows_Idcode) / sizeof(rows_Idcode[0]),
     .cleanup = end_Chip_Id,
     .cleanup_count = sizeof(end_Chip_Id) / sizeof(end_Chip_Id[0]),
     .kind = NSM_FPGA_RULE_IDENTITY,
     .group = "FPGA_Identity",
     .identity = fields_Idcode,
     .identity_count = sizeof(fields_Idcode) / sizeof(fields_Idcode[0])},
};

extern const struct nsm_fpga_part nsm_fpga_part_sdm;
const struct nsm_fpga_part nsm_fpga_part_sdm = {
    .rules = rules_sdm,
    .rule_count = sizeof(rules_sdm) / sizeof(rules_sdm[0]),
};
