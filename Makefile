# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/display/config/konadisp.conf
endif

ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/konadispconf.h
endif

ifeq ($(CONFIG_ARCH_LAHAINA), y)
     ifeq ($(CONFIG_QGKI), y)
		include $(srctree)/techpack/display/config/lahainadisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/lahainadispconf.h
     else
		include $(srctree)/techpack/display/config/gki_lahainadisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/gki_lahainadispconf.h
     endif
     ifeq ($(CONFIG_LOCALVERSION), "-qgki-debug")
		include $(srctree)/techpack/display/config/lahainadisp_dbg.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/lahainadispconf_dbg.h
     endif
endif

ifeq ($(CONFIG_ARCH_HOLI), y)
     ifeq ($(CONFIG_QGKI), y)
		include $(srctree)/techpack/display/config/holidisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/holidispconf.h
     else
		include $(srctree)/techpack/display/config/gki_holidisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/gki_holidispconf.h
     endif
endif

LINUXINCLUDE    += \
		   -I$(srctree)/techpack/display/include/uapi/display \
		   -I$(srctree)/techpack/display/include
USERINCLUDE     += -I$(srctree)/techpack/display/include/uapi/display

ifeq ($(CONFIG_ARCH_LITO), y)
include $(srctree)/techpack/display/config/saipdisp.conf
endif

ifeq ($(CONFIG_ARCH_LITO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/saipdispconf.h
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(srctree)/techpack/display/config/bengaldisp.conf
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/bengaldispconf.h
endif

#MSCHANGE start
ifeq ($(CONFIG_SURFACE_DISPLAY), y)
LINUXINCLUDE	+= \
			-I$(srctree)/techpack/display/surfacedisplay/include
endif
ifeq ($(CONFIG_DSI_MIPI_INJECT), y)
LINUXINCLUDE    += \
                        -I$(srctree)/techpack/display/mtemipiinject/include
endif
#MSCHANGE end

obj-$(CONFIG_DRM_MSM) += msm/
#MSCHANGE start
obj-$(CONFIG_SURFACE_DISPLAY) += surfacedisplay/
obj-$(CONFIG_DSI_MIPI_INJECT) += mtemipiinject/
#MSCHANGE end
