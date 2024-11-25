/*
 * Definitions and platform data for Analog Devices
 * ADP5520/ADP5501 MFD PMICs (Backlight, LED, GPIO and Keys)
 *
 * Copyright 2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */


#ifndef __LINUX_MFD_AW9523_H
#define __LINUX_MFD_AW9523_H

#define ID_AW9523B		0x23

/*
 * AW9523B Register Map
 */
/*register list */
#define P0_INPUT		0x00
#define P1_INPUT 		0x01
#define P0_OUTPUT 		0x02
#define P1_OUTPUT 		0x03
#define P0_DIR			0x04
#define P1_DIR	 		0x05
#define P0_INT			0x06
#define P1_INT			0x07
#define ID_REG			0x10
#define CTL_REG			0x11
#define P0_LED_MODE		0x12
#define P1_LED_MODE		0x13
#define P1_0_DIM0		0x20
#define P1_1_DIM0		0x21
#define P1_2_DIM0		0x22
#define P1_3_DIM0		0x23
#define P0_0_DIM0		0x24
#define P0_1_DIM0		0x25
#define P0_2_DIM0		0x26
#define P0_3_DIM0		0x27
#define P0_4_DIM0		0x28
#define P0_5_DIM0		0x29
#define P0_6_DIM0		0x2A
#define P0_7_DIM0		0x2B
#define P1_4_DIM0		0x2C
#define P1_5_DIM0		0x2D
#define P1_6_DIM0		0x2E
#define P1_7_DIM0		0x2F
#define SW_RSTN			0x7F

struct aw9523_gpio_platform_data {
	unsigned gpio_start;
	u8 gpio_en_mask;
	u8 gpio_pullup_mask;
};

struct aw9523_keys_platform_data {
	int rows_en_mask;		/* Number of rows */
	int cols_en_mask;		/* Number of columns */
	const unsigned short *keymap;	/* Pointer to keymap */
	unsigned short keymapsize;	/* Keymap size */
	unsigned repeat:1;		/* Enable key repeat */
};

struct aw9523_leds_platform_data {
	int num_leds;
	struct led_info	*leds;
	u8 fade_in;		/* Backlight Fade-In Timer */
	u8 fade_out;		/* Backlight Fade-Out Timer */
	u8 led_on_time;
};


/*
 * MFD chip platform data
 */

struct aw9523_platform_data {
	struct aw9523_gpio_platform_data *gpio;
	struct aw9523_leds_platform_data *leds;
	struct aw9523_keys_platform_data *keys;
};

struct aw9523 {
	struct device *dev;
	struct regmap *regmap;

	struct gpio_desc *reset;	/* hardware reset gpio */

	struct aw9523_platform_data *pdata;
	unsigned long pin_used;
};

/*
 * MFD chip functions
 */

extern int aw9523_read(struct aw9523 *aw9523, u8 reg, u8 *read);
extern int aw9523_write(struct aw9523 *aw9523, u8 reg, u8 val);
extern int aw9523_update_bits(struct aw9523 *aw9523, u8 reg, u8 mask, u8 data);


#endif /* __LINUX_MFD_AW9523_H */
