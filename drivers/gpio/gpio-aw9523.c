// SPDX-License-Identifier: GPL-2.0+
/*
 * GPIO driver for AWINIC AW9523B Devices
 *
 * Copyright 2020 camus@rtavs.com
 *
 */

//#define DEBUG
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/gpio/driver.h>
#include <linux/platform_device.h>

#include <linux/mfd/aw9523.h>


struct aw9523_gpio {
	struct gpio_chip chip;
	struct aw9523 *aw9523;
};

static int aw9523_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct aw9523_gpio *aw9523_gpio = gpiochip_get_data(chip);
	struct aw9523 *aw9523 = aw9523_gpio->aw9523;

	dev_dbg(aw9523->dev, "gpio request offset %d\n", offset);

	/* Return an error if the pin is already assigned */
	if (test_and_set_bit(offset, &aw9523->pin_used))
		return -EBUSY;

	return 0;
}

static void aw9523_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct aw9523_gpio *aw9523_gpio = gpiochip_get_data(chip);
	struct aw9523 *aw9523 = aw9523_gpio->aw9523;

	dev_dbg(aw9523->dev, "gpio free offset %d\n", offset);

	clear_bit(offset, &aw9523->pin_used);
}

// return the current direction of a GPIO
// Returns 0 for output, 1 for input, or an error code in case of error.
static int aw9523_gpio_get_direction(struct gpio_chip *chip, unsigned off)
{
	struct aw9523_gpio *aw9523_gpio = gpiochip_get_data(chip);
	struct aw9523 *aw9523 = aw9523_gpio->aw9523;

	unsigned port = off >> 3;
	unsigned shift = off % 8;
	u8 val;

	int ret = aw9523_read(aw9523, P0_DIR + port, &val);
	if (ret < 0){
		return -EINVAL;
	}

	dev_dbg(aw9523->dev, "get_dir offset %d, bank %d, shift %d, val 0x%02x, bit %d\n", off, port, shift, val, (int)(!!(val & BIT(shift))));

	return !!(val & BIT(shift));
}

// set the GPIO direction to input
// Return 0 in case of success, else an error code.
static int aw9523_gpio_direction_input(struct gpio_chip *chip, unsigned off)
{
	struct aw9523_gpio *aw9523_gpio = gpiochip_get_data(chip);
	struct aw9523 *aw9523 = aw9523_gpio->aw9523;

	unsigned port = off >> 3;
	unsigned shift = off % 8;

	int ret =  aw9523_update_bits(aw9523, P0_DIR + port, BIT(shift), BIT(shift));

	dev_dbg(aw9523->dev, "dir_input offset %d, bank %d, shift %d\n", off, port, shift);

	return ret;
}


static int aw9523_gpio_get_value(struct gpio_chip *chip, unsigned off)
{
	struct aw9523_gpio *aw9523_gpio = gpiochip_get_data(chip);
	struct aw9523 *aw9523 = aw9523_gpio->aw9523;

	unsigned port = off >> 3;
	unsigned shift = off % 8;
	u8 dir, val;

	/* check gpio direction */
	int ret = aw9523_read(aw9523, P0_DIR + port, &dir);
	if (ret < 0){
		return -EINVAL;
	}

	if (!!(dir & BIT(shift))) {
		ret = aw9523_read(aw9523, P0_INPUT + port, &val);
	} else {
		ret = aw9523_read(aw9523, P0_OUTPUT + port, &val);
	}

	if (ret < 0){
		return ret;
	}

	dev_dbg(aw9523->dev, "get_value offset %d, bank %d, shift %d, val 0x%02x, bit %d\n", off, port, shift, val, (int)(!!(val & BIT(shift))));

	return !!(val & BIT(shift));
}

static void aw9523_gpio_set_value(struct gpio_chip *chip,
				   unsigned off, int val)
{
	struct aw9523_gpio *aw9523_gpio = gpiochip_get_data(chip);
	struct aw9523 *aw9523 = aw9523_gpio->aw9523;

	unsigned port = off >> 3;
	unsigned shift = off % 8;

	//ret = aw9523_update_bits(aw9523, P0_DIR + port, BIT(shift), 0);
	aw9523_update_bits(aw9523, P0_OUTPUT + port, BIT(shift), (val << shift));

	dev_dbg(aw9523->dev, "set_value offset %d, bank %d, shift %d, value %d\n", off, port, shift, val);
}

// set the GPIO direction to output
static int aw9523_gpio_direction_output(struct gpio_chip *chip,
					 unsigned off, int val)
{
	int ret;
	struct aw9523_gpio *aw9523_gpio = gpiochip_get_data(chip);
	struct aw9523 *aw9523 = aw9523_gpio->aw9523;

	unsigned port = off >> 3;
	unsigned shift = off % 8;

	ret = aw9523_update_bits(aw9523, P0_DIR + port, BIT(shift), 0);
	ret |= aw9523_update_bits(aw9523, P0_OUTPUT + port, BIT(shift), (val << shift));

	dev_dbg(aw9523->dev, "dir_out offset %d, bank %d, shift %d, value %d\n", off, port, shift, (val<<shift));

	return ret;
}

static const struct gpio_chip aw9523_gpio_chip = {
	.label				= "aw9523",
	.owner				= THIS_MODULE,

	.request			= aw9523_gpio_request,
	.free				= aw9523_gpio_free,

	.direction_input	= aw9523_gpio_direction_input,
	.get				= aw9523_gpio_get_value,

	.direction_output	= aw9523_gpio_direction_output,
	.set				= aw9523_gpio_set_value,

	.get_direction		= aw9523_gpio_get_direction,

	.base				= -1,
	.ngpio				= 16,
	.can_sleep			= true,
};


static int aw9523_gpio_probe(struct platform_device *pdev)
{
	struct aw9523 *aw9523 = dev_get_drvdata(pdev->dev.parent);
	struct aw9523_gpio *aw9523_gpio;
	int ret=0,shield=0;
	
	aw9523_gpio = devm_kzalloc(&pdev->dev, sizeof(*aw9523_gpio), GFP_KERNEL);
	if (!aw9523_gpio)
		return -ENOMEM;

	aw9523_gpio->aw9523 = aw9523;
	aw9523_gpio->chip = aw9523_gpio_chip;
	aw9523_gpio->chip.parent = &pdev->dev;

	platform_set_drvdata(pdev, aw9523_gpio);

	ret = devm_gpiochip_add_data(&pdev->dev, &aw9523_gpio->chip, aw9523_gpio);
	if(ret != 0){
		printk("devm_gpiochip_add_data error\n");
	}

	ret = device_property_read_u32(&pdev->dev, "shield-switch", &shield);
	if (ret < 0) {
			dev_err(&pdev->dev, "shield-switch missing %d\n",shield);
		}

	/*
	* N4_PWREN_A: EXTIO_P1_1 -> gpio502
	*
	* N4_PWREN_B: EXTIO_P1_2 -> gpio503
	*/
	if(!shield){
	ret = aw9523_gpio_request(&aw9523_gpio->chip,9);
		if(ret != 0){
		printk("aw9523_gpio_request error\n");
		}
	
		ret = aw9523_gpio_request(&aw9523_gpio->chip,10);
		if(ret != 0){
		printk("aw9523_gpio_request error\n");
		}
	
		
	 
		ret = aw9523_gpio_direction_output(&aw9523_gpio->chip,9,1);
		if(ret <0){
		printk("aw9523_gpio_direction_output error\n");
		 return -EPROBE_DEFER;
		}
		aw9523_gpio_get_direction(&aw9523_gpio->chip,9);
		aw9523_gpio_set_value(&aw9523_gpio->chip,9,1);
		aw9523_gpio_get_value(&aw9523_gpio->chip,9);
	
		ret = aw9523_gpio_direction_output(&aw9523_gpio->chip,10,1);
		if(ret <0){
		printk("aw9523_gpio_direction_output error\n");
		 return -EPROBE_DEFER;
		}
		aw9523_gpio_get_direction(&aw9523_gpio->chip,10);
		aw9523_gpio_set_value(&aw9523_gpio->chip,10,1);
		aw9523_gpio_get_value(&aw9523_gpio->chip,10);
	}

	return ret;

}


static const struct of_device_id aw9523_gpio_of_match[] = {
	{ .compatible = "awinic,aw9523-gpio", },
	{ }
};
MODULE_DEVICE_TABLE(of, aw9523_gpio_of_match);

static struct platform_driver aw9523_gpio_driver = {
	.probe = aw9523_gpio_probe,
	.driver = {
		.name = "aw9523-gpio",
		.of_match_table = aw9523_gpio_of_match,
	},
};
module_platform_driver(aw9523_gpio_driver);

MODULE_DESCRIPTION("AW9523 GPIO Driver");
MODULE_AUTHOR("Kaspter Ju <camus@rtavs.com>");
MODULE_LICENSE("GPL");
