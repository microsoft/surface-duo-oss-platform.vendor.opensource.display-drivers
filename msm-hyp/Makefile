ccflags-y := -I$(srctree)/include/drm -I$(srctree)/techpack/display/msm-hyp
ccflags-y += -I$(srctree)/techpack/display/include/uapi/display
ccflags-y += -I$(srctree)/techpack/display/include
ccflags-y += -I$(srctree)/techpack/display/msm

msm_hyp-y := \
			msm_drv_hyp.o\
			msm_hyp_notifier.o\
			msm_hyp_utils.o

msm_hyp-$(CONFIG_SYNC_FILE) += \
			msm_hyp_fence.o

msm_hyp-$(CONFIG_DRM_MSM_LEASE) += ../msm-lease/msm_lease_drv.o

obj-$(CONFIG_DRM_MSM_HYP) += msm_hyp.o
