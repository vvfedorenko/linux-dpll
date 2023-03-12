/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (c) 2023 Meta Platforms, Inc. and affiliates
 *  Copyright (c) 2023 Intel and affiliates
 */

#ifndef __DPLL_H__
#define __DPLL_H__

#include <uapi/linux/dpll.h>
#include <linux/device.h>
#include <linux/netlink.h>

struct dpll_device;
struct dpll_pin;

struct dpll_device_ops {
	int (*mode_get)(const struct dpll_device *dpll, void *dpll_priv,
			enum dpll_mode *mode, struct netlink_ext_ack *extack);
	int (*mode_set)(const struct dpll_device *dpll, void *dpll_priv,
			const enum dpll_mode mode,
			struct netlink_ext_ack *extack);
	bool (*mode_supported)(const struct dpll_device *dpll, void *dpll_priv,
			       const enum dpll_mode mode,
			       struct netlink_ext_ack *extack);
	int (*source_pin_idx_get)(const struct dpll_device *dpll,
				  void *dpll_priv,
				  u32 *pin_idx,
				  struct netlink_ext_ack *extack);
	int (*lock_status_get)(const struct dpll_device *dpll, void *dpll_priv,
			       enum dpll_lock_status *status,
			       struct netlink_ext_ack *extack);
	int (*temp_get)(const struct dpll_device *dpll, void *dpll_priv,
			s32 *temp, struct netlink_ext_ack *extack);
};

struct dpll_pin_ops {
	int (*frequency_set)(const struct dpll_pin *pin, void *pin_priv,
			     const struct dpll_device *dpll, void *dpll_priv,
			     const u64 frequency,
			     struct netlink_ext_ack *extack);
	int (*frequency_get)(const struct dpll_pin *pin, void *pin_priv,
			     const struct dpll_device *dpll, void *dpll_priv,
			     u64 *frequency, struct netlink_ext_ack *extack);
	int (*direction_set)(const struct dpll_pin *pin, void *pin_priv,
			     const struct dpll_device *dpll, void *dpll_priv,
			     const enum dpll_pin_direction direction,
			     struct netlink_ext_ack *extack);
	int (*direction_get)(const struct dpll_pin *pin, void *pin_priv,
			     const struct dpll_device *dpll, void *dpll_priv,
			     enum dpll_pin_direction *direction,
			     struct netlink_ext_ack *extack);
	int (*state_on_pin_get)(const struct dpll_pin *pin, void *pin_priv,
				const struct dpll_pin *parent_pin,
				enum dpll_pin_state *state,
				struct netlink_ext_ack *extack);
	int (*state_on_dpll_get)(const struct dpll_pin *pin, void *pin_priv,
				 const struct dpll_device *dpll,
				 void *dpll_priv, enum dpll_pin_state *state,
				 struct netlink_ext_ack *extack);
	int (*state_on_pin_set)(const struct dpll_pin *pin, void *pin_priv,
				const struct dpll_pin *parent_pin,
				const enum dpll_pin_state state,
				struct netlink_ext_ack *extack);
	int (*state_on_dpll_set)(const struct dpll_pin *pin, void *pin_priv,
				 const struct dpll_device *dpll,
				 void *dpll_priv,
				 const enum dpll_pin_state state,
				 struct netlink_ext_ack *extack);
	int (*prio_get)(const struct dpll_pin *pin,  void *pin_priv,
			const struct dpll_device *dpll,  void *dpll_priv,
			u32 *prio, struct netlink_ext_ack *extack);
	int (*prio_set)(const struct dpll_pin *pin, void *pin_priv,
			const struct dpll_device *dpll, void *dpll_priv,
			const u32 prio, struct netlink_ext_ack *extack);
};

struct dpll_pin_frequency {
	u64 min;
	u64 max;
};

#define DPLL_PIN_FREQUENCY_RANGE(_min, _max)	\
	{					\
		.min = _min,			\
		.max = _max,			\
	}

#define DPLL_PIN_FREQUENCY(_val) DPLL_PIN_FREQUENCY_RANGE(_val, _val)
#define DPLL_PIN_FREQUENCY_1PPS \
	DPLL_PIN_FREQUENCY(DPLL_PIN_FREQUENCY_1_HZ)
#define DPLL_PIN_FREQUENCY_10MHZ \
	DPLL_PIN_FREQUENCY(DPLL_PIN_FREQUENCY_10_MHZ)
#define DPLL_PIN_FREQUENCY_IRIG_B \
	DPLL_PIN_FREQUENCY(DPLL_PIN_FREQUENCY_10_KHZ)
#define DPLL_PIN_FREQUENCY_DCF77 \
	DPLL_PIN_FREQUENCY(DPLL_PIN_FREQUENCY_77_5_KHZ)

struct dpll_pin_properties {
	const char *label;
	enum dpll_pin_type type;
	unsigned long capabilities;
	u32 freq_supported_num;
	struct dpll_pin_frequency *freq_supported;
};

/**
 * dpll_device_get - find or create dpll_device object
 * @clock_id: a system unique number for a device
 * @dev_driver_id: index of dpll device on parent device
 * @module: register module
 *
 * Returns:
 * * pointer to initialized dpll - success
 * * NULL - memory allocation fail
 */
struct dpll_device
*dpll_device_get(u64 clock_id, u32 dev_driver_id, struct module *module);

/**
 * dpll_device_put - caller drops reference to the device, free resources
 * @dpll: dpll device pointer
 *
 * If all dpll_device_get callers drops their reference, the dpll device
 * resources are freed.
 */
void dpll_device_put(struct dpll_device *dpll);

/**
 * dpll_device_register - register device, make it visible in the subsystem.
 * @dpll: reference previously allocated with dpll_device_get
 * @type: type of dpll
 * @ops: callbacks
 * @priv: private data of registerer
 * @owner: device struct of the owner
 *
 */
int dpll_device_register(struct dpll_device *dpll, enum dpll_type type,
			 const struct dpll_device_ops *ops, void *priv,
			 struct device *owner);

/**
 * dpll_device_unregister - deregister registered dpll
 * @dpll: pointer to dpll
 * @ops: ops for a dpll device
 * @priv: pointer to private information of owner
 *
 * Unregister the dpll from the subsystem, make it unavailable for netlink
 * API users.
 */
void dpll_device_unregister(struct dpll_device *dpll,
			    const struct dpll_device_ops *ops, void *priv);

/**
 * dpll_pin_get - get reference or create new pin object
 * @clock_id: a system unique number of a device
 * @@dev_driver_id: index of dpll device on parent device
 * @module: register module
 * @pin_prop: constant properities of a pin
 *
 * find existing pin with given clock_id, @dev_driver_id and module, or create new
 * and returen its reference.
 *
 * Returns:
 * * pointer to initialized pin - success
 * * NULL - memory allocation fail
 */
struct dpll_pin
*dpll_pin_get(u64 clock_id, u32 dev_driver_id, struct module *module,
	      const struct dpll_pin_properties *prop);

/**
 * dpll_pin_register - register pin with a dpll device
 * @dpll: pointer to dpll object to register pin with
 * @pin: pointer to allocated pin object being registered with dpll
 * @ops: struct with pin ops callbacks
 * @priv: private data pointer passed when calling callback ops
 * @rclk_device: pointer to device struct if pin is used for recovery of a clock
 * from that device
 *
 * Register previously allocated pin object with a dpll device.
 *
 * Return:
 * * 0 - if pin was registered with a parent pin,
 * * -ENOMEM - failed to allocate memory,
 * * -EEXIST - pin already registered with this dpll,
 * * -EBUSY - couldn't allocate id for a pin.
 */
int dpll_pin_register(struct dpll_device *dpll, struct dpll_pin *pin,
		      const struct dpll_pin_ops *ops, void *priv,
		      struct device *rclk_device);

/**
 * dpll_pin_unregister - deregister pin from a dpll device
 * @dpll: pointer to dpll object to deregister pin from
 * @pin: pointer to allocated pin object being deregistered from dpll
 * @ops: ops for a dpll pin ops
 * @priv: pointer to private information of owner
 *
 * Deregister previously registered pin object from a dpll device.
 *
 */
void dpll_pin_unregister(struct dpll_device *dpll, struct dpll_pin *pin,
			 const struct dpll_pin_ops *ops, void *priv);

/**
 * dpll_pin_put - drop reference to a pin acquired with dpll_pin_get
 * @pin: pointer to allocated pin
 *
 * Pins shall be deregistered from all dpll devices before putting them,
 * otherwise the memory won't be freed.
 */
void dpll_pin_put(struct dpll_pin *pin);

/**
 * dpll_pin_on_pin_register - register a pin to a muxed-type pin
 * @parent: parent pin pointer
 * @pin: pointer to allocated pin object being registered with a parent pin
 * @ops: struct with pin ops callbacks
 * @priv: private data pointer passed when calling callback ops
 * @rclk_device: pointer to device struct if pin is used for recovery of a clock
 * from that device
 *
 * In case of multiplexed pins, allows registring them under a single
 * parent pin.
 *
 * Return:
 * * 0 - if pin was registered with a parent pin,
 * * -ENOMEM - failed to allocate memory,
 * * -EEXIST - pin already registered with this parent pin,
 */
int dpll_pin_on_pin_register(struct dpll_pin *parent, struct dpll_pin *pin,
			     const struct dpll_pin_ops *ops, void *priv,
			     struct device *rclk_device);

/**
 * dpll_pin_on_pin_register - register a pin to a muxed-type pin
 * @parent: parent pin pointer
 * @pin: pointer to allocated pin object being registered with a parent pin
 * @ops: struct with pin ops callbacks
 * @priv: private data pointer passed when calling callback ops
 * @rclk_device: pointer to device struct if pin is used for recovery of a clock
 * from that device
 *
 * In case of multiplexed pins, allows registring them under a single
 * parent pin.
 *
 * Return:
 * * 0 - if pin was registered with a parent pin,
 * * -ENOMEM - failed to allocate memory,
 * * -EEXIST - pin already registered with this parent pin,
 */
void dpll_pin_on_pin_unregister(struct dpll_pin *parent, struct dpll_pin *pin,
				const struct dpll_pin_ops *ops, void *priv);

/**
 * dpll_device_notify - notify on dpll device change
 * @dpll: dpll device pointer
 * @attr: changed attribute
 *
 * Broadcast event to the netlink multicast registered listeners.
 *
 * Return:
 * * 0 - success
 * * negative - error
 */
int dpll_device_notify(struct dpll_device *dpll, enum dplla attr);

int dpll_pin_notify(struct dpll_device *dpll, struct dpll_pin *pin,
		    enum dplla attr);



#endif
