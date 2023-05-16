/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/* Do not edit directly, auto-generated from: */
/*	Documentation/netlink/specs/dpll.yaml */
/* YNL-GEN uapi header */

#ifndef _UAPI_LINUX_DPLL_H
#define _UAPI_LINUX_DPLL_H

#define DPLL_FAMILY_NAME	"dpll"
#define DPLL_FAMILY_VERSION	1

/**
 * enum dpll_mode - working-modes a dpll can support, differentiate if and how
 *   dpll selects one of its sources to syntonize with it, valid values for
 *   DPLL_A_MODE attribute
 * @DPLL_MODE_MANUAL: source can be only selected by sending a request to dpll
 * @DPLL_MODE_AUTOMATIC: highest prio, valid source, auto selected by dpll
 * @DPLL_MODE_HOLDOVER: dpll forced into holdover mode
 * @DPLL_MODE_FREERUN: dpll driven on system clk
 */
enum dpll_mode {
	DPLL_MODE_MANUAL = 1,
	DPLL_MODE_AUTOMATIC,
	DPLL_MODE_HOLDOVER,
	DPLL_MODE_FREERUN,

	__DPLL_MODE_MAX,
	DPLL_MODE_MAX = (__DPLL_MODE_MAX - 1)
};

/**
 * enum dpll_lock_status - provides information of dpll device lock status,
 *   valid values for DPLL_A_LOCK_STATUS attribute
 * @DPLL_LOCK_STATUS_UNLOCKED: dpll was not yet locked to any valid source (or
 *   is in mode: DPLL_MODE_FREERUN)
 * @DPLL_LOCK_STATUS_LOCKED: dpll is locked to a valid signal, but no holdover
 *   available
 * @DPLL_LOCK_STATUS_LOCKED_HO_ACQ: dpll is locked and holdover acquired
 * @DPLL_LOCK_STATUS_HOLDOVER: dpll is in holdover state - lost a valid lock or
 *   was forced by selecting DPLL_MODE_HOLDOVER mode (latter possible only when
 *   dpll lock-state was already DPLL_LOCK_STATUS_LOCKED, if dpll lock-state
 *   was not DPLL_LOCK_STATUS_LOCKED, the dpll's lock-state shall remain
 *   DPLL_LOCK_STATUS_UNLOCKED even if DPLL_MODE_HOLDOVER was requested)
 */
enum dpll_lock_status {
	DPLL_LOCK_STATUS_UNLOCKED = 1,
	DPLL_LOCK_STATUS_LOCKED,
	DPLL_LOCK_STATUS_LOCKED_HO_ACQ,
	DPLL_LOCK_STATUS_HOLDOVER,

	__DPLL_LOCK_STATUS_MAX,
	DPLL_LOCK_STATUS_MAX = (__DPLL_LOCK_STATUS_MAX - 1)
};

#define DPLL_TEMP_DIVIDER	1000

/**
 * enum dpll_type - type of dpll, valid values for DPLL_A_TYPE attribute
 * @DPLL_TYPE_PPS: dpll produces Pulse-Per-Second signal
 * @DPLL_TYPE_EEC: dpll drives the Ethernet Equipment Clock
 */
enum dpll_type {
	DPLL_TYPE_PPS = 1,
	DPLL_TYPE_EEC,

	__DPLL_TYPE_MAX,
	DPLL_TYPE_MAX = (__DPLL_TYPE_MAX - 1)
};

/**
 * enum dpll_pin_type - defines possible types of a pin, valid values for
 *   DPLL_A_PIN_TYPE attribute
 * @DPLL_PIN_TYPE_MUX: aggregates another layer of selectable pins
 * @DPLL_PIN_TYPE_EXT: external source
 * @DPLL_PIN_TYPE_SYNCE_ETH_PORT: ethernet port PHY's recovered clock
 * @DPLL_PIN_TYPE_INT_OSCILLATOR: device internal oscillator
 * @DPLL_PIN_TYPE_GNSS: GNSS recovered clock
 */
enum dpll_pin_type {
	DPLL_PIN_TYPE_MUX = 1,
	DPLL_PIN_TYPE_EXT,
	DPLL_PIN_TYPE_SYNCE_ETH_PORT,
	DPLL_PIN_TYPE_INT_OSCILLATOR,
	DPLL_PIN_TYPE_GNSS,

	__DPLL_PIN_TYPE_MAX,
	DPLL_PIN_TYPE_MAX = (__DPLL_PIN_TYPE_MAX - 1)
};

/**
 * enum dpll_pin_direction - defines possible direction of a pin, valid values
 *   for DPLL_A_PIN_DIRECTION attribute
 * @DPLL_PIN_DIRECTION_SOURCE: pin used as a source of a signal
 * @DPLL_PIN_DIRECTION_OUTPUT: pin used to output the signal
 */
enum dpll_pin_direction {
	DPLL_PIN_DIRECTION_SOURCE = 1,
	DPLL_PIN_DIRECTION_OUTPUT,

	__DPLL_PIN_DIRECTION_MAX,
	DPLL_PIN_DIRECTION_MAX = (__DPLL_PIN_DIRECTION_MAX - 1)
};

#define DPLL_PIN_FREQUENCY_1_HZ		1
#define DPLL_PIN_FREQUENCY_10_KHZ	10000
#define DPLL_PIN_FREQUENCY_77_5_KHZ	77500
#define DPLL_PIN_FREQUENCY_10_MHZ	10000000

/**
 * enum dpll_pin_state - defines possible states of a pin, valid values for
 *   DPLL_A_PIN_STATE attribute
 * @DPLL_PIN_STATE_CONNECTED: pin connected, active source of phase locked loop
 * @DPLL_PIN_STATE_DISCONNECTED: pin disconnected, not considered as a valid
 *   source
 * @DPLL_PIN_STATE_SELECTABLE: pin enabled for automatic source selection
 */
enum dpll_pin_state {
	DPLL_PIN_STATE_CONNECTED = 1,
	DPLL_PIN_STATE_DISCONNECTED,
	DPLL_PIN_STATE_SELECTABLE,

	__DPLL_PIN_STATE_MAX,
	DPLL_PIN_STATE_MAX = (__DPLL_PIN_STATE_MAX - 1)
};

/**
 * enum dpll_pin_caps - defines possible capabilities of a pin, valid flags on
 *   DPLL_A_PIN_CAPS attribute
 */
enum dpll_pin_caps {
	DPLL_PIN_CAPS_DIRECTION_CAN_CHANGE = 1,
	DPLL_PIN_CAPS_PRIORITY_CAN_CHANGE = 2,
	DPLL_PIN_CAPS_STATE_CAN_CHANGE = 4,
};

/**
 * enum dpll_event - events of dpll generic netlink family
 * @DPLL_EVENT_DEVICE_CREATE: dpll device created
 * @DPLL_EVENT_DEVICE_DELETE: dpll device deleted
 * @DPLL_EVENT_DEVICE_CHANGE: attribute of dpll device or pin changed, reason
 *   is to be found with an attribute type (DPLL_A_*) received with the event
 */
enum dpll_event {
	DPLL_EVENT_DEVICE_CREATE = 1,
	DPLL_EVENT_DEVICE_DELETE,
	DPLL_EVENT_DEVICE_CHANGE,
};

enum dplla {
	DPLL_A_ID = 1,
	DPLL_A_DEV_NAME,
	DPLL_A_BUS_NAME,
	DPLL_A_MODE,
	DPLL_A_MODE_SUPPORTED,
	DPLL_A_LOCK_STATUS,
	DPLL_A_TEMP,
	DPLL_A_CLOCK_ID,
	DPLL_A_TYPE,
	DPLL_A_PIN_IDX,
	DPLL_A_PIN_LABEL,
	DPLL_A_PIN_TYPE,
	DPLL_A_PIN_DIRECTION,
	DPLL_A_PIN_FREQUENCY,
	DPLL_A_PIN_FREQUENCY_SUPPORTED,
	DPLL_A_PIN_FREQUENCY_MIN,
	DPLL_A_PIN_FREQUENCY_MAX,
	DPLL_A_PIN_PRIO,
	DPLL_A_PIN_STATE,
	DPLL_A_PIN_PARENT,
	DPLL_A_PIN_PARENT_IDX,
	DPLL_A_PIN_RCLK_DEVICE,
	DPLL_A_PIN_DPLL_CAPS,
	DPLL_A_DEVICE,

	__DPLL_A_MAX,
	DPLL_A_MAX = (__DPLL_A_MAX - 1)
};

enum {
	DPLL_CMD_DEVICE_GET = 1,
	DPLL_CMD_DEVICE_SET,
	DPLL_CMD_PIN_GET,
	DPLL_CMD_PIN_SET,

	__DPLL_CMD_MAX,
	DPLL_CMD_MAX = (__DPLL_CMD_MAX - 1)
};

#define DPLL_MCGRP_MONITOR	"monitor"

#endif /* _UAPI_LINUX_DPLL_H */
