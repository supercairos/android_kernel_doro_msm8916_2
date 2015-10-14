/*
 *  drivers/switch/switch_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

struct gpio_switch_data {
	struct switch_dev sdev;
	int gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct delayed_work work;
};

struct gpio_switch_data *data_gpio;

static void gpio_switch_work(struct work_struct *work)
{
	int state;
	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, work.work);

	state = gpio_get_value(data->gpio);
	pr_info("state:%d,cradle_state:%s.\n",state,(state ? "cradle_off":"cradle_on"));
	switch_set_state(&data->sdev, state);
}

int cradle_get_state(void)
{
	return gpio_get_value(data_gpio->gpio);
}
EXPORT_SYMBOL_GPL(cradle_get_state);

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
	    (struct gpio_switch_data *)dev_id;

	schedule_delayed_work(&switch_data->work,msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

static ssize_t switch_gpio_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_off;
	else
		state = switch_data->state_on;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static int gpio_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data;
	int ret = 0;
	printk("start gpio_switch_probe.\n");
	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	data_gpio = switch_data;
	switch_data->sdev.name = "cradle";
	switch_data->name_on = "on";
	switch_data->name_off = "off";
	switch_data->state_on = "1";
	switch_data->state_off = "0";
	switch_data->sdev.print_state = switch_gpio_print_state;
	printk("pdev->dev.of_node:%s.\n",(pdev->dev.of_node->name));
	switch_data->gpio = of_get_named_gpio_flags(pdev->dev.of_node,
				"switch,irq-gpio", 0, NULL);
	if (gpio_is_valid(switch_data->gpio)) {
		printk("%s:have get switch gpio_irq:%d.\n",__func__,switch_data->gpio);
	}else{
		pr_err("%s:err get switch gpio_irq:%d, need return.\n",__func__,switch_data->gpio);
		ret = -EBUSY;
		goto err_switch_dev_register;
	}
	ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0){
		pr_err("err register switch_data.\n");
		goto err_switch_dev_register;
	}
	ret = gpio_request(switch_data->gpio, pdev->name);
	if (ret < 0){
		pr_err("err request switch gpio %d.\n",switch_data->gpio);
		goto err_request_gpio;
	}
	ret = gpio_direction_input(switch_data->gpio);
	if (ret < 0){
		pr_err("err set switch gpio direection.\n");
		goto err_set_gpio_input;
	}
	INIT_DELAYED_WORK(&switch_data->work, gpio_switch_work);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		pr_err("err get switch gpio irq.\n");
		goto err_detect_irq_num_failed;
	}

	ret = request_irq(switch_data->irq, gpio_irq_handler,
			  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, pdev->name, switch_data);
	if (ret < 0){
		pr_err("err request switch irq.\n");
		goto err_request_irq;
	}
	/* Perform initial detection */
	gpio_switch_work(&switch_data->work.work);
	printk("gpio_switch_probe successfully.\n");
	return 0;

err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
	switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&switch_data->work);
	gpio_free(switch_data->gpio);
	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	return 0;
}
static const struct of_device_id switch_gpio_table[] = {
	{.compatible = "switch-gpio"},
	{}
};
static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= gpio_switch_remove,
	.driver		= {
		.name	= "switch-gpio",
		.owner	= THIS_MODULE,
		.of_match_table = switch_gpio_table,
	},
};

static int __init gpio_switch_init(void)
{
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	platform_driver_unregister(&gpio_switch_driver);
}

module_init(gpio_switch_init);
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
