# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifeq (y, $(findstring y, $(CONFIG_ARCH_SA8155) $(CONFIG_ARCH_SA6155) $(CONFIG_ARCH_SA8195)))
include $(srctree)/techpack/display/config/augen3disp.conf
LINUXINCLUDE += -include $(srctree)/techpack/display/config/augen3dispconf.h
LINUXINCLUDE += -I$(srctree)/techpack/display/include \
	-I$(srctree)/techpack/display/include/uapi/display

USERINCLUDE = -I$(srctree)/techpack/display/include/uapi/display
endif

ifeq (y, $(findstring y, $(CONFIG_QTI_QUIN_GVM)))
include $(srctree)/techpack/display/config/gvmgen3disp.conf
LINUXINCLUDE += -include $(srctree)/techpack/display/config/gvmgen3dispconf.h
endif

obj-$(CONFIG_DRM_MSM) += msm/
obj-$(CONFIG_DRM_MSM_HYP) += msm-hyp/
obj-$(CONFIG_MSM_SDE_ROTATOR) += rotator/
obj-$(CONFIG_DRM_ANALOGIX_ANX7625_TECHPACK) += bridge/analogix-anx7625.o
