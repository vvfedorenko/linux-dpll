/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_DPLL_H
#define _UAPI_LINUX_DPLL_H

#define DPLL_NAME_LEN		32
#define DPLL_DESC_LEN		20
#define PIN_DESC_LEN		20

/* Adding event notification support elements */
#define DPLL_FAMILY_NAME		"dpll"
#define DPLL_VERSION			0x01
#define DPLL_MONITOR_GROUP_NAME		"monitor"

#define DPLL_DUMP_FILTER_PINS	1
#define DPLL_DUMP_FILTER_STATUS	2

/* dplla - Attributes of dpll generic netlink family
 *
 * @DPLLA_UNSPEC - invalid attribute
 * @DPLLA_ID - ID of a dpll device (unsigned int)
 * @DPLLA_NAME - human-readable name (char array of DPLL_NAME_LENGTH size)
 * @DPLLA_MODE - working mode of dpll (enum dpll_mode)
 * @DPLLA_MODE_SUPPORTED - list of supported working modes (enum dpll_mode)
 * @DPLLA_SOURCE_PIN_ID - ID of source pin selected to drive dpll
 *	(unsigned int)
 * @DPLLA_LOCK_STATUS - dpll's lock status (enum dpll_lock_status)
 * @DPLLA_TEMP - dpll's temperature (signed int - Celsius degrees)
 * @DPLLA_CLOCK_ID - Unique Clock Identifier of dpll (u64)
 * @DPLLA_CLOCK_CLASS - clock quality class of dpll (enum dpll_clock_class)
 * @DPLLA_DUMP_FILTER - filter bitmask (int, sum of DPLL_DUMP_FILTER_* defines)
 * @DPLLA_PIN - nested attribute, each contains single pin attributes
 * @DPLLA_PIN_IDX - index of a pin on dpll (unsigned int)
 * @DPLLA_PIN_DESCRIPTION - human-readable pin description provided by driver
 *	(char array of PIN_DESC_LEN size)
 * @DPLLA_PIN_TYPE - current type of a pin (enum dpll_pin_type)
 * @DPLLA_PIN_SIGNAL_TYPE - current type of a signal
 *	(enum dpll_pin_signal_type)
 * @DPLLA_PIN_SIGNAL_TYPE_SUPPORTED - pin signal types supported
 *	(enum dpll_pin_signal_type)
 * @DPLLA_PIN_CUSTOM_FREQ - freq value for DPLL_PIN_SIGNAL_TYPE_CUSTOM_FREQ
 *	(unsigned int)
 * @DPLLA_PIN_STATE - state of pin's capabilities (enum dpll_pin_state)
 * @DPLLA_PIN_STATE_SUPPORTED - available pin's capabilities
 *	(enum dpll_pin_state)
 * @DPLLA_PIN_PRIO - priority of a pin on dpll (unsigned int)
 * @DPLLA_PIN_PARENT_IDX - if of a parent pin (unsigned int)
 * @DPLLA_CHANGE_TYPE - type of device change event
 *	(enum dpll_change_type)
 * @DPLLA_PIN_NETIFINDEX - related network interface index for the pin
 **/
enum dplla {
	DPLLA_UNSPEC,
	DPLLA_ID,
	DPLLA_NAME,
	DPLLA_MODE,
	DPLLA_MODE_SUPPORTED,
	DPLLA_SOURCE_PIN_IDX,
	DPLLA_LOCK_STATUS,
	DPLLA_TEMP,
	DPLLA_CLOCK_ID,
	DPLLA_CLOCK_CLASS,
	DPLLA_DUMP_FILTER,
	DPLLA_PIN,
	DPLLA_PIN_IDX,
	DPLLA_PIN_DESCRIPTION,
	DPLLA_PIN_TYPE,
	DPLLA_PIN_SIGNAL_TYPE,
	DPLLA_PIN_SIGNAL_TYPE_SUPPORTED,
	DPLLA_PIN_CUSTOM_FREQ,
	DPLLA_PIN_STATE,
	DPLLA_PIN_STATE_SUPPORTED,
	DPLLA_PIN_PRIO,
	DPLLA_PIN_PARENT_IDX,
	DPLLA_CHANGE_TYPE,
	DPLLA_PIN_NETIFINDEX,
	__DPLLA_MAX,
};

#define DPLLA_MAX (__DPLLA_MAX - 1)

/* dpll_lock_status - DPLL status provides information of device status
 *
 * @DPLL_LOCK_STATUS_UNSPEC - unspecified value
 * @DPLL_LOCK_STATUS_UNLOCKED - dpll was not yet locked to any valid (or is in
 *	DPLL_MODE_FREERUN/DPLL_MODE_NCO modes)
 * @DPLL_LOCK_STATUS_CALIBRATING - dpll is trying to lock to a valid signal
 * @DPLL_LOCK_STATUS_LOCKED - dpll is locked
 * @DPLL_LOCK_STATUS_HOLDOVER - dpll is in holdover state - lost a valid lock
 *	or was forced by DPLL_MODE_HOLDOVER mode)
 **/
enum dpll_lock_status {
	DPLL_LOCK_STATUS_UNSPEC,
	DPLL_LOCK_STATUS_UNLOCKED,
	DPLL_LOCK_STATUS_CALIBRATING,
	DPLL_LOCK_STATUS_LOCKED,
	DPLL_LOCK_STATUS_HOLDOVER,

	__DPLL_LOCK_STATUS_MAX,
};

#define DPLL_LOCK_STATUS_MAX (__DPLL_LOCK_STATUS_MAX - 1)

/* dpll_pin_type - signal types
 *
 * @DPLL_PIN_TYPE_UNSPEC - unspecified value
 * @DPLL_PIN_TYPE_MUX - mux type pin, aggregates selectable pins
 * @DPLL_PIN_TYPE_EXT - external source
 * @DPLL_PIN_TYPE_SYNCE_ETH_PORT - ethernet port PHY's recovered clock
 * @DPLL_PIN_TYPE_INT_OSCILLATOR - device internal oscillator
 * @DPLL_PIN_TYPE_GNSS - GNSS recovered clock
 **/
enum dpll_pin_type {
	DPLL_PIN_TYPE_UNSPEC,
	DPLL_PIN_TYPE_MUX,
	DPLL_PIN_TYPE_EXT,
	DPLL_PIN_TYPE_SYNCE_ETH_PORT,
	DPLL_PIN_TYPE_INT_OSCILLATOR,
	DPLL_PIN_TYPE_GNSS,

	__DPLL_PIN_TYPE_MAX,
};

#define DPLL_PIN_TYPE_MAX (__DPLL_PIN_TYPE_MAX - 1)

/* dpll_pin_signal_type - signal types
 *
 * @DPLL_PIN_SIGNAL_TYPE_UNSPEC - unspecified value
 * @DPLL_PIN_SIGNAL_TYPE_1_PPS - a 1Hz signal
 * @DPLL_PIN_SIGNAL_TYPE_10_MHZ - a 10 MHz signal
 * @DPLL_PIN_SIGNAL_TYPE_CUSTOM_FREQ - custom frequency signal, value defined
 *	with pin's DPLLA_PIN_SIGNAL_TYPE_CUSTOM_FREQ attribute
 **/
enum dpll_pin_signal_type {
	DPLL_PIN_SIGNAL_TYPE_UNSPEC,
	DPLL_PIN_SIGNAL_TYPE_1_PPS,
	DPLL_PIN_SIGNAL_TYPE_10_MHZ,
	DPLL_PIN_SIGNAL_TYPE_CUSTOM_FREQ,

	__DPLL_PIN_SIGNAL_TYPE_MAX,
};

#define DPLL_PIN_SIGNAL_TYPE_MAX (__DPLL_PIN_SIGNAL_TYPE_MAX - 1)

/* dpll_pin_state - available pin states
 *
 * @DPLL_PIN_STATE_UNSPEC - unspecified value
 * @DPLL_PIN_STATE_CONNECTED - pin connected
 * @DPLL_PIN_STATE_DISCONNECTED - pin disconnected
 * @DPLL_PIN_STATE_SOURCE - pin used as an input pin
 * @DPLL_PIN_STATE_OUTPUT - pin used as an output pin
 **/
enum dpll_pin_state {
	DPLL_PIN_STATE_UNSPEC,
	DPLL_PIN_STATE_CONNECTED,
	DPLL_PIN_STATE_DISCONNECTED,
	DPLL_PIN_STATE_SOURCE,
	DPLL_PIN_STATE_OUTPUT,

	__DPLL_PIN_STATE_MAX,
};

#define DPLL_PIN_STATE_MAX (__DPLL_PIN_STATE_MAX - 1)

/**
 * dpll_event - Events of dpll generic netlink family
 *
 * @DPLL_EVENT_UNSPEC - invalid event type
 * @DPLL_EVENT_DEVICE_CREATE - dpll device created
 * @DPLL_EVENT_DEVICE_DELETE - dpll device deleted
 * @DPLL_EVENT_DEVICE_CHANGE - attribute of dpll device or pin changed
 **/
enum dpll_event {
	DPLL_EVENT_UNSPEC,
	DPLL_EVENT_DEVICE_CREATE,
	DPLL_EVENT_DEVICE_DELETE,
	DPLL_EVENT_DEVICE_CHANGE,

	__DPLL_EVENT_MAX,
};

#define DPLL_EVENT_MAX (__DPLL_EVENT_MAX - 1)

/**
 * dpll_change_type - values of events in case of device change event
 * (DPLL_EVENT_DEVICE_CHANGE)
 *
 * @DPLL_CHANGE_UNSPEC - invalid event type
 * @DPLL_CHANGE_MODE - mode changed
 * @DPLL_CHANGE_LOCK_STATUS - lock status changed
 * @DPLL_CHANGE_SOURCE_PIN - source pin changed,
 * @DPLL_CHANGE_TEMP - temperature changed
 * @DPLL_CHANGE_PIN_ADD - source pin added,
 * @DPLL_CHANGE_PIN_DEL - source pin deleted,
 * @DPLL_CHANGE_PIN_TYPE - pin type cahnged,
 * @DPLL_CHANGE_PIN_SIGNAL_TYPE pin signal type changed
 * @DPLL_CHANGE_PIN_CUSTOM_FREQ custom frequency changed
 * @DPLL_CHANGE_PIN_STATE - pin state changed
 * @DPLL_CHANGE_PIN_PRIO - pin prio changed
 **/
enum dpll_event_change {
	DPLL_CHANGE_UNSPEC,
	DPLL_CHANGE_MODE,
	DPLL_CHANGE_LOCK_STATUS,
	DPLL_CHANGE_SOURCE_PIN,
	DPLL_CHANGE_TEMP,
	DPLL_CHANGE_PIN_ADD,
	DPLL_CHANGE_PIN_DEL,
	DPLL_CHANGE_PIN_TYPE,
	DPLL_CHANGE_PIN_SIGNAL_TYPE,
	DPLL_CHANGE_PIN_CUSTOM_FREQ,
	DPLL_CHANGE_PIN_STATE,
	DPLL_CHANGE_PIN_PRIO,

	__DPLL_CHANGE_MAX,
};

#define DPLL_CHANGE_MAX (__DPLL_CHANGE_MAX - 1)

/**
 * dpll_cmd - Commands supported by the dpll generic netlink family
 *
 * @DPLL_CMD_UNSPEC - invalid message type
 * @DPLL_CMD_DEVICE_GET - Get list of dpll devices (dump) or attributes of
 *	single dpll device and it's pins
 * @DPLL_CMD_DEVICE_SET - Set attributes for a dpll
 * @DPLL_CMD_PIN_SET - Set attributes for a pin
 **/
enum dpll_cmd {
	DPLL_CMD_UNSPEC,
	DPLL_CMD_DEVICE_GET,
	DPLL_CMD_DEVICE_SET,
	DPLL_CMD_PIN_SET,

	__DPLL_CMD_MAX,
};

#define DPLL_CMD_MAX (__DPLL_CMD_MAX - 1)

/**
 * dpll_mode - Working-modes a dpll can support. Modes differentiate how
 * dpll selects one of its sources to syntonize with a source.
 *
 * @DPLL_MODE_UNSPEC - invalid
 * @DPLL_MODE_FORCED - source can be only selected by sending a request to dpll
 * @DPLL_MODE_AUTOMATIC - highest prio, valid source, auto selected by dpll
 * @DPLL_MODE_HOLDOVER - dpll forced into holdover mode
 * @DPLL_MODE_FREERUN - dpll driven on system clk, no holdover available
 * @DPLL_MODE_NCO - dpll driven by Numerically Controlled Oscillator
 **/
enum dpll_mode {
	DPLL_MODE_UNSPEC,
	DPLL_MODE_FORCED,
	DPLL_MODE_AUTOMATIC,
	DPLL_MODE_HOLDOVER,
	DPLL_MODE_FREERUN,
	DPLL_MODE_NCO,

	__DPLL_MODE_MAX,
};

#define DPLL_MODE_MAX (__DPLL_MODE_MAX - 1)

/**
 * dpll_clock_class - enumerate quality class of a DPLL clock as specified in
 * Recommendation ITU-T G.8273.2/Y.1368.2.
 */
enum dpll_clock_class {
	DPLL_CLOCK_CLASS_UNSPEC,
	DPLL_CLOCK_CLASS_A,
	DPLL_CLOCK_CLASS_B,
	DPLL_CLOCK_CLASS_C,

	__DPLL_CLOCK_CLASS_MAX,
};

#define DPLL_CLOCK_CLASS_MAX (__DPLL_CLOCK_CLASS_MAX - 1)

#endif /* _UAPI_LINUX_DPLL_H */
