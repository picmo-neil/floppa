// SPDX-License-Identifier: GPL-2.0
/*
 * Crash Key Driver - Trigger kernel panic via key combo for debug
 *
 * Hold Power button and press Volume Down 5 times to trigger a warm reboot
 * via kernel panic. This preserves RAM contents for last_kmsg.
 *
 * Copyright (C) 2026 Flopster101
 * Based on Samsung sec_crash_key.c
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>

static bool power_pressed;
static int voldown_press_count;

static void crash_key_check(void)
{
	if (power_pressed && voldown_press_count >= 5) {
		pr_emerg("Crash key combo completed - triggering panic!\n");
		panic("Crash Key");
	}
}

static void crash_key_event(struct input_handle *handle, unsigned int type,
			    unsigned int code, int value)
{
	if (type != EV_KEY)
		return;

	switch (code) {
	case KEY_POWER:
		power_pressed = !!value;
		if (!power_pressed) {
			/* Power released - reset count */
			voldown_press_count = 0;
		}
		break;
	case KEY_VOLUMEDOWN:
		if (value == 1 && power_pressed) {
			/* VolDown pressed while Power held */
			voldown_press_count++;
			pr_info("Crash key: VolDown press %d/5\n", voldown_press_count);
		}
		break;
	default:
		return;
	}

	crash_key_check();
}

static int crash_key_connect(struct input_handler *handler,
			     struct input_dev *dev,
			     const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "crash_key";

	error = input_register_handle(handle);
	if (error)
		goto err_free;

	error = input_open_device(handle);
	if (error)
		goto err_unregister;

	return 0;

err_unregister:
	input_unregister_handle(handle);
err_free:
	kfree(handle);
	return error;
}

static void crash_key_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id crash_key_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};

MODULE_DEVICE_TABLE(input, crash_key_ids);

static struct input_handler crash_key_handler = {
	.event		= crash_key_event,
	.connect	= crash_key_connect,
	.disconnect	= crash_key_disconnect,
	.name		= "crash_key",
	.id_table	= crash_key_ids,
};

static int __init crash_key_init(void)
{
	int ret;

	ret = input_register_handler(&crash_key_handler);
	if (ret) {
		pr_err("Failed to register input handler: %d\n", ret);
		return ret;
	}

	pr_info("Crash key driver loaded (hold Power, press VolDown 5 times)\n");
	return 0;
}

static void __exit crash_key_exit(void)
{
	input_unregister_handler(&crash_key_handler);
	pr_info("Crash key driver unloaded\n");
}

early_initcall(crash_key_init);
module_exit(crash_key_exit);

MODULE_AUTHOR("Flopster101");
MODULE_DESCRIPTION("Crash key driver - Power+VolDown panic trigger");
MODULE_LICENSE("GPL v2");
