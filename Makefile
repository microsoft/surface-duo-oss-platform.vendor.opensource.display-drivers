# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifeq (y, $(filter y, $(CONFIG_ARCH_SM8150) $(CONFIG_ARCH_SM6150) $(CONFIG_ARCH_SDMSHRIKE)))
include $(srctree)/techpack/display/config/augen3disp.conf
LINUXINCLUDE += -include $(srctree)/techpack/display/config/augen3dispconf.h
endif

obj-$(CONFIG_DRM_MSM) += msm/
obj-$(CONFIG_MSM_SDE_ROTATOR) += rotator/
obj-$(CONFIG_QCOM_MDSS_PLL) += pll/
obj-$(CONFIG_DRM_MSM_LEASE) += msm-lease/
