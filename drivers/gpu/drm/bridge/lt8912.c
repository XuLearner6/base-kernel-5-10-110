// SPDX-License-Identifier: GPL-2.0
 /*
 * Copyright (c) 2018 Rockchip Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/kthread.h>
 
#define EDID_ADDR 0x50
//#define LT8912_HDP_CHECK 0
#define LT8912_REG_CHIP_REVISION_0 (0x00)
#define LT8912_REG_CHIP_REVISION_1 (0x01)
 
#define LT8912_VAL_CHIP_REVISION_0 (0x12)
#define LT8912_VAL_CHIP_REVISION_1 (0xB2)
#define LT8912_DSI_CEC_I2C_ADDR_REG (0xE1)
#define LT8912_RESET_DELAY (100)

#define HDMI_MODE_480P   0
#define HDMI_MODE_720P   0
#define HDMI_MODE_1080P  1
#define HDMI_MODE_720P_TEST_MODE 0
#define DVI_MODE_OUTPUT 0
 
enum lt8912_i2c_addr {
	I2C_ADDR_MAIN = 0x48,
	I2C_ADDR_CEC_DSI = 0x49,
	I2C_ADDR_I2S = 0x4a,
};

struct lt8912_reg_cfg {
	u8 i2c_addr;
	u8 reg;
	u8 val;
	int sleep_in_ms;
};

struct lt8912 {
	int irq;
	u32 irq_gpio;
	u32 irq_flags;
	u32 rst_gpio;
	u32 rst_flags;
	u32 pwr_gpio;
	u32 pwr_flags;
	bool audio;
	void *edid_data;
	bool hpd_state;
 
	struct i2c_client *i2c_client;
};
 static struct lt8912 *glt8912;


#if HDMI_MODE_720P_TEST_MODE
static struct lt8912_reg_cfg lt8912_720p_test_mode[] = {
	//DigitalClockEn
	{I2C_ADDR_MAIN, 0x08, 0xff, 0}, 
	{I2C_ADDR_MAIN, 0x09, 0xff, 0},  
	{I2C_ADDR_MAIN, 0x0a, 0xff, 0},
	{I2C_ADDR_MAIN, 0x0b, 0x7c, 0},
	{I2C_ADDR_MAIN, 0x0c, 0xff, 0},
	
	//	TxAnalog
	{I2C_ADDR_MAIN, 0x31, 0xa1, 0},
	{I2C_ADDR_MAIN, 0x32, 0xbf, 0},
	{I2C_ADDR_MAIN, 0x33, 0x17, 0},
	{I2C_ADDR_MAIN, 0x37, 0x00, 0},
	{I2C_ADDR_MAIN, 0x38, 0x22, 0},
	{I2C_ADDR_MAIN, 0x60, 0x82, 0},

	{I2C_ADDR_MAIN, 0x3a, 0x00, 0},
	//	CbusAnalog
	{I2C_ADDR_MAIN, 0x39, 0x45, 0},
	{I2C_ADDR_MAIN, 0x3b, 0x00, 0},

	//HDMIPllAnalog
	{I2C_ADDR_MAIN, 0x44, 0x30, 0},
	{I2C_ADDR_MAIN, 0x55, 0x44, 0},
	{I2C_ADDR_MAIN, 0x57, 0x01, 0},
	{I2C_ADDR_MAIN, 0x5a, 0x02, 0},
	
	{I2C_ADDR_MAIN, 0xb2, 0x01, 0},// 0x01: HDMI,0x00:DVI;Some monitors require AVI for output.

	//AVI Packet config()
	/*{I2C_ADDR_I2S, 0x3e, 0x0A, 0},
	{I2C_ADDR_I2S, 0x43, 0x44, 0},
	{I2C_ADDR_I2S, 0x44, 0x10, 0},
	{I2C_ADDR_I2S, 0x45, 0x19, 0},
	{I2C_ADDR_I2S, 0x47, 0x04, 0},*/
	
	//720P test pattern I2CADR = 0x92;
	{I2C_ADDR_CEC_DSI, 0x72, 0x12, 0},
	{I2C_ADDR_CEC_DSI, 0x73, 0x04, 0},
	{I2C_ADDR_CEC_DSI, 0x74, 0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x75, 0x19, 0},
	{I2C_ADDR_CEC_DSI, 0x76, 0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x77, 0xd0, 0},
	{I2C_ADDR_CEC_DSI, 0x78, 0x25, 0},
	{I2C_ADDR_CEC_DSI, 0x79, 0x72, 0},
	{I2C_ADDR_CEC_DSI, 0x7a, 0xee, 0},
	{I2C_ADDR_CEC_DSI, 0x7b, 0x26, 0},
	{I2C_ADDR_CEC_DSI, 0x7c, 0x28, 0},
	{I2C_ADDR_CEC_DSI, 0x7d, 0x05, 0},
	
	{I2C_ADDR_CEC_DSI, 0x70, 0x80, 0},
	{I2C_ADDR_CEC_DSI, 0x71, 0x76, 0},

	// 74.25M CLK
	{I2C_ADDR_CEC_DSI, 0x4e, 0x99, 0},
	{I2C_ADDR_CEC_DSI, 0x4f, 0x99, 0},
	{I2C_ADDR_CEC_DSI, 0x50, 0x69, 0},
	{I2C_ADDR_CEC_DSI, 0x51, 0x80, 0},

	{I2C_ADDR_MAIN, 0x03, 0x7f, 1000},
	{I2C_ADDR_MAIN, 0x03, 0xff, 0},
		
};
#else
static struct lt8912_reg_cfg lt8912_init_briage[] = {

/* Digital clock en*/
/* power down */
	{I2C_ADDR_MAIN, 0x08, 0xff, 0},
/* HPD override */
	{I2C_ADDR_MAIN, 0x09, 0x81, 0},
/* color space */
	{I2C_ADDR_MAIN, 0x0a, 0xff, 0},
	{I2C_ADDR_MAIN, 0x0b, 0x64, 0}, 
	/* HDCP */
	{I2C_ADDR_MAIN, 0x0c, 0xff, 0},

	{I2C_ADDR_MAIN, 0x44, 0x31, 0},
	{I2C_ADDR_MAIN, 0x51, 0x1f, 0},
	
/*Tx Analog*/
	{I2C_ADDR_MAIN, 0x31, 0xa1, 0},
	{I2C_ADDR_MAIN, 0x32, 0xbf, 0},
	{I2C_ADDR_MAIN, 0x33, 0x17, 0},
	{I2C_ADDR_MAIN, 0x37, 0x00, 0},
	{I2C_ADDR_MAIN, 0x38, 0x22, 0},
	{I2C_ADDR_MAIN, 0x60, 0x82, 0},
	{I2C_ADDR_MAIN, 0x3a, 0x00, 0},
	
/*Cbus Analog*/
	{I2C_ADDR_MAIN, 0x39, 0x45, 0},
	{I2C_ADDR_MAIN, 0x3b, 0x00, 0},
	
/*HDMI Pll Analog*/ 
	{I2C_ADDR_MAIN, 0x44, 0x30, 0},
	{I2C_ADDR_MAIN, 0x55, 0x44, 0},
	{I2C_ADDR_MAIN, 0x57, 0x01, 0},
	{I2C_ADDR_MAIN, 0x5a, 0x02, 0},

#if DVI_MODE_OUTPUT
	{I2C_ADDR_MAIN, 0xb2, 0x00, 0},// 0x01: HDMI,0x00:DVI;Some monitors require AVI for output.	
#else
	{I2C_ADDR_MAIN, 0xb2, 0x01, 0},
#endif
	
/*MIPI Analog*/
	{I2C_ADDR_MAIN, 0x3e, 0x96, 0}, 
	{I2C_ADDR_MAIN, 0x41, 0x7c, 0}, 

/* MipiBasicSet */
	{I2C_ADDR_CEC_DSI, 0x10, 0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x11, 0x08, 0}, 
	{I2C_ADDR_CEC_DSI, 0x12, 0x04, 0}, 
	{I2C_ADDR_CEC_DSI, 0x13, 0x00, 0}, 
	{I2C_ADDR_CEC_DSI, 0x14, 0x00, 0}, 
	{I2C_ADDR_CEC_DSI, 0x15, 0x00, 0}, 
	{I2C_ADDR_CEC_DSI, 0x1a, 0x03, 0}, 
	{I2C_ADDR_CEC_DSI, 0x1b, 0x03, 0}, 
#if 0
/* 720 MIPIDigital */
	{I2C_ADDR_CEC_DSI, 0x18, 0x28, 0},	  //hsync
	{I2C_ADDR_CEC_DSI, 0x19, 0x05, 0},	  //vsync

	{I2C_ADDR_CEC_DSI, 0x1c, 0x00, 0},	 //hactive
	{I2C_ADDR_CEC_DSI, 0x1d, 0x05, 0},   //hactive >> 8

	{I2C_ADDR_CEC_DSI, 0x1e, 0x67, 0},
	{I2C_ADDR_CEC_DSI, 0x2f, 0x0c, 0},

	{I2C_ADDR_CEC_DSI, 0x34, 0x72, 0},//htotal
	{I2C_ADDR_CEC_DSI, 0x35, 0x06, 0},//htotal>>8

	{I2C_ADDR_CEC_DSI, 0x36, 0xee, 0}, //vtotal
	{I2C_ADDR_CEC_DSI, 0x37, 0x02, 0},//vtotal >> 8

	{I2C_ADDR_CEC_DSI, 0x38, 0x14, 0},//vbp
	{I2C_ADDR_CEC_DSI, 0x39, 0x00, 0},//vbp

	{I2C_ADDR_CEC_DSI, 0x3a, 0x05, 0},//vfp
	{I2C_ADDR_CEC_DSI, 0x3b, 0x00, 0},//vfp>> 8

	{I2C_ADDR_CEC_DSI, 0x3c, 0xdc, 0},//hbp
	{I2C_ADDR_CEC_DSI, 0x3d, 0x00, 0},//hbp>> 8

	{I2C_ADDR_CEC_DSI, 0x3e, 0x6e, 0}, //hfp
	{I2C_ADDR_CEC_DSI, 0x3f, 0x00, 0}, //hfp>> 8
#endif
/* 1080 MIPIDigital */
	{I2C_ADDR_CEC_DSI, 0x18, 0x2c, 0},	  //hsync
	{I2C_ADDR_CEC_DSI, 0x19, 0x05, 0},	  //vsync

	{I2C_ADDR_CEC_DSI, 0x1c, 0x80, 0},	 //hactive
	{I2C_ADDR_CEC_DSI, 0x1d, 0x07, 0},   //hactive >> 8

	{I2C_ADDR_CEC_DSI, 0x1e, 0x38, 0},
	{I2C_ADDR_CEC_DSI, 0x2f, 0x04, 0},

	{I2C_ADDR_CEC_DSI, 0x34, 0x98, 0},//htotal
	{I2C_ADDR_CEC_DSI, 0x35, 0x08, 0},//htotal>>8

	{I2C_ADDR_CEC_DSI, 0x36, 0x25, 0}, //vtotal
	{I2C_ADDR_CEC_DSI, 0x37, 0x11, 0},//vtotal >> 8

	{I2C_ADDR_CEC_DSI, 0x38, 0x24, 0},//vbp
	{I2C_ADDR_CEC_DSI, 0x39, 0x00, 0},//vbp

	{I2C_ADDR_CEC_DSI, 0x3a, 0x04, 0},//vfp
	{I2C_ADDR_CEC_DSI, 0x3b, 0x00, 0},//vfp>> 8

	{I2C_ADDR_CEC_DSI, 0x3c, 0x94, 0},//hbp
	{I2C_ADDR_CEC_DSI, 0x3d, 0x00, 0},//hbp>> 8

	{I2C_ADDR_CEC_DSI, 0x3e, 0x58, 0}, //hfp
	{I2C_ADDR_CEC_DSI, 0x3f, 0x00, 0}, //hfp>> 8



/* DDSConfig */
	{I2C_ADDR_CEC_DSI, 0x4e,0x52, 0}, 
	{I2C_ADDR_CEC_DSI, 0x4f,0xde, 0}, 
	{I2C_ADDR_CEC_DSI, 0x50,0xc0, 0}, 
	{I2C_ADDR_CEC_DSI, 0x51,0x80, 0},

	{I2C_ADDR_CEC_DSI, 0x1e,0x4f, 0},
	{I2C_ADDR_CEC_DSI, 0x1f,0x5e, 0},
	{I2C_ADDR_CEC_DSI, 0x20,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x21,0x2c, 0},
	{I2C_ADDR_CEC_DSI, 0x22,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x23,0xfa, 0},
	{I2C_ADDR_CEC_DSI, 0x24,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x25,0xc8, 0},
	{I2C_ADDR_CEC_DSI, 0x26,0x00, 0},
	
	{I2C_ADDR_CEC_DSI, 0x27,0x5e, 0},
	{I2C_ADDR_CEC_DSI, 0x28,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x29,0x2c, 0},
	{I2C_ADDR_CEC_DSI, 0x2a,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x2b,0xfa, 0},
	{I2C_ADDR_CEC_DSI, 0x2c,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x2d,0xc8, 0},
	{I2C_ADDR_CEC_DSI, 0x2e,0x00, 0}, 
	
	{I2C_ADDR_CEC_DSI, 0x42,0x64, 0},
	{I2C_ADDR_CEC_DSI, 0x43,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x44,0x04, 0},
	{I2C_ADDR_CEC_DSI, 0x45,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x46,0x59, 0},
	{I2C_ADDR_CEC_DSI, 0x47,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x48,0xf2, 0},
	{I2C_ADDR_CEC_DSI, 0x49,0x06, 0},
	{I2C_ADDR_CEC_DSI, 0x4a,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x4b,0x72, 0},
	{I2C_ADDR_CEC_DSI, 0x4c,0x45, 0},
	{I2C_ADDR_CEC_DSI, 0x4d,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x52,0x08, 0},
	{I2C_ADDR_CEC_DSI, 0x53,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x54,0xb2, 0},
	{I2C_ADDR_CEC_DSI, 0x55,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x56,0xe4, 0},
	{I2C_ADDR_CEC_DSI, 0x57,0x0d, 0},
	{I2C_ADDR_CEC_DSI, 0x58,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x59,0xe4, 0},
	{I2C_ADDR_CEC_DSI, 0x5a,0x8a, 0},
	{I2C_ADDR_CEC_DSI, 0x5b,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x5c,0x34, 0},
	{I2C_ADDR_CEC_DSI, 0x51,0x00, 0},

	// mipi rx reset
	{I2C_ADDR_CEC_DSI, 0x03,0x7f,100},
	{I2C_ADDR_CEC_DSI, 0x03,0xff,100},

	// DDS reset
	{I2C_ADDR_CEC_DSI, 0x05,0xfb,100},
	{I2C_ADDR_CEC_DSI, 0x05,0xff,100},
};
#endif


#if DVI_MODE_OUTPUT

static struct lt8912_reg_cfg dvi_cfg[] = {
	{I2C_ADDR_MAIN, 0xb2, 0x00, 0},
	{I2C_ADDR_I2S, 0x3e, 0x0a, 0},
		
#ifdef HDMI_MODE_480P
	{I2C_ADDR_I2S, 0x43, 0x44, 0},
	{I2C_ADDR_I2S, 0x44, 0x10, 0},
	{I2C_ADDR_I2S, 0x45, 0x19, 0},
	{I2C_ADDR_I2S, 0x47, 0x02, 0},
	
#elif HDMI_MODE_720P
	{I2C_ADDR_I2S, 0x43, 0x31, 0},
	{I2C_ADDR_I2S, 0x44, 0x10, 0},
	{I2C_ADDR_I2S, 0x45, 0x2a, 0},
	{I2C_ADDR_I2S, 0x47, 0x04, 0},
	
#elif HDMI_MODE_1080P
	{I2C_ADDR_I2S, 0x43, 0x25, 0},
	{I2C_ADDR_I2S, 0x44, 0x10, 0},
	{I2C_ADDR_I2S, 0x45, 0x2a, 0},
	{I2C_ADDR_I2S, 0x47, 0x10, 0},
};	
#endif

#else // HDMI mode output
static struct lt8912_reg_cfg audio_cfg[] = {
	{I2C_ADDR_MAIN, 0xb2, 0x01, 0},
	{I2C_ADDR_I2S, 0x06, 0x08, 0},
	{I2C_ADDR_I2S, 0x07, 0xf0, 0},
	{I2C_ADDR_I2S, 0x09, 0x00, 0},

// 48K
	{I2C_ADDR_I2S, 0x0f, 0x2b, 0},
	{I2C_ADDR_I2S, 0x37, 0x00, 0},
	{I2C_ADDR_I2S, 0x36, 0x18, 0},
	{I2C_ADDR_I2S, 0x35, 0x00, 0},

	{I2C_ADDR_I2S, 0x34, 0xde, 0},// 32bit 
//	{I2C_ADDR_I2S, 0x34, 0xde, 0},// 16bit
	
	{I2C_ADDR_I2S, 0x3c, 0x41, 0},

};
#endif

static struct lt8912_reg_cfg standby_cfg[]={
	{I2C_ADDR_MAIN, 0x08, 0x00, 0},
	{I2C_ADDR_MAIN, 0x09, 0x81, 0},
	{I2C_ADDR_MAIN, 0x0a, 0x00, 0},
	{I2C_ADDR_MAIN, 0x0b, 0x20, 0},
	{I2C_ADDR_MAIN, 0x0c, 0x00, 0},

	{I2C_ADDR_MAIN, 0x54, 0x1d, 0},
	{I2C_ADDR_MAIN, 0x51, 0x15, 0},

	{I2C_ADDR_MAIN, 0x44, 0x31, 0},
	{I2C_ADDR_MAIN, 0x41, 0xbd, 0},
	{I2C_ADDR_MAIN, 0x5c, 0x11, 0},

	{I2C_ADDR_MAIN, 0x30, 0x08, 0},
	{I2C_ADDR_MAIN, 0x31, 0x00, 0},
	{I2C_ADDR_MAIN, 0x32, 0x00, 0},
	{I2C_ADDR_MAIN, 0x34, 0x00, 0},
	{I2C_ADDR_MAIN, 0x35, 0x00, 0},
	{I2C_ADDR_MAIN, 0x36, 0x00, 0},
	{I2C_ADDR_MAIN, 0x37, 0x00, 0},
	{I2C_ADDR_MAIN, 0x38, 0x00, 0},
};

static int lt8912_write(struct lt8912 *pdata, u8 addr, u8 reg, u8 val)
{
	int ret = 0;
 
	pdata->i2c_client->addr = addr;
	ret = i2c_smbus_write_byte_data(pdata->i2c_client, reg, val);
	if (ret)
		pr_err_ratelimited("%s: wr err: addr 0x%x, reg 0x%x, val 0x%x\n",
				__func__, addr, reg, val);
	return ret;
}
 
static int lt8912_read(struct lt8912 *pdata, u8 addr,
		u8 reg, char *buf, u32 size)
{
	int ret = 0, index = reg;
 
	pdata->i2c_client->addr = addr;
 
	for (index = reg; index < (reg + size); index++) {
		ret = i2c_smbus_read_byte_data(pdata->i2c_client, index);
		if (ret < 0) {
			printk("failed to read byte data index=%d\n", index);
			return -1;
		}
		buf[index-reg] = (u8)ret;
	}

	return 0;
}
 
 
static void lt8912_write_array(struct lt8912 *pdata,
	struct lt8912_reg_cfg *cfg, int size)
{
	int ret = 0, i;
 
	size = size / sizeof(struct lt8912_reg_cfg);
	for (i = 0; i < size; i++) {
		ret = lt8912_write(pdata, cfg[i].i2c_addr,
			cfg[i].reg, cfg[i].val);
		if (ret != 0){
			pr_err("dsi0 %s: lt8912 reg write %02X to %02X failed.\n",
				__func__, cfg[i].val, cfg[i].reg);
		}
		if (cfg[i].sleep_in_ms)
			msleep(cfg[i].sleep_in_ms);
	}
}

static int lt8912_get_chipid(struct lt8912 * pdata)
{
	u8 rev0 = 0, rev1 = 0;
	int ret0, ret1;	
	
	ret0 = lt8912_read(pdata, I2C_ADDR_MAIN, LT8912_REG_CHIP_REVISION_0, &rev0, 1);
	if (ret0|| rev0 != LT8912_VAL_CHIP_REVISION_0){
		pr_err("LT8912_VAL_CHIP_REVISION_0 err, ret =%d,reg = 0x%x,val = 0x%x.\n",  ret0, LT8912_REG_CHIP_REVISION_0, rev0);
	}
	else{
		printk("LT8912_VAL_CHIP_REVISION_0 successful,val = 0x%x\n",rev0);
	}

	ret1 = lt8912_read(pdata, I2C_ADDR_MAIN, LT8912_REG_CHIP_REVISION_1, &rev1, 1);
	if (ret1 || rev1 != LT8912_VAL_CHIP_REVISION_1){
		pr_err("LT8912_VAL_CHIP_REVISION_1 err, ret =%d,reg = 0x%x,val = 0x%x.\n",  ret1, LT8912_REG_CHIP_REVISION_1, rev1);
	}
	else{
		printk("LT8912_VAL_CHIP_REVISION_1 successful,val = 0x%x\n",rev1);
	}

	if(((ret0 || rev0 != LT8912_VAL_CHIP_REVISION_0) > 0 ||  (ret1 || rev1 != LT8912_VAL_CHIP_REVISION_1) > 0)){
		pr_err("dsi0 lt8912 check chip revision not match\n");
		//return -1;
	}
	
	printk("lt8912 check chip successful.\n");
 
    return 0;
}
 
static int lt8912_parse_dt(struct device *dev, struct lt8912 *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;
	int ret = 0;
 
	ret = of_property_read_u32(np, "instance_id", &temp_val);
	printk("dsi0 %s: DT property %s is %X\n", __func__, "instance_id",temp_val);
	if (ret)
		return ret;
	
	//pdata->dev_info.instance_id = temp_val;
 
	pdata->audio = of_property_read_bool(np, "enable-audio");
 
	pdata->irq_gpio = of_get_named_gpio_flags(np,
				"irq-gpio", 0, &pdata->irq_flags);
	
	pdata->rst_gpio = of_get_named_gpio_flags(np,
				"rst-gpio", 0, &pdata->rst_flags);
	
	pdata->pwr_gpio = of_get_named_gpio_flags(np,
				"pwr-gpio", 0, &pdata->pwr_flags);

	return ret;

}
 
static int lt8912_gpio_configure(struct lt8912 *pdata, bool on)
{
	int ret = 0;
 
	if (on) {
		if (gpio_is_valid(pdata->pwr_gpio)) {
			ret = gpio_request(pdata->pwr_gpio, "lt8912_pwr_gpio");
			if (ret) {
				pr_err(" %d unable to request gpio [%d] ret=%d\n",
					__LINE__, pdata->pwr_gpio, ret);
				goto err_none;
			}
			ret = gpio_direction_output(pdata->pwr_gpio, 1);
			if (ret) {
				pr_err("dsi0 unable to set dir for gpio[%d]\n",
					pdata->pwr_gpio);
				goto err_pwr_gpio;
			}
			printk("lt8912_pwr_gpio= %d.\n",gpio_get_value(pdata->pwr_gpio));
		} else {
			pr_err(" pwr gpio not provided\n");
			goto err_none;
		}
		
		if (gpio_is_valid(pdata->rst_gpio)) {
			ret = gpio_request(pdata->rst_gpio, "lt8912_rst_gpio");
			if (ret) {
				pr_err(" %d unable to request gpio [%d] ret=%d\n",
					__LINE__, pdata->rst_gpio, ret);
				goto err_none;
			}

			ret = gpio_direction_output(pdata->rst_gpio, 0);
			if (ret) {
				pr_err(" unable to set dir for gpio[%d]\n",
					pdata->rst_gpio);
				goto err_rst_gpio;
			}
			printk("lt8912_rst_gpio= %d\n",gpio_get_value(pdata->rst_gpio));
			msleep(100);
			gpio_direction_output(pdata->rst_gpio, 1);
			printk("lt8912_rst_gpio= %d\n",gpio_get_value(pdata->rst_gpio));

		} else {
			pr_err(" rst gpio not provided\n");
			goto err_none;
		}
 
		if (gpio_is_valid(pdata->irq_gpio)) {
			ret = gpio_request(pdata->irq_gpio, "lt8912b_irq");
			if (ret) {
				pr_err(" %d unable to request irq [%d] ret=%d\n",
					__LINE__, pdata->irq_gpio, ret);
				goto err_irq_gpio;
			}
			else
				printk("lt8912_irq_gpio is ok.\n ");
			
			pdata->irq = gpio_to_irq(pdata->irq_gpio);
			if(pdata->irq == -ENXIO){
				 pr_err("failed to get irq.\n");
			}
			
		} else {
			pr_err(" irq gpio not provided\n");
			goto err_irq_gpio;
		}
		
		return 0;
	} 
	else{ 
		if (gpio_is_valid(pdata->pwr_gpio))
			gpio_free(pdata->pwr_gpio);
		if (gpio_is_valid(pdata->rst_gpio))
			gpio_free(pdata->rst_gpio);
		if (gpio_is_valid(pdata->irq_gpio))
			gpio_free(pdata->irq_gpio);
 
		return 0;
	}
 
err_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_rst_gpio:
	if (gpio_is_valid(pdata->rst_gpio))
		gpio_free(pdata->rst_gpio);
err_pwr_gpio:
	if (gpio_is_valid(pdata->pwr_gpio))
		gpio_free(pdata->pwr_gpio);

err_none:
	return ret;
}
 
 
static struct i2c_device_id lt8912_id[] = {
	{ "lt8912_dsi0", 0},
	{}
};

void lt8912_enable(int en)
{
	if(en == 1){
#if DVI_MODE_OUTPUT
		lt8912_write_array(glt8912, dvi_cfg, sizeof(dvi_cfg));
		printk("lt8912's 720P dvi mode---.\n");
#else
		if (glt8912->audio) {
			lt8912_write_array(glt8912, audio_cfg, sizeof(audio_cfg));
			printk("%s: enabling default audio configs\n", __func__);
		}
		
		printk("lt8912's 720P hdmi mode---.\n");
#endif

#if HDMI_MODE_720P_TEST_MODE
		lt8912_write_array(glt8912, lt8912_720p_test_mode,
					sizeof(lt8912_720p_test_mode));

		printk("lt8912's 720P test mode---.\n");
#else
		lt8912_write_array(glt8912, lt8912_init_briage,
					sizeof(lt8912_init_briage));
		printk("lt8912's 720P init mode---.\n");		
#endif		
		/*if (glt8912->audio) {
			printk("dsi0 %s: enabling default audio configs\n", __func__);
			lt8912_write_array(glt8912, I2S_cfg, sizeof(I2S_cfg));
		}*/
	}
	else{
		lt8912_write_array(glt8912, standby_cfg,
				sizeof(standby_cfg));
		printk("lt8912's 720P standby mode---.\n");
		}
	
}
EXPORT_SYMBOL_GPL(lt8912_enable);


#ifdef 	LT8912_HDP_CHECK
static int lt8912_hpd_status(void *data)
{
	int connected = 0;
	u8 rev0 = 0;
	
	msleep(10000);
	while(1) {
		lt8912_read(glt8912, I2C_ADDR_MAIN, 0xC1, &rev0, 1);
		connected = (rev0 & BIT(7));
		printk("%s:connected = %d,\n",__FUNCTION__, connected);
		if((connected > 0) &&(glt8912->hpd_state == false)){
			gpio_direction_output(glt8912->rst_gpio, 0);
			msleep(1000);
			gpio_direction_output(glt8912->rst_gpio, 1);
			lt8912_enable(1);
			glt8912->hpd_state = true;
		}else if((connected == 0) && (glt8912->hpd_state == true)){
			lt8912_enable(0);
			glt8912->hpd_state = false;
		}
		msleep(1000);
	}
	return 0;
}
#endif


static int lt8912_probe(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
	static struct lt8912 *pdata;
	int ret = 0;
#ifdef 	LT8912_HDP_CHECK
	int connected = 0;
	unsigned char rev1=1;
	static struct task_struct *lt8912_task=NULL;
#endif
 
	pdata = devm_kzalloc(&client->dev,sizeof(struct lt8912), GFP_KERNEL);
	if (!pdata){
		pr_err("lt8912 alloc pdata failed\n");
		return -ENOMEM;
	}
 
	ret = lt8912_parse_dt(&client->dev, pdata);
	if (ret) {
		pr_err("lt8912 %s: Failed to parse DT\n", __func__);
		goto err_dt_parse;
	}
 
	pdata->i2c_client = client;
    glt8912  = pdata;
 	
	ret = lt8912_gpio_configure(pdata, true);
	if (ret) {
		pr_err("lt8912 %s: Failed to configure GPIOs\n", __func__);
		goto err_dt_parse;
	}

	usleep_range(25000, 30000);
 
	ret = lt8912_get_chipid(pdata);
	if (ret) {
		pr_err("lt8912 %s: Failed to read chip rev\n", __func__);
		//goto err_id;
	}
	pdata->hpd_state = false;

#ifdef 	LT8912_HDP_CHECK
	msleep(1000);
	lt8912_write(pdata,I2C_ADDR_MAIN,0x09,0xff);
	lt8912_write(pdata,I2C_ADDR_MAIN,0x0b,0x7c);
	lt8912_read(pdata, I2C_ADDR_MAIN, 0xC1, &rev1, 1);
	connected =rev1 & BIT(7);

	if(connected > 0) {	
		pdata->hpd_state = true;
	}
	
	printk("%s:connected = %x\n", __FUNCTION__,connected);
	lt8912_task = kthread_run(lt8912_hpd_status,NULL,"lt8912_task");
#endif
   lt8912_enable(1);
	return 0;
//err_id:
//	lt8912_gpio_configure(pdata, false);
err_dt_parse:
	devm_kfree(&client->dev, pdata);
	return ret;
}
 

static int lt8912_remove(struct i2c_client *client)
{
	int ret = -EINVAL;
	struct lt8912 *pdata = i2c_get_clientdata(client);
	
	ret = lt8912_gpio_configure(pdata, false);
	devm_kfree(&client->dev, pdata);
	
	return ret;
}
 
static const struct of_device_id lt8912_dt_match[] = {
	{.compatible = "lontium,lt8912"},
	{}
};
 
static struct i2c_driver lt8912_driver = {
	.driver = {
		.name = "lt8912_dsi0",
		.of_match_table = lt8912_dt_match,
		.owner = THIS_MODULE,
	},
	.probe = lt8912_probe,
	.remove = lt8912_remove,
	.id_table = lt8912_id,
};
 
static int __init lt8912_init(void)
{
	return i2c_add_driver(&lt8912_driver);
}
 
static void __exit lt8912_exit(void)
{
	i2c_del_driver(&lt8912_driver);
}
 
module_init(lt8912_init);
module_exit(lt8912_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("libing <simon.li@rock-chips.com>");
MODULE_DESCRIPTION("Lontium LT8912B MIPI-DSI to HDMI/MHL bridge");
