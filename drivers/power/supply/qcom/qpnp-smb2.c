/* Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/log2.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include "smb-reg.h"
#include "smb-lib.h"
#include "storm-watch.h"
#include <linux/pmic-voter.h>

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/01/22, sjc Add for charging */
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include <linux/proc_fs.h>
#include <linux/iio/consumer.h>
#include <linux/kthread.h>

#include <soc/oppo/boot_mode.h>
#include <soc/oppo/device_info.h>
#include <soc/oppo/oppo_project.h>

#include "../../oppo/oppo_charger.h"
#include "../../oppo/oppo_gauge.h"
#include "../../oppo/oppo_vooc.h"
#include "../../oppo/oppo_short.h"
#include "../../oppo/charger_ic/oppo_short_ic.h"

#include "../../oppo/oppo_adapter.h"
#include "../../oppo/charger_ic/oppo_bq25882.h"
#include "../../oppo/gauge_ic/oppo_bq27541.h"

static struct task_struct *oppo_usbtemp_kthread;
DECLARE_WAIT_QUEUE_HEAD(oppo_usbtemp_wq);
extern struct oppo_chg_chip *g_oppo_chip;
extern 	bool fg_oppo_set_input_current;
#endif

#define SMB2_DEFAULT_WPWR_UW	8000000
bool oppo_ccdetect_check_is_gpio(struct oppo_chg_chip *chip);
int oppo_ccdetect_gpio_init(struct oppo_chg_chip *chip);
void oppo_ccdetect_irq_init(struct oppo_chg_chip *chip);
void oppo_ccdetect_disable(void);
void oppo_ccdetect_enable(void);
int oppo_ccdetect_get_power_role(void);
int qpnp_get_prop_charger_voltage_now(void);
extern  void oppo_set_otg_switch_status_dwc3(bool value);
extern  bool oppo_get_otg_online_status_dwc3(void);
extern  bool oppo_get_otg_switch_status_dwc3(void);
bool oppo_get_otg_switch_status(void);
int oppo_ccdetect_support_check(void);
void otg_disable_id_value (void);
#define OPPO_CHG_MONITOR_INTERVAL round_jiffies_relative(msecs_to_jiffies(5000))
#define OPPO_SUPPORT_CCDETECT_IN_FTM_MODE	2
#define OPPO_SUPPORT_CCDETECT_NOT_FTM_MODE	1
#define	OPPO_NOT_SUPPORT_CCDETECT			0
#define OPPO_DIVIDER_WORK_MODE_AUTO			1
#define OPPO_DIVIDER_WORK_MODE_FIXED		0
#define POWER_SUPPLY_TYPEC_PLUGIN 			1
#define POWER_SUPPLY_TYPEC_PLUGOUT 			0
static struct smb_params v1_params = {
	.fcc			= {
		.name	= "fast charge current",
		.reg	= FAST_CHARGE_CURRENT_CFG_REG,
		.min_u	= 0,
		.max_u	= 4500000,
		.step_u	= 25000,
	},
	.fv			= {
		.name	= "float voltage",
		.reg	= FLOAT_VOLTAGE_CFG_REG,
		.min_u	= 3487500,
		.max_u	= 4920000,
		.step_u	= 7500,
	},
	.usb_icl		= {
		.name	= "usb input current limit",
		.reg	= USBIN_CURRENT_LIMIT_CFG_REG,
		.min_u	= 0,
		.max_u	= 4800000,
		.step_u	= 25000,
	},
	.icl_stat		= {
		.name	= "input current limit status",
		.reg	= ICL_STATUS_REG,
		.min_u	= 0,
		.max_u	= 4800000,
		.step_u	= 25000,
	},
	.otg_cl			= {
		.name	= "usb otg current limit",
		.reg	= OTG_CURRENT_LIMIT_CFG_REG,
		.min_u	= 250000,
		.max_u	= 2000000,
		.step_u	= 250000,
	},
	.dc_icl			= {
		.name	= "dc input current limit",
		.reg	= DCIN_CURRENT_LIMIT_CFG_REG,
		.min_u	= 0,
		.max_u	= 6000000,
		.step_u	= 25000,
	},
	.dc_icl_pt_lv		= {
		.name	= "dc icl PT <8V",
		.reg	= ZIN_ICL_PT_REG,
		.min_u	= 0,
		.max_u	= 3000000,
		.step_u	= 25000,
	},
	.dc_icl_pt_hv		= {
		.name	= "dc icl PT >8V",
		.reg	= ZIN_ICL_PT_HV_REG,
		.min_u	= 0,
		.max_u	= 3000000,
		.step_u	= 25000,
	},
	.dc_icl_div2_lv		= {
		.name	= "dc icl div2 <5.5V",
		.reg	= ZIN_ICL_LV_REG,
		.min_u	= 0,
		.max_u	= 3000000,
		.step_u	= 25000,
	},
	.dc_icl_div2_mid_lv	= {
		.name	= "dc icl div2 5.5-6.5V",
		.reg	= ZIN_ICL_MID_LV_REG,
		.min_u	= 0,
		.max_u	= 3000000,
		.step_u	= 25000,
	},
	.dc_icl_div2_mid_hv	= {
		.name	= "dc icl div2 6.5-8.0V",
		.reg	= ZIN_ICL_MID_HV_REG,
		.min_u	= 0,
		.max_u	= 3000000,
		.step_u	= 25000,
	},
	.dc_icl_div2_hv		= {
		.name	= "dc icl div2 >8.0V",
		.reg	= ZIN_ICL_HV_REG,
		.min_u	= 0,
		.max_u	= 3000000,
		.step_u	= 25000,
	},
	.jeita_cc_comp		= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_REG,
		.min_u	= 0,
		.max_u	= 1575000,
		.step_u	= 25000,
	},
	.freq_buck		= {
		.name	= "buck switching frequency",
		.reg	= CFG_BUCKBOOST_FREQ_SELECT_BUCK_REG,
		.min_u	= 600,
		.max_u	= 2000,
		.step_u	= 200,
	},
	.freq_boost		= {
		.name	= "boost switching frequency",
		.reg	= CFG_BUCKBOOST_FREQ_SELECT_BOOST_REG,
		.min_u	= 600,
		.max_u	= 2000,
		.step_u	= 200,
	},
};

static struct smb_params pm660_params = {
	.freq_buck		= {
		.name	= "buck switching frequency",
		.reg	= FREQ_CLK_DIV_REG,
		.min_u	= 600,
		.max_u	= 1600,
		.set_proc = smblib_set_chg_freq,
	},
	.freq_boost		= {
		.name	= "boost switching frequency",
		.reg	= FREQ_CLK_DIV_REG,
		.min_u	= 600,
		.max_u	= 1600,
		.set_proc = smblib_set_chg_freq,
	},
};

#ifndef VENDOR_EDIT

struct smb_dt_props {
	int	usb_icl_ua;
	int	dc_icl_ua;
	int	boost_threshold_ua;
	int	wipower_max_uw;
	int	min_freq_khz;
	int	max_freq_khz;
	struct	device_node *revid_dev_node;
	int	float_option;
	int	chg_inhibit_thr_mv;
	bool	no_battery;
	bool	hvdcp_disable;
	bool	auto_recharge_soc;
	int	wd_bark_time;
	bool	no_pd;
};

struct smb2 {
	struct smb_charger	chg;
	struct dentry		*dfs_root;
	struct smb_dt_props	dt;
	bool			bad_part;
};
#endif
#ifndef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/03/15, sjc Add for OTG debug */
static int __debug_mask = PR_MISC | PR_OTG | PR_INTERRUPT | PR_REGISTER;
#else
static int __debug_mask;
#endif
module_param_named(
	debug_mask, __debug_mask, int, 0600
);

static int __weak_chg_icl_ua = 500000;
module_param_named(
	weak_chg_icl_ua, __weak_chg_icl_ua, int, 0600);

static int __try_sink_enabled = 1;
module_param_named(
	try_sink_enabled, __try_sink_enabled, int, 0600
);

static int __audio_headset_drp_wait_ms = 100;
module_param_named(
	audio_headset_drp_wait_ms, __audio_headset_drp_wait_ms, int, 0600
);

/************************************************
 ************************************************
 *** THE SECOND PART:  driver sector ***
 ************************************************
 ************************************************/
extern 	bool boot_with_console(void);
 static int oppo_chg_set_2uart_pinctrl_chgID(struct oppo_chg_chip *chip)
{
	struct smb_charger *chg = NULL;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return -EINVAL;
	}

	if (boot_with_console() == true || chip->vbatt_num != 2) {
		return 0;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	if (IS_ERR_OR_NULL(chg->chg_2uart_pinctrl) || IS_ERR_OR_NULL(chg->chg_2uart_sleep)) {
		chg_err("get 2uart chg_2uart_pinctrl fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chg->chg_2uart_pinctrl, chg->chg_2uart_sleep);
	return 0;
}

static int oppo_chg_set_2uart_pinctrl_default(struct oppo_chg_chip *chip)
{
	struct smb_charger *chg = NULL;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return -EINVAL;
	}

	if (boot_with_console() == true || chip->vbatt_num != 2) {
		return 0;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	if (IS_ERR_OR_NULL(chg->chg_2uart_pinctrl) || IS_ERR_OR_NULL(chg->chg_2uart_default)) {
		chg_err("get 2uart chg_2uart_pinctrl fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chg->chg_2uart_pinctrl, chg->chg_2uart_default);
	return 0;
}
int smbchg_get_chargerid_volt(void)
{
	int rc = 0;
	int chargerid_volt = 0;
	struct qpnp_vadc_result results;
	struct oppo_chg_chip *chip = g_oppo_chip;
    	struct smb_charger *chg = &chip->pmic_spmi.smb2_chip->chg;

	if (!chip->pmic_spmi.pm660_vadc_dev) {
		chg_err("pm660_vadc_dev NULL\n");
		return 0;
	}
	if (chip->vbatt_num == 2) {
		oppo_chg_set_2uart_pinctrl_chgID(chip);
		msleep(10);
	}
        if (chg->charger_id_num == 7) {
                rc = qpnp_vadc_read(chip->pmic_spmi.pm660_vadc_dev, P_MUX10_1_1, &results);
        } else {
            	rc = qpnp_vadc_read(chip->pmic_spmi.pm660_vadc_dev, P_MUX3_1_1, &results);
        }
	if (rc) {
		chg_err("unable to read pm660_vadc_dev charger_id rc = %d\n", rc);
		return 0;
	}
	chargerid_volt = (int)results.physical / 1000;
	chg_err("chargerid_volt: %d\n", chargerid_volt);
	if (chip->vbatt_num == 2) {
		oppo_chg_set_2uart_pinctrl_default(chip);
	}
	return chargerid_volt;
}


static int smbchg_chargerid_switch_gpio_init(struct oppo_chg_chip *chip)
{
	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.pinctrl)) {
		chg_err("get normalchg_gpio.pinctrl fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.chargerid_switch_active =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "chargerid_switch_active");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_active)) {
		chg_err("get chargerid_switch_active fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.chargerid_switch_sleep =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "chargerid_switch_sleep");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_sleep)) {
		chg_err("get chargerid_switch_sleep fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.chargerid_switch_default =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "chargerid_switch_default");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_default)) {
		chg_err("get chargerid_switch_default fail\n");
		return -EINVAL;
	}

	if (chip->normalchg_gpio.chargerid_switch_gpio > 0) {
		gpio_direction_output(chip->normalchg_gpio.chargerid_switch_gpio, 0);
	}
	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.chargerid_switch_default);

	return 0;
}

void smbchg_set_chargerid_switch_val(int value)
{
	struct oppo_chg_chip *chip = g_oppo_chip;
	
	if (chip->normalchg_gpio.chargerid_switch_gpio <= 0) {
		chg_err("chargerid_switch_gpio not exist, return\n");
		return;
	}

	if (IS_ERR_OR_NULL(chip->normalchg_gpio.pinctrl)
		|| IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_active)
		|| IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_sleep)
		|| IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_default)) {
		chg_err("pinctrl null, return\n");
		return;
	}

	if (oppo_vooc_get_adapter_update_real_status() == ADAPTER_FW_NEED_UPDATE
		|| oppo_vooc_get_btb_temp_over() == true) {
		chg_err("adapter update or btb_temp_over, return\n");
		return;
	}
#if 0
	if (chip->pmic_spmi.not_support_1200ma && !value && !is_usb_present(chip)) {
	/* BugID 879716 : Solve some situatuion ChargerID is not 0 mV when usb is not present */
	// wenbin.liu@BSP.CHG.Basic, 2016/11/14
		chip->chargerid_volt = 0;
		chip->chargerid_volt_got = false;
	}
#endif
	if (value) {
		gpio_direction_output(chip->normalchg_gpio.chargerid_switch_gpio, 1);
		pinctrl_select_state(chip->normalchg_gpio.pinctrl,
				chip->normalchg_gpio.chargerid_switch_default);
	} else {
		gpio_direction_output(chip->normalchg_gpio.chargerid_switch_gpio, 0);
		pinctrl_select_state(chip->normalchg_gpio.pinctrl,
				chip->normalchg_gpio.chargerid_switch_default);
	}
	chg_err("set value:%d, gpio_val:%d\n",
		value, gpio_get_value(chip->normalchg_gpio.chargerid_switch_gpio));
}

int smbchg_get_chargerid_switch_val(void)
{
	struct oppo_chg_chip *chip = g_oppo_chip;
	
	if (chip->normalchg_gpio.chargerid_switch_gpio <= 0) {
		chg_err("chargerid_switch_gpio not exist, return\n");
		return -1;
	}

	if (IS_ERR_OR_NULL(chip->normalchg_gpio.pinctrl)
		|| IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_active)
		|| IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_sleep)) {
		chg_err("pinctrl null, return\n");
		return -1;
	}

	return gpio_get_value(chip->normalchg_gpio.chargerid_switch_gpio);
}

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/03/02, sjc Add for using gpio as shipmode stm6620 */
static int oppo_ship_gpio_init(struct oppo_chg_chip *chip)
{
	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return -EINVAL;
	}

	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);

	chip->normalchg_gpio.ship_active =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "ship_active");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.ship_active)) {
		chg_err("get ship_active fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.ship_sleep =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "ship_sleep");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.ship_sleep)) {
		chg_err("get ship_sleep fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.ship_sleep);

	return 0;
}

static bool oppo_ship_check_is_gpio(struct oppo_chg_chip *chip)
{
	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return false;
	}

	if (gpio_is_valid(chip->normalchg_gpio.ship_gpio))
		return true;

	return false;
}

#define PWM_COUNT	5
static void smbchg_enter_shipmode(struct oppo_chg_chip *chip)
{
	int i = 0;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return;
	}

	if (oppo_ship_check_is_gpio(chip) == true) {
		chg_debug("select gpio control\n");
		if (!IS_ERR_OR_NULL(chip->normalchg_gpio.ship_sleep)) {
			pinctrl_select_state(chip->normalchg_gpio.pinctrl,
				chip->normalchg_gpio.ship_sleep);
		}
		for (i = 0; i < PWM_COUNT; i++) {
			//gpio_direction_output(chip->normalchg_gpio.ship_gpio, 1);
			pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.ship_active);
			mdelay(3);
			//gpio_direction_output(chip->normalchg_gpio.ship_gpio, 0);
			pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.ship_sleep);
			mdelay(3);
		}
		chg_debug("power off after 15s\n");
	}
}
#endif /* VENDOR_EDIT */

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/03/02, sjc Add for HW shortc */
static int oppo_shortc_gpio_init(struct oppo_chg_chip *chip)
{
	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return -EINVAL;
	}

	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);

	chip->normalchg_gpio.shortc_active =
		pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "shortc_active");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.shortc_active)) {
		chg_err("get shortc_active fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.shortc_active);

	/*chg->fcc_stepper_enable = of_property_read_bool(node,
					"qcom,fcc-stepping-enable");*/

	chg->ufp_only_mode = of_property_read_bool(node,
					"qcom,ufp-only-mode");

	return 0;
}

static bool oppo_shortc_check_is_gpio(struct oppo_chg_chip *chip)
{
	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return false;
	}

	if (gpio_is_valid(chip->normalchg_gpio.shortc_gpio))
		return true;

	return false;
}

#ifdef CONFIG_OPPO_SHORT_HW_CHECK
static bool oppo_chg_get_shortc_hw_gpio_status(void)
{
	bool shortc_hw_status = 1;
	struct oppo_chg_chip *chip = g_oppo_chip;
	return true;
	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return shortc_hw_status;
	}

	if (oppo_shortc_check_is_gpio(chip) == true) {
		shortc_hw_status = !!(gpio_get_value(chip->normalchg_gpio.shortc_gpio));
	}
	return shortc_hw_status;
}
#else
static bool oppo_chg_get_shortc_hw_gpio_status(void)
{
	bool shortc_hw_status = 1;

	return shortc_hw_status;
}
#endif /* CONFIG_OPPO_SHORT_HW_CHECK */
#endif /* VENDOR_EDIT */
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/01/30, sjc Add for using gpio as CC detect */
int oppo_ccdetect_gpio_init(struct oppo_chg_chip *chip)
{
	struct smb_charger *chg = NULL;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return -EINVAL;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	chg->ccdetect_pinctrl = devm_pinctrl_get(chip->dev);

	if (IS_ERR_OR_NULL(chg->ccdetect_pinctrl)) {
		chg_err("get ccdetect ccdetect_pinctrl fail\n");
		return -EINVAL;
	}

	chg->ccdetect_active = pinctrl_lookup_state(chg->ccdetect_pinctrl, "ccdetect_active");
	if (IS_ERR_OR_NULL(chg->ccdetect_active)) {
		chg_err("get ccdetect_active fail\n");
		return -EINVAL;
	}

	chg->ccdetect_sleep = pinctrl_lookup_state(chg->ccdetect_pinctrl, "ccdetect_sleep");
	if (IS_ERR_OR_NULL(chg->ccdetect_sleep)) {
		chg_err("get ccdetect_sleep fail\n");
		return -EINVAL;
	}

	if (chg->ccdetect_gpio > 0) {
		gpio_direction_input(chg->ccdetect_gpio);
	}

	pinctrl_select_state(chg->ccdetect_pinctrl, chg->ccdetect_active);

	return 0;
}

void oppo_ccdetect_irq_init(struct oppo_chg_chip *chip)
{
	struct smb_charger *chg = NULL;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	chg->ccdetect_irq = gpio_to_irq(chg->ccdetect_gpio);
    printk(KERN_ERR "[OPPO_CHG][%s]: chg->ccdetect_irq[%d]!\n", __func__, chg->ccdetect_irq);
}

void oppo_ccdetect_enable(void)
{
    oppo_set_otg_switch_status_dwc3(true);
}

void oppo_ccdetect_disable(void)
{    
	bool value = false;
	if(oppo_ccdetect_support_check() == OPPO_SUPPORT_CCDETECT_IN_FTM_MODE)
	{
		printk(KERN_ERR "[OPPO_CHG][%s]: ccdetect is in FTM mode, enable otg!\n", __func__);
		value = true;
	}
	if (g_oppo_chip && g_oppo_chip->vbatt_num == 2) {
		otg_disable_id_value();
	} else {
		oppo_set_otg_switch_status_dwc3(value);
	}
}

int oppo_ccdetect_get_power_role(void)
{
	int rc;
	struct smb_charger *chg = NULL;
	union power_supply_propval val = {0,};

	if (!g_oppo_chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return POWER_SUPPLY_TYPEC_PR_NONE;
	}
	chg = &g_oppo_chip->pmic_spmi.smb2_chip->chg;

	rc = smblib_get_prop_typec_power_role(chg, &val);
	if (rc < 0) {
		printk(KERN_ERR "[OPPO_CHG][%s]: Couldn't get typec power role, rc=%d\n", __func__, rc);
		return POWER_SUPPLY_TYPEC_PR_DUAL;
	}
	return val.intval;
}

bool oppo_ccdetect_check_is_gpio(struct oppo_chg_chip *chip)
{
	struct smb_charger *chg = NULL;
	int boot_mode = get_boot_mode();

    if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return false;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	/* HW engineer requirement */
	if (boot_mode == MSM_BOOT_MODE__RF || boot_mode == MSM_BOOT_MODE__WLAN
			|| boot_mode == MSM_BOOT_MODE__FACTORY)
		return false;

	if (gpio_is_valid(chg->ccdetect_gpio))
		return true;

	return false;
}
int oppo_ccdetect_support_check(void)
{
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;
	int boot_mode = get_boot_mode();

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: g_oppo_chip not ready!\n", __func__);
		return OPPO_NOT_SUPPORT_CCDETECT;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;
	if (boot_mode == MSM_BOOT_MODE__RF || boot_mode == MSM_BOOT_MODE__WLAN
			|| boot_mode == MSM_BOOT_MODE__FACTORY) {
			return OPPO_SUPPORT_CCDETECT_IN_FTM_MODE;
	}
	if (gpio_is_valid(chg->ccdetect_gpio))
		return OPPO_SUPPORT_CCDETECT_NOT_FTM_MODE;

	return OPPO_NOT_SUPPORT_CCDETECT;
}
EXPORT_SYMBOL(oppo_ccdetect_support_check);
static void oppo_ccdetect_irq_register(struct oppo_chg_chip *chip)
{
	int ret = 0;
	struct smb_charger *chg = NULL;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	ret = devm_request_threaded_irq(chip->dev, chg->ccdetect_irq,
			NULL, oppo_ccdetect_change_handler, IRQF_TRIGGER_FALLING
			| IRQF_TRIGGER_RISING | IRQF_ONESHOT, "ccdetect-change", chip);
	if (ret < 0) {
		chg_err("Unable to request ccdetect-change irq: %d\n", ret);
	}
    printk(KERN_ERR "%s: !!!!! irq register\n", __FUNCTION__);

	ret = enable_irq_wake(chg->ccdetect_irq);
	if (ret != 0) {
		chg_err("enable_irq_wake: ccdetect_irq failed %d\n", ret);
	}
}
#endif /* VENDOR_EDIT */
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/05/25, sjc Add for usbtemp */
static bool oppo_usbtemp_check_is_gpio(struct oppo_chg_chip *chip)
{
	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return false;
	}

	if (gpio_is_valid(chip->normalchg_gpio.dischg_gpio))
		return true;

	return false;
}

bool oppo_usbtemp_check_is_support(void)
{
	if(oppo_usbtemp_check_is_gpio(g_oppo_chip) == true)
		return true;
	
	chg_err("dischg return false\n");

	return false;
}

static int oppo_get_usbtemp_volt(struct oppo_chg_chip *chip)
{
	int rc = 0;
	int usbtemp_volt = 0;
	struct qpnp_vadc_result results;

	if (!chip->pmic_spmi.pm660_usbtemp_vadc_dev) {
		chg_err("usbtemp_vadc_dev NULL\n");
		return 0;
	}

	rc = qpnp_vadc_read(chip->pmic_spmi.pm660_usbtemp_vadc_dev, P_MUX5_1_1, &results);
	if (rc) {
		chg_err("unable to read usbtemp_vadc_dev VADC_AMUX1_GPIO_PU2 rc = %d\n", rc);
		return 0;
	}
	usbtemp_volt = (int)results.physical / 1000;

	return usbtemp_volt;
}

static int oppo_dischg_gpio_init(struct oppo_chg_chip *chip)
{
	if (!chip) {
		chg_err("chip NULL\n");
		return EINVAL;
	}

	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);

	if (IS_ERR_OR_NULL(chip->normalchg_gpio.pinctrl)) {
		chg_err("get dischg_pinctrl fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.dischg_enable = pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "dischg_enable");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable)) {
		chg_err("get dischg_enable fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.dischg_disable = pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "dischg_disable");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_disable)) {
		chg_err("get dischg_disable fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.dischg_disable);

	return 0;
}

#define USB_50C_VOLT	450
#define USB_55C_VOLT	448
#define USB_60C_VOLT	327
#define USB_VBUS_SHORT_DISABLE_VOLT		509
#define USB_VBUS_SHORT_ENABLE_VOLT		392
#define MIN_MONITOR_INTERVAL	50//50ms
#define MAX_MONITOR_INTERVAL	200//200ms
static int oppo_usbtemp_monitor_main(void *data)
{
	int delay = 0;
	int usbtemp_volt = 0;
	static bool dischg_flag = false;
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;

	chg = &chip->pmic_spmi.smb2_chip->chg;

	while (!kthread_should_stop()) {
		usbtemp_volt = oppo_get_usbtemp_volt(chip);
		if (usbtemp_volt <= USB_55C_VOLT)
			delay = MIN_MONITOR_INTERVAL;
		else
			delay = MAX_MONITOR_INTERVAL;

		if (oppo_chg_is_usb_present() == true || usbtemp_volt <= USB_VBUS_SHORT_DISABLE_VOLT) {
			if (usbtemp_volt <= USB_VBUS_SHORT_ENABLE_VOLT && dischg_flag == false) {
				if (!IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable)) {
					dischg_flag = true;
					chg_err("dischg enable...[%d]\n", usbtemp_volt);
					pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.dischg_enable);
				}
			}
			msleep(delay);
		} else {
			if (dischg_flag == true) {
				if (!IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_disable)) {
					dischg_flag = false;
					chg_err("dischg disable...[%d]\n", usbtemp_volt);
					pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.dischg_disable);
				}
			}
			wait_event_interruptible(oppo_usbtemp_wq, oppo_chg_is_usb_present() == true);
		}
		chg_err("==================usbtemp_volt[%d], level[]\n", usbtemp_volt);
	}

	return 0;
}

static void oppo_usbtemp_thread_init(void)
{
	oppo_usbtemp_kthread =
			kthread_run(oppo_usbtemp_monitor_main, 0, "usbtemp_kthread");
	if (IS_ERR(oppo_usbtemp_kthread)) {
		chg_err("failed to cread oppo_usbtemp_kthread\n");
	}
}

void oppo_wake_up_usbtemp_thread(void)
{
	if (oppo_usbtemp_check_is_support() == true){
		wake_up_interruptible(&oppo_usbtemp_wq);
	}
}
#endif /*VENDOR_EDIT*/

static int oppo_chg_parse_custom_dt(struct oppo_chg_chip *chip)
{
	int rc = 0;
	struct device_node *node = chip->dev->of_node;
	struct smb_charger *chg = &chip->pmic_spmi.smb2_chip->chg;
	if (!node) {
			pr_err("device tree node missing\n");
			return -EINVAL;
	}
	
#ifdef VENDOR_EDIT
	/* Jianchao.Shi@BSP.CHG.Basic, 2017/01/22, sjc Add for charging*/
	if (g_oppo_chip) {
		g_oppo_chip->normalchg_gpio.chargerid_switch_gpio =
				of_get_named_gpio(node, "qcom,chargerid_switch-gpio", 0);
		if (g_oppo_chip->normalchg_gpio.chargerid_switch_gpio <= 0) {
			chg_err("Couldn't read chargerid_switch-gpio rc = %d, chargerid_switch_gpio:%d\n",
					rc, g_oppo_chip->normalchg_gpio.chargerid_switch_gpio);
		} else {
			if (gpio_is_valid(g_oppo_chip->normalchg_gpio.chargerid_switch_gpio)) {
				rc = gpio_request(g_oppo_chip->normalchg_gpio.chargerid_switch_gpio, "charging-switch1-gpio");
				if (rc) {
					chg_err("unable to request chargerid_switch_gpio:%d\n", g_oppo_chip->normalchg_gpio.chargerid_switch_gpio);
				} else {
					smbchg_chargerid_switch_gpio_init(g_oppo_chip);
				}
			}
			chg_err("chargerid_switch_gpio:%d\n", g_oppo_chip->normalchg_gpio.chargerid_switch_gpio);
		}
	}
#endif /*VENDOR_EDIT*/
#ifdef VENDOR_EDIT
/* tongfeng.Huang@BSP.CHG.Basic, 2018/05/08,  Add for using gpio as USB vbus short */
	if (g_oppo_chip) {
		g_oppo_chip->normalchg_gpio.dischg_gpio = of_get_named_gpio(node, "qcom,dischg-gpio", 0);
		if (g_oppo_chip->normalchg_gpio.dischg_gpio <= 0) {
			chg_err("Couldn't read qcom,dischg-gpio rc=%d, qcom,dischg-gpio:%d\n",
				rc, g_oppo_chip->normalchg_gpio.dischg_gpio);
		} else {
			if (oppo_usbtemp_check_is_support() == true) {
				if (gpio_is_valid(g_oppo_chip->normalchg_gpio.dischg_gpio)) {
					rc = gpio_request(g_oppo_chip->normalchg_gpio.dischg_gpio, "dischg-gpio");
					if (rc) {
						chg_err("unable to request dischg-gpio:%d\n", g_oppo_chip->normalchg_gpio.dischg_gpio);
					} else {
						oppo_dischg_gpio_init(g_oppo_chip);
					}
				}
			}
			chg_err("dischg-gpio:%d\n", g_oppo_chip->normalchg_gpio.dischg_gpio);
		}
	}
#endif /*VENDOR_EDIT*/
	
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/03/02, sjc Add for using gpio as shipmode stm6620 */
	if (g_oppo_chip) {
		g_oppo_chip->normalchg_gpio.ship_gpio =
				of_get_named_gpio(node, "qcom,ship-gpio", 0);
		if (g_oppo_chip->normalchg_gpio.ship_gpio <= 0) {
			chg_err("Couldn't read qcom,ship-gpio rc = %d, qcom,ship-gpio:%d\n",
					rc, g_oppo_chip->normalchg_gpio.ship_gpio);
		} else {
			if (oppo_ship_check_is_gpio(g_oppo_chip) == true) {
				rc = gpio_request(g_oppo_chip->normalchg_gpio.ship_gpio, "ship-gpio");
				if (rc) {
					chg_err("unable to request ship-gpio:%d\n",
							g_oppo_chip->normalchg_gpio.ship_gpio);
				} else {
					oppo_ship_gpio_init(g_oppo_chip);
					if (rc)
						chg_err("unable to init ship-gpio:%d\n", g_oppo_chip->normalchg_gpio.ship_gpio);
				}
			}
			chg_err("ship-gpio:%d\n", g_oppo_chip->normalchg_gpio.ship_gpio);
		}
	}
#endif /*VENDOR_EDIT*/
	
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/03/02, sjc Add for HW shortc */
	if (g_oppo_chip) {
		g_oppo_chip->normalchg_gpio.shortc_gpio =
				of_get_named_gpio(node, "qcom,shortc-gpio", 0);
		if (g_oppo_chip->normalchg_gpio.shortc_gpio <= 0) {
			chg_err("Couldn't read qcom,shortc-gpio rc = %d, qcom,shortc-gpio:%d\n",
					rc, g_oppo_chip->normalchg_gpio.shortc_gpio);
		} else {
			if (oppo_shortc_check_is_gpio(g_oppo_chip) == true) {
				rc = gpio_request(g_oppo_chip->normalchg_gpio.shortc_gpio, "shortc-gpio");
				if (rc) {
					chg_err("unable to request shortc-gpio:%d\n",
							g_oppo_chip->normalchg_gpio.shortc_gpio);
				} else {
					oppo_shortc_gpio_init(g_oppo_chip);
					if (rc)
						chg_err("unable to init ship-gpio:%d\n", g_oppo_chip->normalchg_gpio.ship_gpio);
				}
			}
			chg_err("shortc-gpio:%d\n", g_oppo_chip->normalchg_gpio.shortc_gpio);
		}
	}
#endif /* VENDOR_EDIT */
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/01/30, sjc Add for using gpio as CC detect */
    if (chip) {
        chg->ccdetect_gpio = of_get_named_gpio(node, "qcom,ccdetect-gpio", 0);
        if (chg->ccdetect_gpio <= 0) {
            chg_err("Couldn't read qcom,ccdetect-gpio rc=%d, qcom,ccdetect-gpio:%d\n",
                    rc, chg->ccdetect_gpio);
        } else {
            if (oppo_ccdetect_check_is_gpio(chip) == true) {
                rc = gpio_request(chg->ccdetect_gpio, "ccdetect-gpio");
                if (rc) {
                    chg_err("unable to request ccdetect-gpio:%d\n", chg->ccdetect_gpio);
                } else {
                    rc = oppo_ccdetect_gpio_init(chip);
                    if (rc)
                        chg_err("unable to init ccdetect-gpio:%d\n", chg->ccdetect_gpio);
                    else
                        oppo_ccdetect_irq_init(chip);
                }
            }
            chg_err("ccdetect-gpio:%d\n", chg->ccdetect_gpio);
        }
    }
#endif /*VENDOR_EDIT*/
	return rc;

}

#ifndef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/05/27, sjc Modify for OTG current limit (V3.1)  */
#define MICRO_1P5A		1500000
#else
#define MICRO_1P5A		1000000
#endif

#define MICRO_P1A		100000
#define OTG_DEFAULT_DEGLITCH_TIME_MS	50
#define MIN_WD_BARK_TIME		16
#define DEFAULT_WD_BARK_TIME		64
#define BITE_WDOG_TIMEOUT_8S		0x3
#define BARK_WDOG_TIMEOUT_MASK		GENMASK(3, 2)
#define BARK_WDOG_TIMEOUT_SHIFT		2
static int smb2_parse_dt(struct smb2 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	int rc, byte_len;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	chg->reddragon_ipc_wa = of_property_read_bool(node,
				"qcom,qcs605-ipc-wa");

	chg->step_chg_enabled = of_property_read_bool(node,
				"qcom,step-charging-enable");

	chg->sw_jeita_enabled = of_property_read_bool(node,
				"qcom,sw-jeita-enable");
#ifdef VENDOR_EDIT
/* Qiao.Hu@BSP.CHG.basic, 2018/11/02, add for chargerid */
	rc = of_property_read_u32(node,"qcom,charger_id_num",
	                &chg->charger_id_num);
	if (rc < 0) {
		chg->charger_id_num = 0;
    }

#endif
	rc = of_property_read_u32(node, "qcom,wd-bark-time-secs",
					&chip->dt.wd_bark_time);
	if (rc < 0 || chip->dt.wd_bark_time < MIN_WD_BARK_TIME)
		chip->dt.wd_bark_time = DEFAULT_WD_BARK_TIME;

	chip->dt.no_battery = of_property_read_bool(node,
						"qcom,batteryless-platform");

	chip->dt.no_pd = of_property_read_bool(node,
						"qcom,pd-not-supported");

	rc = of_property_read_u32(node,
				"qcom,fcc-max-ua", &chg->batt_profile_fcc_ua);
	if (rc < 0)
		chg->batt_profile_fcc_ua = -EINVAL;

	rc = of_property_read_u32(node,
				"qcom,fv-max-uv", &chg->batt_profile_fv_uv);
	if (rc < 0)
		chg->batt_profile_fv_uv = -EINVAL;

	rc = of_property_read_u32(node,
				"qcom,usb-icl-ua", &chip->dt.usb_icl_ua);
	if (rc < 0)
		chip->dt.usb_icl_ua = -EINVAL;

	rc = of_property_read_u32(node,
				"qcom,otg-cl-ua", &chg->otg_cl_ua);
	if (rc < 0)
		chg->otg_cl_ua = MICRO_1P5A;

	rc = of_property_read_u32(node,
				"qcom,dc-icl-ua", &chip->dt.dc_icl_ua);
	if (rc < 0)
		chip->dt.dc_icl_ua = -EINVAL;

	rc = of_property_read_u32(node,
				"qcom,boost-threshold-ua",
				&chip->dt.boost_threshold_ua);
	if (rc < 0)
		chip->dt.boost_threshold_ua = MICRO_P1A;

	rc = of_property_read_u32(node,
				"qcom,min-freq-khz",
				&chip->dt.min_freq_khz);
	if (rc < 0)
		chip->dt.min_freq_khz = -EINVAL;

	rc = of_property_read_u32(node,
				"qcom,max-freq-khz",
				&chip->dt.max_freq_khz);
	if (rc < 0)
		chip->dt.max_freq_khz = -EINVAL;

	rc = of_property_read_u32(node, "qcom,wipower-max-uw",
				&chip->dt.wipower_max_uw);
	if (rc < 0)
		chip->dt.wipower_max_uw = -EINVAL;

	if (of_find_property(node, "qcom,thermal-mitigation", &byte_len)) {
		chg->thermal_mitigation = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->thermal_mitigation == NULL)
			return -ENOMEM;

		chg->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"qcom,thermal-mitigation",
				chg->thermal_mitigation,
				chg->thermal_levels);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	of_property_read_u32(node, "qcom,float-option", &chip->dt.float_option);
	if (chip->dt.float_option < 0 || chip->dt.float_option > 4) {
		pr_err("qcom,float-option is out of range [0, 4]\n");
		return -EINVAL;
	}

	chip->dt.hvdcp_disable = of_property_read_bool(node,
						"qcom,hvdcp-disable");

	of_property_read_u32(node, "qcom,chg-inhibit-threshold-mv",
				&chip->dt.chg_inhibit_thr_mv);
	if ((chip->dt.chg_inhibit_thr_mv < 0 ||
		chip->dt.chg_inhibit_thr_mv > 300)) {
		pr_err("qcom,chg-inhibit-threshold-mv is incorrect\n");
		return -EINVAL;
	}

	chip->dt.auto_recharge_soc = of_property_read_bool(node,
						"qcom,auto-recharge-soc");

	chg->use_extcon = of_property_read_bool(node,
						"qcom,use-extcon");

#ifndef VENDOR_EDIT
	/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/28, sjc Modify for charging */
		chg->dcp_icl_ua = chip->dt.usb_icl_ua;
#else
		chg->dcp_icl_ua = -EINVAL;
#endif

	chg->suspend_input_on_debug_batt = of_property_read_bool(node,
					"qcom,suspend-input-on-debug-batt");

	rc = of_property_read_u32(node, "qcom,otg-deglitch-time-ms",
					&chg->otg_delay_ms);
	if (rc < 0)
		chg->otg_delay_ms = OTG_DEFAULT_DEGLITCH_TIME_MS;

	chg->disable_stat_sw_override = of_property_read_bool(node,
					"qcom,disable-stat-sw-override");

	chg->fcc_stepper_enable = of_property_read_bool(node,
					"qcom,fcc-stepping-enable");

	return 0;
}

/************************
 * USB PSY REGISTRATION *
 ************************/

static enum power_supply_property smb2_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_PD_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_TYPEC_POWER_ROLE,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_PD_ALLOWED,
	POWER_SUPPLY_PROP_PD_ACTIVE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_BOOST_CURRENT,
	POWER_SUPPLY_PROP_PE_START,
	POWER_SUPPLY_PROP_CTM_CURRENT_MAX,
	POWER_SUPPLY_PROP_HW_CURRENT_MAX,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_PR_SWAP,
	POWER_SUPPLY_PROP_PD_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_PD_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_SDP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONNECTOR_TYPE,
	POWER_SUPPLY_PROP_MOISTURE_DETECTED,
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/02/18, sjc Add for OTG sw */
	POWER_SUPPLY_PROP_OTG_SWITCH,
	POWER_SUPPLY_PROP_OTG_ONLINE,
#endif
};

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/04/09, sjc Add for OTG sw */
/**************************************************************
 * bit[0]=0: NO standard typec device/cable connected(ccdetect gpio in high level)
 * bit[0]=1: standard typec device/cable connected(ccdetect gpio in low level)
 * bit[1]=0: NO OTG typec device/cable connected
 * bit[1]=1: OTG typec device/cable connected
 **************************************************************/
#define DISCONNECT						0
#define STANDARD_TYPEC_DEV_CONNECT	BIT(0)
#define OTG_DEV_CONNECT				BIT(1)

bool oppo_get_otg_switch_status(void)
{
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return false;
	}
	if (oppo_ccdetect_check_is_gpio(chip) != true) {
        return oppo_get_otg_switch_status_dwc3();
    }

	return chip->otg_switch;
}

static int oppo_get_otg_online_status(void)
{
	union power_supply_propval val;
	int ret;
	int online = 0;
	int level = 0;
	int typec_otg = 0;
	static int pre_level = 1;
	static int pre_typec_otg = 0;
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return false;
	}

	chg = &chip->pmic_spmi.smb2_chip->chg;

	if (oppo_ccdetect_check_is_gpio(chip) == true) {
		level = gpio_get_value(chg->ccdetect_gpio);
		if (level != gpio_get_value(chg->ccdetect_gpio)) {
			printk(KERN_ERR "[OPPO_CHG][%s]: ccdetect_gpio is unstable, try again...\n", __func__);
			usleep_range(5000, 5100);
			level = gpio_get_value(chg->ccdetect_gpio);
		}
	} else {
        return oppo_get_otg_online_status_dwc3();
        
    }

	online = (level == 1) ? DISCONNECT : STANDARD_TYPEC_DEV_CONNECT;

	ret = power_supply_get_property(chg->usb_psy, POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	if (ret) {
		printk(KERN_ERR "%s: Unable to read USB TYPEC_MODE\n", __func__);
		val.intval = 0;
	}
	if (val.intval >= POWER_SUPPLY_TYPEC_SINK
			&& val.intval <= POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY) {
		typec_otg = 1;
	} else {
		typec_otg = 0;
	}
	online = online | ((typec_otg == 1) ? OTG_DEV_CONNECT : DISCONNECT);

	if ((pre_level ^ level) || (pre_typec_otg ^ typec_otg)) {
		pre_level = level;
		pre_typec_otg = typec_otg;
		printk(KERN_ERR "[OPPO_CHG][%s]: gpio[%s], c-otg[%d], otg_online[%d]\n",
				__func__, level ? "H" : "L", typec_otg, online);
	}

	chip->otg_online = online;
	return chip->otg_online;
}
extern int oppo_set_dpdm_status(int status);
static void oppo_set_otg_switch_status(bool value)
{
	int level = 0;
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;
	if(oppo_ccdetect_support_check() == OPPO_SUPPORT_CCDETECT_IN_FTM_MODE)
	{
		printk(KERN_ERR "[OPPO_CHG][%s]: ccdetect is in FTM mode, enable otg!\n", __func__);
		value = true;
	}
	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	/*boot-up with newman OTG connected, android will set persist.sys.oppo.otg_support, so...*/
	if (oppo_ccdetect_check_is_gpio(chip) == true) {
		level = gpio_get_value(chg->ccdetect_gpio);
		if (level != 1) {
			printk(KERN_ERR "[OPPO_CHG][%s]: gpio[%s], should set, return\n", __func__, level ? "H" : "L");
			return;
		}
	} else {
      		 oppo_set_otg_switch_status_dwc3(value);
        return;
    }

	chip->otg_switch = !!value;
	if (value) {
        oppo_set_dpdm_status(POWER_SUPPLY_TYPEC_PLUGIN);
		oppo_ccdetect_enable();
	} else {
        oppo_set_dpdm_status(POWER_SUPPLY_TYPEC_PLUGOUT);
		oppo_ccdetect_disable();
	}
	printk(KERN_ERR "[OPPO_CHG][%s]: otg_switch=%d, otg_online=%d\n",
			__func__, chip->otg_switch, chip->otg_online);
}
#endif /* VENDOR_EDIT */

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/26, sjc Add for charging */
static bool use_present_status = false;
#endif

static int smb2_usb_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb2 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		if (chip->bad_part)
			val->intval = 1;
		else
			rc = smblib_get_prop_usb_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
#ifdef VENDOR_EDIT
		/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/26, sjc Modify for charging */
		if (use_present_status)
			rc = smblib_get_prop_usb_present(chg, val);
		else
			rc = smblib_get_prop_usb_online(chg, val);
#else
		rc = smblib_get_prop_usb_online(chg, val);
#endif

		if (!val->intval)
			break;

		if (((chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT)
		   || (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB))
		   && (chg->real_charger_type == POWER_SUPPLY_TYPE_USB))
			val->intval = 0;
		else
			val->intval = 1;
		if (chg->real_charger_type == POWER_SUPPLY_TYPE_UNKNOWN)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_get_prop_usb_voltage_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		rc = smblib_get_prop_usb_voltage_max_design(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = smblib_get_prop_usb_voltage_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable, PD_VOTER);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		if (chip->bad_part)
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		else
			val->intval = chg->real_charger_type;
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
			val->intval = POWER_SUPPLY_TYPEC_NONE;
		else if (chip->bad_part)
			val->intval = POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
		else
			val->intval = chg->typec_mode;
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
			val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		else
			rc = smblib_get_prop_typec_power_role(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
			val->intval = 0;
		else
			rc = smblib_get_prop_typec_cc_orientation(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_ALLOWED:
		rc = smblib_get_prop_pd_allowed(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		val->intval = chg->pd_active;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		rc = smblib_get_prop_usb_current_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_BOOST_CURRENT:
		val->intval = chg->boost_current_ua;
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		rc = smblib_get_prop_pd_in_hard_reset(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		val->intval = chg->system_suspend_supported;
		break;
	case POWER_SUPPLY_PROP_PE_START:
		rc = smblib_get_pe_start(chg, val);
		break;
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable, CTM_VOTER);
		break;
	case POWER_SUPPLY_PROP_HW_CURRENT_MAX:
		rc = smblib_get_charge_current(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_PR_SWAP:
		rc = smblib_get_prop_pr_swap_in_progress(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		val->intval = chg->voltage_max_uv;
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		val->intval = chg->voltage_min_uv;
		break;
	case POWER_SUPPLY_PROP_SDP_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable,
					      USB_PSY_VOTER);
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TYPE:
		val->intval = chg->connector_type;
		break;
	case POWER_SUPPLY_PROP_MOISTURE_DETECTED:
		val->intval = get_client_vote(chg->disable_power_role_switch,
					      MOISTURE_VOTER);
		break;

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/02/18, sjc Add for OTG sw */
	case POWER_SUPPLY_PROP_OTG_SWITCH:
		val->intval = oppo_get_otg_switch_status();
		break;
	case POWER_SUPPLY_PROP_OTG_ONLINE:
		val->intval = oppo_get_otg_online_status();
		break;
#endif /*VENDOR_EDIT*/
	default:
		pr_debug("get prop %d is not supported in usb\n", psp);
		rc = -EINVAL;
		break;
	}
	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}
	return 0;
}

static int smb2_usb_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb2 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	mutex_lock(&chg->lock);
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/04/09,  sjc Modify for OTG sw */
	if (!chg->typec_present && psp != POWER_SUPPLY_PROP_OTG_SWITCH) {
#else
	if (!chg->typec_present) {
#endif
		switch (psp) {
		case POWER_SUPPLY_PROP_MOISTURE_DETECTED:
			vote(chg->disable_power_role_switch, MOISTURE_VOTER,
			     val->intval > 0, 0);
			break;
		default:
			rc = -EINVAL;
			break;
		}

		goto unlock;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		rc = smblib_set_prop_pd_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		rc = smblib_set_prop_typec_power_role(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		rc = smblib_set_prop_pd_active(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		rc = smblib_set_prop_pd_in_hard_reset(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		chg->system_suspend_supported = val->intval;
		break;
	case POWER_SUPPLY_PROP_BOOST_CURRENT:
		rc = smblib_set_prop_boost_current(chg, val);
		break;
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
		rc = vote(chg->usb_icl_votable, CTM_VOTER,
						val->intval >= 0, val->intval);
		break;
	case POWER_SUPPLY_PROP_PR_SWAP:
		rc = smblib_set_prop_pr_swap_in_progress(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		rc = smblib_set_prop_pd_voltage_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		rc = smblib_set_prop_pd_voltage_min(chg, val);
		break;
	case POWER_SUPPLY_PROP_SDP_CURRENT_MAX:
		rc = smblib_set_prop_sdp_current_max(chg, val);
		break;
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/02/18, sjc Add for OTG sw */
	case POWER_SUPPLY_PROP_OTG_SWITCH:
		oppo_set_otg_switch_status(!!val->intval);
		break;
#endif
	default:
		pr_err("set prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

unlock:
	mutex_unlock(&chg->lock);
	return rc;
}

static int smb2_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
		return 1;
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/02/18, sjc Add for OTG sw */
	case POWER_SUPPLY_PROP_OTG_SWITCH:
		return 1;
#endif
	default:
		break;
	}

	return 0;
}

static int smb2_init_usb_psy(struct smb2 *chip)
{
	struct power_supply_config usb_cfg = {};
	struct smb_charger *chg = &chip->chg;

	chg->usb_psy_desc.name			= "usb";
#ifndef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/03/28, sjc Modify for charging */
	chg->usb_psy_desc.type			= POWER_SUPPLY_TYPE_USB_PD;
#else
	chg->usb_psy_desc.type			= POWER_SUPPLY_TYPE_UNKNOWN;
#endif
	chg->usb_psy_desc.properties		= smb2_usb_props;
	chg->usb_psy_desc.num_properties	= ARRAY_SIZE(smb2_usb_props);
	chg->usb_psy_desc.get_property		= smb2_usb_get_prop;
	chg->usb_psy_desc.set_property		= smb2_usb_set_prop;
	chg->usb_psy_desc.property_is_writeable	= smb2_usb_prop_is_writeable;

	usb_cfg.drv_data = chip;
	usb_cfg.of_node = chg->dev->of_node;
	chg->usb_psy = power_supply_register(chg->dev,
						  &chg->usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}

	return 0;
}

/********************************
 * USB PC_PORT PSY REGISTRATION *
 ********************************/
static enum power_supply_property smb2_usb_port_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static int smb2_usb_port_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb2 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/26, sjc Modify for charging */
		if (use_present_status)
			rc = smblib_get_prop_usb_present(chg, val);
		else
			rc = smblib_get_prop_usb_online(chg, val);
#else
		rc = smblib_get_prop_usb_online(chg, val);
#endif
		if (!val->intval)
			break;

		if (((chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT)
		   || (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB))
			&& (chg->real_charger_type == POWER_SUPPLY_TYPE_USB))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	default:
		pr_err_ratelimited("Get prop %d is not supported in pc_port\n",
				psp);
		return -EINVAL;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int smb2_usb_port_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int rc = 0;

	switch (psp) {
	default:
		pr_err_ratelimited("Set prop %d is not supported in pc_port\n",
				psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static const struct power_supply_desc usb_port_psy_desc = {
	.name		= "pc_port",
	.type		= POWER_SUPPLY_TYPE_USB,
	.properties	= smb2_usb_port_props,
	.num_properties	= ARRAY_SIZE(smb2_usb_port_props),
	.get_property	= smb2_usb_port_get_prop,
	.set_property	= smb2_usb_port_set_prop,
};

static int smb2_init_usb_port_psy(struct smb2 *chip)
{
	struct power_supply_config usb_port_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_port_cfg.drv_data = chip;
	usb_port_cfg.of_node = chg->dev->of_node;
	chg->usb_port_psy = power_supply_register(chg->dev,
						  &usb_port_psy_desc,
						  &usb_port_cfg);
	if (IS_ERR(chg->usb_port_psy)) {
		pr_err("Couldn't register USB pc_port power supply\n");
		return PTR_ERR(chg->usb_port_psy);
	}

	return 0;
}

/*****************************
 * USB MAIN PSY REGISTRATION *
 *****************************/

static enum power_supply_property smb2_usb_main_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED,
	POWER_SUPPLY_PROP_FCC_DELTA,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TOGGLE_STAT,
	/*
	 * TODO move the TEMP and TEMP_MAX properties here,
	 * and update the thermal balancer to look here
	 */
};

static int smb2_usb_main_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb2 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_get_charge_param(chg, &chg->param.fv, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblib_get_charge_param(chg, &chg->param.fcc,
							&val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_MAIN;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED:
		rc = smblib_get_prop_input_voltage_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_FCC_DELTA:
		rc = smblib_get_prop_fcc_delta(chg, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_icl_current(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TOGGLE_STAT:
		val->intval = 0;
		break;
	default:
		pr_debug("get prop %d is not supported in usb-main\n", psp);
		rc = -EINVAL;
		break;
	}
	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}
	return 0;
}

static int smb2_usb_main_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb2 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_set_charge_param(chg, &chg->param.fv, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblib_set_charge_param(chg, &chg->param.fcc, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_set_icl_current(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_TOGGLE_STAT:
		rc = smblib_toggle_stat(chg, val->intval);
		break;
	default:
		pr_err("set prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int smb2_usb_main_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_TOGGLE_STAT:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc usb_main_psy_desc = {
	.name		= "main",
	.type		= POWER_SUPPLY_TYPE_MAIN,
	.properties	= smb2_usb_main_props,
	.num_properties	= ARRAY_SIZE(smb2_usb_main_props),
	.get_property	= smb2_usb_main_get_prop,
	.set_property	= smb2_usb_main_set_prop,
	.property_is_writeable = smb2_usb_main_prop_is_writeable,
};

static int smb2_init_usb_main_psy(struct smb2 *chip)
{
	struct power_supply_config usb_main_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_main_cfg.drv_data = chip;
	usb_main_cfg.of_node = chg->dev->of_node;
	chg->usb_main_psy = power_supply_register(chg->dev,
						  &usb_main_psy_desc,
						  &usb_main_cfg);
	if (IS_ERR(chg->usb_main_psy)) {
		pr_err("Couldn't register USB main power supply\n");
		return PTR_ERR(chg->usb_main_psy);
	}

	return 0;
}

/*************************
 * DC PSY REGISTRATION   *
 *************************/
#ifndef VENDOR_EDIT
	/* Jianchao.Shi@BSP.CHG.Basic, 2016/12/26, sjc Delete for charging*/

static enum power_supply_property smb2_dc_props[] = {
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_REAL_TYPE,
};

static int smb2_dc_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb2 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		val->intval = get_effective_result(chg->dc_suspend_votable);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblib_get_prop_dc_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = smblib_get_prop_dc_online(chg, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_dc_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = POWER_SUPPLY_TYPE_WIPOWER;
		break;
	default:
		return -EINVAL;
	}
	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}
	return 0;
}

static int smb2_dc_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb2 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = vote(chg->dc_suspend_votable, WBC_VOTER,
				(bool)val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_set_prop_dc_current_max(chg, val);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int smb2_dc_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc dc_psy_desc = {
	.name = "dc",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = smb2_dc_props,
	.num_properties = ARRAY_SIZE(smb2_dc_props),
	.get_property = smb2_dc_get_prop,
	.set_property = smb2_dc_set_prop,
	.property_is_writeable = smb2_dc_prop_is_writeable,
};

static int smb2_init_dc_psy(struct smb2 *chip)
{
	struct power_supply_config dc_cfg = {};
	struct smb_charger *chg = &chip->chg;

	dc_cfg.drv_data = chip;
	dc_cfg.of_node = chg->dev->of_node;
	chg->dc_psy = power_supply_register(chg->dev,
						  &dc_psy_desc,
						  &dc_cfg);
	if (IS_ERR(chg->dc_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->dc_psy);
	}

	return 0;
}
#endif /* VENDOR_EDIT */


#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/03/07, sjc Add for charging*/
/*************************
 * AC PSY REGISTRATION *
 *************************/
static enum power_supply_property ac_props[] = {
 /*oppo own ac props*/
	POWER_SUPPLY_PROP_ONLINE,
};

static int smb2_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int rc = 0;

    rc = oppo_ac_get_property(psy, psp, val);

	return rc;
}

static const struct power_supply_desc ac_psy_desc = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = ac_props,
	.num_properties = ARRAY_SIZE(ac_props),
	.get_property = smb2_ac_get_property,
};

static int smb2_init_ac_psy(struct smb2 *chip)
{
	struct power_supply_config ac_cfg = {};
	struct smb_charger *chg = &chip->chg;

	ac_cfg.drv_data = chip;
	ac_cfg.of_node = chg->dev->of_node;
	chg->ac_psy = devm_power_supply_register(chg->dev,
						  &ac_psy_desc,
						  &ac_cfg);
	if (IS_ERR(chg->ac_psy)) {
		pr_err("Couldn't register AC power supply\n");
		return PTR_ERR(chg->ac_psy);
	}

	return 0;
}
#endif /* VENDOR_EDIT */

/*************************
 * BATT PSY REGISTRATION *
 *************************/

static enum power_supply_property smb2_batt_props[] = {
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGER_TEMP,
	POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_QNOVO,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_QNOVO,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_SW_JEITA_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_DONE,
	POWER_SUPPLY_PROP_PARALLEL_DISABLE,
	POWER_SUPPLY_PROP_SET_SHIP_MODE,
	POWER_SUPPLY_PROP_DIE_HEALTH,
	POWER_SUPPLY_PROP_RERUN_AICL,
	POWER_SUPPLY_PROP_DP_DM,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
#ifdef VENDOR_EDIT
    /*oppo own battery props*/
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_TECHNOLOGY,
        POWER_SUPPLY_PROP_CAPACITY,
        POWER_SUPPLY_PROP_TEMP,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
        POWER_SUPPLY_PROP_VOLTAGE_MIN,
        POWER_SUPPLY_PROP_CURRENT_NOW,
    
        POWER_SUPPLY_PROP_CHARGE_NOW,
        POWER_SUPPLY_PROP_AUTHENTICATE,
        POWER_SUPPLY_PROP_CHARGE_TIMEOUT,
        POWER_SUPPLY_PROP_CHARGE_TECHNOLOGY,
        POWER_SUPPLY_PROP_FAST_CHARGE,
        POWER_SUPPLY_PROP_MMI_CHARGING_ENABLE,
        POWER_SUPPLY_PROP_BATTERY_FCC,
        POWER_SUPPLY_PROP_BATTERY_SOH,
        POWER_SUPPLY_PROP_BATTERY_CC,
        POWER_SUPPLY_PROP_BATTERY_RM,
        POWER_SUPPLY_PROP_BATTERY_NOTIFY_CODE,
        POWER_SUPPLY_PROP_ADAPTER_FW_UPDATE,
        POWER_SUPPLY_PROP_VOOCCHG_ING,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
#ifdef CONFIG_OPPO_CHECK_CHARGERID_VOLT
        POWER_SUPPLY_PROP_CHARGERID_VOLT,
#endif
#ifdef CONFIG_OPPO_SHIP_MODE_SUPPORT
        POWER_SUPPLY_PROP_SHIP_MODE,
#endif
	POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE,
#ifdef CONFIG_OPPO_SHORT_C_BATT_CHECK
#ifdef CONFIG_OPPO_SHORT_USERSPACE
        POWER_SUPPLY_PROP_SHORT_C_LIMIT_CHG,
        POWER_SUPPLY_PROP_SHORT_C_LIMIT_RECHG,
        POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
        POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
#else
        POWER_SUPPLY_PROP_SHORT_C_BATT_UPDATE_CHANGE,
        POWER_SUPPLY_PROP_SHORT_C_BATT_IN_IDLE,
        POWER_SUPPLY_PROP_SHORT_C_BATT_CV_STATUS,
#endif//CONFIG_OPPO_SHORT_USERSPACE
#endif
#ifdef CONFIG_OPPO_SHORT_HW_CHECK
        POWER_SUPPLY_PROP_SHORT_C_HW_FEATURE,
        POWER_SUPPLY_PROP_SHORT_C_HW_STATUS,
#endif
#ifdef CONFIG_OPPO_SHORT_IC_CHECK
        POWER_SUPPLY_PROP_SHORT_C_IC_OTP_STATUS,
        POWER_SUPPLY_PROP_SHORT_C_IC_VOLT_THRESH,
        POWER_SUPPLY_PROP_SHORT_C_IC_OTP_VALUE,
#endif
#endif//VENDOR_EDIT
};

static int smb2_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb_charger *chg = power_supply_get_drvdata(psy);
	int rc = 0;
#ifndef VENDOR_EDIT
    /* Jianchao.Shi@BSP.CHG.Basic, 2017/03/07, sjc Delete for charging */
        union power_supply_propval pval = {0, };
#endif

	switch (psp) {
#ifndef VENDOR_EDIT
/* LiZhiJie@BSP.CHG.Basic, 2019/04/06, sjc Delete for charging */
	case POWER_SUPPLY_PROP_STATUS:
		rc = smblib_get_prop_batt_status(chg, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = smblib_get_prop_batt_health(chg, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblib_get_prop_batt_present(chg, val);
		break;
#endif
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = !get_effective_result(chg->chg_disable_votable);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smblib_get_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = smblib_get_prop_batt_charge_type(chg, val);
		break;
#ifndef VENDOR_EDIT
/*ZhiJie.Li@BSP.CHG.Basic, 2019/4/1 lzj delete for charger*/
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smblib_get_prop_batt_capacity(chg, val);
		break;
#endif
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = smblib_get_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		rc = smblib_get_prop_system_temp_level_max(chg, val);
		break;
#ifndef VENDOR_EDIT
    /* Jianchao.Shi@BSP.CHG.Basic, 2017/03/07, sjc Modify for charging */
    /* CHARGER_TEMP and CHARGER_TEMP_MAX is dependent on FG and only for HVDCP */
	case POWER_SUPPLY_PROP_CHARGER_TEMP:
		/* do not query RRADC if charger is not present */
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0)
			pr_err("Couldn't get usb present rc=%d\n", rc);

		rc = -ENODATA;
		if (pval.intval)
			rc = smblib_get_prop_charger_temp(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		rc = smblib_get_prop_charger_temp_max(chg, val);
		break;
#else
        case POWER_SUPPLY_PROP_CHARGER_TEMP:
        case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
            val->intval = -1;
            break;
#endif
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		rc = smblib_get_prop_input_current_limited(chg, val);
		break;
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		val->intval = chg->step_chg_enabled;
		break;
	case POWER_SUPPLY_PROP_SW_JEITA_ENABLED:
		val->intval = chg->sw_jeita_enabled;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = get_client_vote(chg->fv_votable,
				BATT_PROFILE_VOTER);
		break;
	case POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE:
		rc = smblib_get_prop_charge_qnovo_enable(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_QNOVO:
		val->intval = get_client_vote_locked(chg->fv_votable,
				QNOVO_VOTER);
		break;
	case POWER_SUPPLY_PROP_CURRENT_QNOVO:
		val->intval = get_client_vote_locked(chg->fcc_votable,
				QNOVO_VOTER);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_client_vote(chg->fcc_votable,
					      BATT_PROFILE_VOTER);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = get_client_vote(chg->fcc_votable,
					      FG_ESR_VOTER);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		rc = smblib_get_prop_batt_charge_done(chg, val);
		break;
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
		val->intval = get_client_vote(chg->pl_disable_votable,
					      USER_VOTER);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as device is active */
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		if (chg->die_health == -EINVAL)
			rc = smblib_get_prop_die_health(chg, val);
		else
			val->intval = chg->die_health;
		break;
	case POWER_SUPPLY_PROP_DP_DM:
		val->intval = chg->pulse_cnt;
		break;
	case POWER_SUPPLY_PROP_RERUN_AICL:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
#ifndef VENDOR_EDIT
/*zhijie li@BSP.CHG.Basic 2019/04/12 lzj delete for charger*/	
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_TEMP:
#endif
		rc = smblib_get_prop_from_bms(chg, psp, val);
		break;
		
#ifdef VENDOR_EDIT
/* Jianchao.Shi@PSW.BSP.CHG.Basic, 2018/04/19, sjc Add for CTS */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
#endif

#ifndef VENDOR_EDIT
/*zhijie li@BSP.CHG.Basic 2019/04/12 lzj delete for charger*/
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = smblib_get_prop_from_bms(chg, psp, val);
		if (!rc)
			val->intval *= (-1);
		break;
#endif	
	case POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE:
		val->intval = chg->fcc_stepper_enable;
		break;
	default:
//#ifdef VENDOR_EDIT
         /*oppo own battery props*/
		rc = oppo_battery_get_property(psy, psp, val);
//#endif
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int smb2_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct smb_charger *chg = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smblib_set_prop_batt_status(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		vote(chg->chg_disable_votable, USER_VOTER, !!!val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smblib_set_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = smblib_set_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smblib_set_prop_batt_capacity(chg, val);
		break;
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
		vote(chg->pl_disable_votable, USER_VOTER, (bool)val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		chg->batt_profile_fv_uv = val->intval;
		vote(chg->fv_votable, BATT_PROFILE_VOTER, true, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE:
		rc = smblib_set_prop_charge_qnovo_enable(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_QNOVO:
		vote(chg->fv_votable, QNOVO_VOTER,
			(val->intval >= 0), val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_QNOVO:
		vote(chg->pl_disable_votable, PL_QNOVO_VOTER,
			val->intval != -EINVAL && val->intval < 2000000, 0);
		if (val->intval == -EINVAL) {
			vote(chg->fcc_votable, BATT_PROFILE_VOTER,
					true, chg->batt_profile_fcc_ua);
			vote(chg->fcc_votable, QNOVO_VOTER, false, 0);
		} else {
			vote(chg->fcc_votable, QNOVO_VOTER, true, val->intval);
			vote(chg->fcc_votable, BATT_PROFILE_VOTER, false, 0);
		}
		break;
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		chg->step_chg_enabled = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_SW_JEITA_ENABLED:
		if (chg->sw_jeita_enabled != (!!val->intval)) {
			rc = smblib_disable_hw_jeita(chg, !!val->intval);
			if (rc == 0)
				chg->sw_jeita_enabled = !!val->intval;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		chg->batt_profile_fcc_ua = val->intval;
		vote(chg->fcc_votable, BATT_PROFILE_VOTER, true, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (val->intval)
			vote(chg->fcc_votable, FG_ESR_VOTER, true, val->intval);
		else
			vote(chg->fcc_votable, FG_ESR_VOTER, false, 0);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as the device is active */
		if (!val->intval)
			break;
		if (chg->pl.psy)
			power_supply_set_property(chg->pl.psy,
				POWER_SUPPLY_PROP_SET_SHIP_MODE, val);
		rc = smblib_set_prop_ship_mode(chg, val);
		break;
	case POWER_SUPPLY_PROP_RERUN_AICL:
		rc = smblib_rerun_aicl(chg);
		break;
	case POWER_SUPPLY_PROP_DP_DM:
		rc = smblib_dp_dm(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		rc = smblib_set_prop_input_current_limited(chg, val);
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		chg->die_health = val->intval;
		power_supply_changed(chg->batt_psy);
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static int smb2_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
        int rc = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
	case POWER_SUPPLY_PROP_DP_DM:
	case POWER_SUPPLY_PROP_RERUN_AICL:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_SW_JEITA_ENABLED:
	case POWER_SUPPLY_PROP_DIE_HEALTH:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = 1;
	default:
#ifdef VENDOR_EDIT
	/*oppo own battery props*/
            rc = oppo_battery_property_is_writeable(psy, psp);
#endif
		break;
	}

	return rc;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = smb2_batt_props,
	.num_properties = ARRAY_SIZE(smb2_batt_props),
	.get_property = smb2_batt_get_prop,
	.set_property = smb2_batt_set_prop,
	.property_is_writeable = smb2_batt_prop_is_writeable,
};

static int smb2_init_batt_psy(struct smb2 *chip)
{
	struct power_supply_config batt_cfg = {};
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;
	chg->batt_psy = power_supply_register(chg->dev,
						   &batt_psy_desc,
						   &batt_cfg);
	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}

	return rc;
}

#ifdef VENDOR_EDIT
/*oppo own battery props*/
static int oppo_power_supply_init(struct smb2 *chip)
{
    int rc = 0;

    rc = smb2_init_ac_psy(chip);
    if (rc < 0) {
        pr_err("Couldn't initialize ac psy rc=%d\n", rc);
        return rc;
    }
//kong
    rc = smb2_init_batt_psy(chip);
    if (rc < 0) {
        pr_err("Couldn't initialize batt psy rc=%d\n", rc);
        return rc;
    }

//kong
    rc = smb2_init_usb_psy(chip);
    if (rc < 0) {
        pr_err("Couldn't initialize usb psy rc=%d\n", rc);
       return rc;
    }

    return rc;
}
#endif /* VENDOR_EDIT */

/******************************
 * VBUS REGULATOR REGISTRATION *
 ******************************/

static struct regulator_ops smb2_vbus_reg_ops = {
	.enable = smblib_vbus_regulator_enable,
	.disable = smblib_vbus_regulator_disable,
	.is_enabled = smblib_vbus_regulator_is_enabled,
};

static int smb2_init_vbus_regulator(struct smb2 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct regulator_config cfg = {};
	int rc = 0;

	chg->vbus_vreg = devm_kzalloc(chg->dev, sizeof(*chg->vbus_vreg),
				      GFP_KERNEL);
	if (!chg->vbus_vreg)
		return -ENOMEM;

	cfg.dev = chg->dev;
	cfg.driver_data = chip;

	chg->vbus_vreg->rdesc.owner = THIS_MODULE;
	chg->vbus_vreg->rdesc.type = REGULATOR_VOLTAGE;
	chg->vbus_vreg->rdesc.ops = &smb2_vbus_reg_ops;
	chg->vbus_vreg->rdesc.of_match = "qcom,smb2-vbus";
	chg->vbus_vreg->rdesc.name = "qcom,smb2-vbus";

	chg->vbus_vreg->rdev = devm_regulator_register(chg->dev,
						&chg->vbus_vreg->rdesc, &cfg);
	if (IS_ERR(chg->vbus_vreg->rdev)) {
		rc = PTR_ERR(chg->vbus_vreg->rdev);
		chg->vbus_vreg->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't register VBUS regualtor rc=%d\n", rc);
	}

	return rc;
}

/******************************
 * VCONN REGULATOR REGISTRATION *
 ******************************/

static struct regulator_ops smb2_vconn_reg_ops = {
	.enable = smblib_vconn_regulator_enable,
	.disable = smblib_vconn_regulator_disable,
	.is_enabled = smblib_vconn_regulator_is_enabled,
};

static int smb2_init_vconn_regulator(struct smb2 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct regulator_config cfg = {};
	int rc = 0;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	chg->vconn_vreg = devm_kzalloc(chg->dev, sizeof(*chg->vconn_vreg),
				      GFP_KERNEL);
	if (!chg->vconn_vreg)
		return -ENOMEM;

	cfg.dev = chg->dev;
	cfg.driver_data = chip;

	chg->vconn_vreg->rdesc.owner = THIS_MODULE;
	chg->vconn_vreg->rdesc.type = REGULATOR_VOLTAGE;
	chg->vconn_vreg->rdesc.ops = &smb2_vconn_reg_ops;
	chg->vconn_vreg->rdesc.of_match = "qcom,smb2-vconn";
	chg->vconn_vreg->rdesc.name = "qcom,smb2-vconn";

	chg->vconn_vreg->rdev = devm_regulator_register(chg->dev,
						&chg->vconn_vreg->rdesc, &cfg);
	if (IS_ERR(chg->vconn_vreg->rdev)) {
		rc = PTR_ERR(chg->vconn_vreg->rdev);
		chg->vconn_vreg->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't register VCONN regualtor rc=%d\n", rc);
	}

	return rc;
}

/***************************
 * HARDWARE INITIALIZATION *
 ***************************/
static int smb2_config_wipower_input_power(struct smb2 *chip, int uw)
{
	int rc;
	int ua;
	struct smb_charger *chg = &chip->chg;
	s64 nw = (s64)uw * 1000;

	if (uw < 0)
		return 0;

	ua = div_s64(nw, ZIN_ICL_PT_MAX_MV);
	rc = smblib_set_charge_param(chg, &chg->param.dc_icl_pt_lv, ua);
	if (rc < 0) {
		pr_err("Couldn't configure dc_icl_pt_lv rc = %d\n", rc);
		return rc;
	}

	ua = div_s64(nw, ZIN_ICL_PT_HV_MAX_MV);
	rc = smblib_set_charge_param(chg, &chg->param.dc_icl_pt_hv, ua);
	if (rc < 0) {
		pr_err("Couldn't configure dc_icl_pt_hv rc = %d\n", rc);
		return rc;
	}

	ua = div_s64(nw, ZIN_ICL_LV_MAX_MV);
	rc = smblib_set_charge_param(chg, &chg->param.dc_icl_div2_lv, ua);
	if (rc < 0) {
		pr_err("Couldn't configure dc_icl_div2_lv rc = %d\n", rc);
		return rc;
	}

	ua = div_s64(nw, ZIN_ICL_MID_LV_MAX_MV);
	rc = smblib_set_charge_param(chg, &chg->param.dc_icl_div2_mid_lv, ua);
	if (rc < 0) {
		pr_err("Couldn't configure dc_icl_div2_mid_lv rc = %d\n", rc);
		return rc;
	}

	ua = div_s64(nw, ZIN_ICL_MID_HV_MAX_MV);
	rc = smblib_set_charge_param(chg, &chg->param.dc_icl_div2_mid_hv, ua);
	if (rc < 0) {
		pr_err("Couldn't configure dc_icl_div2_mid_hv rc = %d\n", rc);
		return rc;
	}

	ua = div_s64(nw, ZIN_ICL_HV_MAX_MV);
	rc = smblib_set_charge_param(chg, &chg->param.dc_icl_div2_hv, ua);
	if (rc < 0) {
		pr_err("Couldn't configure dc_icl_div2_hv rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int smb2_configure_typec(struct smb_charger *chg)
{
	int rc;

	/*
	 * trigger the usb-typec-change interrupt only when the CC state
	 * changes
	 */
	rc = smblib_write(chg, TYPE_C_INTRPT_ENB_REG,
			  TYPEC_CCSTATE_CHANGE_INT_EN_BIT);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	/*
	 * disable Type-C factory mode and stay in Attached.SRC state when VCONN
	 * over-current happens
	 */
	rc = smblib_masked_write(chg, TYPE_C_CFG_REG,
			FACTORY_MODE_DETECTION_EN_BIT | VCONN_OC_CFG_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure Type-C rc=%d\n", rc);
		return rc;
	}

	/* increase VCONN softstart */
	rc = smblib_masked_write(chg, TYPE_C_CFG_2_REG,
			VCONN_SOFTSTART_CFG_MASK, VCONN_SOFTSTART_CFG_MASK);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't increase VCONN softstart rc=%d\n",
			rc);
		return rc;
	}

	/* disable try.SINK mode and legacy cable IRQs */
	rc = smblib_masked_write(chg, TYPE_C_CFG_3_REG, EN_TRYSINK_MODE_BIT |
				TYPEC_NONCOMPLIANT_LEGACY_CABLE_INT_EN_BIT |
				TYPEC_LEGACY_CABLE_INT_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set Type-C config rc=%d\n", rc);
		return rc;
	}

	/* Set CC threshold to 1.6 V in source mode */
	rc = smblib_masked_write(chg, TYPE_C_CFG_2_REG, DFP_CC_1P4V_OR_1P6V_BIT,
				 DFP_CC_1P4V_OR_1P6V_BIT);
	if (rc < 0)
		dev_err(chg->dev,
			"Couldn't configure CC threshold voltage rc=%d\n", rc);

	return rc;
}

static int smb2_disable_typec(struct smb_charger *chg)
{
	int rc;

	/* Move to typeC mode */
	/* configure FSM in idle state and disable UFP_ENABLE bit */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
			TYPEC_DISABLE_CMD_BIT | UFP_EN_CMD_BIT,
			TYPEC_DISABLE_CMD_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't put FSM in idle rc=%d\n", rc);
		return rc;
	}

	/* wait for FSM to enter idle state */
	msleep(200);
	/* configure TypeC mode */
	rc = smblib_masked_write(chg, TYPE_C_CFG_REG,
			TYPE_C_OR_U_USB_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable micro USB mode rc=%d\n", rc);
		return rc;
	}

	/* wait for mode change before enabling FSM */
	usleep_range(10000, 11000);
	/* release FSM from idle state */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
			TYPEC_DISABLE_CMD_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't release FSM rc=%d\n", rc);
		return rc;
	}

	/* wait for FSM to start */
	msleep(100);
	/* move to uUSB mode */
	/* configure FSM in idle state */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
			TYPEC_DISABLE_CMD_BIT, TYPEC_DISABLE_CMD_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't put FSM in idle rc=%d\n", rc);
		return rc;
	}

	/* wait for FSM to enter idle state */
	msleep(200);
	/* configure micro USB mode */
	rc = smblib_masked_write(chg, TYPE_C_CFG_REG,
			TYPE_C_OR_U_USB_BIT, TYPE_C_OR_U_USB_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable micro USB mode rc=%d\n", rc);
		return rc;
	}

	/* wait for mode change before enabling FSM */
	usleep_range(10000, 11000);
	/* release FSM from idle state */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
			TYPEC_DISABLE_CMD_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't release FSM rc=%d\n", rc);
		return rc;
	}

	return rc;
}

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/24, sjc Add for otg id value change support */
static void otg_enable_pmic_id_value (void)
{
	int rc;
	u8 stat;
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	/* set DRP mode */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
			TYPEC_POWER_ROLE_CMD_MASK, 0x0);//bit[2:0]=0
	if (rc < 0) {
		printk(KERN_ERR "[OPPO_CHG][%s]: Couldn't clear 0x1368[0] rc=%d\n", __func__, rc);
	}

	rc = smblib_read(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG, &stat);
	if (rc < 0) {
		printk(KERN_ERR "[OPPO_CHG][%s]: Couldn't read 0x1368 rc=%d\n", __func__, rc);
	} else {
		printk(KERN_ERR "[OPPO_CHG][%s]: reg0x1368[0x%x], bit[2:0]=0(DRP)\n", __func__, stat);
	}
}

static void otg_disable_pmic_id_value (void)
{
	int rc;
	u8 stat;
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (!chip) {
		printk(KERN_ERR "[OPPO_CHG][%s]: smb2_chg not ready!\n", __func__);
		return;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	/* set sink mode only */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
			TYPEC_POWER_ROLE_CMD_MASK, UFP_EN_CMD_BIT);//bit[2:0]=0x4
	if (rc < 0) {
		printk(KERN_ERR "[OPPO_CHG][%s]: Couldn't set 0x1368[2] rc=%d\n", __func__, rc);
	}

	rc = smblib_read(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG, &stat);
	if (rc < 0) {
		printk(KERN_ERR "[OPPO_CHG][%s]: Couldn't read 0x1368 rc=%d\n", __func__, rc);
	} else {
		printk(KERN_ERR "[OPPO_CHG][%s]: reg0x1368[0x%x], bit[2:0]=4(UFP)\n", __func__, stat);
	}

}

void otg_enable_id_value (void)
{
	otg_enable_pmic_id_value();
}

void otg_disable_id_value (void)
{
	otg_disable_pmic_id_value();
}
#endif /* VENDOR_EDIT */

static int smb2_init_hw(struct smb2 *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc;
	u8 stat, val;

	if (chip->dt.no_battery)
		chg->fake_capacity = 50;

	if (chg->batt_profile_fcc_ua < 0)
		smblib_get_charge_param(chg, &chg->param.fcc,
				&chg->batt_profile_fcc_ua);

	if (chg->batt_profile_fv_uv < 0)
		smblib_get_charge_param(chg, &chg->param.fv,
				&chg->batt_profile_fv_uv);

	smblib_get_charge_param(chg, &chg->param.usb_icl,
				&chg->default_icl_ua);
	if (chip->dt.usb_icl_ua < 0)
		chip->dt.usb_icl_ua = chg->default_icl_ua;

	if (chip->dt.dc_icl_ua < 0)
		smblib_get_charge_param(chg, &chg->param.dc_icl,
					&chip->dt.dc_icl_ua);

	if (chip->dt.min_freq_khz > 0) {
		chg->param.freq_buck.min_u = chip->dt.min_freq_khz;
		chg->param.freq_boost.min_u = chip->dt.min_freq_khz;
	}

	if (chip->dt.max_freq_khz > 0) {
		chg->param.freq_buck.max_u = chip->dt.max_freq_khz;
		chg->param.freq_boost.max_u = chip->dt.max_freq_khz;
	}

	/* set a slower soft start setting for OTG */
	rc = smblib_masked_write(chg, DC_ENG_SSUPPLY_CFG2_REG,
				ENG_SSUPPLY_IVREF_OTG_SS_MASK, OTG_SS_SLOW);
	if (rc < 0) {
		pr_err("Couldn't set otg soft start rc=%d\n", rc);
		return rc;
	}

	/* set OTG current limit */
	rc = smblib_set_charge_param(chg, &chg->param.otg_cl,
				(chg->wa_flags & OTG_WA) ?
				chg->param.otg_cl.min_u : chg->otg_cl_ua);
	if (rc < 0) {
		pr_err("Couldn't set otg current limit rc=%d\n", rc);
		return rc;
	}
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/05/27, sjc Add for OTG battery UVLO */
	smblib_masked_write(chg, BAT_UVLO_THRESHOLD_CFG_REG, BAT_UVLO_THRESHOLD_MASK, 0x3);
#endif

	chg->boost_threshold_ua = chip->dt.boost_threshold_ua;

	rc = smblib_read(chg, APSD_RESULT_STATUS_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read APSD_RESULT_STATUS rc=%d\n", rc);
		return rc;
	}

	smblib_rerun_apsd_if_required(chg);

	/* clear the ICL override if it is set */
	if (smblib_icl_override(chg, false) < 0) {
		pr_err("Couldn't disable ICL override rc=%d\n", rc);
		return rc;
	}

	/* votes must be cast before configuring software control */
	/* vote 0mA on usb_icl for non battery platforms */
	vote(chg->usb_icl_votable,
		DEFAULT_VOTER, chip->dt.no_battery, 0);
	vote(chg->dc_suspend_votable,
		DEFAULT_VOTER, chip->dt.no_battery, 0);
	vote(chg->fcc_votable,
		BATT_PROFILE_VOTER, true, chg->batt_profile_fcc_ua);
	vote(chg->fv_votable,
		BATT_PROFILE_VOTER, true, chg->batt_profile_fv_uv);
	vote(chg->dc_icl_votable,
		DEFAULT_VOTER, true, chip->dt.dc_icl_ua);
	vote(chg->hvdcp_disable_votable_indirect, DEFAULT_VOTER,
		chip->dt.hvdcp_disable, 0);
	vote(chg->pd_disallowed_votable_indirect, CC_DETACHED_VOTER,
			true, 0);
	vote(chg->pd_disallowed_votable_indirect, HVDCP_TIMEOUT_VOTER,
			true, 0);
	vote(chg->pd_disallowed_votable_indirect, PD_NOT_SUPPORTED_VOTER,
			chip->dt.no_pd, 0);
	/*
	 * AICL configuration:
	 * start from min and AICL ADC disable
	 */
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/06, sjc Modify for charging */
	rc = smblib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG,
			SUSPEND_ON_COLLAPSE_USBIN_BIT | USBIN_AICL_START_AT_MAX_BIT
				| USBIN_AICL_ADC_EN_BIT | USBIN_AICL_RERUN_EN_BIT, USBIN_AICL_RERUN_EN_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure AICL rc=%d\n", rc);
		return rc;
	}
#else

	rc = smblib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG,
			USBIN_AICL_START_AT_MAX_BIT
				| USBIN_AICL_ADC_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure AICL rc=%d\n", rc);
		return rc;
	}
#endif
	/* Configure charge enable for software control; active high */
	rc = smblib_masked_write(chg, CHGR_CFG2_REG,
				 CHG_EN_POLARITY_BIT |
				 CHG_EN_SRC_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure charger rc=%d\n", rc);
		return rc;
	}

	/* enable the charging path */
	rc = vote(chg->chg_disable_votable, DEFAULT_VOTER, false, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable charging rc=%d\n", rc);
		return rc;
	}

	/* Check USB connector type (typeC/microUSB) */
	rc = smblib_read(chg, RID_CC_CONTROL_7_0_REG, &val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't read RID_CC_CONTROL_7_0 rc=%d\n",
			rc);
		return rc;
	}
	chg->connector_type = (val & EN_MICRO_USB_MODE_BIT) ?
					POWER_SUPPLY_CONNECTOR_MICRO_USB
					: POWER_SUPPLY_CONNECTOR_TYPEC;
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		rc = smb2_disable_typec(chg);
	else
		rc = smb2_configure_typec(chg);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	/* Connector types based votes */
	vote(chg->hvdcp_disable_votable_indirect, PD_INACTIVE_VOTER,
		(chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC), 0);
	vote(chg->hvdcp_disable_votable_indirect, VBUS_CC_SHORT_VOTER,
		(chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC), 0);
	vote(chg->pd_disallowed_votable_indirect, MICRO_USB_VOTER,
		(chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB), 0);
#ifndef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2016/12/20, sjc Modify for charging */
	vote(chg->hvdcp_enable_votable, MICRO_USB_VOTER,
		(chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB), 0);
#else
	vote(chg->hvdcp_enable_votable, MICRO_USB_VOTER,
			false, 0);
#endif

	/* configure VCONN for software control */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				 VCONN_EN_SRC_BIT | VCONN_EN_VALUE_BIT,
				 VCONN_EN_SRC_BIT);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure VCONN for SW control rc=%d\n", rc);
		return rc;
	}

	/* configure VBUS for software control */
	rc = smblib_masked_write(chg, OTG_CFG_REG, OTG_EN_SRC_CFG_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure VBUS for SW control rc=%d\n", rc);
		return rc;
	}

	val = (ilog2(chip->dt.wd_bark_time / 16) << BARK_WDOG_TIMEOUT_SHIFT) &
						BARK_WDOG_TIMEOUT_MASK;
	val |= BITE_WDOG_TIMEOUT_8S;
	rc = smblib_masked_write(chg, SNARL_BARK_BITE_WD_CFG_REG,
			BITE_WDOG_DISABLE_CHARGING_CFG_BIT |
			BARK_WDOG_TIMEOUT_MASK | BITE_WDOG_TIMEOUT_MASK,
			val);
	if (rc) {
		pr_err("Couldn't configue WD config rc=%d\n", rc);
		return rc;
	}

	/* enable WD BARK and enable it on plugin */
	rc = smblib_masked_write(chg, WD_CFG_REG,
			WATCHDOG_TRIGGER_AFP_EN_BIT |
			WDOG_TIMER_EN_ON_PLUGIN_BIT |
			BARK_WDOG_INT_EN_BIT,
			WDOG_TIMER_EN_ON_PLUGIN_BIT |
			BARK_WDOG_INT_EN_BIT);
	if (rc) {
		pr_err("Couldn't configue WD config rc=%d\n", rc);
		return rc;
	}

	/* configure wipower watts */
	rc = smb2_config_wipower_input_power(chip, chip->dt.wipower_max_uw);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure wipower rc=%d\n", rc);
		return rc;
	}
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/02/14, sjc Add for charging */
	/* disable FG default iterm */
	rc = smblib_masked_write(chg, FG_UPDATE_CFG_2_SEL_REG,
				IBT_LT_CHG_TERM_THRESH_SEL_BIT, 1);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't disable FG iterm override rc=%d\n",
			rc);
	}

	rc = smblib_masked_write(chg, CHGR_CFG2_REG, I_TERM_BIT, 1);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't disable PM660 iterm override rc=%d\n",
			rc);
	}
#endif

	/* disable h/w autonomous parallel charging control */
	rc = smblib_masked_write(chg, MISC_CFG_REG,
				 STAT_PARALLEL_1400MA_EN_CFG_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't disable h/w autonomous parallel control rc=%d\n",
			rc);
		return rc;
	}

	/*
	 * allow DRP.DFP time to exceed by tPDdebounce time.
	 */
	rc = smblib_masked_write(chg, TAPER_TIMER_SEL_CFG_REG,
				TYPEC_DRP_DFP_TIME_CFG_BIT,
				TYPEC_DRP_DFP_TIME_CFG_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure DRP.DFP time rc=%d\n",
			rc);
		return rc;
	}
	
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/03/29, sjc Add for TYPE_C_CHANGE_IRQ storm(and counter current) */
	smblib_masked_write(chg, 0x1380, 0x03, 0x3);
	smblib_masked_write(chg, 0x1365, 0x03, 0x3);
#endif
	
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/06/28, sjc Add for reducing DCD timeout */
	smblib_masked_write(chg, 0x1363, 0x20, 0);
#endif
	
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/06/29, sjc Add for reducing chg delay after unsuspend */
	smblib_masked_write(chg, 0x1052, 0x02, 0);
	smblib_masked_write(chg, 0x1053, 0x40, 0);
#endif
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/01/23, sjc Add for disable thermal cfg */
	smblib_masked_write(chg, 0x1670, 0xff, 0);
#endif /*VENDOR_EDIT*/
#ifdef VENDOR_EDIT
	/* tongfeng.Huang@BSP.CHG.Basic, 2018/07/05, sjc Add for set hw aicl to 4.4V */
	smblib_masked_write(chg, 0x1381, 0x07, 0x4);		// set hw aicl to 4.4V
	fg_oppo_set_input_current = false;
#endif /*VENDOR_EDIT*/

	/* configure float charger options */
	switch (chip->dt.float_option) {
	case 1:
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK, 0);
		break;
	case 2:
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK, FORCE_FLOAT_SDP_CFG_BIT);
		break;
	case 3:
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK, FLOAT_DIS_CHGING_CFG_BIT);
		break;
	case 4:
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK, SUSPEND_FLOAT_CFG_BIT);
		break;
	default:
		rc = 0;
		break;
	}

	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure float charger options rc=%d\n",
			rc);
		return rc;
	}

	rc = smblib_read(chg, USBIN_OPTIONS_2_CFG_REG, &chg->float_cfg);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't read float charger options rc=%d\n",
			rc);
		return rc;
	}

	switch (chip->dt.chg_inhibit_thr_mv) {
	case 50:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				CHARGE_INHIBIT_THRESHOLD_50MV);
		break;
	case 100:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				CHARGE_INHIBIT_THRESHOLD_100MV);
		break;
	case 200:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				CHARGE_INHIBIT_THRESHOLD_200MV);
		break;
	case 300:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				CHARGE_INHIBIT_THRESHOLD_300MV);
		break;
	case 0:
		rc = smblib_masked_write(chg, CHGR_CFG2_REG,
				CHARGER_INHIBIT_BIT, 0);
	default:
		break;
	}

	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure charge inhibit threshold rc=%d\n",
			rc);
		return rc;
	}

	if (chip->dt.auto_recharge_soc) {
		rc = smblib_masked_write(chg, FG_UPDATE_CFG_2_SEL_REG,
				SOC_LT_CHG_RECHARGE_THRESH_SEL_BIT |
				VBT_LT_CHG_RECHARGE_THRESH_SEL_BIT,
				VBT_LT_CHG_RECHARGE_THRESH_SEL_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure FG_UPDATE_CFG2_SEL_REG rc=%d\n",
				rc);
			return rc;
		}
	} else {
		rc = smblib_masked_write(chg, FG_UPDATE_CFG_2_SEL_REG,
				SOC_LT_CHG_RECHARGE_THRESH_SEL_BIT |
				VBT_LT_CHG_RECHARGE_THRESH_SEL_BIT,
				SOC_LT_CHG_RECHARGE_THRESH_SEL_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure FG_UPDATE_CFG2_SEL_REG rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chg->sw_jeita_enabled) {
		rc = smblib_disable_hw_jeita(chg, true);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't set hw jeita rc=%d\n", rc);
			return rc;
		}
	}

	if (chg->disable_stat_sw_override) {
		rc = smblib_masked_write(chg, STAT_CFG_REG,
				STAT_SW_OVERRIDE_CFG_BIT, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable STAT SW override rc=%d\n",
				rc);
			return rc;
		}
	}

	return rc;
}

static int smb2_post_init(struct smb2 *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc;
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/01/30, sjc Add for using gpio as CC detect */
    int level = 0;
#endif
	/* In case the usb path is suspended, we would have missed disabling
	 * the icl change interrupt because the interrupt could have been
	 * not requested
	 */
	rerun_election(chg->usb_icl_votable);

	/* configure power role for dual-role */
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/01/30, sjc Add for using gpio as CC detect */
    if (g_oppo_chip && oppo_ccdetect_check_is_gpio(g_oppo_chip) == true) {
        level = gpio_get_value(chg->ccdetect_gpio);
        usleep_range(2000, 2100);
        if (level != gpio_get_value(chg->ccdetect_gpio)) {
            printk(KERN_ERR "[OPPO_CHG][%s]: ccdetect_gpio is unstable, try again...\n", __func__);
            usleep_range(10000, 11000);
            level = gpio_get_value(chg->ccdetect_gpio);
        }
        if (level <= 0) {
            //oppo_ccdetect_enable();
            otg_enable_pmic_id_value();
        }
	/* Force charger in Sink Only mode */
     } else if (chg->ufp_only_mode) {
		rc = smblib_read(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				&stat);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't read SOFTWARE_CTRL_REG rc=%d\n", rc);
			return rc;
		}

		if (!(stat & UFP_EN_CMD_BIT)) {
			/* configure charger in UFP only mode */
			rc  = smblib_force_ufp(chg);
			if (rc < 0) {
				dev_err(chg->dev,
					"Couldn't force UFP mode rc=%d\n", rc);
				return rc;
			}
		}
	} else {
		/* configure power role for dual-role */
		rc = smblib_masked_write(chg,
					TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
					TYPEC_POWER_ROLE_CMD_MASK, 0);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't configure power role for DRP rc=%d\n",
				rc);
			return rc;
		}
	#endif

	rerun_election(chg->usb_irq_enable_votable);

	return 0;
}

static int smb2_chg_config_init(struct smb2 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct pmic_revid_data *pmic_rev_id;
	struct device_node *revid_dev_node;

	revid_dev_node = of_parse_phandle(chip->chg.dev->of_node,
					  "qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property\n");
		return -EINVAL;
	}

	pmic_rev_id = get_revid_data(revid_dev_node);
	if (IS_ERR_OR_NULL(pmic_rev_id)) {
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	switch (pmic_rev_id->pmic_subtype) {
	case PMI8998_SUBTYPE:
		chip->chg.smb_version = PMI8998_SUBTYPE;
		chip->chg.wa_flags |= BOOST_BACK_WA | QC_AUTH_INTERRUPT_WA_BIT
				| TYPEC_PBS_WA_BIT;
		if (pmic_rev_id->rev4 == PMI8998_V1P1_REV4) /* PMI rev 1.1 */
			chg->wa_flags |= QC_CHARGER_DETECTION_WA_BIT;
		if (pmic_rev_id->rev4 == PMI8998_V2P0_REV4) /* PMI rev 2.0 */
			chg->wa_flags |= TYPEC_CC2_REMOVAL_WA_BIT;
		chg->chg_freq.freq_5V		= 600;
		chg->chg_freq.freq_6V_8V	= 800;
		chg->chg_freq.freq_9V		= 1000;
		chg->chg_freq.freq_12V		= 1200;
		chg->chg_freq.freq_removal	= 1000;
		chg->chg_freq.freq_below_otg_threshold = 2000;
		chg->chg_freq.freq_above_otg_threshold = 800;
		break;
	case PM660_SUBTYPE:
		chip->chg.smb_version = PM660_SUBTYPE;
#ifdef VENDOR_EDIT
/* tongfeng.Huang@BSP.CHG.Basic, 2018/07/09, sjc Add for SVOOC OTG*/
		if (g_oppo_chip != NULL && g_oppo_chip->vbatt_num == 2) {
			chip->chg.wa_flags |= BOOST_BACK_WA;
		}
		else{
			chip->chg.wa_flags |= BOOST_BACK_WA | OTG_WA;
		}
#else
		chip->chg.wa_flags |= BOOST_BACK_WA | OTG_WA | OV_IRQ_WA_BIT
				| TYPEC_PBS_WA_BIT;
#endif
		chg->param.freq_buck = pm660_params.freq_buck;
		chg->param.freq_boost = pm660_params.freq_boost;
		chg->chg_freq.freq_5V		= 650;
		chg->chg_freq.freq_6V_8V	= 850;
		chg->chg_freq.freq_9V		= 1050;
		chg->chg_freq.freq_12V		= 1200;
		chg->chg_freq.freq_removal	= 1050;
		chg->chg_freq.freq_below_otg_threshold = 1600;
		chg->chg_freq.freq_above_otg_threshold = 800;
		break;
	default:
		pr_err("PMIC subtype %d not supported\n",
				pmic_rev_id->pmic_subtype);
		return -EINVAL;
	}

	return 0;
}

/****************************
 * DETERMINE INITIAL STATUS *
 ****************************/

static int smb2_determine_initial_status(struct smb2 *chip)
{
	struct smb_irq_data irq_data = {chip, "determine-initial-status"};
	struct smb_charger *chg = &chip->chg;

	if (chg->bms_psy)
		smblib_suspend_on_debug_battery(chg);
	smblib_handle_usb_plugin(0, &irq_data);
	smblib_handle_usb_typec_change(0, &irq_data);
	smblib_handle_usb_source_change(0, &irq_data);
	smblib_handle_chg_state_change(0, &irq_data);
	smblib_handle_icl_change(0, &irq_data);
	smblib_handle_batt_temp_changed(0, &irq_data);
	smblib_handle_wdog_bark(0, &irq_data);

	return 0;
}

/**************************
 * INTERRUPT REGISTRATION *
 **************************/

static struct smb_irq_info smb2_irqs[] = {
/* CHARGER IRQs */
	[CHG_ERROR_IRQ] = {
		.name		= "chg-error",
		.handler	= smblib_handle_debug,
	},
	[CHG_STATE_CHANGE_IRQ] = {
		.name		= "chg-state-change",
		.handler	= smblib_handle_chg_state_change,
		.wake		= true,
	},
	[STEP_CHG_STATE_CHANGE_IRQ] = {
		.name		= "step-chg-state-change",
		.handler	= NULL,
	},
	[STEP_CHG_SOC_UPDATE_FAIL_IRQ] = {
		.name		= "step-chg-soc-update-fail",
		.handler	= NULL,
	},
	[STEP_CHG_SOC_UPDATE_REQ_IRQ] = {
		.name		= "step-chg-soc-update-request",
		.handler	= NULL,
	},
/* OTG IRQs */
	[OTG_FAIL_IRQ] = {
		.name		= "otg-fail",
		.handler	= smblib_handle_debug,
	},
	[OTG_OVERCURRENT_IRQ] = {
		.name		= "otg-overcurrent",
		.handler	= smblib_handle_otg_overcurrent,
	},
	[OTG_OC_DIS_SW_STS_IRQ] = {
		.name		= "otg-oc-dis-sw-sts",
		.handler	= smblib_handle_debug,
	},
	[TESTMODE_CHANGE_DET_IRQ] = {
		.name		= "testmode-change-detect",
		.handler	= smblib_handle_debug,
	},
/* BATTERY IRQs */
	[BATT_TEMP_IRQ] = {
		.name		= "bat-temp",
		.handler	= smblib_handle_batt_temp_changed,
		.wake		= true,
	},
	[BATT_OCP_IRQ] = {
		.name		= "bat-ocp",
		.handler	= smblib_handle_batt_psy_changed,
	},
	[BATT_OV_IRQ] = {
		.name		= "bat-ov",
		.handler	= smblib_handle_batt_psy_changed,
	},
	[BATT_LOW_IRQ] = {
		.name		= "bat-low",
		.handler	= smblib_handle_batt_psy_changed,
	},
	[BATT_THERM_ID_MISS_IRQ] = {
		.name		= "bat-therm-or-id-missing",
		.handler	= smblib_handle_batt_psy_changed,
	},
	[BATT_TERM_MISS_IRQ] = {
		.name		= "bat-terminal-missing",
		.handler	= smblib_handle_batt_psy_changed,
	},
/* USB INPUT IRQs */
	[USBIN_COLLAPSE_IRQ] = {
		.name		= "usbin-collapse",
		.handler	= smblib_handle_debug,
	},
	[USBIN_LT_3P6V_IRQ] = {
		.name		= "usbin-lt-3p6v",
		.handler	= smblib_handle_debug,
	},
	[USBIN_UV_IRQ] = {
		.name		= "usbin-uv",
		.handler	= smblib_handle_usbin_uv,
	},
	[USBIN_OV_IRQ] = {
		.name		= "usbin-ov",
		.handler	= smblib_handle_debug,
	},
	[USBIN_PLUGIN_IRQ] = {
		.name		= "usbin-plugin",
		.handler	= smblib_handle_usb_plugin,
		.wake		= true,
	},
	[USBIN_SRC_CHANGE_IRQ] = {
		.name		= "usbin-src-change",
		.handler	= smblib_handle_usb_source_change,
		.wake		= true,
	},
	[USBIN_ICL_CHANGE_IRQ] = {
		.name		= "usbin-icl-change",
		.handler	= smblib_handle_icl_change,
		.wake		= true,
	},
	[TYPE_C_CHANGE_IRQ] = {
		.name		= "type-c-change",
		.handler	= smblib_handle_usb_typec_change,
		.wake		= true,
	},
/* DC INPUT IRQs */
	[DCIN_COLLAPSE_IRQ] = {
		.name		= "dcin-collapse",
		.handler	= smblib_handle_debug,
	},
	[DCIN_LT_3P6V_IRQ] = {
		.name		= "dcin-lt-3p6v",
		.handler	= smblib_handle_debug,
	},
	[DCIN_UV_IRQ] = {
		.name		= "dcin-uv",
		.handler	= smblib_handle_debug,
	},
	[DCIN_OV_IRQ] = {
		.name		= "dcin-ov",
		.handler	= smblib_handle_debug,
	},
	[DCIN_PLUGIN_IRQ] = {
		.name		= "dcin-plugin",
		.handler	= smblib_handle_dc_plugin,
		.wake		= true,
	},
	[DIV2_EN_DG_IRQ] = {
		.name		= "div2-en-dg",
		.handler	= smblib_handle_debug,
	},
	[DCIN_ICL_CHANGE_IRQ] = {
		.name		= "dcin-icl-change",
		.handler	= smblib_handle_debug,
	},
/* MISCELLANEOUS IRQs */
	[WDOG_SNARL_IRQ] = {
		.name		= "wdog-snarl",
		.handler	= NULL,
	},
	[WDOG_BARK_IRQ] = {
		.name		= "wdog-bark",
		.handler	= smblib_handle_wdog_bark,
		.wake		= true,
	},
	[AICL_FAIL_IRQ] = {
		.name		= "aicl-fail",
		.handler	= smblib_handle_debug,
	},
	[AICL_DONE_IRQ] = {
		.name		= "aicl-done",
		.handler	= smblib_handle_debug,
	},
	[HIGH_DUTY_CYCLE_IRQ] = {
		.name		= "high-duty-cycle",
		.handler	= smblib_handle_high_duty_cycle,
		.wake		= true,
	},
	[INPUT_CURRENT_LIMIT_IRQ] = {
		.name		= "input-current-limiting",
		.handler	= smblib_handle_debug,
	},
	[TEMPERATURE_CHANGE_IRQ] = {
		.name		= "temperature-change",
		.handler	= smblib_handle_debug,
	},
	[SWITCH_POWER_OK_IRQ] = {
		.name		= "switcher-power-ok",
		.handler	= smblib_handle_switcher_power_ok,
		.wake		= true,
		.storm_data	= {true, 1000, 8},
	},
};

static int smb2_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb2_irqs); i++) {
		if (strcmp(smb2_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int smb2_request_interrupt(struct smb2 *chip,
				struct device_node *node, const char *irq_name)
{
	struct smb_charger *chg = &chip->chg;
	int rc, irq, irq_index;
	struct smb_irq_data *irq_data;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		pr_err("Couldn't get irq %s byname\n", irq_name);
		return irq;
	}

	irq_index = smb2_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	if (!smb2_irqs[irq_index].handler)
		return 0;

	irq_data = devm_kzalloc(chg->dev, sizeof(*irq_data), GFP_KERNEL);
	if (!irq_data)
		return -ENOMEM;

	irq_data->parent_data = chip;
	irq_data->name = irq_name;
	irq_data->storm_data = smb2_irqs[irq_index].storm_data;
	mutex_init(&irq_data->storm_data.storm_lock);

	rc = devm_request_threaded_irq(chg->dev, irq, NULL,
					smb2_irqs[irq_index].handler,
					IRQF_ONESHOT, irq_name, irq_data);
	if (rc < 0) {
		pr_err("Couldn't request irq %d\n", irq);
		return rc;
	}

	smb2_irqs[irq_index].irq = irq;
	smb2_irqs[irq_index].irq_data = irq_data;
	if (smb2_irqs[irq_index].wake)
		enable_irq_wake(irq);

	return rc;
}

static int smb2_request_interrupts(struct smb2 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	struct device_node *child;
	int rc = 0;
	const char *name;
	struct property *prop;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
					    prop, name) {
			rc = smb2_request_interrupt(chip, child, name);
			if (rc < 0)
				return rc;
		}
	}
	if (chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq)
		chg->usb_icl_change_irq_enabled = true;

	return rc;
}

static void smb2_free_interrupts(struct smb_charger *chg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb2_irqs); i++) {
		if (smb2_irqs[i].irq > 0) {
			if (smb2_irqs[i].wake)
				disable_irq_wake(smb2_irqs[i].irq);

			devm_free_irq(chg->dev, smb2_irqs[i].irq,
					smb2_irqs[i].irq_data);
		}
	}
}

static void smb2_disable_interrupts(struct smb_charger *chg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb2_irqs); i++) {
		if (smb2_irqs[i].irq > 0)
			disable_irq(smb2_irqs[i].irq);
	}
}

#if defined(CONFIG_DEBUG_FS)

static int force_batt_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->batt_psy);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_batt_psy_update_ops, NULL,
			force_batt_psy_update_write, "0x%02llx\n");

static int force_usb_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->usb_psy);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_usb_psy_update_ops, NULL,
			force_usb_psy_update_write, "0x%02llx\n");

static int force_dc_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;
	
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/05/09, sjc Add for charging */
	if (chg->dc_psy)
#endif
	power_supply_changed(chg->dc_psy);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_dc_psy_update_ops, NULL,
			force_dc_psy_update_write, "0x%02llx\n");

static void smb2_create_debugfs(struct smb2 *chip)
{
	struct dentry *file;

	chip->dfs_root = debugfs_create_dir("charger", NULL);
	if (IS_ERR_OR_NULL(chip->dfs_root)) {
		pr_err("Couldn't create charger debugfs rc=%ld\n",
			(long)chip->dfs_root);
		return;
	}

	file = debugfs_create_file("force_batt_psy_update", 0600,
			    chip->dfs_root, chip, &force_batt_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_batt_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_file("force_usb_psy_update", 0600,
			    chip->dfs_root, chip, &force_usb_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_usb_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_file("force_dc_psy_update", 0600,
			    chip->dfs_root, chip, &force_dc_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_dc_psy_update file rc=%ld\n",
			(long)file);
}

#else

static void smb2_create_debugfs(struct smb2 *chip)
{}

#endif

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/05/22, sjc Add for dump registers */
static bool d_reg_mask = false;
static ssize_t dump_registers_mask_write(struct file *file, const char __user *buff, size_t count, loff_t *ppos)
{
	char mask[16];

	if (copy_from_user(&mask, buff, count)) {
		printk(KERN_ERR "dump_registers_mask_write error.\n");
		return -EFAULT;
	}

	if (strncmp(mask, "dump808", 7) == 0) {
		d_reg_mask = true;
		printk(KERN_ERR "dump registers mask enable.\n");
	} else {
		d_reg_mask = false;
		return -EFAULT;
	}

	return count;
}

static const struct file_operations dump_registers_mask_fops = {
	.write = dump_registers_mask_write,
	.llseek = noop_llseek,
};

static void init_proc_dump_registers_mask(void)
{
	if (!proc_create("d_reg_mask", S_IWUSR | S_IWGRP | S_IWOTH, NULL, &dump_registers_mask_fops)) {
		printk(KERN_ERR "proc_create dump_registers_mask_fops fail\n");
	}
}
#endif /* VENDOR_EDIT */

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2016/12/26, sjc Add for charging*/
//static int get_boot_mode(void);
static int smbchg_usb_suspend_disable(void);
static int smbchg_usb_suspend_enable(void);
static int smbchg_charging_enble(void);
bool oppo_chg_is_usb_present(void);
int qpnp_get_prop_charger_voltage_now(void);

static void dump_regs(void)
{
	int i;
	int j;
	int rc;
	u8 stat;
	int base[] = {0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1600, 0x1800, 0x1900};
	struct smb_charger *chg = NULL;

	if (!g_oppo_chip || !d_reg_mask)
		return;

	chg = &g_oppo_chip->pmic_spmi.smb2_chip->chg;
	if (!chg)
		return;

	pr_err("================= %s: begin ======================\n", __func__);

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 255; i++) {
			rc = smblib_read(chg, base[j] + i, &stat);
			if (rc < 0) {
				pr_err("Couldn't read %x rc=%d\n", base[j] + i, rc);
			} else {
				pr_err("%x : %x\n", base[j] + i, stat);
			}
		}

		msleep(1000);
	}

	pr_err("================= %s: end ======================\n", __func__);

	d_reg_mask = false;
}

static int smbchg_kick_wdt(void)
{
	return 0;
}

static int oppo_chg_hw_init(void)
{
	int boot_mode = get_boot_mode();

	if (boot_mode != MSM_BOOT_MODE__RF && boot_mode != MSM_BOOT_MODE__WLAN) {
		smbchg_usb_suspend_disable();
	} else {
		smbchg_usb_suspend_enable();
	}
	smbchg_charging_enble();

	return 0;
}

static int smbchg_set_fastchg_current_raw(int current_ma)
{
	int rc = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	rc = vote(chip->pmic_spmi.smb2_chip->chg.fcc_votable, DEFAULT_VOTER,
			true, current_ma * 1000);
	if (rc < 0)
		chg_err("Couldn't vote fcc_votable[%d], rc=%d\n", current_ma, rc);

	return rc;
}

static void smbchg_set_aicl_point(int vol)
{
	struct oppo_chg_chip *chip = g_oppo_chip;

	if(chip == NULL){
		chg_err("%s: g_oppo_chip is not ready\n", __FUNCTION__);
		return;
	}
	if (vol > 4100) {
		smblib_masked_write(&chip->pmic_spmi.smb2_chip->chg, 0x1381, 0x07, 0x5);
	} else {
		smblib_masked_write(&chip->pmic_spmi.smb2_chip->chg, 0x1381, 0x07, 0x4);
	}
	
}

static void smbchg_aicl_enable(bool enable)
{
	int rc = 0;
	u8 aicl_op;
	struct oppo_chg_chip *chip = g_oppo_chip;

	rc = smblib_masked_write(&chip->pmic_spmi.smb2_chip->chg, USBIN_AICL_OPTIONS_CFG_REG,
			USBIN_AICL_EN_BIT, enable ? USBIN_AICL_EN_BIT : 0);
	if (rc < 0)
		chg_err("Couldn't write USBIN_AICL_OPTIONS_CFG_REG rc=%d\n", rc);
	rc = smblib_read(&chip->pmic_spmi.smb2_chip->chg, 0x1380, &aicl_op);
	if (!rc)
		chg_err("AICL_OPTIONS 0x1380 = 0x%02x\n", aicl_op); //dump 0x1380
}
static void smbchg_usbin_collapse_irq_enable(bool enable)
{
	static bool collapse_en = true;
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (enable & !collapse_en){
		enable_irq(chip->pmic_spmi.smb2_chip->chg.irq_info[USBIN_COLLAPSE_IRQ].irq);
	}else if (!enable && collapse_en){
		disable_irq(chip->pmic_spmi.smb2_chip->chg.irq_info[USBIN_COLLAPSE_IRQ].irq);
	}
	collapse_en = enable;
}
static void smbchg_rerun_aicl(void)
{
	smbchg_aicl_enable(false);
	/* Add a delay so that AICL successfully clears */
	msleep(50);
	smbchg_aicl_enable(true);
}

static bool  oppo_chg_is_normal_mode(void)
{
	int boot_mode = get_boot_mode();

	if (boot_mode == MSM_BOOT_MODE__RF || boot_mode == MSM_BOOT_MODE__WLAN)
		return false;
	return true;
}

static bool oppo_chg_is_suspend_status(void)
{
	int rc = 0;
	u8 stat;
	struct smb_charger *chg = NULL;

	if (!g_oppo_chip)
		return false;

	chg = &g_oppo_chip->pmic_spmi.smb2_chip->chg;

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		printk(KERN_ERR "oppo_chg_is_suspend_status: Couldn't read POWER_PATH_STATUS rc=%d\n", rc);
		return false;
	}

	return (bool)(stat & USBIN_SUSPEND_STS_BIT);
}

static void oppo_chg_clear_suspend(void)
{
	int rc;
	struct smb_charger *chg = NULL;

	if (!g_oppo_chip)
		return;

	chg = &g_oppo_chip->pmic_spmi.smb2_chip->chg;

	rc = smblib_masked_write(chg, USBIN_CMD_IL_REG, USBIN_SUSPEND_BIT, 1);
	if (rc < 0) {
		printk(KERN_ERR "oppo_chg_monitor_work: Couldn't set USBIN_SUSPEND_BIT rc=%d\n", rc);
	}
	msleep(50);
	rc = smblib_masked_write(chg, USBIN_CMD_IL_REG, USBIN_SUSPEND_BIT, 0);
	if (rc < 0) {
		printk(KERN_ERR "oppo_chg_monitor_work: Couldn't clear USBIN_SUSPEND_BIT rc=%d\n", rc);
	}
}

static void oppo_chg_check_clear_suspend(void)
{
	use_present_status = true;
	oppo_chg_clear_suspend();
	use_present_status = false;
}

static int usb_icl[] = {
	300, 500, 900, 1200, 1350, 1500, 1750, 2000, 3000,
};

#define USBIN_25MA	25000
static int oppo_chg_set_input_current(int current_ma)
{
	int rc = 0, i = 0, n = 0;
	int chg_vol = 0;
	int aicl_point = 0;
	u8 stat = 0;
    int pre_current = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER, false, 0);
	}

	if (chip->pmic_spmi.smb2_chip->chg.pre_current_ma == current_ma)
		return rc;
	else {
		pre_current = chip->pmic_spmi.smb2_chip->chg.pre_current_ma;
		chip->pmic_spmi.smb2_chip->chg.pre_current_ma = current_ma;
	}

	chg_debug( "usb input max current limit=%d setting %02x, pre_current[%d]\n", current_ma, i, pre_current);

	fg_oppo_set_input_current = true;

	if (chip->batt_volt > 4100 )
		aicl_point = 4550;
	else
		aicl_point = 4500;

	smbchg_aicl_enable(false);
    if (pre_current > current_ma){
		for (n = sizeof(usb_icl)/sizeof(int); n > 0; n--){
			if (pre_current > usb_icl[n]){
				break;
			}
		}
		chg_debug( "downTo: usb input max current limit=%d setting %d\n", current_ma, n);
		if (usb_icl[n] > 1200){
			rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[n] * 1000);
			msleep(90);
			n--;

			if (usb_icl[n] >= 1200){
				rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[n] * 1000);
				msleep(90);
				n--;

				if (usb_icl[n] >= 1200){
					rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[n] * 1000);
					msleep(90);
					n--;
				}
			}
		}
	}

	smbchg_usbin_collapse_irq_enable(false);
	if (current_ma < 500) {
		i = 0;
		goto aicl_end;
	}

	i = 1; /* 500 */
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	msleep(90);

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		chg_debug( "use 500 here\n");
		goto aicl_boost_back;
	}

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		chg_debug( "use 500 here\n");
		goto aicl_boost_back;
	}
	if (chg_vol < aicl_point) {
		chg_debug( "use 500 here\n");
		goto aicl_end;
	} else if (current_ma < 900)
		goto aicl_end;

	i = 2; /* 900 */
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	msleep(90);

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 1;
		goto aicl_boost_back;
	}
	if (oppo_chg_is_suspend_status() && oppo_chg_is_usb_present() && oppo_chg_is_normal_mode()) {
		i = i - 1;
		goto aicl_suspend;
	}

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 1;
		goto aicl_boost_back;
	}
	if (chg_vol < aicl_point) {
		i = i - 1;
		goto aicl_pre_step;
	} else if (current_ma < 1200)
		goto aicl_end;

	i = 3; /* 1200 */
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	msleep(90);

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 1;
		goto aicl_boost_back;
	}
	if (oppo_chg_is_suspend_status() && oppo_chg_is_usb_present() && oppo_chg_is_normal_mode()) {
		i = i - 1;
		goto aicl_suspend;
	}

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 1;
		goto aicl_boost_back;
	}
	if (chg_vol < aicl_point) {
		i = i - 1;
		goto aicl_pre_step;
	}

	i = 4; /* 1350 */
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	msleep(130);

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 2;
		goto aicl_boost_back;
	}
	if (oppo_chg_is_suspend_status() && oppo_chg_is_usb_present() && oppo_chg_is_normal_mode()) {
		i = i - 2;
		goto aicl_suspend;
	}

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 2;
		goto aicl_boost_back;
	}
	if (chg_vol < aicl_point) {
		i = i - 2; 
		goto aicl_pre_step;
	} 
	
	i = 5; /* 1500 */
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	msleep(120);

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 3;
		goto aicl_boost_back;
	}
	if (oppo_chg_is_suspend_status() && oppo_chg_is_usb_present() && oppo_chg_is_normal_mode()) {
		i = i - 3;
		goto aicl_suspend;
	}

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 3;
		goto aicl_boost_back;
	}
	if (chg_vol < aicl_point) {
		i = i - 3; //We DO NOT use 1.2A here
		goto aicl_pre_step;
	} else if (current_ma < 1500) {
		i = i - 2; //We use 1.2A here
		goto aicl_end;
	} else if (current_ma < 2000)
		goto aicl_end;

	i = 6; /* 1750 */
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	msleep(120);

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 3;
		goto aicl_boost_back;
	}
	if (oppo_chg_is_suspend_status() && oppo_chg_is_usb_present() && oppo_chg_is_normal_mode()) {
		i = i - 3;
		goto aicl_suspend;
	}

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 3;
		goto aicl_boost_back;
	}
	if (chg_vol < aicl_point) {
		i = i - 3; //1.2
		goto aicl_pre_step;
	}

	i = 7; /* 2000 */
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	msleep(90);

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 2;
		goto aicl_boost_back;
	}
	if (oppo_chg_is_suspend_status() && oppo_chg_is_usb_present() && oppo_chg_is_normal_mode()) {
		i = i - 2;
		goto aicl_suspend;
	}

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 2;
		goto aicl_boost_back;
	}
	if (chg_vol < aicl_point) {
		i =  i - 2;//1.5
		goto aicl_pre_step;
	} else if (current_ma < 3000)
		goto aicl_end;

	i = 8; /* 3000 */
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	msleep(90);

	if (get_client_vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER) == 0
			&& get_effective_result(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable) <= USBIN_25MA) {
		i = i - 1;
		goto aicl_boost_back;
	}
	if (oppo_chg_is_suspend_status() && oppo_chg_is_usb_present() && oppo_chg_is_normal_mode()) {
		i = i - 1;
		goto aicl_suspend;
	}

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		i = i - 1;
		goto aicl_pre_step;
	} else if (current_ma >= 3000)
		goto aicl_end;

aicl_pre_step:
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	chg_debug( "usb input max current limit aicl chg_vol=%d j[%d]=%d sw_aicl_point:%d aicl_pre_step\n", chg_vol, i, usb_icl[i], aicl_point);
	smbchg_rerun_aicl();
	smbchg_usbin_collapse_irq_enable(true);
	goto aicl_return;
aicl_end:
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	chg_debug( "usb input max current limit aicl chg_vol=%d j[%d]=%d sw_aicl_point:%d aicl_end\n", chg_vol, i, usb_icl[i], aicl_point);
	smbchg_rerun_aicl();
	smbchg_usbin_collapse_irq_enable(true);
	goto aicl_return;
aicl_boost_back:
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	chg_debug( "usb input max current limit aicl chg_vol=%d j[%d]=%d sw_aicl_point:%d aicl_boost_back\n", chg_vol, i, usb_icl[i], aicl_point);
	if (chip->pmic_spmi.smb2_chip->chg.wa_flags & BOOST_BACK_WA)
		vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, BOOST_BACK_VOTER, false, 0);
	smbchg_rerun_aicl();
	smbchg_usbin_collapse_irq_enable(true);
	goto aicl_return;
aicl_suspend:
	rc = vote(chip->pmic_spmi.smb2_chip->chg.usb_icl_votable, USB_PSY_VOTER, true, usb_icl[i] * 1000);
	chg_debug( "usb input max current limit aicl chg_vol=%d j[%d]=%d sw_aicl_point:%d aicl_suspend\n", chg_vol, i, usb_icl[i], aicl_point);
	oppo_chg_check_clear_suspend();
	smbchg_rerun_aicl();
	smbchg_usbin_collapse_irq_enable(true);
	goto aicl_return;
aicl_return:
	/*FORCE icl 500mA for AUDIO_ADAPTER combo cable*/
	if (chip->pmic_spmi.smb2_chip->chg.typec_mode == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
		chg_debug( "AUDIO ADAPTER MODE\n");
		rc = smblib_read(&chip->pmic_spmi.smb2_chip->chg, USBIN_LOAD_CFG_REG, &stat);
		if (rc < 0) {
			chg_debug( "read USBIN_LOAD_CFG_REG, failed rc=%d\n", rc);
		}
		if ((bool)(stat& ICL_OVERRIDE_AFTER_APSD_BIT)) {
			rc = smblib_write(&chip->pmic_spmi.smb2_chip->chg, USBIN_CURRENT_LIMIT_CFG_REG, 0x14);
			if (rc < 0) {
				chg_debug( "Couldn't write USBIN_CURRENT_LIMIT_CFG_REG rc=%d\n", rc);
			} else {
				chg_debug( "FORCE icl 500\n");
			}
		}
	}
	return rc;
}

static int smbchg_float_voltage_set(int vfloat_mv)
{
	int rc = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	rc = vote(chip->pmic_spmi.smb2_chip->chg.fv_votable, BATT_PROFILE_VOTER/*DEFAULT_VOTER*/,
			true, vfloat_mv * 1000);
	if (rc < 0)
		chg_err("Couldn't vote fv_votable[%d], rc=%d\n", vfloat_mv, rc);

	return rc;
}

static int smbchg_term_current_set(int term_current)
{
	int rc = 0;
	u8 val_raw = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (term_current < 0 || term_current > 750)
		term_current = 150;

	val_raw = term_current / 50;
	rc = smblib_masked_write(&chip->pmic_spmi.smb2_chip->chg, TCCC_CHARGE_CURRENT_TERMINATION_CFG_REG,
			TCCC_CHARGE_CURRENT_TERMINATION_SETTING_MASK, val_raw);
	if (rc < 0)
		chg_err("Couldn't write TCCC_CHARGE_CURRENT_TERMINATION_CFG_REG rc=%d\n", rc);

	return rc;
}

static int smbchg_charging_enble(void)
{
	int rc = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	rc = vote(chip->pmic_spmi.smb2_chip->chg.chg_disable_votable, DEFAULT_VOTER,
			false, 0);
	if (rc < 0)
		chg_err("Couldn't enable charging, rc=%d\n", rc);

	return rc;
}

static int smbchg_charging_disble(void)
{
	int rc = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	rc = vote(chip->pmic_spmi.smb2_chip->chg.chg_disable_votable, DEFAULT_VOTER,
			true, 0);
	if (rc < 0)
		chg_err("Couldn't disable charging, rc=%d\n", rc);

	chip->pmic_spmi.smb2_chip->chg.pre_current_ma = -1;

	return rc;
}

static int smbchg_get_charge_enable(void)
{
	int rc = 0;
	u8 temp = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	rc = smblib_read(&chip->pmic_spmi.smb2_chip->chg, CHARGING_ENABLE_CMD_REG, &temp);
	if (rc < 0) {
		chg_err("Couldn't read CHARGING_ENABLE_CMD_REG rc=%d\n", rc);
		return 0;
	}
	rc = temp & CHARGING_ENABLE_CMD_BIT;

	return rc;
}

static int smbchg_usb_suspend_enable(void)
{
	int rc = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	rc = smblib_set_usb_suspend(&chip->pmic_spmi.smb2_chip->chg, true);
	if (rc < 0)
		chg_err("Couldn't write enable to USBIN_SUSPEND_BIT rc=%d\n", rc);

	chip->pmic_spmi.smb2_chip->chg.pre_current_ma = -1;

	return rc;
}

static int smbchg_usb_suspend_disable(void)
{
	int rc = 0;
	int boot_mode = get_boot_mode();
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (boot_mode == MSM_BOOT_MODE__RF || boot_mode == MSM_BOOT_MODE__WLAN) {
		chg_err("RF/WLAN, suspending...\n");
		rc = smblib_set_usb_suspend(&chip->pmic_spmi.smb2_chip->chg, true);
		if (rc < 0)
			chg_err("Couldn't write enable to USBIN_SUSPEND_BIT rc=%d\n", rc);
		return rc;
	}

	rc = smblib_set_usb_suspend(&chip->pmic_spmi.smb2_chip->chg, false);
	if (rc < 0)
		chg_err("Couldn't write disable to USBIN_SUSPEND_BIT rc=%d\n", rc);

	return rc;
}

static int smbchg_set_rechg_vol(int rechg_vol)
{
	return 0;
}

static int smbchg_reset_charger(void)
{
	return 0;
}

static int smbchg_read_full(void)
{
	int rc = 0;
	u8 stat = 0;
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (!oppo_chg_is_usb_present())
		return 0;

	rc = smblib_read(&chip->pmic_spmi.smb2_chip->chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		chg_err("Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n", rc);
		return 0;
	}
	stat = stat & BATTERY_CHARGER_STATUS_MASK;

	if (stat == TERMINATE_CHARGE || stat == INHIBIT_CHARGE)
		return 1;
	return 0;
}

static int smbchg_otg_enable(void)
{
	return 0;
}

static int smbchg_otg_disable(void)
{
	return 0;
}

static int oppo_set_chging_term_disable(void)
{
	return 0;
}

static bool qcom_check_charger_resume(void)
{
	return true;
}

bool smbchg_need_to_check_ibatt(void)
{
	return true;
}

static int smbchg_get_chg_current_step(void)
{
	return 25;
}

int opchg_get_charger_type(void)
{
	u8 apsd_stat;
	int rc;
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;
	
	if (!chip)
		return POWER_SUPPLY_TYPE_UNKNOWN;

	chg = &chip->pmic_spmi.smb2_chip->chg;

	/* reset for fastchg to normal */
	if (chip->charger_type == POWER_SUPPLY_TYPE_UNKNOWN)
		chg->pre_current_ma = -1;

	rc = smblib_read(chg, APSD_STATUS_REG, &apsd_stat);
	if (rc < 0) {
		chg_err("Couldn't read APSD_STATUS rc=%d\n", rc);
		return POWER_SUPPLY_TYPE_UNKNOWN;
	}
	chg_debug("APSD_STATUS = 0x%02x\n", apsd_stat);

	if (!(apsd_stat & APSD_DTC_STATUS_DONE_BIT))
		return POWER_SUPPLY_TYPE_UNKNOWN;

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB
			|| chg->real_charger_type == POWER_SUPPLY_TYPE_USB_CDP
			|| chg->real_charger_type == POWER_SUPPLY_TYPE_USB_DCP) {
		oppo_chg_soc_update();
	}

	if (POWER_SUPPLY_TYPE_UNKNOWN == chg->real_charger_type) {
		smblib_update_usb_type(chg);
		chg_debug("update_usb_type: get_charger_type=%d\n", chg->real_charger_type);
	}

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_CDP)
		return POWER_SUPPLY_TYPE_USB;

	return chg->real_charger_type;
}


int qpnp_get_prop_charger_voltage_now(void)
{
	int val = 0;
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;
	
	if (!chip)
		return 0;

	//if (!oppo_chg_is_usb_present())
	//	return 0;

	chg = &chip->pmic_spmi.smb2_chip->chg;
	if (!chg->iio.usbin_v_chan || PTR_ERR(chg->iio.usbin_v_chan) == -EPROBE_DEFER)
		chg->iio.usbin_v_chan = iio_channel_get(chg->dev, "usbin_v");

	if (IS_ERR(chg->iio.usbin_v_chan))
		return PTR_ERR(chg->iio.usbin_v_chan);

	iio_read_channel_processed(chg->iio.usbin_v_chan, &val);

	if (val < 2000 * 1000)
		chg->pre_current_ma = -1;

	return val / 1000;
}


bool oppo_chg_is_usb_present(void)
{
	int rc = 0;
	u8 stat = 0;
	bool vbus_rising = false;
	struct oppo_chg_chip *chip = g_oppo_chip;
	
	if (!chip)
		return false;

#ifdef VENDOR_EDIT//Fanhong.Kong@ProDrv.CHG,add 2018/06/02 for SVOOC OTG
	if ((chip->pmic_spmi.smb2_chip->chg.typec_mode == POWER_SUPPLY_TYPEC_SINK
		|| chip->pmic_spmi.smb2_chip->chg.typec_mode == POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE)
		&& chip->vbatt_num == 2 ) {
		chg_err("chg->typec_mode = SINK,oppo_chg_is_usb_present return false!\n");
		rc = false;
		return rc ;
	}
#endif/*VENDOR_EDIT*/

	rc = smblib_read(&chip->pmic_spmi.smb2_chip->chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		chg_err("Couldn't read USB_INT_RT_STS, rc=%d\n", rc);
		return false;
	}
	vbus_rising = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);

	if (vbus_rising == false && oppo_vooc_get_fastchg_started() == true) {
		if (qpnp_get_prop_charger_voltage_now() > 2000) {
			chg_err("USBIN_PLUGIN_RT_STS_BIT low but fastchg started true and chg vol > 2V\n");
			vbus_rising = true;
		}
	}
#ifdef VENDOR_EDIT//Fanhong.Kong@ProDrv.CHG,add 2018/04/20 for SVOOC
	if (vbus_rising == false && (oppo_vooc_get_fastchg_started() == true && (chip->vbatt_num == 2))) {
			chg_err("USBIN_PLUGIN_RT_STS_BIT low but fastchg started true and SVOOC\n");
			vbus_rising = true;
	}
#endif/*VENDOR_EDIT*/

	if (vbus_rising == false)
		chip->pmic_spmi.smb2_chip->chg.pre_current_ma = -1;

	return vbus_rising;
}


int qpnp_get_battery_voltage(void)
{
	return 3800;//Not use anymore
}
#if 0
static int get_boot_mode(void)
{
	return 0;
}
#endif
int smbchg_get_boot_reason(void)
{
	return 0;
}

int oppo_chg_get_shutdown_soc(void)
{
	return 0;
}

int oppo_chg_backup_soc(int backup_soc)
{
	return 0;
}

static int smbchg_get_aicl_level_ma(void)
{
	return 0;
}

static int smbchg_force_tlim_en(bool enable)
{
	return 0;
}

static int smbchg_system_temp_level_set(int lvl_sel)
{
	return 0;
}

static int smbchg_set_prop_flash_active(enum skip_reason reason, bool disable)
{
	return 0;
}

static int smbchg_dp_dm(int val)
{
	return 0;
}

static int smbchg_calc_max_flash_current(void)
{
	return 0;
}

static int oppo_chg_get_fv(struct oppo_chg_chip *chip)
{
	int flv = chip->limits.temp_normal_vfloat_mv;
	int batt_temp = chip->temperature;

	if (batt_temp > chip->limits.hot_bat_decidegc) {//53C
		//default
	} else if (batt_temp >= chip->limits.warm_bat_decidegc) {//45C
		flv = chip->limits.temp_warm_vfloat_mv;
	} else if (batt_temp >= chip->limits.normal_bat_decidegc) {//16C
		flv = chip->limits.temp_normal_vfloat_mv;
	} else if (batt_temp >= chip->limits.little_cool_bat_decidegc) {//12C
		flv = chip->limits.temp_little_cool_vfloat_mv;
	} else if (batt_temp >= chip->limits.cool_bat_decidegc) {//5C
		flv = chip->limits.temp_cool_vfloat_mv;
	} else if (batt_temp >= chip->limits.little_cold_bat_decidegc) {//0C
		flv = chip->limits.temp_little_cold_vfloat_mv;
	} else if (batt_temp >= chip->limits.cold_bat_decidegc) {//-3C
		flv = chip->limits.temp_cold_vfloat_mv;
	} else {
		//default
	}

	return flv;
}

static int oppo_chg_get_charging_current(struct oppo_chg_chip *chip)
{
	int charging_current = 0;
	int batt_temp = chip->temperature;

	if (batt_temp > chip->limits.hot_bat_decidegc) {//53C
		charging_current = 0;
	} else if (batt_temp >= chip->limits.warm_bat_decidegc) {//45C
		charging_current = chip->limits.temp_warm_fastchg_current_ma;
	} else if (batt_temp >= chip->limits.normal_bat_decidegc) {//16C
		charging_current = chip->limits.temp_normal_fastchg_current_ma;
	} else if (batt_temp >= chip->limits.little_cool_bat_decidegc) {//12C
		charging_current = chip->limits.temp_little_cool_fastchg_current_ma;
	} else if (batt_temp >= chip->limits.cool_bat_decidegc) {//5C
		if (chip->batt_volt > 4180)
			charging_current = chip->limits.temp_cool_fastchg_current_ma_low;
		else
			charging_current = chip->limits.temp_cool_fastchg_current_ma_high;
	} else if (batt_temp >= chip->limits.little_cold_bat_decidegc) {//0C
		charging_current = chip->limits.temp_little_cold_fastchg_current_ma;
	} else if (batt_temp >= chip->limits.cold_bat_decidegc) {//-3C
		charging_current = chip->limits.temp_cold_fastchg_current_ma;
	} else {
		charging_current = 0;
	}

	return charging_current;
}

#ifdef CONFIG_OPPO_RTC_DET_SUPPORT
static int rtc_reset_check(void)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc = 0;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return 0;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	if ((tm.tm_year == 70) && (tm.tm_mon == 0) && (tm.tm_mday <= 1)) {
		chg_debug(": Sec: %d, Min: %d, Hour: %d, Day: %d, Mon: %d, Year: %d  @@@ wday: %d, yday: %d, isdst: %d\n",
			tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon, tm.tm_year,
			tm.tm_wday, tm.tm_yday, tm.tm_isdst);
		rtc_class_close(rtc);
		return 1;
	}

	chg_debug(": Sec: %d, Min: %d, Hour: %d, Day: %d, Mon: %d, Year: %d  ###  wday: %d, yday: %d, isdst: %d\n",
		tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon, tm.tm_year,
		tm.tm_wday, tm.tm_yday, tm.tm_isdst);

close_time:
	rtc_class_close(rtc);
	return 0;
}
#endif /* CONFIG_OPPO_RTC_DET_SUPPORT */

#ifdef CONFIG_OPPO_SHORT_C_BATT_CHECK
/* This function is getting the dynamic aicl result/input limited in mA.
 * If charger was suspended, it must return 0(mA).
 * It meets the requirements in SDM660 platform.
 */
static int oppo_chg_get_dyna_aicl_result(void)
{
	struct power_supply *usb_psy = NULL;
	union power_supply_propval pval = {0, };

	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
				&pval);
		return pval.intval / 1000;
	}

	return 1000;
}
#endif /* CONFIG_OPPO_SHORT_C_BATT_CHECK */

static int get_current_time(unsigned long *now_tm_sec)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, now_tm_sec);

close_time:
	rtc_class_close(rtc);
	return rc;
}

static unsigned long suspend_tm_sec = 0;
static int smb2_pm_resume(struct device *dev)
{
	int rc = 0;
	unsigned long resume_tm_sec = 0;
	unsigned long sleep_time = 0;

	if (!g_oppo_chip)
		return 0;

	rc = get_current_time(&resume_tm_sec);
	if (rc || suspend_tm_sec == -1) {
		chg_err("RTC read failed\n");
		sleep_time = 0;
	} else {
		sleep_time = resume_tm_sec - suspend_tm_sec;
	}

	if (sleep_time < 0) {
		sleep_time = 0;
	}

	oppo_chg_soc_update_when_resume(sleep_time);

	return 0;
}

static int smb2_pm_suspend(struct device *dev)
{
	if (!g_oppo_chip)
		return 0;

	if (get_current_time(&suspend_tm_sec)) {
		chg_err("RTC read failed\n");
		suspend_tm_sec = -1;
	}

	return 0;
}

static const struct dev_pm_ops smb2_pm_ops = {
	.resume		= smb2_pm_resume,
	.suspend		= smb2_pm_suspend,
};

struct oppo_chg_operations  smb2_chg_ops = {
	.dump_registers = dump_regs,
	.kick_wdt = smbchg_kick_wdt,
	.hardware_init = oppo_chg_hw_init,
	.charging_current_write_fast = smbchg_set_fastchg_current_raw,
	.set_aicl_point = smbchg_set_aicl_point,
	.input_current_write = oppo_chg_set_input_current,
	.float_voltage_write = smbchg_float_voltage_set,
	.term_current_set = smbchg_term_current_set,
	.charging_enable = smbchg_charging_enble,
	.charging_disable = smbchg_charging_disble,
	.get_charging_enable = smbchg_get_charge_enable,
	.charger_suspend = smbchg_usb_suspend_enable,
	.charger_unsuspend = smbchg_usb_suspend_disable,
	.set_rechg_vol = smbchg_set_rechg_vol,
	.reset_charger = smbchg_reset_charger,
	.read_full = smbchg_read_full,
	.otg_enable = smbchg_otg_enable,
	.otg_disable = smbchg_otg_disable,
	.set_charging_term_disable = oppo_set_chging_term_disable,
	.check_charger_resume = qcom_check_charger_resume,
	.get_chargerid_volt = smbchg_get_chargerid_volt,
	.set_chargerid_switch_val = smbchg_set_chargerid_switch_val,
	.get_chargerid_switch_val = smbchg_get_chargerid_switch_val,
	.need_to_check_ibatt = smbchg_need_to_check_ibatt,
	.get_chg_current_step = smbchg_get_chg_current_step,
#ifdef CONFIG_OPPO_CHARGER_MTK
	.get_charger_type = mt_power_supply_type_check,
	.get_charger_volt = battery_meter_get_charger_voltage,
	.check_chrdet_status = pmic_chrdet_status,
	.get_instant_vbatt = battery_meter_get_battery_voltage,
	.get_boot_mode = get_boot_mode,
	.get_boot_reason = get_boot_reason,
#ifdef CONFIG_MTK_HAFG_20
	.get_rtc_soc = get_rtc_spare_oppo_fg_value,
	.set_rtc_soc = set_rtc_spare_oppo_fg_value,
#else
	.get_rtc_soc = get_rtc_spare_fg_value,
	.set_rtc_soc = set_rtc_spare_fg_value,
#endif	/* CONFIG_MTK_HAFG_20 */
	.set_power_off = mt_power_off,
	.usb_connect = mt_usb_connect,
	.usb_disconnect = mt_usb_disconnect,
#else
	.get_charger_type = opchg_get_charger_type,
	.get_charger_volt = qpnp_get_prop_charger_voltage_now,
	.check_chrdet_status = oppo_chg_is_usb_present,
	.get_instant_vbatt = qpnp_get_battery_voltage,
	.get_boot_mode = get_boot_mode,
	.get_boot_reason = smbchg_get_boot_reason,
	.get_rtc_soc = oppo_chg_get_shutdown_soc,
	.set_rtc_soc = oppo_chg_backup_soc,
	.get_aicl_ma = smbchg_get_aicl_level_ma,
	.rerun_aicl = smbchg_rerun_aicl,
	.tlim_en = smbchg_force_tlim_en,
	.set_system_temp_level = smbchg_system_temp_level_set,
	.otg_pulse_skip_disable = smbchg_set_prop_flash_active,
	.set_dp_dm = smbchg_dp_dm,
	.calc_flash_current = smbchg_calc_max_flash_current,
#endif	/* CONFIG_OPPO_CHARGER_MTK */
#ifdef CONFIG_OPPO_RTC_DET_SUPPORT
	.check_rtc_reset = rtc_reset_check,
#endif
#ifdef CONFIG_OPPO_SHORT_C_BATT_CHECK
	.get_dyna_aicl_result = oppo_chg_get_dyna_aicl_result,
#endif
	.get_shortc_hw_gpio_status = oppo_chg_get_shortc_hw_gpio_status,
};
#endif /* VENDOR_EDIT */

#ifndef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/05/12, sjc Add for BOB noise*/
#define REGULATOR_MODE_FAST			0x1
#define REGULATOR_MODE_NORMAL			0x2
#define REGULATOR_MODE_IDLE			0x4
#define REGULATOR_MODE_STANDBY			0x8

int pm660l_bob_regulator_get_mode(unsigned int *mode)
{
	int rc;
	unsigned int bob_mode;
	struct smb_charger *chg = NULL;

	if (!g_oppo_chip) {
		printk(KERN_ERR "pm660l_bob_regulator_get_mode: g_oppo_chip NULL\n");
		return -1;
	}
	chg = &g_oppo_chip->pmic_spmi.smb2_chip->chg;

	if (!chg || !chg->pm660l_bob_reg) {
		printk(KERN_ERR "%s: pm660l_bob_reg NULL\n", __func__);
		return -1;
	}

	rc = regulator_enable(chg->pm660l_bob_reg);
	if (rc < 0) {
		printk(KERN_ERR "%s: Couldn't enable regulator rc=%d\n", __func__, rc);
		return -1;
	}

	bob_mode = regulator_get_mode(chg->pm660l_bob_reg);
	if (bob_mode != REGULATOR_MODE_FAST && bob_mode != REGULATOR_MODE_NORMAL
			&& bob_mode != REGULATOR_MODE_IDLE && bob_mode != REGULATOR_MODE_STANDBY) {
		printk(KERN_ERR "%s: Couldn't get regulator mode=%d\n", __func__, bob_mode);
		*mode = 0;
		goto err;
	}
	*mode = bob_mode;

err:
	rc = regulator_disable(chg->pm660l_bob_reg);
	if (rc < 0) {
		printk(KERN_ERR "%s: Couldn't disable regulator rc=%d\n", __func__, rc);
		return -1;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(pm660l_bob_regulator_get_mode);

/*return -1 for error, return 0 for ok, return 1 for invalid*/
int pm660l_bob_regulator_set_mode(unsigned int mode)
{
	int rc;
	int ua_load;
	static unsigned int pre_mode = 0;
	struct smb_charger *chg = NULL;
	struct oppo_chg_chip *chip = g_oppo_chip;

	if (!chip) {
		printk(KERN_ERR "%s: chip NULL\n", __func__);
		return -1;
	}
	chg = &chip->pmic_spmi.smb2_chip->chg;

	if (!chg || !chg->pm660l_bob_reg) {
		printk(KERN_ERR "%s: pm660l_bob_reg NULL\n", __func__);
		return -1;
	}

	if (mode != REGULATOR_MODE_FAST && mode != REGULATOR_MODE_NORMAL) {
		printk(KERN_ERR "%s: Invalid mode: %d", __func__, mode);
		return -1;
	}

	if (pre_mode == mode) {
		printk(KERN_ERR "%s: pre_mode[%d], mode[%d], return\n", __func__, pre_mode, mode);
		return 1;
	}

	if (mode == REGULATOR_MODE_FAST) {
			ua_load = 2000000;
		rc = regulator_set_load(chg->pm660l_bob_reg, ua_load);
		if (rc < 0) {
			printk(KERN_ERR "%s: Couldn't set regulator load=%d rc=%d\n", __func__, ua_load, rc);
			return -1;
		}
		pre_mode = mode;
	} else if (mode == REGULATOR_MODE_NORMAL) {
		ua_load = 0;
		rc = regulator_set_load(chg->pm660l_bob_reg, ua_load);
		if (rc < 0) {
			printk(KERN_ERR "%s: Couldn't set regulator load=%d rc=%d\n", __func__, ua_load, rc);
			return -1;
		}
		pre_mode = mode;
	}
	return 0;
}

EXPORT_SYMBOL_GPL(pm660l_bob_regulator_set_mode);
#endif /* VENDOR_EDIT */

static int smb2_probe(struct platform_device *pdev)
{
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2016/12/26, sjc Add for charging*/
	struct oppo_chg_chip *oppo_chip;
	struct power_supply *main_psy = NULL;
	union power_supply_propval pval = {0, };
#endif
	struct smb2 *chip;
	struct smb_charger *chg;
	int rc = 0;
	union power_supply_propval val;
	int usb_present, batt_present, batt_health, batt_charge_type;

#ifdef VENDOR_EDIT//Fanhong.Kong@ProDrv.CHG,add 2018/04/20 for SVOOC
	oppo_chip = devm_kzalloc(&pdev->dev, sizeof(*oppo_chip), GFP_KERNEL);
	if (!oppo_chip)
		return -ENOMEM;

	oppo_chip->dev = &pdev->dev;
	rc = oppo_chg_parse_svooc_dt(oppo_chip);

	if (oppo_chip->vbatt_num == 1) {
		if (oppo_gauge_check_chip_is_null()) {
			chg_err("gauge chip null, will do after bettery init.\n");
			return -EPROBE_DEFER;
		}
		oppo_chip->chg_ops = &smb2_chg_ops;
	} else {
		if (oppo_gauge_ic_chip_is_null() || oppo_vooc_check_chip_is_null()
				|| oppo_charger_ic_chip_is_null() || oppo_adapter_check_chip_is_null()) {
			chg_err("[oppo_chg_init] vooc || gauge || chg not ready, will do after bettery init.\n");
			return -EPROBE_DEFER;
		}
		oppo_chip->chg_ops = (oppo_get_chg_ops());
	}
	g_oppo_chip = oppo_chip;
	chg_debug("SMB2_Probe Start----\n");
#endif

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2016/12/26, sjc Add for charging*/
	oppo_chip->pmic_spmi.smb2_chip = chip;
#endif
	chg = &chip->chg;
	chg->dev = &pdev->dev;
	chg->param = v1_params;
	chg->debug_mask = &__debug_mask;
	chg->try_sink_enabled = &__try_sink_enabled;
	chg->weak_chg_icl_ua = &__weak_chg_icl_ua;
	chg->mode = PARALLEL_MASTER;
	chg->irq_info = smb2_irqs;
	chg->die_health = -EINVAL;
	chg->name = "PMI";
	chg->audio_headset_drp_wait_ms = &__audio_headset_drp_wait_ms;

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/08/10, sjc Add for charging */
	chg->pre_current_ma = -1;
#endif

#ifdef VENDOR_EDIT
	/* Jianchao.Shi@BSP.CHG.Basic, 2017/01/22, sjc Add for charging*/
	if (of_find_property(oppo_chip->dev->of_node, "qcom,pm660chg-vadc", NULL)) {
		oppo_chip->pmic_spmi.pm660_vadc_dev = qpnp_get_vadc(oppo_chip->dev, "pm660chg");
		if (IS_ERR(oppo_chip->pmic_spmi.pm660_vadc_dev)) {
			rc = PTR_ERR(oppo_chip->pmic_spmi.pm660_vadc_dev);
			oppo_chip->pmic_spmi.pm660_vadc_dev = NULL;
			if (rc != -EPROBE_DEFER)
				chg_err("Couldn't get vadc rc=%d\n", rc);
			else {
				chg_err("Couldn't get vadc, try again...\n");
				return -EPROBE_DEFER;
			}
		}
	}
#endif

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/05/25, sjc Add for usbtemp */
	if (of_find_property(oppo_chip->dev->of_node, "qcom,pm660usbtemp-vadc", NULL)) {
		oppo_chip->pmic_spmi.pm660_usbtemp_vadc_dev = qpnp_get_vadc(oppo_chip->dev, "pm660usbtemp");
		if (IS_ERR(oppo_chip->pmic_spmi.pm660_usbtemp_vadc_dev)) {
			rc = PTR_ERR(oppo_chip->pmic_spmi.pm660_usbtemp_vadc_dev);
			oppo_chip->pmic_spmi.pm660_usbtemp_vadc_dev = NULL;
			if (rc != -EPROBE_DEFER)
				chg_err("Couldn't get usbtemp vadc rc=%d\n", rc);
			else {
				chg_err("Couldn't get usbtemp vadc, try again...\n");
				return -EPROBE_DEFER;
			}
		}
	}
#endif

	chg->regmap = dev_get_regmap(chg->dev->parent, NULL);
	if (!chg->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}

	rc = smb2_chg_config_init(chip);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't setup chg_config rc=%d\n", rc);
		return rc;
	}

	rc = smb2_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto cleanup;
	}

	rc = smblib_init(chg);
	if (rc < 0) {
		pr_err("Smblib_init failed rc=%d\n", rc);
		goto cleanup;
	}

	/* set driver data before resources request it */
	platform_set_drvdata(pdev, chip);

	rc = smb2_init_vbus_regulator(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize vbus regulator rc=%d\n",
			rc);
		goto cleanup;
	}

	rc = smb2_init_vconn_regulator(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize vconn regulator rc=%d\n",
				rc);
		goto cleanup;
	}

	/* extcon registration */
	chg->extcon = devm_extcon_dev_allocate(chg->dev, smblib_extcon_cable);
	if (IS_ERR(chg->extcon)) {
		rc = PTR_ERR(chg->extcon);
		dev_err(chg->dev, "failed to allocate extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	rc = devm_extcon_dev_register(chg->dev, chg->extcon);
	if (rc < 0) {
		dev_err(chg->dev, "failed to register extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	rc = smb2_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto cleanup;
	}

#ifndef VENDOR_EDIT
	rc = smb2_init_dc_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize dc psy rc=%d\n", rc);
		goto cleanup;
	}
#endif

#ifndef VENDOR_EDIT
	rc = smb2_init_usb_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb psy rc=%d\n", rc);
		goto cleanup;
	}
#else
	//kong
	rc = oppo_power_supply_init(chip);
	if (rc < 0) {
			pr_err("Couldn't initialize usb main psy rc=%d\n", rc);
			goto cleanup;
	}
#endif

	rc = smb2_init_usb_main_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb main psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb2_init_usb_port_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb pc_port psy rc=%d\n", rc);
		goto cleanup;
	}

#ifndef VENDOR_EDIT
	rc = smb2_init_batt_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}
#endif
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/11, sjc Add for charging*/
	if (oppo_chg_is_usb_present()) {
		rc = smblib_masked_write(chg, CHARGING_ENABLE_CMD_REG,
				CHARGING_ENABLE_CMD_BIT, 0);
		if (rc < 0)
			pr_err("Couldn't disable at bootup rc=%d\n", rc);
		msleep(100);
		rc = smblib_masked_write(chg, CHARGING_ENABLE_CMD_REG,
				CHARGING_ENABLE_CMD_BIT, CHARGING_ENABLE_CMD_BIT);
		if (rc < 0)
			pr_err("Couldn't enable at bootup rc=%d\n", rc);
	}
#endif

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2016/12/26, sjc Add for charging*/
	oppo_chg_parse_custom_dt(oppo_chip);
	oppo_chg_parse_charger_dt(oppo_chip);
	oppo_chg_init(oppo_chip);
	main_psy = power_supply_get_by_name("main");
	if (main_psy) {
		pval.intval = 1000 * oppo_chg_get_fv(oppo_chip);
		power_supply_set_property(main_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX,
				&pval);
		pval.intval = 1000 * oppo_chg_get_charging_current(oppo_chip);
		power_supply_set_property(main_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
				&pval);
	}
#endif

	rc = smb2_determine_initial_status(chip);
	if (rc < 0) {
		pr_err("Couldn't determine initial status rc=%d\n",
			rc);
		goto cleanup;
	}

	rc = smb2_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		goto cleanup;
	}
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/01/30, sjc Add for using gpio as CC detect */
    if (oppo_ccdetect_check_is_gpio(oppo_chip) == true)
        oppo_ccdetect_irq_register(oppo_chip);
#endif
	rc = smb2_post_init(chip);
	if (rc < 0) {
		pr_err("Failed in post init rc=%d\n", rc);
		goto cleanup;
	}

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/24, sjc Add for otg id value change support */
    if (oppo_ccdetect_support_check() == OPPO_NOT_SUPPORT_CCDETECT)
    	otg_disable_pmic_id_value();
#endif

	smb2_create_debugfs(chip);

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2016/12/26, sjc Add for charging*/
//	g_oppo_chip->authenticate = oppo_gauge_get_batt_authenticate();
//	if(!g_oppo_chip->authenticate)
//		smbchg_charging_disble();
	oppo_chg_wake_update_work();
#endif

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		goto cleanup;
	}
	usb_present = val.intval;

	rc = smblib_get_prop_batt_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt present rc=%d\n", rc);
		goto cleanup;
	}
	batt_present = val.intval;

#ifndef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/04/27, sjc Modify for charging */
	rc = smblib_get_prop_batt_health(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt health rc=%d\n", rc);
		val.intval = POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	batt_health = val.intval;
#else
	batt_health = oppo_chg_get_prop_batt_health(oppo_chip);
#endif

	rc = smblib_get_prop_batt_charge_type(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt charge type rc=%d\n", rc);
		goto cleanup;
	}
	batt_charge_type = val.intval;

	device_init_wakeup(chg->dev, true);

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/05/22, sjc Add for dump register */
	init_proc_dump_registers_mask();
#endif

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/05/25, sjc Add for usbtemp */
	if (oppo_usbtemp_check_is_support() == true)
		oppo_usbtemp_thread_init();
#endif

	pr_info("QPNP SMB2 probed successfully usb:present=%d type=%d batt:present = %d health = %d charge = %d\n",
		usb_present, chg->real_charger_type,
		batt_present, batt_health, batt_charge_type);
	return rc;

cleanup:
	smb2_free_interrupts(chg);
	if (chg->batt_psy)
		power_supply_unregister(chg->batt_psy);
	if (chg->usb_main_psy)
		power_supply_unregister(chg->usb_main_psy);
	if (chg->usb_psy)
		power_supply_unregister(chg->usb_psy);
	if (chg->usb_port_psy)
		power_supply_unregister(chg->usb_port_psy);
	if (chg->dc_psy)
		power_supply_unregister(chg->dc_psy);
	if (chg->vconn_vreg && chg->vconn_vreg->rdev)
		devm_regulator_unregister(chg->dev, chg->vconn_vreg->rdev);
	if (chg->vbus_vreg && chg->vbus_vreg->rdev)
		devm_regulator_unregister(chg->dev, chg->vbus_vreg->rdev);

	smblib_deinit(chg);

	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int smb2_remove(struct platform_device *pdev)
{
	struct smb2 *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;

	power_supply_unregister(chg->batt_psy);
	power_supply_unregister(chg->usb_psy);
	power_supply_unregister(chg->usb_port_psy);
	regulator_unregister(chg->vconn_vreg->rdev);
	regulator_unregister(chg->vbus_vreg->rdev);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void smb2_shutdown(struct platform_device *pdev)
{
	struct smb2 *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;
#ifdef VENDOR_EDIT
	/* Jianchao.Shi@BSP.CHG.Basic, 2017/01/22, sjc Add for charging*/
	if (g_oppo_chip) {
        oppo_vooc_reset_mcu();
		smbchg_set_chargerid_switch_val(0);
        oppo_vooc_switch_mode(NORMAL_CHARGER_MODE);
		msleep(30);
	}
#endif

	/* disable all interrupts */
	smb2_disable_interrupts(chg);

	if (!chg->ufp_only_mode)
		/* configure power role for UFP */
		smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				TYPEC_POWER_ROLE_CMD_MASK, UFP_EN_CMD_BIT);

	/* force HVDCP to 5V */
	smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT, 0);
	smblib_write(chg, CMD_HVDCP_2_REG, FORCE_5V_BIT);

	/* force enable APSD */
	smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				 AUTO_SRC_DETECT_BIT, AUTO_SRC_DETECT_BIT);

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/03/02, sjc Add for using gpio as shipmode stm6620 */
	if (g_oppo_chip && g_oppo_chip->enable_shipmode) {
		msleep(1000);
		smbchg_enter_shipmode(g_oppo_chip);
	}
#endif /* VENDOR_EDIT */
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,qpnp-smb2", },
	{ },
};

static struct platform_driver smb2_driver = {
	.driver		= {
		.name		= "qcom,qpnp-smb2",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2017/01/25, sjc Add for charging */
		.pm		= &smb2_pm_ops,
#endif
	},
	.probe		= smb2_probe,
	.remove		= smb2_remove,
	.shutdown	= smb2_shutdown,
};
module_platform_driver(smb2_driver);

MODULE_DESCRIPTION("QPNP SMB2 Charger Driver");
MODULE_LICENSE("GPL v2");
