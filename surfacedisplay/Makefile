# SPDX-License-Identifier: GPL-2.0
ccflags-y := -I$(srctree)/techpack/display/msm/dsi -I$(srctree)/techpack/display/msm
ccflags-y += -I$(srctree)/drivers/clk/qcom/

surfacedisplay-$(CONFIG_SURFACE_DISPLAY) += surface_dsi/surface_panel.o

obj-$(CONFIG_DISPLAY_BUILD)	+= surfacedisplay.o
