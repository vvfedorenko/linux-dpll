// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022, Intel Corporation. */

#include "ice.h"
#include "ice_lib.h"
#include "ice_trace.h"
#include <linux/dpll.h>
#include <uapi/linux/dpll.h>

#define CGU_STATE_ACQ_ERR_THRESHOLD	50

static const enum dpll_lock_status
ice_dpll_status[__DPLL_LOCK_STATUS_MAX] = {
	[ICE_CGU_STATE_INVALID] = DPLL_LOCK_STATUS_UNSPEC,
	[ICE_CGU_STATE_FREERUN] = DPLL_LOCK_STATUS_UNLOCKED,
	[ICE_CGU_STATE_LOCKED] = DPLL_LOCK_STATUS_CALIBRATING,
	[ICE_CGU_STATE_LOCKED_HO_ACQ] = DPLL_LOCK_STATUS_LOCKED,
	[ICE_CGU_STATE_HOLDOVER] = DPLL_LOCK_STATUS_HOLDOVER,
};

/**
 * ice_dpll_pin_attr_to_freq - get freq from pin attr struct
 * @new: pointer to pin attr with new signal type info
 * @old: pointer to pin attr with old signal type supported info
 *
 * Decide frequency of pin's signal type from pin attr struct.
 *
 * Return:
 * * 0 - frequency not found in attr
 * * positive - valid frequency
 * * negative - error
 */
int ice_dpll_pin_attr_to_freq(const struct dpll_pin_attr *new,
			      const struct dpll_pin_attr *old)
{
	enum dpll_pin_signal_type sig_t;

	if (!dpll_pin_attr_valid(DPLLA_PIN_SIGNAL_TYPE, new))
		return 0;
	sig_t = dpll_pin_attr_signal_type_get(new);
	if (sig_t == DPLL_PIN_SIGNAL_TYPE_UNSPEC)
		return -EINVAL;
	if (!dpll_pin_attr_signal_type_supported(old, sig_t))
		return -EINVAL;
	if (sig_t == DPLL_PIN_SIGNAL_TYPE_1_PPS)
		return 1;
	else if (sig_t == DPLL_PIN_SIGNAL_TYPE_10_MHZ)
		return 10000000;
	else
		return -EINVAL;
}

/**
 * ice_dpll_pin_freq_to_signal_type
 * @freq: frequency to translate
 *
 * Pin's frequency translated to a pin signal type.
 *
 * Return: signal type of a pin based on frequency.
 */
static inline enum dpll_pin_signal_type
ice_dpll_pin_freq_to_signal_type(u32 freq)
{
	if (freq == 1)
		return DPLL_PIN_SIGNAL_TYPE_1_PPS;
	else if (freq == 10000000)
		return DPLL_PIN_SIGNAL_TYPE_10_MHZ;
	else
		return DPLL_PIN_SIGNAL_TYPE_CUSTOM_FREQ;
}

/**
 * ice_dpll_pin_signal_type_set - set pin's signal type
 * @pf: Board private structure
 * @pin: pointer to a pin
 * @input: if input or output pin
 * @attr: structure with pin attributes
 *
 * Translates pin signal type to frequency and sets it on a pin.
 *
 * Return:
 * * 0 - OK or no change required
 * * negative - error
 */
static int
ice_dpll_pin_signal_type_set(struct ice_pf *pf, struct ice_dpll_pin *pin,
			     bool input, const struct dpll_pin_attr *attr)
{
	u8 flags;
	u32 freq;
	int ret;

	freq = ice_dpll_pin_attr_to_freq(attr, pin->attr);
	if (freq <= 0)
		return freq;

	if (input) {
		flags = ICE_AQC_SET_CGU_IN_CFG_FLG1_UPDATE_FREQ;
		ret = ice_aq_set_input_pin_cfg(&pf->hw, pin->idx, flags,
					       pin->flags, freq, 0);
	} else {
		flags = pin->flags | ICE_AQC_SET_CGU_OUT_CFG_UPDATE_FREQ;
		ret = ice_aq_set_output_pin_cfg(&pf->hw, pin->idx, flags,
						0, freq, 0);
	}

	if (ret) {
		dev_dbg(ice_pf_to_dev(pf),
			"err:%d %s failed to set pin freq:%u on pin:%u\n",
			ret, ice_aq_str(pf->hw.adminq.sq_last_status),
			freq, pin->idx);
	} else {
		enum dpll_pin_signal_type sig_t =
			ice_dpll_pin_freq_to_signal_type(freq);

		dpll_pin_attr_signal_type_set(pin->attr, sig_t);
	}

	return ret;
}

/**
 * ice_dpll_pin_enable - enable a pin on dplls
 * @hw: board private hw structure
 * @pin: pointer to a pin
 * @input: if input or output pin
 *
 * Enable a pin on both dplls. Store current state in pin->flags.
 *
 * Return:
 * * 0 - OK
 * * negative - error
 */
static int
ice_dpll_pin_enable(struct ice_hw *hw, struct ice_dpll_pin *pin, bool input)
{
	u8 flags = pin->flags;
	int ret;

	if (input) {
		flags |= ICE_AQC_GET_CGU_IN_CFG_FLG2_INPUT_EN;
		ret = ice_aq_set_input_pin_cfg(hw, pin->idx, 0, flags, 0, 0);
	} else {
		flags |= ICE_AQC_SET_CGU_OUT_CFG_OUT_EN;
		ret = ice_aq_set_output_pin_cfg(hw, pin->idx, flags, 0, 0, 0);
	}
	if (ret)
		dev_dbg(ice_pf_to_dev((struct ice_pf *)(hw->back)),
			"err:%d %s failed to enable %s pin:%u\n",
			ret, ice_aq_str(hw->adminq.sq_last_status),
			input ? "input" : "output", pin->idx);
	else
		pin->flags = flags;

	return ret;
}

/**
 * ice_dpll_pin_disable - disable a pin on dplls
 * @hw: board private hw structure
 * @pin: pointer to a pin
 * @input: if input or output pin
 *
 * Disable a pin on both dplls. Store current state in pin->flags.
 *
 * Return:
 * * 0 - OK
 * * negative - error
 */
static int
ice_dpll_pin_disable(struct ice_hw *hw, struct ice_dpll_pin *pin, bool input)
{
	u8 flags = pin->flags;
	int ret;

	if (input) {
		flags &= ~(ICE_AQC_GET_CGU_IN_CFG_FLG2_INPUT_EN);
		ret = ice_aq_set_input_pin_cfg(hw, pin->idx, 0, flags, 0, 0);
	} else {
		flags &= ~(ICE_AQC_SET_CGU_OUT_CFG_OUT_EN);
		ret = ice_aq_set_output_pin_cfg(hw, pin->idx, flags, 0, 0, 0);
	}
	if (ret)
		dev_dbg(ice_pf_to_dev((struct ice_pf *)(hw->back)),
			"err:%d %s failed to disable %s pin:%u\n",
			ret, ice_aq_str(hw->adminq.sq_last_status),
			input ? "input" : "output", pin->idx);
	else
		pin->flags = flags;

	return ret;
}

/**
 * ice_dpll_pin_state_set - set pin's state
 * @pf: Board private structure
 * @pin: pointer to a pin
 * @input: if input or output pin
 * @attr: structure with pin attributes
 *
 * Determine requested pin state set it on a pin.
 *
 * Return:
 * * 0 - OK or no change required
 * * negative - error
 */
static int
ice_dpll_pin_state_set(struct ice_pf *pf, struct ice_dpll_pin *pin, bool input,
		       const struct dpll_pin_attr *attr)
{
	int ret;

	if (!dpll_pin_attr_valid(DPLLA_PIN_STATE, attr))
		return 0;
	if (dpll_pin_attr_state_enabled(attr,
					DPLL_PIN_STATE_CONNECTED))
		ret = ice_dpll_pin_enable(&pf->hw, pin, input);
	else if (dpll_pin_attr_state_enabled(attr,
					     DPLL_PIN_STATE_DISCONNECTED))
		ret = ice_dpll_pin_disable(&pf->hw, pin, input);
	else
		ret = -EINVAL;

	return ret;
}

/**
 * ice_dpll_pin_attr_state_update - update pin attr struct with new pin state
 * @attr: structure with pin attributes to be updated
 * @input: if input or output pin
 * @flags: current pin flags from firmware
 *
 * Determine pin state and update pin attr struct.
 *
 * Return:
 * * 0 - OK
 * * negative - error
 */
int
ice_dpll_pin_attr_state_update(struct dpll_pin_attr *attr, bool input, u8 flags)
{
	enum dpll_pin_state s;

	if (input) {
		if (ICE_AQC_GET_CGU_IN_CFG_FLG2_INPUT_EN & flags)
			s = DPLL_PIN_STATE_CONNECTED;
		else
			s = DPLL_PIN_STATE_DISCONNECTED;
	} else {
		if (ICE_AQC_SET_CGU_OUT_CFG_OUT_EN & flags)
			s = DPLL_PIN_STATE_CONNECTED;
		else
			s = DPLL_PIN_STATE_DISCONNECTED;
	}

	return dpll_pin_attr_state_set(attr, s);
}

/**
 * ice_find_dpll - find ice_dpll on a pf
 * @pf: private board structure
 * @dpll: kernel's dpll_device pointer to be searched
 *
 * Return:
 * * pointer if ice_dpll with given device dpll pointer is found
 * * NULL if not found
 */
static struct ice_dpll
*ice_find_dpll(struct ice_pf *pf, struct dpll_device *dpll)
{
	if (!pf || !dpll)
		return NULL;

	return (dpll == pf->dplls.eec.dpll ? &pf->dplls.eec :
		dpll == pf->dplls.pps.dpll ? &pf->dplls.pps : NULL);
}

/**
 * ice_find_pin - find ice_dpll_pin on a pf
 * @pf: private board structure
 * @pin: kernel's dpll_pin pointer to be searched
 * @pins: list of pins to iterate over
 * @count: number of items in the pins list
 *
 * Return:
 * * pointer if ice_dpll_pin with given device dpll_pin pointer is found
 * * NULL if not found
 */
static struct ice_dpll_pin
*ice_find_pin(struct ice_pf *pf, struct dpll_pin *pin,
	      struct ice_dpll_pin *pins, u32 count)

{
	int i;

	if (!pins || !pin || !pf)
		return NULL;

	for (i = 0; i < count; i++)
		if (pin == pins[i].pin)
			return &pins[i];

	return NULL;
}

/**
 * ice_dpll_source_prio_set - set source priority value
 * @pf: board private structure
 * @dpll: registered dpll pointer
 * @pin: dpll pin object
 * @attr: structure with pin attributes to be updated
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int
ice_dpll_source_prio_set(struct ice_pf *pf, struct ice_dpll *dpll,
			 struct ice_dpll_pin *pin,
			 const struct dpll_pin_attr *attr)
{
	u32 prio;
	int ret;

	if (!dpll_pin_attr_valid(DPLLA_PIN_PRIO, attr))
		return 0;
	ret = dpll_pin_attr_prio_get(attr, &prio);
	if (ret)
		return ret;
	if (prio > ICE_DPLL_PRIO_MAX)
		return -EINVAL;
	ret = ice_aq_set_cgu_ref_prio(&pf->hw, dpll->dpll_idx, pin->idx,
				      (u8)prio);
	if (ret)
		dev_dbg(ice_pf_to_dev(pf),
			"err:%d %s failed to set pin prio:%u on pin:%u\n",
			ret, ice_aq_str(pf->hw.adminq.sq_last_status),
			prio, pin->idx);
	else
		dpll->input_prio[pin->idx] = prio;

	return ret;
}

/**
 * ice_dpll_dev_get - get dpll parameters
 * @dpll: registered dpll pointer
 * @attr: structure with dpll attributes to be updated
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int ice_dpll_dev_get(struct dpll_device *dpll, struct dpll_attr *attr)
{
	struct ice_pf *pf = dpll_priv(dpll);
	struct ice_dpll *d;

	d = ice_find_dpll(pf, dpll);
	if (!d)
		return -EFAULT;

	mutex_lock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf), "%s: dpll:%p, pf:%p\n", __func__, dpll, pf);

	dpll_attr_copy(attr, d->attr);
	dpll_attr_lock_status_set(attr, ice_dpll_status[d->dpll_state]);
	dpll_attr_source_idx_set(attr, (u32)d->source_idx);
	mutex_unlock(&pf->dplls.lock);

	return 0;
}

/**
 * ice_dpll_dev_set - get dpll parameters
 * @dpll: registered dpll pointer
 * @attr: structure with dpll attributes to be updated
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int ice_dpll_dev_set(struct dpll_device *dpll, const struct dpll_attr *attr)
{
	struct ice_pf *pf = dpll_priv(dpll);

	dev_dbg(ice_pf_to_dev(pf), "%s: dpll:%p, pf:%p\n", __func__, dpll, pf);

	return 0;
}

/**
 * ice_dpll_output_get - get dpll output pin parameters
 * @dpll: registered dpll pointer
 * @pin: pointer to a pin
 * @attr: structure with pin attributes to be updated
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int ice_dpll_output_get(struct dpll_device *dpll, struct dpll_pin *pin,
			       struct dpll_pin_attr *attr)
{
	struct ice_pf *pf = dpll_pin_priv(dpll, pin);
	struct ice_dpll_pin *p;
	int ret = 0;

	mutex_lock(&pf->dplls.lock);
	p = ice_find_pin(pf, pin, pf->dplls.outputs, pf->dplls.num_outputs);
	if (!p) {
		ret = -EFAULT;
		goto unlock;
	} else {
		ret = dpll_pin_attr_copy(attr, p->attr);
	}
	if (ret)
		goto unlock;
	ret = ice_dpll_pin_attr_state_update(attr, false, p->flags);
unlock:
	mutex_unlock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf),
		"%s: dpll:%p, pin:%p, pf:%p, p:%p, p->pin:%p, ret:%d\n",
		__func__, dpll, pin, pf, p, p->pin, ret);

	return ret;
}

/**
 * ice_dpll_output_set - set dpll output pin parameters
 * @dpll: registered dpll pointer
 * @pin: pointer to a pin
 * @attr: structure with pin attributes to be updated
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int ice_dpll_output_set(struct dpll_device *dpll, struct dpll_pin *pin,
			       const struct dpll_pin_attr *attr)
{
	struct ice_pf *pf = dpll_pin_priv(dpll, pin);
	struct ice_dpll_pin *p;
	int ret = -EFAULT;

	mutex_lock(&pf->dplls.lock);
	p = ice_find_pin(pf, pin, pf->dplls.outputs, pf->dplls.num_outputs);
	if (!p)
		goto unlock;
	ret = ice_dpll_pin_signal_type_set(pf, p, false, attr);
	if (ret)
		goto unlock;
	ret = ice_dpll_pin_state_set(pf, p, false, attr);
unlock:
	mutex_unlock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf),
		"%s: dpll:%p, pin:%p, pf:%p, p:%p, p->pin:%p, ret:%d\n",
		__func__, dpll, pin, pf, p, p ? p->pin : NULL,  ret);

	return ret;
}

/**
 * ice_dpll_source_get - get dpll input pin parameters
 * @dpll: registered dpll pointer
 * @pin: pointer to a pin
 * @attr: structure with pin attributes to be updated
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int ice_dpll_source_get(struct dpll_device *dpll, struct dpll_pin *pin,
			       struct dpll_pin_attr *attr)
{
	struct ice_pf *pf = dpll_pin_priv(dpll, pin);
	struct ice_dpll *d = NULL;
	struct ice_dpll_pin *p;
	int ret = -EFAULT;

	mutex_lock(&pf->dplls.lock);
	p = ice_find_pin(pf, pin, pf->dplls.inputs, pf->dplls.num_inputs);
	if (!p)
		goto unlock;
	ret = dpll_pin_attr_copy(attr, p->attr);
	if (ret)
		goto unlock;
	d = ice_find_dpll(pf, dpll);
	if (!d) {
		ret = -EFAULT;
		goto unlock;
	}
	ret = dpll_pin_attr_prio_set(attr, (u32)d->input_prio[p->idx]);
	if (ret)
		goto unlock;
	ret = ice_dpll_pin_attr_state_update(attr, true, p->flags);
unlock:
	mutex_unlock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf),
		"%s: dpll:%p, pin:%p, pf:%p, p:%p, p->pin:%p, d:%p, ret:%d\n",
		__func__, dpll, pin, pf, p, p ? p->pin : NULL, d, ret);

	return ret;
}

/**
 * ice_dpll_source_set - set dpll input pin parameters
 * @dpll: registered dpll pointer
 * @pin: pointer to a pin
 * @attr: structure with pin attributes to be updated
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int ice_dpll_source_set(struct dpll_device *dpll, struct dpll_pin *pin,
			       const struct dpll_pin_attr *attr)
{
	struct ice_pf *pf = dpll_pin_priv(dpll, pin);
	struct ice_dpll *d = NULL;
	struct ice_dpll_pin *p;
	int ret = -EFAULT;

	mutex_lock(&pf->dplls.lock);
	p = ice_find_pin(pf, pin, pf->dplls.inputs, pf->dplls.num_inputs);
	if (!p)
		goto unlock;
	d = ice_find_dpll(pf, dpll);
	if (!d)
		goto unlock;
	ret = ice_dpll_source_prio_set(pf, d, p, attr);
	if (ret)
		goto unlock;
	ret = ice_dpll_pin_signal_type_set(pf, p, true, attr);
	if (ret)
		goto unlock;
	ret = ice_dpll_pin_state_set(pf, p, true, attr);
unlock:
	mutex_unlock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf), "%s: dpll:%p, pin:%p, pf:%p ret:%d\n",
		__func__, dpll, pin, pf, ret);

	return ret;
}

/**
 * ice_dpll_rclk_get - get dpll recovered clock pin parameters
 * @dpll: registered dpll pointer
 * @pin: pointer to a pin
 * @attr: structure with pin attributes to be updated
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int ice_dpll_rclk_get(struct dpll_device *dpll, struct dpll_pin *pin,
			     struct dpll_pin_attr *attr)
{
	struct ice_pf *pf = dpll_pin_priv(dpll, pin);
	struct ice_dpll_pin *p;
	int ret;

	mutex_lock(&pf->dplls.lock);
	p = ice_find_pin(pf, pin, pf->dplls.rclk, pf->dplls.num_rclk);
	if (!p) {
		ret = -EFAULT;
		goto unlock;
	}
	ret = dpll_pin_attr_copy(attr, p->attr);
unlock:
	mutex_unlock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf),
		"%s: dpll:%p, pin:%p, pf:%p, p:%p, p->pin:%p, ret:%d\n",
		__func__, dpll, pin, pf, p, p->pin, ret);

	return ret;
}

/**
 * ice_dpll_rclk_select - callback for select dpll recovered pin
 * @dpll: registered dpll pointer
 * @pin: pointer to a pin
 *
 * dpll subsystem callback.
 *
 * Return:
 * * 0 - success
 * * negative - failure
 */
static int ice_dpll_rclk_select(struct dpll_device *dpll, struct dpll_pin *pin)
{
	struct ice_pf *pf = dpll_pin_priv(dpll, pin);
	struct ice_dpll_pin *p;
	u32 freq;
	int ret;

	mutex_lock(&pf->dplls.lock);
	p = ice_find_pin(pf, pin, pf->dplls.rclk, pf->dplls.num_rclk);
	if (!p) {
		ret = -EFAULT;
		goto unlock;
	}
	ret = ice_aq_set_phy_rec_clk_out(&pf->hw, p->rclk_idx, true, &freq);
unlock:
	mutex_unlock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf), "%s: dpll:%p, pin:%p, pf:%p ret:%d\n",
		__func__, dpll, pin, pf, ret);

	return ret;
}

static struct dpll_pin_ops ice_dpll_rclk_ops = {
	.get = ice_dpll_rclk_get,
	.select = ice_dpll_rclk_select,
};

static struct dpll_pin_ops ice_dpll_source_ops = {
	.get = ice_dpll_source_get,
	.set = ice_dpll_source_set,
};

static struct dpll_pin_ops ice_dpll_output_ops = {
	.get = ice_dpll_output_get,
	.set = ice_dpll_output_set,
};

static struct dpll_device_ops ice_dpll_ops = {
	.get = ice_dpll_dev_get,
	.set = ice_dpll_dev_set,
};

/**
 * ice_dpll_release_info
 * @pf: board private structure
 *
 * Release memory allocated by ice_dpll_init_info.
 */
static void ice_dpll_release_info(struct ice_pf *pf)
{
	kfree(pf->dplls.inputs);
	pf->dplls.inputs = NULL;
	kfree(pf->dplls.outputs);
	pf->dplls.outputs = NULL;
	kfree(pf->dplls.eec.input_prio);
	pf->dplls.eec.input_prio = NULL;
	kfree(pf->dplls.pps.input_prio);
	pf->dplls.pps.input_prio = NULL;
}

/**
 * ice_dpll_init_pins
 * @hw: Board private structure
 * @input: input or output pins
 * @num_pins: number of pins to be initialized
 * @pins: pointer to the pins array
 * @de: index of EEC dpll
 * @dp: index of PPS dpll
 *
 * Init info about input or output pins, cache them in pins struct.
 */
static int ice_dpll_init_pins(struct ice_hw *hw, bool input, int num_pins,
			      struct ice_dpll_pin *pins, struct ice_dpll *de,
			      struct ice_dpll *dp)
{
	int ret = -EINVAL;
	u8 i;

	for (i = 0; i < num_pins; i++) {
		enum dpll_pin_type t = ice_cgu_get_pin_type(hw, i, input);
		enum dpll_pin_signal_type st;
		unsigned long sig_type_mask;
		enum dpll_pin_state s;
		u32 freq;

		pins[i].attr = dpll_pin_attr_alloc();
		if (!pins[i].attr)
			return -ENOMEM;

		pins[i].name = ice_cgu_get_pin_name(hw, i, input);
		dpll_pin_attr_type_set(pins[i].attr, t);
		dpll_pin_attr_type_supported_set(pins[i].attr, t);
		s = DPLL_PIN_STATE_CONNECTED;
		dpll_pin_attr_state_supported_set(pins[i].attr, s);
		s = DPLL_PIN_STATE_DISCONNECTED;
		dpll_pin_attr_state_supported_set(pins[i].attr, s);
		if (input) {
			ret = ice_aq_get_cgu_ref_prio(hw, de->dpll_idx, i,
						      &de->input_prio[i]);
			if (ret)
				return ret;

			ret = ice_aq_get_cgu_ref_prio(hw, dp->dpll_idx, i,
						      &dp->input_prio[i]);
			if (ret)
				return ret;

			ret = ice_aq_get_input_pin_cfg(hw, i, NULL, NULL,
						       NULL, &pins[i].flags,
						       &freq, NULL);
			if (ret)
				return ret;
			s = DPLL_PIN_STATE_SOURCE;
		} else {
			ret = ice_aq_get_output_pin_cfg(hw, i, &pins[i].flags,
							NULL, &freq, NULL);
			if (ret)
				return ret;
			s = DPLL_PIN_STATE_OUTPUT;
		}
		dpll_pin_attr_state_supported_set(pins[i].attr, s);
		dpll_pin_attr_state_set(pins[i].attr, s);
		st = ice_dpll_pin_freq_to_signal_type(freq);
		dpll_pin_attr_signal_type_set(pins[i].attr, st);
		if (st == DPLL_PIN_SIGNAL_TYPE_CUSTOM_FREQ)
			dpll_pin_attr_custom_freq_set(pins[i].attr, freq);
		sig_type_mask = ice_cgu_get_pin_sig_type_mask(hw, i, input);
		if (test_bit(DPLL_PIN_SIGNAL_TYPE_1_PPS, &sig_type_mask)) {
			st = DPLL_PIN_SIGNAL_TYPE_1_PPS;
			dpll_pin_attr_signal_type_supported_set(pins[i].attr,
								st);
		}
		if (test_bit(DPLL_PIN_SIGNAL_TYPE_10_MHZ, &sig_type_mask)) {
			st = DPLL_PIN_SIGNAL_TYPE_10_MHZ;
			dpll_pin_attr_signal_type_supported_set(pins[i].attr,
								st);
		}
		pins[i].idx = i;
	}

	return ret;
}

/**
 * ice_dpll_release_pins
 * @dpll_eec: dpll_eec dpll pointer
 * @dpll_pps: dpll_pps dpll pointer
 * @pins: pointer to pins array
 * @count: number of pins
 *
 * Release dpll pins from dpll subsystem.
 *
 * Return:
 * * 0 - success
 * * positive - number of errors encounterd on pin's deregistration.
 */
static int
ice_dpll_release_pins(struct dpll_device *dpll_eec,
		      struct dpll_device *dpll_pps, struct ice_dpll_pin *pins,
		      int count)
{
	int i, ret, err;

	for (i = 0; i < count; i++) {
		struct ice_dpll_pin *p = &pins[i];

		if (p && p->pin) {
			if (dpll_eec) {
				ret = dpll_pin_deregister(dpll_eec, p->pin);
				if (ret)
					err++;
			}
			if (dpll_pps) {
				ret = dpll_pin_deregister(dpll_pps, p->pin);
				if (ret)
					err++;
			}
			dpll_pin_free(p->pin);
			dpll_pin_attr_free(p->attr);
			p->pin = NULL;
			p->attr = NULL;
		}
	}

	return err;
}

/**
 * ice_dpll_register_pins
 * @pf: board private structure
 * @dpll: registered dpll pointer
 * @pins: pointer to the pins array
 * @count: number of pins
 * @input: pins type
 *
 * Register pins within a ice DPLL in dpll subsystem.
 *
 * Return:
 * * 0 - success
 * * negative - error
 */
static int
ice_dpll_register_pins(struct ice_pf *pf, struct dpll_device *dpll,
		       struct ice_dpll_pin *pins, int count, bool input)
{
	struct dpll_pin_ops *ops;
	int ret, i = 0;

	if (input)
		ops = &ice_dpll_source_ops;
	else
		ops = &ice_dpll_output_ops;

	for (i = 0; i < count; i++) {
		pins[i].pin = dpll_pin_alloc(pins[i].name,
					     strnlen(pins[i].name,
						     PIN_DESC_LEN));
		if (!pins[i].pin)
			return -ENOMEM;

		ret = dpll_pin_register(dpll, pins[i].pin, ops, pf);
		if (ret)
			return -ENOSPC;
	}

	return 0;
}

/**
 * ice_dpll_register_shared_pins
 * @pf: board private structure
 * @dpll_o: registered dpll pointer (owner)
 * @dpll: registered dpll pointer
 * @pins: pointer to the pins array
 * @count: number of pins
 * @input: pins type
 *
 * Register pins within a ice DPLL in dpll subsystem.
 *
 * Return:
 * * 0 - success
 * * negative - error
 */
static int
ice_dpll_register_shared_pins(struct ice_pf *pf, struct dpll_device *dpll_o,
			      struct dpll_device *dpll,
			      struct ice_dpll_pin *pins, int count, bool input)
{
	struct dpll_pin_ops *ops;
	int ret, i;

	if (input)
		ops = &ice_dpll_source_ops;
	else
		ops = &ice_dpll_output_ops;

	for (i = 0; i < count; i++) {
		u32 pin_idx;

		pin_idx = dpll_pin_idx(dpll_o, pins[i].pin);
		if (pin_idx == PIN_IDX_INVALID)
			return -EINVAL;

		ret = dpll_shared_pin_register(dpll_o, dpll, pin_idx, ops, pf);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * ice_dpll_init_info
 * @pf: board private structure
 *
 * Acquire (from HW) and set basic dpll information (on pf->dplls struct).
 *
 * Return:
 *  0 - success
 *  negative - AQ error
 */
static int ice_dpll_init_info(struct ice_pf *pf)
{
	struct ice_aqc_get_cgu_abilities abilities;
	struct ice_dpll *de = &pf->dplls.eec;
	struct ice_dpll *dp = &pf->dplls.pps;
	struct ice_dplls *d = &pf->dplls;
	struct ice_hw *hw = &pf->hw;
	int ret, alloc_size;

	ret = ice_aq_get_cgu_abilities(hw, &abilities);
	if (ret) {
		dev_err(ice_pf_to_dev(pf),
			"err:%d %s failed to read cgu abilities\n",
			ret, ice_aq_str(hw->adminq.sq_last_status));
		return ret;
	}

	de->dpll_idx = abilities.eec_dpll_idx;
	dp->dpll_idx = abilities.pps_dpll_idx;
	d->num_inputs = abilities.num_inputs;
	alloc_size = sizeof(*d->inputs) * d->num_inputs;
	d->inputs = kzalloc(alloc_size, GFP_KERNEL);
	if (!d->inputs)
		return -ENOMEM;

	alloc_size = sizeof(*de->input_prio) * d->num_inputs;
	de->input_prio =  kzalloc(alloc_size, GFP_KERNEL);
	if (!de->input_prio)
		return -ENOMEM;

	dp->input_prio =  kzalloc(alloc_size, GFP_KERNEL);
	if (!dp->input_prio)
		return -ENOMEM;

	ret = ice_dpll_init_pins(hw, true, d->num_inputs, d->inputs, de, dp);
	if (ret)
		goto release_info;

	d->num_outputs = abilities.num_outputs;
	alloc_size = sizeof(*d->outputs) * d->num_outputs;
	d->outputs = kzalloc(alloc_size, GFP_KERNEL);
	if (!d->outputs)
		goto release_info;

	ret = ice_dpll_init_pins(hw, false, d->num_outputs, d->outputs, de, dp);
	if (ret)
		goto release_info;

	dev_dbg(ice_pf_to_dev(pf), "%s - success, inputs:%u, outputs:%u\n", __func__,
		abilities.num_inputs, abilities.num_outputs);

	return 0;

release_info:
	dev_err(ice_pf_to_dev(pf),
		"%s - fail: d->inputs:%p, de->input_prio:%p, dp->input_prio:%p, d->outputs:%p\n",
		__func__, d->inputs, de->input_prio,
		dp->input_prio, d->outputs);
	ice_dpll_release_info(pf);
	return ret;
}

static void ice_gen_cookie(struct ice_pf *pf, u8 *cookie, int cookie_size)
{
	u64 dsn = pci_get_dsn(pf->pdev);

	memset(cookie, 0, sizeof(*cookie) * cookie_size);
	memcpy(cookie, &dsn, sizeof(dsn));
}

static int ice_dpll_init_attrs(struct ice_pf *pf, struct ice_dpll *dpll)
{
	dpll->attr = dpll_attr_alloc();
	if (!dpll->attr)
		return -ENOMEM;

	dpll_attr_mode_set(dpll->attr, DPLL_MODE_AUTOMATIC);
	dpll_attr_mode_supported_set(dpll->attr, DPLL_MODE_AUTOMATIC);

	return 0;
}

/**
 * ice_dpll_init_dpll
 * @pf: board private structure
 *
 * Allocate and register dpll in dpll subsystem.
 * Return:
 * * 0 - success
 * * -ENOMEM - allocation fails
 */
static int ice_dpll_init_dpll(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_dpll *de = &pf->dplls.eec;
	struct ice_dpll *dp = &pf->dplls.pps;
	u8 cookie_dpll[DPLL_COOKIE_LEN];
	int ret = 0;

	ice_gen_cookie(pf, cookie_dpll, sizeof(cookie_dpll));

	de->dpll = dpll_device_alloc(&ice_dpll_ops, DPLL_TYPE_EEC,
				     cookie_dpll, 0, pf, dev);
	if (!de->dpll) {
		dev_err(ice_pf_to_dev(pf), "dpll_device_alloc failed (eec)\n");
		return -ENOMEM;
	}
	dpll_device_register(de->dpll);
	ret = ice_dpll_init_attrs(pf, de);
	if (ret)
		return ret;

	dp->dpll = dpll_device_alloc(&ice_dpll_ops, DPLL_TYPE_PPS,
				     cookie_dpll, 0, pf, dev);
	if (!dp->dpll) {
		dev_err(ice_pf_to_dev(pf), "dpll_device_alloc failed (pps)\n");
		return -ENOMEM;
	}
	dpll_device_register(dp->dpll);
	ret = ice_dpll_init_attrs(pf, dp);

	return ret;
}

/**
 * ice_dpll_update_state
 * @hw: board private structure
 * @d: pointer to queried dpll device
 *
 * Poll current state of dpll from hw and update ice_dpll struct.
 * Return:
 * * 0 - success
 * * negative - AQ failure
 */
static int ice_dpll_update_state(struct ice_hw *hw, struct ice_dpll *d)
{
	int ret;

	ret = ice_get_cgu_state(hw, d->dpll_idx, d->prev_dpll_state,
				&d->source_idx, &d->ref_state, &d->eec_mode,
				&d->phase_offset, &d->dpll_state);

	dev_dbg(ice_pf_to_dev((struct ice_pf *)(hw->back)),
		"update dpll=%d, src_idx:%u, state:%d, prev:%d\n",
		d->dpll_idx, d->source_idx,
		d->dpll_state, d->prev_dpll_state);

	if (ret)
		dev_err(ice_pf_to_dev((struct ice_pf *)(hw->back)),
			"update dpll=%d state failed, ret=%d %s\n",
			d->dpll_idx, ret,
			ice_aq_str(hw->adminq.sq_last_status));

	return ret;
}

/**
 * ice_dpll_notify_changes - notify dpll subsystem about changes
 * @d: pointer do dpll
 *
 * Once change detected appropriate event is submitted to the dpll subsystem.
 */
static void ice_dpll_notify_changes(struct ice_dpll *d)
{
	if (d->prev_dpll_state != d->dpll_state) {
		d->prev_dpll_state = d->dpll_state;
		dpll_device_notify(d->dpll, DPLL_CHANGE_LOCK_STATUS);
	}
	if (d->prev_source_idx != d->source_idx) {
		d->prev_source_idx = d->source_idx;
		dpll_device_notify(d->dpll, DPLL_CHANGE_SOURCE_PIN);
	}
}

/**
 * ice_dpll_periodic_work - DPLLs periodic worker
 * @work: pointer to kthread_work structure
 *
 * DPLLs periodic worker is responsible for polling state of dpll.
 */
static void ice_dpll_periodic_work(struct kthread_work *work)
{
	struct ice_dplls *d = container_of(work, struct ice_dplls, work.work);
	struct ice_pf *pf = container_of(d, struct ice_pf, dplls);
	struct ice_dpll *de = &pf->dplls.eec;
	struct ice_dpll *dp = &pf->dplls.pps;
	int ret = 0;

	if (!test_bit(ICE_FLAG_DPLL, pf->flags))
		return;
	mutex_lock(&d->lock);
	ret = ice_dpll_update_state(&pf->hw, de);
	if (!ret)
		ret = ice_dpll_update_state(&pf->hw, dp);
	if (ret) {
		d->cgu_state_acq_err_num++;
		/* stop rescheduling this worker */
		if (d->cgu_state_acq_err_num >
		    CGU_STATE_ACQ_ERR_THRESHOLD) {
			dev_err(ice_pf_to_dev(pf),
				"EEC/PPS DPLLs periodic work disabled\n");
			return;
		}
	}
	mutex_unlock(&d->lock);
	ice_dpll_notify_changes(de);
	ice_dpll_notify_changes(dp);

	/* Run twice a second or reschedule if update failed */
	kthread_queue_delayed_work(d->kworker, &d->work,
				   ret ? msecs_to_jiffies(10) :
				   msecs_to_jiffies(500));
}

/**
 * ice_dpll_init_worker - Initialize DPLLs periodic worker
 * @pf: board private structure
 *
 * Create and start DPLLs periodic worker.
 * Return:
 * * 0 - success
 * * negative - create worker failure
 */
static int ice_dpll_init_worker(struct ice_pf *pf)
{
	struct ice_dplls *d = &pf->dplls;
	struct kthread_worker *kworker;

	ice_dpll_update_state(&pf->hw, &d->eec);
	ice_dpll_update_state(&pf->hw, &d->pps);
	kthread_init_delayed_work(&d->work, ice_dpll_periodic_work);
	kworker = kthread_create_worker(0, "ice-dplls-%s",
					dev_name(ice_pf_to_dev(pf)));
	if (IS_ERR(kworker))
		return PTR_ERR(kworker);
	d->kworker = kworker;
	d->cgu_state_acq_err_num = 0;
	kthread_queue_delayed_work(d->kworker, &d->work, 0);

	return 0;
}

/**
 * __ice_dpll_release - Disable the driver/HW support for DPLL and unregister
 * the dpll device.
 * @pf: board private structure
 *
 * This function handles the cleanup work required from the initialization by
 * freeing resources and unregistering the dpll.
 *
 * Context: Called under pf->dplls.lock
 */
static void __ice_dpll_release(struct ice_pf *pf)
{
	struct ice_dplls *d = &pf->dplls;
	struct ice_dpll *de = &d->eec;
	struct ice_dpll *dp = &d->pps;
	int ret;

	dpll_attr_free(de->attr);
	dpll_attr_free(dp->attr);

	ret = ice_dpll_release_pins(de->dpll, dp->dpll, d->inputs,
				    d->num_inputs);
	if (ret)
		dev_warn(ice_pf_to_dev(pf),
			 "pin deregister on PPS dpll err=%d\n", ret);
	ret = ice_dpll_release_pins(de->dpll, dp->dpll, d->outputs,
				    d->num_outputs);
	if (ret)
		dev_warn(ice_pf_to_dev(pf),
			 "pin deregister on PPS dpll err=%d\n", ret);
	ice_dpll_release_info(pf);
	if (dp->dpll) {
		dpll_device_unregister(dp->dpll);
		dpll_device_free(dp->dpll);
		dev_dbg(ice_pf_to_dev(pf), "PPS dpll removed\n");
	}

	if (de->dpll) {
		dpll_device_unregister(de->dpll);
		dpll_device_free(de->dpll);
		dev_dbg(ice_pf_to_dev(pf), "EEC dpll removed\n");
	}

	kthread_cancel_delayed_work_sync(&d->work);
	if (d->kworker) {
		kthread_destroy_worker(d->kworker);
		d->kworker = NULL;
		dev_dbg(ice_pf_to_dev(pf), "DPLLs worker removed\n");
	}
}

/**
 * ice_dpll_init - Initialize DPLLs support
 * @pf: board private structure
 *
 * Set up the device as owner of DPLLs feature.
 * Register in dpll subsystem, and allow userpsace to obtain state of PLL
 * and handle its configuration requests.
 * Return:
 * * 0 - success
 * * negative - init failure
 */
int ice_dpll_init(struct ice_pf *pf)
{
	struct ice_dplls *d = &pf->dplls;
	int err;

	mutex_init(&d->lock);
	mutex_lock(&d->lock);
	err = ice_dpll_init_info(pf);
	if (err)
		goto unlock;
	err = ice_dpll_init_dpll(pf);
	if (err)
		goto release;
	err = ice_dpll_register_pins(pf, d->eec.dpll, d->inputs,
				     d->num_inputs, true);
	if (err)
		goto release;
	err = ice_dpll_register_pins(pf, d->eec.dpll, d->outputs,
				     d->num_outputs, false);
	if (err)
		goto release;
	err = ice_dpll_register_shared_pins(pf, d->eec.dpll, d->pps.dpll,
					    d->inputs, d->num_inputs, true);
	if (err)
		goto release;
	err = ice_dpll_register_shared_pins(pf, d->eec.dpll, d->pps.dpll,
					    d->outputs, d->num_outputs, false);
	if (err)
		goto release;
	set_bit(ICE_FLAG_DPLL, pf->flags);
	err = ice_dpll_init_worker(pf);
	if (err)
		goto release;
	mutex_unlock(&d->lock);
	dev_dbg(ice_pf_to_dev(pf), "DPLLs init successful\n");

	return err;
release:
	__ice_dpll_release(pf);
unlock:
	clear_bit(ICE_FLAG_DPLL, pf->flags);
	mutex_unlock(&d->lock);
	mutex_destroy(&d->lock);
	dev_warn(ice_pf_to_dev(pf), "DPLLs init failure\n");

	return err;
}

/**
 * ice_dpll_release - Disable the driver/HW support for DPLLs and unregister
 * the dpll device.
 * @pf: board private structure
 *
 * This function handles the cleanup work required from the initialization by
 * freeing resources and unregistering the dpll.
 */
void ice_dpll_release(struct ice_pf *pf)
{
	if (test_bit(ICE_FLAG_DPLL, pf->flags)) {
		mutex_lock(&pf->dplls.lock);
		clear_bit(ICE_FLAG_DPLL, pf->flags);
		__ice_dpll_release(pf);
		mutex_unlock(&pf->dplls.lock);
		mutex_destroy(&pf->dplls.lock);
	}
}

/**
 * ice_dpll_rclk_pin_init_attr - init the pin attributes for recovered clock
 * @attr: structure with pin attributes
 *
 * Return:
 * * 0 - success
 * * negative - init failure
 */
int ice_dpll_rclk_pin_init_attr(struct dpll_pin_attr *a)
{
	int ret;

	ret = dpll_pin_attr_type_set(a, DPLL_PIN_TYPE_SYNCE_ETH_PORT);
	if (ret)
		return ret;
	ret = dpll_pin_attr_type_supported_set(a, DPLL_PIN_TYPE_SYNCE_ETH_PORT);
	if (ret)
		return ret;
	ret = dpll_pin_attr_state_supported_set(a, DPLL_PIN_STATE_CONNECTED);
	if (ret)
		return ret;
	ret = dpll_pin_attr_state_supported_set(a, DPLL_PIN_STATE_DISCONNECTED);
	if (ret)
		return ret;
	ret = dpll_pin_attr_state_supported_set(a, DPLL_PIN_STATE_SOURCE);
	if (ret)
		return ret;
	ret = dpll_pin_attr_state_set(a, DPLL_PIN_STATE_SOURCE);
	if (ret)
		return ret;
	ret = dpll_pin_attr_signal_type_set(a,
					    DPLL_PIN_SIGNAL_TYPE_CUSTOM_FREQ);
	return ret;
}

/**
 * __ice_dpll_rclk_release - unregister the recovered pin for dpll device
 * @pf: board private structure
 *
 * This function handles the cleanup work required from the initialization by
 * freeing resources and unregistering the recovered pin.
 */
void __ice_dpll_rclk_release(struct ice_pf *pf)
{
	int ret = 0;

	if (pf->dplls.eec.dpll) {
		if (pf->dplls.rclk[0].pin)
			ret = dpll_pin_deregister(pf->dplls.eec.dpll,
						  pf->dplls.rclk[0].pin);
		dpll_pin_free(pf->dplls.rclk->pin);
		dpll_pin_attr_free(pf->dplls.rclk->attr);
		kfree(pf->dplls.rclk);
	}
	dev_dbg(ice_pf_to_dev(pf), "PHY RCLK release ret:%d\n", ret);
}

/**
 * ice_dpll_rclk_pins_init - init the pin for recovered clock
 * @pf: board private structure
 * @pin: pointer to a parent pin
 *
 * Return:
 * * 0 - success
 * * negative - init failure
 */
int ice_dpll_rclk_pins_init(struct ice_pf *pf, struct ice_dpll_pin *first_parent)
{
	struct ice_dpll_pin *parent, *p;
	u32 pin_idx;
	char *name;
	int i, ret;

	if (pf->dplls.rclk)
		return -EEXIST;
	pf->dplls.rclk = kcalloc(pf->dplls.num_rclk, sizeof(*pf->dplls.rclk),
				 GFP_KERNEL);
	if (!pf->dplls.rclk)
		goto release;
	for (i = ICE_RCLKA_PIN; i < pf->dplls.num_rclk; i++) {
		p = &pf->dplls.rclk[i];
		if (!p)
			goto release;
		p->attr = dpll_pin_attr_alloc();
		if (!p->attr)
			goto release;
		ret = ice_dpll_rclk_pin_init_attr(p->attr);
		if (ret)
			goto release;
		parent = first_parent + i;
		if (!parent)
			goto release;
		p->rclk_idx = i;
		name = kcalloc(PIN_DESC_LEN, sizeof(*p->name), GFP_KERNEL);
		if (!name)
			goto release;
		snprintf(name, PIN_DESC_LEN - 1, "%s-%u",
			 parent->name, pf->hw.pf_id);
		p->name = name;
		p->pin = dpll_pin_alloc(p->name,
					strnlen(p->name, PIN_DESC_LEN));
		if (!p->pin)
			goto release;
		ret = dpll_muxed_pin_register(pf->dplls.eec.dpll, parent->pin,
					      p->pin, &ice_dpll_rclk_ops, pf);
		if (ret)
			goto release;
		pin_idx = dpll_pin_idx(pf->dplls.eec.dpll, p->pin);
		if (pin_idx == PIN_IDX_INVALID) {
			ret = -EINVAL;
			goto release;
		}
		ret = dpll_shared_pin_register(pf->dplls.eec.dpll,
					       pf->dplls.pps.dpll,
					       pin_idx, &ice_dpll_rclk_ops, pf);
		if (ret)
			goto release;
	}

	return ret;
release:
	dev_dbg(ice_pf_to_dev(pf),
		"%s releasing - p: %p, parent:%p, name:%s, p->attr:%p, ret:%d\n",
		__func__, p, parent, name, p->attr, ret);
	__ice_dpll_rclk_release(pf);
	return -ENOMEM;
}

/**
 * ice_dpll_rclk_find_dplls - find the device-wide DPLLs by cookie
 * @pf: board private structure
 *
 * Return:
 * * 0 - success
 * * negative - init failure
 */
static int ice_dpll_rclk_find_dplls(struct ice_pf *pf)
{
	u8 cookie_dpll[DPLL_COOKIE_LEN];

	ice_gen_cookie(pf, cookie_dpll, sizeof(cookie_dpll));
	pf->dplls.eec.dpll = dpll_device_get_by_cookie(cookie_dpll,
						       DPLL_TYPE_EEC, 0);
	if (!pf->dplls.eec.dpll)
		return -EFAULT;
	pf->dplls.pps.dpll = dpll_device_get_by_cookie(cookie_dpll,
						       DPLL_TYPE_PPS, 0);
	if (!pf->dplls.pps.dpll)
		return -EFAULT;

	return 0;
}

/**
 * ice_dpll_rclk_parent_pins_init - initialize the recovered clock parent pins
 * @pf: board private structure
 * @base_rclk_idx: number of first recovered clock pin in DPLL
 *
 * This function is run only if ICE_FLAG_DPLL feature is not supported.
 * Return:
 * * 0 - success
 * * negative - init failure
 */
static int ice_dpll_rclk_parent_pins_init(struct ice_pf *pf, u8 base_rclk_idx)
{
	int i;

	if (pf->dplls.inputs)
		return -EINVAL;
	pf->dplls.inputs = kcalloc(pf->dplls.num_rclk,
				   sizeof(*pf->dplls.inputs), GFP_KERNEL);

	for (i = ICE_RCLKA_PIN; i < pf->dplls.num_rclk; i++) {
		struct dpll_device *d;
		const char *desc;

		desc = ice_cgu_get_pin_name(&pf->hw, base_rclk_idx + i, true);
		if (!desc)
			return -EINVAL;
		if (pf->dplls.eec.dpll)
			d = pf->dplls.eec.dpll;
		else
			return -EFAULT;
		pf->dplls.inputs[i].pin = dpll_pin_get_by_description(d, desc);
		if (!pf->dplls.inputs[i].pin)
			return -EFAULT;
		pf->dplls.inputs[i].name = desc;
	}
	return 0;
}

/**
 * ice_dpll_rclk_init - Enable support for DPLL's PHY clock recovery
 * @pf: board private structure
 *
 * Context:
 * Acquires a pf->dplls.lock. If PF is not an owner of DPLL it shall find and
 * connect its pins with the device dpll.
 *
 * This function handles enablement of PHY clock recovery part for timesync
 * capabilities.
 * Prepares and initalizes resources required to register its PHY clock sources
 * within DPLL subsystem.
 * Return:
 * * 0 - success
 * * negative - init failure
 */
int ice_dpll_rclk_init(struct ice_pf *pf)
{
	struct ice_dpll_pin *first_parent = NULL;
	u8 base_rclk_idx;
	int ret;

	ret = ice_get_cgu_rclk_pin_info(&pf->hw, &base_rclk_idx,
					&pf->dplls.num_rclk);
	if (ret)
		return ret;

	mutex_lock(&pf->dplls.lock);
	if (!test_bit(ICE_FLAG_DPLL, pf->flags)) {
		ret = ice_dpll_rclk_find_dplls(pf);
		dev_dbg(ice_pf_to_dev(pf), "ecc:%p, pps:%p\n",
			pf->dplls.eec.dpll, pf->dplls.pps.dpll);
		if (ret)
			return ret;
		ret = ice_dpll_rclk_parent_pins_init(pf, base_rclk_idx);
		if (ret)
			return ret;
		first_parent = &pf->dplls.inputs[0];
	} else {
		first_parent = &pf->dplls.inputs[base_rclk_idx];
	}
	if (!first_parent)
		return -EFAULT;
	ret = ice_dpll_rclk_pins_init(pf, first_parent);
	mutex_unlock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf), "PHY RCLK init ret=%d\n", ret);

	return ret;
}

/**
 * ice_dpll_rclk_release - Disable the support for DPLL's PHY clock recovery
 * @pf: board private structure
 *
 * Context:
 * Acquires a pf->dplls.lock. Requires dplls to be present, must be called
 * before dplls are realesed.
 *
 * This function handles the cleanup work of resources allocated for enablement
 * of PHY recovery clock mechanics.
 * Unregisters RCLK pins and frees pin's memory allocated by ice_dpll_rclk_init.
 */
void ice_dpll_rclk_release(struct ice_pf *pf)
{
	int i, ret = 0;

	if (!pf->dplls.rclk)
		return;

	mutex_lock(&pf->dplls.lock);
	for (i = ICE_RCLKA_PIN; i < pf->dplls.num_rclk; i++) {
		if (pf->dplls.rclk[i].pin) {
			dpll_pin_deregister(pf->dplls.eec.dpll,
					    pf->dplls.rclk[i].pin);
			dpll_pin_deregister(pf->dplls.pps.dpll,
					    pf->dplls.rclk[i].pin);
			dpll_pin_free(pf->dplls.rclk[i].pin);
			pf->dplls.rclk[i].pin = NULL;
		}
		if (pf->dplls.rclk[i].attr) {
			dpll_pin_attr_free(pf->dplls.rclk[i].attr);
			pf->dplls.rclk[i].attr = NULL;
		}
		kfree(pf->dplls.rclk[i].name);
		pf->dplls.rclk[i].name = NULL;
	}
	/* inputs were prepared only for RCLK, release them here */
	if (!test_bit(ICE_FLAG_DPLL, pf->flags)) {
		kfree(pf->dplls.inputs);
		pf->dplls.inputs = NULL;
	}
	kfree(pf->dplls.rclk);
	pf->dplls.rclk = NULL;
	mutex_unlock(&pf->dplls.lock);
	dev_dbg(ice_pf_to_dev(pf), "PHY RCLK release ret:%d\n", ret);
}
