/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022, Intel Corporation. */

#ifndef _ICE_DPLL_H_
#define _ICE_DPLL_H_

#include "ice.h"

#define ICE_DPLL_PRIO_MAX	0xF
#define ICE_DPLL_RCLK_NUM_MAX	4
/** ice_dpll_pin - store info about pins
 * @pin: dpll pin structure
 * @flags: pin flags returned from HW
 * @idx: ice pin private idx
 * @state: state of a pin
 * @type: type of a pin
 * @freq_mask: mask of supported frequencies
 * @freq: current frequency of a pin
 * @caps: capabilities of a pin
 * @name: pin name
 */
struct ice_dpll_pin {
	struct dpll_pin *pin;
	u8 idx;
	u8 num_parents;
	u8 parent_idx[ICE_DPLL_RCLK_NUM_MAX];
	u8 flags[ICE_DPLL_RCLK_NUM_MAX];
	u8 state[ICE_DPLL_RCLK_NUM_MAX];
	struct dpll_pin_properties prop;
	u32 freq;
};

/** ice_dpll - store info required for DPLL control
 * @dpll: pointer to dpll dev
 * @dpll_idx: index of dpll on the NIC
 * @source_idx: source currently selected
 * @prev_source_idx: source previously selected
 * @ref_state: state of dpll reference signals
 * @eec_mode: eec_mode dpll is configured for
 * @phase_offset: phase delay of a dpll
 * @input_prio: priorities of each input
 * @dpll_state: current dpll sync state
 * @prev_dpll_state: last dpll sync state
 * @active_source: pointer to active source pin
 * @prev_source: pointer to previous active source pin
 */
struct ice_dpll {
	struct dpll_device *dpll;
	int dpll_idx;
	u8 source_idx;
	u8 prev_source_idx;
	u8 ref_state;
	u8 eec_mode;
	s64 phase_offset;
	u8 *input_prio;
	enum ice_cgu_state dpll_state;
	enum ice_cgu_state prev_dpll_state;
	struct dpll_pin *active_source;
	struct dpll_pin *prev_source;
};

/** ice_dplls - store info required for CCU (clock controlling unit)
 * @kworker: periodic worker
 * @work: periodic work
 * @lock: locks access to configuration of a dpll
 * @eec: pointer to EEC dpll dev
 * @pps: pointer to PPS dpll dev
 * @inputs: input pins pointer
 * @outputs: output pins pointer
 * @rclk: recovered pins pointer
 * @num_inputs: number of input pins available on dpll
 * @num_outputs: number of output pins available on dpll
 * @num_rclk: number of recovered clock pins available on dpll
 * @cgu_state_acq_err_num: number of errors returned during periodic work
 */
struct ice_dplls {
	struct kthread_worker *kworker;
	struct kthread_delayed_work work;
	struct mutex lock;
	struct ice_dpll eec;
	struct ice_dpll pps;
	struct ice_dpll_pin *inputs;
	struct ice_dpll_pin *outputs;
	struct ice_dpll_pin rclk;
	u32 num_inputs;
	u32 num_outputs;
	int cgu_state_acq_err_num;
	int lock_err_num;
	u8 base_rclk_idx;
	u64 clock_id;
};

int ice_dpll_init(struct ice_pf *pf);

void ice_dpll_release(struct ice_pf *pf);

int ice_dpll_rclk_init(struct ice_pf *pf);

void ice_dpll_rclk_release(struct ice_pf *pf);

#endif
