/*
 * mtk_kcal_ctrl - sysfs-tunable RGB gain / black-level color calibration
 * for the DISP_GAMMA hardware LUT. Disabled (passthrough) by default;
 * the gamma driver's existing write path is untouched unless enabled.
 */

#ifndef __MTK_KCAL_CTRL_H__
#define __MTK_KCAL_CTRL_H__

#include <linux/types.h>

bool kcal_is_enabled(void);
unsigned int kcal_gamma_entry(unsigned int index, unsigned int lut_size);

#endif
