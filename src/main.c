#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "protocol.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

CAN_MSGQ_DEFINE(rx_msgq, 16);

static uint16_t hb_counter;
static bool led_on;

static void set_led(bool on)
{
	led_on = on;
	gpio_pin_set_dt(&led, on);
}

static int send_frame(uint32_t id, const void *data, uint8_t len)
{
	struct can_frame f = { .id = id, .dlc = len };

	memcpy(f.data, data, len);
	int err = can_send(can_dev, &f, K_MSEC(100), NULL, NULL);

	if (err) {
		LOG_WRN("TX id=0x%03x failed (%d)", id, err);
	}
	return err;
}

static void state_change_cb(const struct device *dev, enum can_state state,
			    struct can_bus_err_cnt err_cnt, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
	LOG_WRN("CAN state=%d tx_err=%u rx_err=%u", state,
		err_cnt.tx_err_cnt, err_cnt.rx_err_cnt);
}

static void handle_cmd(const struct can_frame *f)
{
	if (f->dlc < sizeof(struct cmd_msg)) {
		LOG_WRN("short command frame (dlc=%u)", f->dlc);
		return;
	}

	const struct cmd_msg *c = (const struct cmd_msg *)f->data;
	uint8_t result = 0; /* 0 = OK */

	switch (c->op) {
	case CMD_LED_SET:
		set_led(c->arg0);
		break;
	case CMD_LED_TOGGLE:
		set_led(!led_on);
		break;
	case CMD_PING:
		break;
	default:
		result = 0xFF;
		LOG_WRN("unknown op 0x%02x", c->op);
	}

	uint8_t ack[2] = { c->op, result };

	send_frame(CAN_ID_CMD_ACK, ack, sizeof(ack));
}

static void rx_thread_fn(void *a, void *b, void *c)
{
	const struct can_filter filter = {
		.id = CAN_ID_CMD,
		.mask = CAN_STD_ID_MASK,   /* exact match */
		.flags = 0,
	};
	struct can_frame f;

	int fid = can_add_rx_filter_msgq(can_dev, &rx_msgq, &filter);

	if (fid < 0) {
		LOG_ERR("add filter failed (%d)", fid);
		return;
	}

	while (1) {
		k_msgq_get(&rx_msgq, &f, K_FOREVER);
		LOG_INF("RX id=0x%03x dlc=%u", f.id, f.dlc);
		LOG_HEXDUMP_INF(f.data, f.dlc, "data");
		handle_cmd(&f);
	}
}

K_THREAD_DEFINE(rx_thread, 1024, rx_thread_fn, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
	int err;

	if (!device_is_ready(can_dev) || !gpio_is_ready_dt(&led)) {
		LOG_ERR("CAN or LED device not ready");
		return 0;
	}
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	err = can_set_bitrate(can_dev, CONFIG_APP_CAN_BITRATE);
	if (err) {
		LOG_ERR("set bitrate failed (%d)", err);
		return 0;
	}

	can_mode_t mode = IS_ENABLED(CONFIG_APP_CAN_LOOPBACK) ? CAN_MODE_LOOPBACK
							       : CAN_MODE_NORMAL;
	err = can_set_mode(can_dev, mode);
	if (err) {
		LOG_ERR("set mode failed (%d)", err);
		return 0;
	}

	can_set_state_change_callback(can_dev, state_change_cb, NULL);

	err = can_start(can_dev);
	if (err) {
		LOG_ERR("can_start failed (%d)", err);
		return 0;
	}
	LOG_INF("CAN started: %u bit/s, %s", CONFIG_APP_CAN_BITRATE,
		IS_ENABLED(CONFIG_APP_CAN_LOOPBACK) ? "LOOPBACK" : "NORMAL");

	while (1) {
		enum can_state st;
		struct can_bus_err_cnt cnt;

		can_get_state(can_dev, &st, &cnt);

		struct heartbeat_msg hb = {
			.uptime_ms = (uint32_t)k_uptime_get(),
			.counter = hb_counter++,
			.can_state = st,
			.led_state = led_on,
		};

		send_frame(CAN_ID_HEARTBEAT, &hb, sizeof(hb));

		if (IS_ENABLED(CONFIG_APP_CAN_LOOPBACK) && (hb_counter % 10) == 0) {
			/* Self-test: inject a command frame to ourselves */
			struct cmd_msg c = { .op = CMD_LED_TOGGLE };

			send_frame(CAN_ID_CMD, &c, sizeof(c));
		}

		k_msleep(CONFIG_APP_HEARTBEAT_MS);
	}
}
