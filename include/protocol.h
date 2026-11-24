#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>

/* 11-bit standard IDs, lower value = higher priority */
#define CAN_ID_HEARTBEAT  0x100  /* node -> bus, periodic        */
#define CAN_ID_CMD        0x200  /* bus -> node, command         */
#define CAN_ID_CMD_ACK    0x201  /* node -> bus, command reply   */

/* Heartbeat payload (8 bytes, little endian) */
struct __packed heartbeat_msg {
	uint32_t uptime_ms;
	uint16_t counter;
	uint8_t  can_state;   /* enum can_state */
	uint8_t  led_state;
};

/* Command payload */
enum cmd_op {
	CMD_LED_SET    = 0x01,  /* arg0: 0/1 */
	CMD_LED_TOGGLE = 0x02,
	CMD_PING       = 0x03,
};

struct __packed cmd_msg {
	uint8_t op;
	uint8_t arg0;
};

#endif
