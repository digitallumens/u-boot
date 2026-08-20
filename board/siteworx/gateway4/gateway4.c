// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 */

#include <init.h>
#include <asm/arch/clock.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/io.h>
#include <config.h>
#include <env.h>
#include <fsl_esdhc_imx.h>
#include <linux/sizes.h>
#include <mmc.h>
#include <miiphy.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	gd->ram_size = imx_ddr_size();
	//gd->ram_size = ((ulong)CONFIG_DDR_MB * SZ_1M);

	return 0;
}

int board_mmc_get_env_dev(int devno)
{
	return devno;
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno;
}

int board_early_init_f(void)
{
	return 0;
}

#ifdef CONFIG_FEC_MXC
static int setup_fec(int fec_id)
{
	struct iomuxc *const iomuxc_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	int ret;

	if (fec_id == 0) {
		/*
		 * Use 50MHz anatop loopback REF_CLK1 for ENET1,
		 * clear gpr1[13], set gpr1[17].
		 */
		clrsetbits_le32(&iomuxc_regs->gpr[1], IOMUX_GPR1_FEC1_MASK,
				IOMUX_GPR1_FEC1_CLOCK_MUX1_SEL_MASK);
	} else {
		/*
		 * Use 50MHz anatop loopback REF_CLK2 for ENET2,
		 * clear gpr1[14], set gpr1[18].
		 */
		clrsetbits_le32(&iomuxc_regs->gpr[1], IOMUX_GPR1_FEC2_MASK,
				IOMUX_GPR1_FEC2_CLOCK_MUX1_SEL_MASK);
	}

	ret = enable_fec_anatop_clock(fec_id, ENET_50MHZ);
	if (ret)
		return ret;

	enable_enet_clk(1);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x8190);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}
#endif

int board_init(void)
{
	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

#ifdef	CONFIG_FEC_MXC
	setup_fec(CFG_FEC_ENET_DEV);
#endif

	return 0;
}

#ifdef CONFIG_CMD_BMODE
static const struct boot_mode board_boot_modes[] = {
	/* 4 bit bus width */
	{"sd1", MAKE_CFGVAL(0x42, 0x20, 0x00, 0x00)},
	{"sd2", MAKE_CFGVAL(0x40, 0x28, 0x00, 0x00)},
	{"qspi1", MAKE_CFGVAL(0x10, 0x00, 0x00, 0x00)},
	{NULL,	 0},
};
#endif

int board_late_init(void)
{
#ifdef CONFIG_CMD_BMODE
	add_board_boot_modes(board_boot_modes);
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "GATEWAY4");
	env_set("board_rev", "A");
#endif

	return 0;
}

int checkboard(void)
{
	puts("Board: Gateway 4\n");

	return 0;
}

static int do_gateway_boot(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	char bootargs_buf[256];

	char *upgrade_avail = env_get("boot_upgrade_available");
    char *boot_part = env_get("boot_part");
    char *boot_failed = env_get("boot_failed");
	char *mtdparts = env_get("mtdparts");
	bool boot_part_a = false;
	if (!boot_part) {
		//printf("No boot_part flag found, defaulting to A\n");
		boot_part = "A"; // Default to partition A if not set
	}
	if (!mtdparts) {
		mtdparts = "mtdparts=mtdparts=gpmi-nand:4m(boot),-(ubi)";
	}
	if (!upgrade_avail) {
		//printf("No upgrade_available flag found, defaulting to 0\n");
		upgrade_avail = "0";
	}
	if (!boot_failed) {
		//printf("No boot_failed flag found, defaulting to 0\n");
		boot_failed = "0";
	}

	//Sanitize the boot_part variable to ensure it's either "A" or "B"
	if (strcmp(boot_part, "A") != 0 && strcmp(boot_part, "B") != 0) {
		printf("Invalid boot_part value '%s', defaulting to A\n", boot_part);
		boot_part = "A";
	}

    printf("--- Running Update Boot Script ---\n");

	if (strcmp(boot_part, "A") == 0) {
		boot_part_a = true;
	}

    if (strcmp(upgrade_avail, "1") == 0) {
		if (boot_part_a) {
            env_set("boot_part", "B");
			boot_part = "B";
			boot_part_a = false;
        } else {
            env_set("boot_part", "A");
			boot_part = "A";
			boot_part_a = true;
        }
        printf("Trying upgrade on %s...\n", boot_part);
        env_set("boot_upgrade_available", "0");
        env_set("boot_failed", "1");
        env_save(); // Triggers the flash write
    } else if (strcmp(boot_failed, "1") == 0) {
        printf("Upgrade failed on %s, reverting...\n", boot_part);
        env_set("boot_failed", "0");
        if (boot_part_a) {
            env_set("boot_part", "B");
			boot_part = "B";
			boot_part_a = false;
        } else {
            env_set("boot_part", "A");
			boot_part = "A";
			boot_part_a = true;
        }
        env_save();
    } else {
        printf("No upgrade available, booting from %s\n", boot_part);
    }

	printf("--- Loading Kernel from %s ---\n", boot_part);
	run_command("ubi part ubi; ubi read 0x8A000000 KERNEL-${boot_part}", 0);
	snprintf(bootargs_buf, sizeof(bootargs_buf),
		"boot_part=%s console=ttymxc0,115200 clk_ignore_unused %s ubi.mtd=ubi ubi.block=0,ROOT-%s",
		boot_part, mtdparts, boot_part);
	env_set("bootargs", bootargs_buf);
	if (boot_part_a) {
		env_set("root_blk_dev", "/dev/ubiblock0_4");
	} else {
		env_set("root_blk_dev", "/dev/ubiblock0_5");
	}
	run_command("source 0x8A000000", 0);

	printf("--- Boot " CONFIG_GATEWAY_FIT_CONFIG " ---\n");
	return run_command("bootm 0x8A000000#" CONFIG_GATEWAY_FIT_CONFIG "; reset", 0);
}

U_BOOT_CMD(
    gateway_boot, 1, 0, do_gateway_boot,
    "Execute the Gateway A/B secure boot logic",
    ""
);

void reset_cpu(void)
{
    /* * Write 0x04 to WDOG1_WCR (0x020BC000) to instantly:
     * - Enable the watchdog (WDE, bit 2 = 1)
     * - Assert the Software Reset Signal (SRS, bit 4 = 0)
     */
    writew(0x04, 0x020BC000);
    
    /* Trap the CPU while the hardware reset fires (takes a few milliseconds) */
    while (1);
}
