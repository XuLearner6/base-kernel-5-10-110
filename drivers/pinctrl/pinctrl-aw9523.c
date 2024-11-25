// SPDX-License-Identifier: GPL-2.0
/*
 * Pinctrl driver for AW9523
 *
 */

#include <linux/gpio/driver.h>
#include <linux/kernel.h>
#include <linux/mfd/aw9523.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/mfd/aw9523.h>
#include "pinctrl-utils.h"

struct aw9523_pin_function {
	const char *name;
	const char *const *groups;
	unsigned int ngroups;
	int mux_option;
};

struct aw9523_pin_group {
	const char *name;
	const unsigned int pins[1];
	unsigned int npins;
};

struct aw9523_pin_config {
	u8 fun_reg;
	u8 fun_msk;
	u8 reg;
	u8 dir_msk;
	u8 val_msk;
};

struct aw9523_pctrl_info {
	struct aw9523 *aw9523;
	struct device *dev;
	struct pinctrl_dev *pctl;
	struct gpio_chip gpio_chip;
	struct pinctrl_desc pinctrl_desc;
	const struct aw9523_pin_function *functions;
	unsigned int num_functions;
	const struct aw9523_pin_group *groups;
	int num_pin_groups;
	const struct pinctrl_pin_desc *pins;
	unsigned int num_pins;
	const struct aw9523_pin_config *pin_cfg;
};

#define AW9523_PWRCTRL1_DR	BIT(0)
#define AW9523_PWRCTRL2_DR	BIT(1)
#define AW9523_PWRCTRL3_DR	BIT(2)
#define AW9523_PWRCTRL1_DATA	BIT(4)
#define AW9523_PWRCTRL2_DATA	BIT(5)
#define AW9523_PWRCTRL3_DATA	BIT(6)
#define AW9523_PWRCTRL1_FUN	0x07
#define AW9523_PWRCTRL2_FUN	0x70
#define AW9523_PWRCTRL3_FUN	0x07

enum aw9523_pinmux_option {
	AW9523_PINMUX_FUN0 = 0,
	AW9523_PINMUX_FUN1,
	AW9523_PINMUX_FUN2,
	AW9523_PINMUX_FUN3,
	AW9523_PINMUX_FUN4,
	AW9523_PINMUX_FUN5,
};

enum {
	AW9523_GPIO_0,
	AW9523_GPIO_1,
	AW9523_GPIO_2,
	AW9523_GPIO_3
};

static const char *const aw9523_gpio_groups[] = {
	"gpio_0",
	"gpio_1",
	"gpio_2",
	"gpio_3",
};

static const struct pinctrl_pin_desc aw9523_pins_desc[] = {
	PINCTRL_PIN(AW9523_GPIO_0, "gpio_0"), /* offset 0 pin */
	PINCTRL_PIN(AW9523_GPIO_1, "gpio_1"), /* offset 1 pin */
	PINCTRL_PIN(AW9523_GPIO_2, "gpio_2"), /* offset 2 pin */
	PINCTRL_PIN(AW9523_GPIO_3, "gpio_3") /* offset 3 pin */
};

static const struct aw9523_pin_function aw9523_pin_functions[] = {
	{
		.name = "pin_fun0",
		.groups = aw9523_gpio_groups,
		.ngroups = ARRAY_SIZE(aw9523_gpio_groups),
		.mux_option = AW9523_PINMUX_FUN0,
	},
	{
		.name = "pin_fun1",
		.groups = aw9523_gpio_groups,
		.ngroups = ARRAY_SIZE(aw9523_gpio_groups),
		.mux_option = AW9523_PINMUX_FUN1,
	},
	{
		.name = "pin_fun2",
		.groups = aw9523_gpio_groups,
		.ngroups = ARRAY_SIZE(aw9523_gpio_groups),
		.mux_option = AW9523_PINMUX_FUN2,
	},
	{
		.name = "pin_fun3",
		.groups = aw9523_gpio_groups,
		.ngroups = ARRAY_SIZE(aw9523_gpio_groups),
		.mux_option = AW9523_PINMUX_FUN3,
	},
	{
		.name = "pin_fun4",
		.groups = aw9523_gpio_groups,
		.ngroups = ARRAY_SIZE(aw9523_gpio_groups),
		.mux_option = AW9523_PINMUX_FUN4,
	},
	{
		.name = "pin_fun5",
		.groups = aw9523_gpio_groups,
		.ngroups = ARRAY_SIZE(aw9523_gpio_groups),
		.mux_option = AW9523_PINMUX_FUN5,
	},

};

static const struct aw9523_pin_group aw9523_pin_groups[] = {
	{
		.name = "gpio_0",
		.pins = { AW9523_GPIO_0 },
		.npins = 1,
	},
	{
		.name = "gpio_1",
		.pins = { AW9523_GPIO_1 },
		.npins = 1,
	},
	{
		.name = "gpio_2",
		.pins = { AW9523_GPIO_2 },
		.npins = 1,
	},
	{
		.name = "gpio_3",
		.pins = { AW9523_GPIO_3 },
		.npins = 1,
	}
};

static __maybe_unused struct  aw9523_pin_config aw9523_gpio_cfgs[] = {
	/*
	{
		//.fun_reg = NULL,
		.fun_msk = AW9523_PWRCTRL3_FUN,
		//.reg = NULL,
		.val_msk = AW9523_PWRCTRL3_DATA,
		.dir_msk = AW9523_PWRCTRL3_DR,
	},
	*/
	{
		//.fun_reg = NULL,
		.fun_msk = DT_UNKNOWN,
		//.reg = NULL,
		.val_msk = DT_UNKNOWN,
		.dir_msk = DT_UNKNOWN,
	},
	{
		//.fun_reg = NULL,
		.fun_msk = DT_UNKNOWN,
		//.reg = NULL,
		.val_msk = DT_UNKNOWN,
		.dir_msk = DT_UNKNOWN,
	},
	{
		//.fun_reg = NULL,
		.fun_msk = DT_UNKNOWN,
		//.reg = NULL,
		.val_msk = DT_UNKNOWN,
		.dir_msk = DT_UNKNOWN,
	}
};

/* generic gpio chip */
static int aw9523_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	//struct aw9523_pctrl_info *aw9523pctrl = gpiochip_get_data(chip);
	printk("DEBUG %s %d\n",__func__,__LINE__);

	return 0;
}

static void aw9523_gpio_set(struct gpio_chip *chip,
			   unsigned int offset,
			   int value)
{
	printk("DEBUG %s %d\n",__func__,__LINE__);
}

static int aw9523_gpio_direction_input(struct gpio_chip *chip,
				      unsigned int offset)
{
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return 0;
}

static int aw9523_gpio_direction_output(struct gpio_chip *chip,
				       unsigned int offset,
				       int value)
{
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return 0;
}

static int aw9523_gpio_get_direction(struct gpio_chip *chip,
				    unsigned int offset)
{
	//struct aw9523_pctrl_info *aw9523pctrl = gpiochip_get_data(chip);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return 0;
}

static struct gpio_chip aw9523_gpio_chip = {
	.label			= "aw9523-gpio",
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.get_direction		= aw9523_gpio_get_direction,
	.get			= aw9523_gpio_get,
	.set			= aw9523_gpio_set,
	.direction_input	= aw9523_gpio_direction_input,
	.direction_output	= aw9523_gpio_direction_output,
	.can_sleep		= true,
	.base			= -1,
	.owner			= THIS_MODULE,
};

/* generic pinctrl */
static int aw9523_pinctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return aw9523pctrl->num_pin_groups;
}

static const char *aw9523_pinctrl_get_group_name(struct pinctrl_dev *pctldev,
						unsigned int group)
{
	struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return aw9523pctrl->groups[group].name;
}

static int aw9523_pinctrl_get_group_pins(struct pinctrl_dev *pctldev,
					unsigned int group,
					const unsigned int **pins,
					unsigned int *num_pins)
{
	struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	*pins = aw9523pctrl->groups[group].pins;
	*num_pins = aw9523pctrl->groups[group].npins;
	return 0;
}

static const struct pinctrl_ops aw9523_pinctrl_ops = {
	.get_groups_count = aw9523_pinctrl_get_groups_count,
	.get_group_name = aw9523_pinctrl_get_group_name,
	.get_group_pins = aw9523_pinctrl_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinctrl_utils_free_map,
};

static int aw9523_pinctrl_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return aw9523pctrl->num_functions;
}

static const char *aw9523_pinctrl_get_func_name(struct pinctrl_dev *pctldev,
					       unsigned int function)
{
	struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return aw9523pctrl->functions[function].name;
}

static int aw9523_pinctrl_get_func_groups(struct pinctrl_dev *pctldev,
					 unsigned int function,
					 const char *const **groups,
					 unsigned int *const num_groups)
{
	struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	*groups = aw9523pctrl->functions[function].groups;
	*num_groups = aw9523pctrl->functions[function].ngroups;
	return 0;
}

static int  _aw9523_pinctrl_set_mux(struct pinctrl_dev *pctldev,
				  unsigned int offset,
				  int mux)
{
	struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	int ret;
	printk("DEBUG %s %d %d %d\n",__func__,__LINE__,offset,mux);

	if(mux)
	ret=aw9523_gpio_direction_input(&aw9523pctrl->gpio_chip, offset);
	else
	ret=aw9523_gpio_direction_output(&aw9523pctrl->gpio_chip,offset, 1);

	return ret;
}

static int aw9523_pinctrl_set_mux(struct pinctrl_dev *pctldev,
				 unsigned int function,
				 unsigned int group)
{
	struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	int mux = aw9523pctrl->functions[function].mux_option;
	int offset = group;
	printk("DEBUG %s %d\n",__func__,__LINE__);
	_aw9523_pinctrl_set_mux(pctldev, offset, mux);
	return 0;
}

static int aw9523_pmx_gpio_set_direction(struct pinctrl_dev *pctldev,
					struct pinctrl_gpio_range *range,
					unsigned int offset, bool input)
{
	//struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return 0;
}

static int aw9523_pinctrl_gpio_request_enable(struct pinctrl_dev *pctldev,
					     struct pinctrl_gpio_range *range,
					     unsigned int offset)
{
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return 0;
}

static const struct pinmux_ops aw9523_pinmux_ops = {
	.gpio_request_enable	= aw9523_pinctrl_gpio_request_enable,
	.get_functions_count	= aw9523_pinctrl_get_funcs_count,
	.get_function_name	= aw9523_pinctrl_get_func_name,
	.get_function_groups	= aw9523_pinctrl_get_func_groups,
	.set_mux		= aw9523_pinctrl_set_mux,
	.gpio_set_direction	= aw9523_pmx_gpio_set_direction,
};

static int aw9523_pinconf_get(struct pinctrl_dev *pctldev,
			     unsigned int pin,
			     unsigned long *config)
{
	//struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return 0;
}

static int aw9523_pinconf_set(struct pinctrl_dev *pctldev,
			     unsigned int pin,
			     unsigned long *configs,
			     unsigned int num_configs)
{
	//struct aw9523_pctrl_info *aw9523pctrl = pinctrl_dev_get_drvdata(pctldev);
	printk("DEBUG %s %d\n",__func__,__LINE__);
	return 0;
}

static const struct pinconf_ops aw9523_pinconf_ops = {
	.pin_config_get = aw9523_pinconf_get,
	.pin_config_set = aw9523_pinconf_set,
};

static struct pinctrl_desc aw9523_pinctrl_desc = {
	.name = "aw9523-pinctrl",
	.pctlops = &aw9523_pinctrl_ops,
	.pmxops = &aw9523_pinmux_ops,
	.confops = &aw9523_pinconf_ops,
	.owner = THIS_MODULE,
};


static int aw9523_pinctrl_probe(struct platform_device *pdev)
{
	struct aw9523_pctrl_info *aw9523pctrl;
	struct device_node *np;
	int ret;
	printk("DEBUG %s %d\n",__func__,__LINE__);

	aw9523pctrl = devm_kzalloc(&pdev->dev, sizeof(*aw9523pctrl), GFP_KERNEL);
	if (!aw9523pctrl)
		return -ENOMEM;
	printk("DEBUG %s %d\n",__func__,__LINE__);

	aw9523pctrl->dev = &pdev->dev;
	np = of_get_child_by_name(pdev->dev.parent->of_node, "aw9523_pinctrl");
	if (np){
		aw9523pctrl->dev->of_node = np;
		printk("DEBUG %s %d\n",__func__,__LINE__);
		}
	else{
		printk("DEBUG %s %d\n",__func__,__LINE__);
		aw9523pctrl->dev->of_node = pdev->dev.parent->of_node;
		}
	aw9523pctrl->aw9523 = dev_get_drvdata(pdev->dev.parent);
	printk("DEBUG %s %d\n",__func__,__LINE__);

	platform_set_drvdata(pdev, aw9523pctrl);
	printk("DEBUG %s %d\n",__func__,__LINE__);

	aw9523pctrl->pinctrl_desc = aw9523_pinctrl_desc;
	aw9523pctrl->gpio_chip = aw9523_gpio_chip;
	aw9523pctrl->pins = aw9523_pins_desc;
	aw9523pctrl->num_pins = ARRAY_SIZE(aw9523_pins_desc);
	aw9523pctrl->functions = aw9523_pin_functions;
	aw9523pctrl->num_functions = ARRAY_SIZE(aw9523_pin_functions);
	aw9523pctrl->groups = aw9523_pin_groups;
	aw9523pctrl->num_pin_groups = ARRAY_SIZE(aw9523_pin_groups);
	aw9523pctrl->pinctrl_desc.pins = aw9523_pins_desc;
	aw9523pctrl->pinctrl_desc.npins = ARRAY_SIZE(aw9523_pins_desc);
	aw9523pctrl->pin_cfg = aw9523_gpio_cfgs;
	aw9523pctrl->gpio_chip.ngpio = ARRAY_SIZE(aw9523_gpio_cfgs);
	printk("DEBUG %s %d\n",__func__,__LINE__);

	aw9523pctrl->gpio_chip.parent = &pdev->dev;

	if (np)
		aw9523pctrl->gpio_chip.of_node = np;
	else
		aw9523pctrl->gpio_chip.of_node = pdev->dev.parent->of_node;
	printk("DEBUG %s %d\n",__func__,__LINE__);

	/* Add gpiochip */
	ret = devm_gpiochip_add_data(&pdev->dev, &aw9523pctrl->gpio_chip, aw9523pctrl);
	if (ret < 0) {
		dev_err(&pdev->dev, "Couldn't add gpiochip\n");
		return ret;
	}
	printk("DEBUG %s %d\n",__func__,__LINE__);

	/* Add pinctrl */
	aw9523pctrl->pctl = devm_pinctrl_register(&pdev->dev, &aw9523pctrl->pinctrl_desc, aw9523pctrl);
	if (IS_ERR(aw9523pctrl->pctl)) {
		dev_err(&pdev->dev, "Couldn't add pinctrl\n");
		return PTR_ERR(aw9523pctrl->pctl);
	}
	printk("DEBUG %s %d\n",__func__,__LINE__);

	/* Add pin range */
	ret = gpiochip_add_pin_range(&aw9523pctrl->gpio_chip,
				     dev_name(&pdev->dev),
				     0,
				     0,
				     aw9523pctrl->gpio_chip.ngpio);
	if (ret < 0) {
		dev_err(&pdev->dev, "Couldn't add gpiochip pin range\n");
		return ret;
	}
	printk("DEBUG %s %d\n",__func__,__LINE__);

	return 0;
}

static const struct of_device_id aw9523_pinctrl_of_match[] = {
	{ .compatible = "awinic,aw9523-pinctrl", },
	{ }
};
MODULE_DEVICE_TABLE(of, aw9523_gpio_of_match);


static struct platform_driver aw9523_pinctrl_driver = {
	.probe = aw9523_pinctrl_probe,
	.driver = {
		.name = "aw9523-pinctrl",
		.of_match_table = aw9523_pinctrl_of_match,
	},
};

//module_platform_driver(aw9523_pinctrl_driver);

static int __init aw9523_pinctrl_driver_register(void)
{
	return platform_driver_register(&aw9523_pinctrl_driver);
}

fs_initcall_sync(aw9523_pinctrl_driver_register);

MODULE_DESCRIPTION("AW9523 pin control and GPIO driver");
MODULE_AUTHOR("DaHua <support@neardi.com>");
MODULE_LICENSE("GPL v2");
