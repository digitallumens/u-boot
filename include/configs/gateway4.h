/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 *
 * Configuration settings for the Freescale i.MX6UL 14x14 EVK board.
 */
#ifndef __MX6ULLEVK_CONFIG_H
#define __MX6ULLEVK_CONFIG_H

#include <asm/arch/imx-regs.h>
#include <linux/sizes.h>
#include <linux/stringify.h>
#include "mx6_common.h"
#include <asm/mach-imx/gpio.h>

#define PHYS_SDRAM_SIZE	SZ_512M

#define CFG_MXC_UART_BASE		UART1_BASE

/* MMC Configs */
#ifdef CONFIG_FSL_USDHC
#define CFG_SYS_FSL_ESDHC_ADDR	USDHC2_BASE_ADDR
#define CFG_SYS_FSL_USDHC_NUM	2
#endif

#define CFG_EXTRA_ENV_SETTINGS \
	"boot_order=ab\0" \
	"boot_a_left=3\0" \
	"boot_b_left=3\0" \
	"console=ttymxc0\0" \
	"fastboot_buffer=0x8a000000\0"

#define CONFIG_ENV_FLAGS_LIST_DEFAULT "boot_order:xw,boot_a_left:dw,boot_b_left:dw"
#define CFG_ENV_FLAGS_LIST_STATIC "boot_order:xw,boot_a_left:dw,boot_b_left:dw"

/* Miscellaneous configurable options */

/* Physical Memory Map */
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR

#define CFG_SYS_SDRAM_BASE		PHYS_SDRAM
#define CFG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CFG_SYS_INIT_RAM_SIZE	IRAM_SIZE

/* environment organization */

#ifdef CONFIG_CMD_NET
#define CFG_FEC_ENET_DEV		1
#endif

#endif
