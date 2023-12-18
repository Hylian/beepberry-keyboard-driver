// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * input_iface.c: Key handler implementation
 */

#include <linux/input.h>
#include <linux/module.h>

#include "config.h"
#include "debug_levels.h"

#include "input_iface.h"

int input_touch_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	ctx->touch.activation = TOUCH_ACT_META;
	ctx->touch.input_as = TOUCH_INPUT_AS_KEYS;
	ctx->touch.enabled = 0;
	ctx->touch.rel_x = 0;
	ctx->touch.rel_y = 0;

	return 0;
}

void input_touch_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{}

void input_touch_report_event(struct kbd_ctx *ctx)
{
	const int threshold = 10000;
	static u64 prev_avg_time_ns = 0;
	static u64 prev_key_time_ns = 0;
	static int factor_x = 0;
	static int factor_y = 0;

	if (!ctx || !ctx->touch.enabled) {
		return;
	}

	dev_info_ld(&ctx->i2c_client->dev,
		"%s X Reg: %d Y Reg: %d.\n",
		__func__, ctx->touch.rel_x, ctx->touch.rel_y);

	// Report mouse movement
	if (ctx->touch.input_as == TOUCH_INPUT_AS_MOUSE) {
#if 0
		input_report_mouse(ctx->input_dev, ctx->touch.rel_x, ctx->touch.rel_y);
#endif
	// Report arrow key movement
	} else if (ctx->touch.input_as == TOUCH_INPUT_AS_KEYS) {
		u64 cur_time_ns = ktime_get_ns();
		if (cur_time_ns < prev_avg_time_ns) {
			prev_avg_time_ns = cur_time_ns;
			return;
		}
		if (cur_time_ns < prev_key_time_ns) {
			prev_key_time_ns = cur_time_ns;
			return;
		}

		// Clamp factor to multiple of threshold
		const int clamp = 1*threshold;
		if (factor_x > clamp) {
			factor_x = clamp;
		} else if (factor_x < -1*clamp) {
			factor_x = -1*clamp;
		}

		if (factor_y > clamp) {
			factor_y = clamp;
		} else if (factor_y < -1*clamp) {
			factor_y = -1*clamp;
		}

		// Every 100ms, halve the running average factor.
		int decimate = (cur_time_ns - prev_avg_time_ns)/10000;
		if (decimate) {
			for (int i = 0; i < decimate; i++) {
				factor_x /= 2;
				factor_y /= 2;
			}
			prev_avg_time_ns = cur_time_ns;
		}

		// Add in the current inputs to the running average.
		factor_x += ctx->touch.rel_x*3200;
		factor_y += ctx->touch.rel_y*1700;

		// Rate limit emitted key events to every 25ms.
		u64 time_since_last_key_ms = (cur_time_ns - prev_key_time_ns)/1000000;
		if (time_since_last_key_ms < 25) {
			return;
		}
		prev_key_time_ns = cur_time_ns;

		// Multiply running average factor and input, with attenuation factor
		// from the other axis.
		int virt_x = ((abs(factor_x)/(abs(factor_y)+1)) * ctx->touch.rel_x);
		int virt_y = ((abs(factor_y)/(abs(factor_x)+1)) * ctx->touch.rel_y);

		if (abs(virt_x) > threshold) {
			if (virt_x > 0) {
				input_report_key(ctx->input_dev, KEY_RIGHT, TRUE);
				input_report_key(ctx->input_dev, KEY_RIGHT, FALSE);
			} else {
				input_report_key(ctx->input_dev, KEY_LEFT, TRUE);
				input_report_key(ctx->input_dev, KEY_LEFT, FALSE);
			}
		}

		if (abs(virt_y) > threshold) {
			if (virt_y > 0) {
				input_report_key(ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(ctx->input_dev, KEY_DOWN, FALSE);
			} else {
				input_report_key(ctx->input_dev, KEY_UP, TRUE);
				input_report_key(ctx->input_dev, KEY_UP, FALSE);
			}
		}
	}
}

// Touch enabled: touchpad click sends enter / mouse click
// Touch disabled: touchpad click enters meta mode
int input_touch_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state)
{
	// Touchpad click
	// Touch off: enable meta mode
	// Touch on: enter or mouse click
	if (keycode == KEY_COMPOSE) {

		if (ctx->touch.enabled) {

			// Keys mode, send enter
			if ((ctx->touch.input_as == TOUCH_INPUT_AS_KEYS)
			 && (state == KEY_STATE_RELEASED)) {
				//input_report_key(ctx->input_dev, KEY_ENTER, TRUE);
				//input_report_key(ctx->input_dev, KEY_ENTER, FALSE);

			// Mouse mode, send mouse click
			} else if (ctx->touch.input_as == TOUCH_INPUT_AS_MOUSE) {
				// TODO: mouse click
			}

			return 1;
		}

		// If touch off, touchpad click will be handled by meta handler

	// Berry key
	// Touch off: sends Tmux prefix (Control + code 171 in keymap)
	// Touch on: enable meta mode
	} else if (keycode == KEY_PROPS) {

		if (state == KEY_STATE_RELEASED) {

			// Send Tmux prefix
			if (!ctx->touch.enabled) {
				input_report_key(ctx->input_dev, KEY_LEFTCTRL, TRUE);
				input_report_key(ctx->input_dev, 171, TRUE);
				input_report_key(ctx->input_dev, 171, FALSE);
				input_report_key(ctx->input_dev, KEY_LEFTCTRL, FALSE);

			// Enable meta mode
			} else {
				input_meta_enable(ctx);
			}
		}

		return 1;
	}

	return 0;
}

void input_touch_enable(struct kbd_ctx *ctx)
{
	ctx->touch.enabled = 1;
	input_fw_enable_touch_interrupts(ctx);
}

void input_touch_disable(struct kbd_ctx *ctx)
{
	ctx->touch.enabled = 0;
	input_fw_disable_touch_interrupts(ctx);
}

void input_touch_set_activation(struct kbd_ctx *ctx, uint8_t activation)
{
	if (activation == TOUCH_ACT_ALWAYS) {
		ctx->touch.activation = TOUCH_ACT_ALWAYS;
		input_touch_enable(ctx);

	} else if (activation == TOUCH_ACT_META) {
		ctx->touch.activation = TOUCH_ACT_META;
		input_touch_disable(ctx);
	}
}

void input_touch_set_input_as(struct kbd_ctx *ctx, uint8_t input_as)
{
	ctx->touch.input_as = input_as;
}
