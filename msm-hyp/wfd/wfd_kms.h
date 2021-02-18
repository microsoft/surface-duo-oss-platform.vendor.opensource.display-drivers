/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

/* Copyright © 2006 Keith Packard
 * Copyright © 2007-2008 Dave Airlie
 * Copyright © 2007-2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __WFD_KMS_H__
#define __WFD_KMS_H__

#include "wire_user.h"
#include "wire_format.h"
#include <msm_drv_hyp.h>

#define PANEL_NAME_LEN 13

enum WFD_QDILayerType {
	WFD_QDI_LAYER_NONE		= 0,
	WFD_QDI_LAYER_GRAPHICS,
	WFD_QDI_LAYER_OVERLAY,
	WFD_QDI_LAYER_DMA,
	WFD_QDI_LAYER_CURSOR,
	WFD_QDI_LAYER_MAX,
	WFD_QDI_LAYER_FORCE_32BIT	= 0x7FFFFFFF
};

struct wfd_connector_info_priv {
	struct msm_hyp_connector_info base;
	struct drm_crtc *crtc;
	WFDDevice wfd_device;
	WFDPort wfd_port;
	WFDint wfd_port_id;
	int wfd_port_idx;

	int connector_status;
	uint32_t mode_count;
	struct drm_display_mode *modes;
	char panel_name[PANEL_NAME_LEN];
};

struct wfd_plane_info_priv {
	struct msm_hyp_plane_info base;
	WFDDevice wfd_device;
	WFDPort wfd_port;
	WFDPipeline wfd_pipeline;
	enum WFD_QDILayerType wfd_type;
	bool committed;
};

struct wfd_crtc_info_priv {
	struct msm_hyp_crtc_info base;
	WFDDevice wfd_device;
	WFDPort wfd_port;
	WFDint wfd_port_id;
	int wfd_port_idx;
	bool vblank_enable;
	struct msm_hyp_prop_blob_info extra_info;
};

struct wfd_framebuffer_priv {
	struct msm_hyp_framebuffer_info base;
	WFDDevice wfd_device;
	WFDEGLImage wfd_image;
	WFDint wfd_format;
	WFDint wfd_usage;
	WFDSource wfd_source;
};

struct wfd_kms {
	struct msm_hyp_kms base;
	struct drm_device *dev;
	uint32_t client_id;
	WFDDevice wfd_device[MAX_DEVICE_CNT];
	int wfd_device_cnt;

	WFDPort ports[MAX_PORT_CNT];
	WFDint port_ids[MAX_PORT_CNT];
	WFDDevice port_devs[MAX_PORT_CNT];
	int port_cnt;

	WFDPipeline pipelines[MAX_PORT_CNT][MAX_PIPELINE_CNT];
	int master_idx[MAX_PORT_CNT][MAX_PIPELINE_CNT];
	int pipeline_cnt[MAX_PORT_CNT];

	uint32_t max_sdma_width;
};

#define to_wfd_kms(x)\
	container_of((x), struct wfd_kms, base)

#endif /* __WFD_KMS_H__ */
