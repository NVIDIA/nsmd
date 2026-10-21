// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
/* clang-format off */
/* Generated from fpga-rules-lp30.rows by gen-fpga-rules.py, do not edit. */
#include "fpga-tables.h"

#include <math.h>

static const struct nsm_fpga_station stations_p0[] = {
    {0, NSM_FPGA_STATION_I2C_BYTE, 0, 0},
    {1, NSM_FPGA_STATION_I2C_BYTE, 0, 0},
    {2, NSM_FPGA_STATION_I2C_BYTE, 0, 0},
    {3, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {4, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {5, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {6, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {7, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {8, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {9, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {10, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {11, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {12, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {13, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {14, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {15, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {16, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {17, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {18, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {19, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {20, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {21, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {22, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {23, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {24, NSM_FPGA_STATION_WORD, 0, 0xffff},
    {25, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {26, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {27, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {28, NSM_FPGA_STATION_WORD, 0, 0xfffff},
    {29, NSM_FPGA_STATION_WORD, 0x5000200, 0x500080f},
    {30, NSM_FPGA_STATION_WORD, 0x5000200, 0x500080f},
    {31, NSM_FPGA_STATION_WORD, 0x5000200, 0x500080f},
    {32, NSM_FPGA_STATION_WORD, 0x5000200, 0x500080f},
    {33, NSM_FPGA_STATION_WORD, 0x5000200, 0x500080f},
    {34, NSM_FPGA_STATION_WORD, 0x5000200, 0x500080f},
    {35, NSM_FPGA_STATION_WORD, 0x5000200, 0x500080f},
    {36, NSM_FPGA_STATION_WORD, 0x5000200, 0x500080f},
    {37, NSM_FPGA_STATION_SDM, 0, 0x23},
};

static const struct nsm_fpga_map map_p0 = {
    .window = {2, 0x1000, 0x1020, 0x1040, 0x1060, 0x10a0, 0x10e0},
    .stations = stations_p0,
    .station_count = sizeof(stations_p0) / sizeof(stations_p0[0]),
};

const struct nsm_fpga_map *nsm_fpga_map_p0(void)
{
	return &map_p0;
}
