#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/workqueue.h>



static int export_gpio_probe(struct platform_device *pdev)
{

	struct device_node *node;
	int length;
	int gpio_temp[10],i;
	const char *name;
	
	enum of_gpio_flags flags;

	node = of_find_node_by_name(NULL, "gpio-export");

	if (IS_ERR_OR_NULL(node)) {
		dev_err(&pdev->dev, "%s dev node err\n",  __func__);
		return -ENODEV;
	}

	length = of_gpio_named_count(node, "neardi,gpio-export");
	if(length < 0) {
		dev_err(&pdev->dev, "%s gpio-export count err\n",  __func__);
		return -ENODEV;
	}

	if (length > 0 && length < 10) {
		for (i = 0; i < length; i++) {
			gpio_temp[i] = of_get_named_gpio_flags(node, "neardi,gpio-export",i,&flags);
										 			
			if (!gpio_is_valid(gpio_temp[i])){
				dev_err(&pdev->dev, " %d gpio invalid %s err\n",gpio_temp[i],__func__);
				break;
			}
				
			//if (of_property_read_string_index(node, "neardi,gpio-name", i, (const char **)&name))
			//	break;
			devm_gpio_request_one(&pdev->dev,gpio_temp[i],((flags & OF_GPIO_ACTIVE_LOW) ? GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH)|GPIOF_EXPORT,name);

		}
	}

	//count = of_property_count_strings(node, "neardi,gpio-name");
	//if (count < 1)
	//	return -ENODEV;
	
	return 0;

}


static int export_gpio_remove(struct platform_device *pdev)
{
	
	return 0;
}


static const struct of_device_id of_export_gpio_match[] = {
{ .compatible = "neardi,gpio-export", },
	{},
};
MODULE_DEVICE_TABLE(of, of_export_gpio_match);


static struct platform_driver export_gpio_driver = {
	.probe = export_gpio_probe,
	.remove = export_gpio_remove,
	.driver		= {
		.name	= "gpio-export",
		.of_match_table = of_export_gpio_match,
	},
};


module_platform_driver(export_gpio_driver);
