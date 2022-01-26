# SPDX-License-Identifier: GPL-2.0
ccflags-y := -I$(srctree)/techpack/display/msm
ccflags-y += -I$(srctree)/techpack/display/msm/dsi
ccflags-y += -I$(srctree)/include/drm
ccflags-y += -I$(srctree)/drivers/clk/qcom/

mtemipiinject-$(CONFIG_DSI_MIPI_INJECT) += src/dsi_mipi_inject.o
mtemipiinject-$(CONFIG_DSI_MIPI_INJECT) += src/qsync_enable.o
mtemipiinject-$(CONFIG_DSI_MIPI_INJECT) += src/mte_panel.o

obj-$(CONFIG_DSI_MIPI_INJECT)	+= mtemipiinject.o
