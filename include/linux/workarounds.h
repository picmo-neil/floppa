/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _XIMI_WORKAROUNDS_H
#define _XIMI_WORKAROUNDS_H

#include <linux/jump_label.h>
#include <linux/mi_detect.h>

bool is_legacy_timestamp(void);
int is_bpf_spoof_enabled(void);
bool always_warm_reboot(void);
bool msm_perf_disabled(void);
bool is_using_legacy_ir_hal(void);
bool is_modem_dead(void);

/* Fast-path helper for legacy timestamp selection */
extern struct static_key_false legacy_timestamp_key;
static inline bool is_legacy_timestamp_fast(void)
{
	return static_branch_unlikely(&legacy_timestamp_key);
}

bool uses_kernel_dimming(void);
extern struct static_key_true uses_kernel_dimming_key;
static inline bool uses_kernel_dimming_fast(void)
{
	return static_branch_likely(&uses_kernel_dimming_key);
}

extern struct static_key_false device_c3j_key;
extern struct static_key_false device_f9s_key;
void mi_detect_static_branch_init(void);

// Devices
static inline bool is_device_c3j(void)
{
	return mi_is_ginkgo();
}

static inline bool is_device_f9s(void)
{
	return mi_is_laurel();
}

#endif /* _XIMI_WORKAROUNDS_H */
