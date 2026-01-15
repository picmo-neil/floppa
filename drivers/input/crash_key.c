// SPDX-License-Identifier: GPL-2.0
/*
 * Crash Key Driver - Trigger kernel panic via key combo for debug
 *
 * Hold Power + Volume Down for 5 seconds to trigger a warm reboot
 * via kernel panic. This preserves RAM contents for last_kmsg.
 *
 * Copyright (C) 2026 Flopster101
 * Based on Samsung sec_crash_key.c
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define CRASH_KEY_HOLD_MS	5000	/* 5 seconds */

static bool power_pressed;
static bool voldown_pressed;
static struct timer_list crash_timer;
static bool timer_active;

static void crash_key_timeout(struct timer_list *t)
{
	/* Both keys still held after timeout - trigger panic */
	if (power_pressed && voldown_pressed) {
		pr_emerg("Crash key combo held for %d seconds - triggering panic!\n",
			 CRASH_KEY_HOLD_MS / 1000);
		panic("Crash Key");
	}
	timer_active = false;
}

static void crash_key_check(void)
{
	if (power_pressed && voldown_pressed) {
		if (!timer_active) {
			pr_info("Crash key combo detected, hold for %d seconds to trigger panic\n",
				CRASH_KEY_HOLD_MS / 1000);
			mod_timer(&crash_timer, jiffies + msecs_to_jiffies(CRASH_KEY_HOLD_MS));
			timer_active = true;
		}
	} else {
		if (timer_active) {
			del_timer(&crash_timer);
			timer_active = false;
		}
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
		break;
	case KEY_VOLUMEDOWN:
		voldown_pressed = !!value;
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

	timer_setup(&crash_timer, crash_key_timeout, 0);

	ret = input_register_handler(&crash_key_handler);
	if (ret) {
		pr_err("Failed to register input handler: %d\n", ret);
		return ret;
	}

	pr_info("Crash key driver loaded (Power+VolDown for %d seconds)\n",
		CRASH_KEY_HOLD_MS / 1000);
	return 0;
}

static void __exit crash_key_exit(void)
{
	del_timer_sync(&crash_timer);
	input_unregister_handler(&crash_key_handler);
	pr_info("Crash key driver unloaded\n");
}

module_init(crash_key_init);
module_exit(crash_key_exit);

MODULE_AUTHOR("Flopster101");
MODULE_DESCRIPTION("Crash key driver - Power+VolDown panic trigger");
MODULE_LICENSE("GPL v2");
