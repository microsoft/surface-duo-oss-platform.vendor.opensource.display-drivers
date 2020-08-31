# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifeq (y, $(findstring y, $(CONFIG_ARCH_SM8150) $(CONFIG_ARCH_SM6150) $(CONFIG_ARCH_SDMSHRIKE)))
include $(srctree)/techpack/display/config/augen3disp.conf
LINUXINCLUDE += -include $(srctree)/techpack/display/config/augen3dispconf.h
endif

obj-$(CONFIG_MSM_DRM_TECHPACK) += msm/
obj-$(CONFIG_MSM_DRM_TECHPACK) += rotator/
obj-$(CONFIG_MSM_DRM_TECHPACK) += pll/
obj-$(CONFIG_MSM_DRM_TECHPACK) += msm-lease/
