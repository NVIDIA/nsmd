// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga-maps.h"

#include <string.h>

const struct nsm_fpga_map_identity
    nsm_fpga_map_identity_table[NSM_FPGA_PLATFORM_COUNT] = {
	[NSM_FPGA_PLATFORM_LP30] = {"LP30",
				    "P0 ibc1 schema 13",
				    NSM_FPGA_P0_CONFIGURATION_TYPE,
				    NSM_FPGA_P0_IDENTITY_ADDRESS,
				    NSM_FPGA_P0_IDENTITY_DWORDS,
				    {0x30, 0x72, 0xef, 0x68, 0x28, 0xb1, 0x8c,
				     0x23, 0xac, 0x87, 0x5c, 0x03, 0x0d, 0x8d,
				     0xda, 0x9c, 0x29, 0x4f, 0x15, 0xa2, 0xf0,
				     0xd6, 0x27, 0xc9, 0xe5, 0x68, 0x2a, 0xc1,
				     0xc2, 0x7a, 0xf5, 0x0f}},
};

const struct nsm_fpga_map_identity *
nsm_fpga_map_identity_by_platform(const char *platform)
{
	if (platform == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < NSM_FPGA_PLATFORM_COUNT; ++i) {
		if (strcmp(nsm_fpga_map_identity_table[i].platform, platform) ==
		    0) {
			return &nsm_fpga_map_identity_table[i];
		}
	}
	return NULL;
}
