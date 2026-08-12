/*
 * mtk_kcal_ctrl - sysfs-tunable RGB gain / black-level color calibration,
 * applied to the DISP_GAMMA hardware LUT by mtk_disp_gamma.c.
 *
 * Values are exposed at /sys/module/mediatek_drm/parameters/kcal_* since
 * this file is linked into the mediatek-drm composite module (see Makefile).
 * kcal_enable defaults to 0: the gamma driver's normal write path is
 * completely unmodified until this is explicitly turned on.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "mtk_kcal_ctrl.h"

#define KCAL_10BIT_MAX 1023
#define KCAL_GAIN_UNITY 256

static unsigned int kcal_r = KCAL_GAIN_UNITY;
static unsigned int kcal_g = KCAL_GAIN_UNITY;
static unsigned int kcal_b = KCAL_GAIN_UNITY;
static unsigned int kcal_min;
static unsigned int kcal_enable;
static unsigned int kcal_invert;

static int kcal_gain_set(const char *val, const struct kernel_param *kp)
{
	unsigned int n;
	int ret = kstrtouint(val, 0, &n);

	if (ret)
		return ret;
	if (n > KCAL_GAIN_UNITY)
		return -EINVAL;
	return param_set_uint(val, kp);
}

static const struct kernel_param_ops kcal_gain_ops = {
	.set = kcal_gain_set,
	.get = param_get_uint,
};

static int kcal_10bit_set(const char *val, const struct kernel_param *kp)
{
	unsigned int n;
	int ret = kstrtouint(val, 0, &n);

	if (ret)
		return ret;
	if (n > KCAL_10BIT_MAX)
		return -EINVAL;
	return param_set_uint(val, kp);
}

static const struct kernel_param_ops kcal_10bit_ops = {
	.set = kcal_10bit_set,
	.get = param_get_uint,
};

module_param_cb(kcal_r, &kcal_gain_ops, &kcal_r, 0644);
module_param_cb(kcal_g, &kcal_gain_ops, &kcal_g, 0644);
module_param_cb(kcal_b, &kcal_gain_ops, &kcal_b, 0644);
module_param_cb(kcal_min, &kcal_10bit_ops, &kcal_min, 0644);
module_param(kcal_enable, uint, 0644);
module_param(kcal_invert, uint, 0644);

bool kcal_is_enabled(void)
{
	return kcal_enable != 0;
}

unsigned int kcal_gamma_entry(unsigned int index, unsigned int lut_size)
{
	unsigned int base, r, g, b;

	base = index * KCAL_10BIT_MAX / (lut_size - 1);

	r = min_t(unsigned int, (base * kcal_r) / KCAL_GAIN_UNITY, KCAL_10BIT_MAX);
	g = min_t(unsigned int, (base * kcal_g) / KCAL_GAIN_UNITY, KCAL_10BIT_MAX);
	b = min_t(unsigned int, (base * kcal_b) / KCAL_GAIN_UNITY, KCAL_10BIT_MAX);

	r = max(r, kcal_min);
	g = max(g, kcal_min);
	b = max(b, kcal_min);

	if (kcal_invert) {
		r = KCAL_10BIT_MAX - r;
		g = KCAL_10BIT_MAX - g;
		b = KCAL_10BIT_MAX - b;
	}

	return (r << 20) | (g << 10) | b;
}
