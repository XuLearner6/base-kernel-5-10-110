// SPDX-License-Identifier: GPL-2.0+
/*
 * MFD driver for AWINIC AW9523B Devices
 * LEDs		: drivers/led/leds-aw9523.c
 * GPIO		: drivers/gpio/gpio-aw9523.c
 * Keys		: drivers/input/keyboard/aw9523-keys.c
 *
 * Copyright 2020 camus@rtavs.com
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <linux/mfd/aw9523.h>

static const struct mfd_cell aw9523_devs[] = {
	{
		.name = "aw9523-leds",
		.of_compatible = "awinic,awinic-leds",
	},
	{
		.name = "aw9523-gpio",
		.of_compatible = "awinic,awinic-gpio",
	},
	{
		.name = "aw9523-keypad",
		.of_compatible = "awinic,awinic-keypad",
	},
};

int aw9523_read(struct aw9523 *aw9523, u8 reg, u8 *read)
{
	int ret;
	unsigned int val;

	ret = regmap_read(aw9523->regmap, reg, &val);
	if (ret < 0)
		return ret;

	*read = (u8)val;
	return 0;
}
EXPORT_SYMBOL_GPL(aw9523_read);

int aw9523_write(struct aw9523 *aw9523, u8 reg, u8 val)
{
	return regmap_write(aw9523->regmap, reg, val);
}
EXPORT_SYMBOL_GPL(aw9523_write);


int aw9523_update_bits(struct aw9523 *aw9523, u8 reg, u8 mask, u8 data)
{
	return regmap_update_bits(aw9523->regmap, reg, mask, data);
}
EXPORT_SYMBOL_GPL(aw9523_update_bits);


static void aw9523_hw_reset(struct aw9523 *aw9523)
{
	gpiod_set_value(aw9523->reset, 0);
	usleep_range(3000, 3100);
	gpiod_set_value(aw9523->reset, 1);
	usleep_range(3000, 3100);
	gpiod_set_value(aw9523->reset, 0);
	usleep_range(10000, 11000);
}

static int aw9523_sw_reset(struct aw9523 *aw9523)
{
	int ret = aw9523_write(aw9523, SW_RSTN, 0);
	if (ret < 0)
		dev_err(aw9523->dev, "soft reset failed %d\n", ret);

	return ret;
}

static const struct regmap_config aw9523_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SW_RSTN,
};

static int aw9523_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct aw9523 *aw9523;
	struct device *dev = &client->dev;
	int ret;
	u8 revid = 0;

	aw9523 = devm_kzalloc(dev, sizeof(*aw9523), GFP_KERNEL);
	if (!aw9523)
		return -ENOMEM;

	aw9523->regmap = devm_regmap_init_i2c(client, &aw9523_regmap_config);
	if (IS_ERR(aw9523->regmap))
		return PTR_ERR(aw9523->regmap);

	aw9523->reset = devm_gpiod_get_optional(&client->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(aw9523->reset))
		return PTR_ERR(aw9523->reset);

	aw9523->dev = dev;
	aw9523->pdata = dev_get_platdata(dev);
	if (!aw9523->pdata) {
		dev_info(aw9523->dev, "aw9523 no platdata\n");
	}

	aw9523_hw_reset(aw9523);

	ret = aw9523_sw_reset(aw9523);

	ret = aw9523_read(aw9523, ID_REG, &revid);

	if (ret < 0 || revid != ID_AW9523B) {
		dev_err(aw9523->dev, "ge chipid failed %d, revid=0x%02x\n", ret, revid);
		return -EINVAL;
	}

	dev_info(aw9523->dev, "aw9523 detected: 0x%02x\n", revid);

	/* always enable push-pull mode and P0 */
	aw9523_update_bits(aw9523, CTL_REG, BIT(4), 1 << 4);

	i2c_set_clientdata(client, aw9523);

	ret = mfd_add_devices(dev, -1, aw9523_devs,
				    ARRAY_SIZE(aw9523_devs),
				    NULL, 0, NULL);
	if (ret) {
		dev_err(aw9523->dev, "failed to add MFD devices: %d\n", ret);
		return ret;
	}
	return 0;

}

static const struct i2c_device_id aw9523_ids[] = {
	{"aw9523", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw9523_ids);

#ifdef CONFIG_OF
static const struct of_device_id aw9523_of_match[] = {
	{ .compatible = "awinic,aw9523", },
	{ }
};
MODULE_DEVICE_TABLE(of, aw9523_of_match);
#endif

static struct i2c_driver aw9523_driver = {
	.driver = {
		.name = "aw9523",
		.of_match_table = of_match_ptr(aw9523_of_match),
	},
	.probe = aw9523_probe,
	.id_table = aw9523_ids,
};

module_i2c_driver(aw9523_driver);

MODULE_DESCRIPTION("AW9523 MFD Core Driver");
MODULE_AUTHOR("Kaspter Ju <camus@rtavs.com>");
MODULE_LICENSE("GPL");
