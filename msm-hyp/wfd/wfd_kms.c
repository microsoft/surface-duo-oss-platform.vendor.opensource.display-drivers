/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

/* Copyright (C) 2014 Red Hat
 * Copyright (C) 2014 Intel Corp.
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
 *
 * Authors:
 * Rob Clark <robdclark@gmail.com>
 * Daniel Vetter <daniel.vetter@ffwll.ch>
 */

/*
 * Copyright (c) 2006-2008 Intel Corporation
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 * Copyright (c) 2008 Red Hat Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *      Keith Packard
 *      Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */

/*
 * Copyright © 1997-2003 by The XFree86 Project, Inc.
 * Copyright © 2007 Dave Airlie
 * Copyright © 2007-2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 * Copyright 2005-2006 Luc Verhaegen
 * Copyright (c) 2001, Andy Ritger  aritger@nvidia.com
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
 *
 * Except as contained in this notice, the name of the copyright holder(s)
 * and author(s) shall not be used in advertising or otherwise to promote
 * the sale, use or other dealings in this Software without prior written
 * authorization from the copyright holder(s) and author(s).
 */

/* Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

#include <linux/sort.h>
#include <drm/drm_atomic.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include "msm_hyp_utils.h"
#include "wfd_kms.h"

#define MAX_RECTS_PER_PIPE     2
#define MASTER_PIPE_IDX        0
#define CLIENT_ID_LEN_IN_CHARS 5
#define MAX_MDP_CLK_KHZ        412500
#define MAX_HORZ_DECIMATION    4
#define MAX_VERT_DECIMATION    4
#define SSPP_UNITY_SCALE       1
#define MAX_RECTS_PER_PIPE     2
#define MAX_NUM_LIMIT_PAIRS    16

struct limit_val_pair {
	const char *str;
	uint32_t val;
};

struct limit_constraints {
	uint32_t sdma_width;
	struct limit_val_pair pairs[MAX_NUM_LIMIT_PAIRS];
};

static struct limit_constraints constraints_table[] = {
	{
		/* SA6155 */
		1080,
		{
			{"sspp_linewidth_usecases", 3},
			{"vig",   0x1},
			{"dma",   0x2},
			{"scale", 0x4},
			{"sspp_linewidth_values", 3},
			{"limit_usecase", 0x1},
			{"limit_value",  2160},
			{"limit_usecase", 0x5},
			{"limit_value",  2160},
			{"limit_usecase", 0x2},
			{"limit_value",  2160},
		}
	},
	{
		/* SA8155/SA8195 */
		2048,
		{
			{"sspp_linewidth_usecases", 3},
			{"vig",   0x1},
			{"dma",   0x2},
			{"scale", 0x4},
			{"sspp_linewidth_values", 3},
			{"limit_usecase", 0x1},
			{"limit_value",  2560},
			{"limit_usecase", 0x5},
			{"limit_value",  2560},
			{"limit_usecase", 0x2},
			{"limit_value",  4096},
		}
	},
	{
		/* SA8295 */
		2560,
		{
			{"sspp_linewidth_usecases", 3},
			{"vig",   0x1},
			{"dma",   0x2},
			{"scale", 0x4},
			{"sspp_linewidth_values", 3},
			{"limit_usecase", 0x1},
			{"limit_value",  2560},
			{"limit_usecase", 0x5},
			{"limit_value",  2560},
			{"limit_usecase", 0x2},
			{"limit_value",  5120},
		}
	},
};

static const char * const disp_order_str[] = {
	"primary",
	"secondary",
	"tertiary",
	"quaternary",
	"quinary",
	"senary",
	"septenary",
	"octonary",
};

static int _wfd_kms_parse_dt(struct device_node *node, u32 *client_id)
{
	int len = 0;
	int ret = 0;
	const char *client_id_str;

	client_id_str = of_get_property(node, "qcom,client-id", &len);
	if (!client_id_str || len != CLIENT_ID_LEN_IN_CHARS) {
		pr_err("client_id_str len(%d) is invalid\n", len);
		ret = -EINVAL;
	} else {
		/* Try node as a hex value */
		ret = kstrtouint(client_id_str, 16, client_id);
		if (ret) {
			/* Otherwise, treat at 4cc code */
			*client_id = fourcc_code(client_id_str[0],
					client_id_str[1],
					client_id_str[2],
					client_id_str[3]);

			ret = 0;
		}
	}

	return ret;
}

static int _wfd_kms_connector_get_type(WFDDevice dev,
		WFDPort port, WFDint port_id, char *name)
{
	WFDint port_type;
	int connector_type;

	port_type = wfdGetPortAttribi_User(
			dev,
			port,
			WFD_PORT_TYPE);

	switch (port_type) {
	case WFD_PORT_TYPE_INTERNAL:
	case WFD_PORT_TYPE_HDMI:
		connector_type = DRM_MODE_CONNECTOR_HDMIA;
		snprintf(name, PANEL_NAME_LEN, "%s_%d", "HDMI", port_id);
		break;
	case WFD_PORT_TYPE_DSI:
		connector_type = DRM_MODE_CONNECTOR_DSI;
		snprintf(name, PANEL_NAME_LEN, "%s_%d", "DSI", port_id);
		break;
	case WFD_PORT_TYPE_DISPLAYPORT:
		connector_type = DRM_MODE_CONNECTOR_DisplayPort;
		snprintf(name, PANEL_NAME_LEN, "%s_%d", "DP", port_id);
		break;
	default:
		connector_type = DRM_MODE_CONNECTOR_Unknown;
		snprintf(name, PANEL_NAME_LEN, "%s_%d", "Unknown", port_id);
		break;
	}

	pr_debug("%s - port_type = %x name = %s\n", __func__, port_type, name);

	return connector_type;
}

static int _wfd_kms_plane_get_format(struct wfd_plane_info_priv *priv)
{
	int i, ret = 0;
	int format_count = 0;
	WFDint reported_format_count = 0;
	WFDint formats[MAX_PIPELINE_ATTRIBS];

	reported_format_count = wfdGetPipelineAttribi_User(
			priv->wfd_device,
			priv->wfd_pipeline,
			WFD_PIPELINE_PIXEL_FORMATS_COUNT);

	if (reported_format_count < 1) {
		ret = -EINVAL;
		goto fail;
	}

	if (reported_format_count > MAX_PIPELINE_ATTRIBS)
		reported_format_count = MAX_PIPELINE_ATTRIBS;

	memset(formats, 0, MAX_PIPELINE_ATTRIBS * sizeof(WFDint));
	wfdGetPipelineAttribiv_User(priv->wfd_device,
				priv->wfd_pipeline,
				WFD_PIPELINE_PIXEL_FORMATS,
				reported_format_count,
				formats);

	for (i = 0; i < reported_format_count; i++) {
		if (formats[i]) {
			format_count++;
			pr_debug("%s - formats[%d] = %d\n",
					__func__, i, formats[i]);
		} else
			break;
	}

	if (format_count < 1) {
		ret = -EINVAL;
		goto fail;
	}

	priv->base.format_count = format_count;
	priv->base.format_types = kcalloc(format_count, sizeof(uint32_t),
			GFP_KERNEL);
	if (priv->base.format_types == NULL) {
		ret = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < format_count; i++)
		switch (formats[i]) {
		case WFD_FORMAT_BYTE:
			break;
		case WFD_FORMAT_RGBA4444:
			priv->base.format_types[i] = DRM_FORMAT_ARGB4444;
			break;
		case WFD_FORMAT_RGBX4444:
			priv->base.format_types[i] = DRM_FORMAT_XRGB4444;
			break;
		case WFD_FORMAT_RGBA5551:
			priv->base.format_types[i] = DRM_FORMAT_ARGB1555;
			break;
		case WFD_FORMAT_RGBX5551:
			priv->base.format_types[i] = DRM_FORMAT_XRGB1555;
			break;
		case WFD_FORMAT_RGB565:
			priv->base.format_types[i] = DRM_FORMAT_RGB565;
			break;
		case WFD_FORMAT_RGB888:
			priv->base.format_types[i] = DRM_FORMAT_RGB888;
			break;
		case WFD_FORMAT_RGBA8888:
			priv->base.format_types[i] = DRM_FORMAT_ARGB8888;
			break;
		case WFD_FORMAT_RGBX8888:
			priv->base.format_types[i] = DRM_FORMAT_XRGB8888;
			break;
		case WFD_FORMAT_YVU9:
			priv->base.format_types[i] = DRM_FORMAT_YVU410;
			break;
		case WFD_FORMAT_YUV420:
			priv->base.format_types[i] = DRM_FORMAT_YUV420;
			break;
		case WFD_FORMAT_NV12:
			priv->base.format_types[i] = DRM_FORMAT_NV12;
			break;
		case WFD_FORMAT_YV12:
			priv->base.format_types[i] = DRM_FORMAT_YVU420;
			break;
		case WFD_FORMAT_UYVY:
			priv->base.format_types[i] = DRM_FORMAT_UYVY;
			break;
		case WFD_FORMAT_YUY2:
			priv->base.format_types[i] = DRM_FORMAT_YUYV;
			break;
		case WFD_FORMAT_YVYU:
			priv->base.format_types[i] = DRM_FORMAT_YVYU;
			break;
		case WFD_FORMAT_V422:
			priv->base.format_types[i] = DRM_FORMAT_VYUY;
			break;
		case WFD_FORMAT_AYUV:
			priv->base.format_types[i] = DRM_FORMAT_AYUV;
			break;
		case WFD_FORMAT_P010:
			priv->base.format_types[i] = DRM_FORMAT_NV12;
			break;
		case WFD_FORMAT_TP10:
			priv->base.format_types[i] = DRM_FORMAT_NV12;
			break;
		case WFD_FORMAT_BGRA8888:
			priv->base.format_types[i] = DRM_FORMAT_ABGR8888;
			break;
		case WFD_FORMAT_BGRX8888:
			priv->base.format_types[i] = DRM_FORMAT_XBGR8888;
			break;
		case WFD_FORMAT_BGR565:
			priv->base.format_types[i] = DRM_FORMAT_BGR565;
			break;
		case WFD_FORMAT_RGBA1010102:
			priv->base.format_types[i] = DRM_FORMAT_ARGB2101010;
			break;
		case WFD_FORMAT_RGBX1010102:
			priv->base.format_types[i] = DRM_FORMAT_XRGB2101010;
			break;
		case WFD_FORMAT_BGRA1010102:
			priv->base.format_types[i] = DRM_FORMAT_ABGR2101010;
			break;
		case WFD_FORMAT_BGRX1010102:
			priv->base.format_types[i] = DRM_FORMAT_XBGR2101010;
			break;
		default:
			pr_debug("%s - formats[%d] = %d is not supported!\n",
				__func__, i, formats[i]);
			break;
	}
fail:
	return ret;
}

static bool _wfd_kms_plane_is_rect_changed(struct drm_plane_state *pre,
	struct drm_plane_state *cur, bool src)
{
	bool ret = false;

	if (src) {
		if ((pre->src_x != cur->src_x) ||
			(pre->src_y != cur->src_y) ||
			(pre->src_w != cur->src_w) ||
			(pre->src_h != cur->src_h))
			ret = true;
	} else {
		if ((pre->crtc_x != cur->crtc_x) ||
			(pre->crtc_y != cur->crtc_y) ||
			(pre->crtc_w != cur->crtc_w) ||
			(pre->crtc_h != cur->crtc_h))
			ret = true;
	}

	return ret;
}

static bool _wfd_kms_plane_is_csc_matrix_changed(
		struct msm_hyp_plane_state *pre,
		struct msm_hyp_plane_state *cur, WFDint *color_space)
{
	bool ret = false;

	/*
	 * The ctm_coeff[4] value is unique for each CSC matrix. We can use
	 * this to identify the color space associated with each matrix. The
	 * index of each element corresponds to the associated WFD color space
	 * enum value.
	 */
	static const int64_t msm_hyp_csc_unique_coeffs[] = {
		0x0,		/* WFD_COLOR_SPACE_UNCORRECTED */
		0x0,		/* WFD_COLOR_SPACE_SRGB */
		0x0,		/* WFD_COLOR_SPACE_LRGB */
		0x7F9B800000,	/* WFD_COLOR_SPACE_BT601 */
		0x7fa8000000,	/* WFD_COLOR_SPACE_BT601_FULL */
		0x7fc9800000,	/* WFD_COLOR_SPACE_BT709 */
		0x0		/* WFD_COLOR_SPACE_BT709_FULL */
	};

	/* ctm_coeff[4] is unique for each matrix */
	uint32_t unique_coeff_idx = 4;

	if (pre && cur) {
		/*
		 * Do not need to compare the entire matrix. It should be
		 * sufficient to only check the uniqe coefficient.
		 */
		if (pre->csc.ctm_coeff[unique_coeff_idx] !=
			cur->csc.ctm_coeff[unique_coeff_idx])
			ret = true;
	}

	if (color_space && ret) {
		if (msm_hyp_csc_unique_coeffs[WFD_COLOR_SPACE_BT601] ==
				cur->csc.ctm_coeff[unique_coeff_idx])
			*color_space = WFD_COLOR_SPACE_BT601;
		else if (msm_hyp_csc_unique_coeffs[WFD_COLOR_SPACE_BT601_FULL]
				== cur->csc.ctm_coeff[unique_coeff_idx])
			*color_space = WFD_COLOR_SPACE_BT601_FULL;
		else if (msm_hyp_csc_unique_coeffs[WFD_COLOR_SPACE_BT709] ==
				cur->csc.ctm_coeff[unique_coeff_idx])
			*color_space = WFD_COLOR_SPACE_BT709;
		else
			*color_space = WFD_COLOR_SPACE_BT601;
	}

	return ret;
}

static int _wfd_kms_format_to_openwfd_format(uint32_t format,
		uint64_t modifier, WFDint *wfd_format, WFDint *wfd_usage)
{
	if ((modifier & DRM_FORMAT_MOD_QTI_COMPRESSED) ==
			DRM_FORMAT_MOD_QTI_COMPRESSED)
		*wfd_usage = WFD_USAGE_DISPLAY | WFD_USAGE_COMPRESSION;
	else
		*wfd_usage = WFD_USAGE_DISPLAY;

	switch (format) {
	case DRM_FORMAT_ARGB4444:
		*wfd_format = WFD_FORMAT_RGBA4444;
		break;
	case DRM_FORMAT_XRGB4444:
		*wfd_format = WFD_FORMAT_RGBX4444;
		break;
	case DRM_FORMAT_ARGB1555:
		*wfd_format = WFD_FORMAT_RGBA5551;
		break;
	case DRM_FORMAT_XRGB1555:
		*wfd_format = WFD_FORMAT_RGBX5551;
		break;
	case DRM_FORMAT_RGB565:
		*wfd_format = WFD_FORMAT_RGB565;
		break;
	case DRM_FORMAT_BGR565:
		if (*wfd_usage & WFD_USAGE_COMPRESSION)
			*wfd_format = WFD_FORMAT_RGB565;
		else
			*wfd_format = WFD_FORMAT_BGR565;
		break;
	case DRM_FORMAT_RGB888:
		*wfd_format = WFD_FORMAT_RGB888;
		break;
	case DRM_FORMAT_ARGB8888:
		*wfd_format = WFD_FORMAT_RGBA8888;
		break;
	case DRM_FORMAT_XRGB8888:
		*wfd_format = WFD_FORMAT_RGBX8888;
		break;
	case DRM_FORMAT_XBGR8888:
		if (*wfd_usage & WFD_USAGE_COMPRESSION)
			*wfd_format = WFD_FORMAT_RGBA8888;
		else
			*wfd_format = WFD_FORMAT_BGRX8888;
		break;
	case DRM_FORMAT_ABGR8888:
		if (*wfd_usage & WFD_USAGE_COMPRESSION)
			*wfd_format = WFD_FORMAT_RGBA8888;
		else
			*wfd_format = WFD_FORMAT_BGRA8888;
		break;
	case DRM_FORMAT_ARGB2101010:
		*wfd_format = WFD_FORMAT_RGBA1010102;
		break;
	case DRM_FORMAT_XRGB2101010:
		*wfd_format = WFD_FORMAT_RGBX1010102;
		break;
	case DRM_FORMAT_XBGR2101010:
		if (*wfd_usage & WFD_USAGE_COMPRESSION)
			*wfd_format = WFD_FORMAT_RGBA1010102;
		else
			*wfd_format = WFD_FORMAT_BGRX1010102;
		break;
	case DRM_FORMAT_ABGR2101010:
		if (*wfd_usage & WFD_USAGE_COMPRESSION)
			*wfd_format = WFD_FORMAT_RGBA1010102;
		else
			*wfd_format = WFD_FORMAT_BGRA1010102;
		break;
	case DRM_FORMAT_YVU410:
		*wfd_format = WFD_FORMAT_YVU9;
		break;
	case DRM_FORMAT_YUV420:
		*wfd_format = WFD_FORMAT_YUV420;
		break;
	case DRM_FORMAT_NV12:
		if ((modifier & fourcc_mod_code(QTI, 0x7)) ==
				fourcc_mod_code(QTI, 0x7))
			*wfd_format = WFD_FORMAT_TP10;
		else if ((modifier & fourcc_mod_code(QTI, 0x2)) ==
				fourcc_mod_code(QTI, 0x2))
			*wfd_format = WFD_FORMAT_P010;
		else
			*wfd_format = WFD_FORMAT_NV12;
		break;
	case DRM_FORMAT_YVU420:
		*wfd_format = WFD_FORMAT_YV12;
		break;
	case DRM_FORMAT_UYVY:
		*wfd_format = WFD_FORMAT_UYVY;
		break;
	case DRM_FORMAT_YUYV:
		*wfd_format = WFD_FORMAT_YUY2;
		break;
	case DRM_FORMAT_YVYU:
		*wfd_format = WFD_FORMAT_YVYU;
		break;
	case DRM_FORMAT_VYUY:
		*wfd_format = WFD_FORMAT_V422;
		break;
	case DRM_FORMAT_AYUV:
		*wfd_format = WFD_FORMAT_AYUV;
		break;
	default:
		*wfd_format = WFD_FORMAT_RGBA8888;
		break;
	}

	return 0;
}

static void wfd_kms_destroy_framebuffer(struct drm_framebuffer *framebuffer)
{
	struct msm_hyp_framebuffer *fb = to_msm_hyp_fb(framebuffer);
	struct wfd_framebuffer_priv *fb_priv = container_of(fb->info,
				struct wfd_framebuffer_priv, base);

	if (!fb->info)
		return;

	if (fb_priv->wfd_source != WFD_INVALID_HANDLE)
		wfdDestroySource_User(fb_priv->wfd_device,
			fb_priv->wfd_source);

	if (fb_priv->wfd_image != WFD_INVALID_HANDLE)
		wfdDestroyWFDEGLImages_User(
			fb_priv->wfd_device,
			1, &fb_priv->wfd_image, NULL);

	kfree(fb_priv);
	fb->info = NULL;
}

static int _wfd_kms_create_image(struct msm_hyp_framebuffer *fb)
{
	struct wfd_framebuffer_priv *fb_priv = container_of(fb->info,
				struct wfd_framebuffer_priv, base);
	WFDErrorCode wfd_err;
	struct dma_buf *dma_buf;
	int ret = 0;

	if (fb_priv->wfd_image)
		return 0;

	if (!fb->bo) {
		pr_err("no bo attached to fb\n");
		return -EINVAL;
	}

	dma_buf = fb->bo->dma_buf;

	if (!dma_buf) {
		mutex_lock(&fb->base.dev->object_name_lock);

		dma_buf = drm_gem_prime_export(fb->bo, 0);
		if (IS_ERR(dma_buf)) {
			ret = PTR_ERR(dma_buf);
			dma_buf = NULL;
		} else {
			fb->bo->dma_buf = dma_buf;
			get_dma_buf(dma_buf);
		}

		mutex_unlock(&fb->base.dev->object_name_lock);

		if (!dma_buf) {
			pr_err("failed to create dma buf\n");
			return ret;
		}
	}

	wfd_err = wfdCreateWFDEGLImagesPreAlloc_User(
			fb_priv->wfd_device,
			fb->base.width,
			fb->base.height,
			fb_priv->wfd_format,
			fb_priv->wfd_usage,
			1,
			fb->bo->size,
			&fb_priv->wfd_image,
			(void **)&dma_buf,
			fb->base.pitches,
			fb->base.offsets,
			0x00);
	if (wfd_err != WFD_ERROR_NONE) {
		pr_err("failed to create wfd image\n");
		ret = -EINVAL;
	}

	return ret;
}

static int wfd_kms_get_framebuffer_info(struct msm_hyp_kms *kms,
		struct drm_framebuffer *framebuffer,
		struct msm_hyp_framebuffer_info **fb_info)
{
	struct wfd_framebuffer_priv *fb_priv;
	WFDint wfd_format, wfd_usage;
	int ret;

	ret = _wfd_kms_format_to_openwfd_format(framebuffer->format->format,
			framebuffer->modifier, &wfd_format, &wfd_usage);
	if (ret)
		return ret;

	fb_priv = kzalloc(sizeof(*fb_priv), GFP_KERNEL);
	if (!fb_priv)
		return -ENOMEM;

	fb_priv->base.destroy = wfd_kms_destroy_framebuffer;
	fb_priv->wfd_format = wfd_format;
	fb_priv->wfd_usage = wfd_usage;
	*fb_info = &fb_priv->base;

	return 0;
}

static void _wfd_kms_pipeline_init(struct wfd_kms *kms,
		WFDDevice dev, WFDPort port, int port_idx)
{
	WFDint pipe_ids[MAX_PIPELINE_ATTRIBS];
	WFDint pipe_id;
	WFDPipeline pipeline, master_pipeline;
	int i, j, num_pipeline, pipeline_idx;

	num_pipeline = wfdGetPortAttribi_User(
			dev,
			port,
			WFD_PORT_PIPELINE_ID_COUNT);

	if (num_pipeline <= 0)
		return;

	wfdGetPortAttribiv_User(dev,
		port, WFD_PORT_BINDABLE_PIPELINE_IDS,
		num_pipeline, pipe_ids);

	for (i = num_pipeline - 1; i >= 0; i--) {
		master_pipeline = WFD_INVALID_PIPELINE_ID;
		for (j = 0; j < MAX_RECTS_PER_PIPE; j++) {
			if (j == MASTER_PIPE_IDX) {
				pipe_id = pipe_ids[i];
			} else if (master_pipeline) {
				pipe_id = wfdGetPipelineAttribi_User(
					dev,
					master_pipeline,
					WFD_PIPELINE_VIRTUAL_PIPE_ID);
			}

			if (pipe_id == WFD_INVALID_PIPELINE_ID)
				continue;

			pipeline = wfdCreatePipeline_User(
					dev,
					pipe_id, NULL);
			if (pipeline == WFD_INVALID_HANDLE)
				continue;

			pipeline_idx = kms->pipeline_cnt[port_idx];
			kms->pipelines[port_idx][pipeline_idx] = pipeline;

			if (master_pipeline)
				kms->master_idx[port_idx][pipeline_idx] =
						pipeline_idx - 1;
			else
				kms->master_idx[port_idx][pipeline_idx] = -1;

			kms->pipeline_cnt[port_idx]++;

			if (j == MASTER_PIPE_IDX)
				master_pipeline = pipeline;
		}
	}
}

static int _wfd_kms_hw_init(struct wfd_kms *kms)
{
	WFDint wfd_ids[MAX_DEVICE_CNT];
	WFDint num_dev = 0;
	WFDDevice wfd_dev = WFD_INVALID_HANDLE;
	WFDint attribs[3];
	WFDint wfd_port_ids[MAX_PORT_CNT];
	WFDPort port;
	int i, j, num_port, port_idx;
	int rc;

	attribs[0] = WFD_DEVICE_CLIENT_TYPE;
	attribs[1] = kms->client_id;
	attribs[2] = WFD_NONE;

	rc = wire_user_init(kms->client_id, WIRE_INIT_EVENT_SUPPORT);
	if (rc) {
		pr_err("failed to init wire user for client %x\n", kms->client_id);
		return rc;
	}

	/* open a open WFD device */
	num_dev = wfdEnumerateDevices_User(NULL, 0, attribs);
	if (!num_dev) {
		pr_err("wfdEnumerateDevices_User - failed!\n");
		return -ENODEV;
	}

	wfdEnumerateDevices_User(wfd_ids, num_dev, attribs);

	for (j = 0; j < num_dev; j++) {
		wfd_dev = wfdCreateDevice_User(wfd_ids[j], attribs);
		if (wfd_dev == WFD_INVALID_HANDLE) {
			pr_debug("wfdCreateDevice_User - failed\n");
			continue;
		}

		kms->wfd_device[kms->wfd_device_cnt] = wfd_dev;
		kms->wfd_device_cnt++;

		num_port = wfdEnumeratePorts_User(wfd_dev, NULL, 0, NULL);

		wfdEnumeratePorts_User(wfd_dev, wfd_port_ids, num_port, NULL);

		for (i = 0; i < num_port; i++) {
			port = wfdCreatePort_User(
					wfd_dev, wfd_port_ids[i], NULL);
			if (port == WFD_INVALID_HANDLE)
				continue;

			port_idx = kms->port_cnt;
			kms->ports[port_idx] = port;
			kms->port_ids[port_idx] = wfd_port_ids[i];
			kms->port_devs[port_idx] = wfd_dev;
			kms->port_cnt++;

			_wfd_kms_pipeline_init(kms, wfd_dev, port, port_idx);
		}
	}

	if (!kms->wfd_device_cnt) {
		pr_err("can't find valid WFD device\n");
		return -ENODEV;
	}

	return 0;
}

static int wfd_kms_connector_detect_ctx(struct drm_connector *connector,
			  struct drm_modeset_acquire_ctx *ctx,
			  bool force)
{
	struct msm_hyp_connector *c = to_msm_hyp_connector(connector);
	struct wfd_connector_info_priv *priv = container_of(c->info,
			struct wfd_connector_info_priv, base);

	return priv->connector_status;
}

static int wfd_kms_connector_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *m;
	struct msm_hyp_connector *c_conn = to_msm_hyp_connector(connector);
	struct wfd_connector_info_priv *priv = container_of(c_conn->info,
			struct wfd_connector_info_priv, base);
	uint32_t i;

	for (i = 0; i < priv->mode_count; i++) {
		m = drm_mode_duplicate(connector->dev,
				&priv->modes[i]);
		if (!m)
			return i;
		drm_mode_probed_add(connector, m);
	}

	msm_hyp_connector_init_edid(connector, priv->panel_name);

	return priv->mode_count;
}

static struct drm_encoder *
wfd_kms_connector_best_encoder(struct drm_connector *connector)
{
	struct msm_hyp_connector *c_conn = to_msm_hyp_connector(connector);

	return &c_conn->encoder;
}

static const struct drm_connector_helper_funcs wfd_connector_helper_funcs = {
	.detect_ctx = wfd_kms_connector_detect_ctx,
	.get_modes = wfd_kms_connector_get_modes,
	.best_encoder = wfd_kms_connector_best_encoder,
};

static void wfd_kms_bridge_mode_set(struct drm_bridge *drm_bridge,
		const struct drm_display_mode *mode,
		const struct drm_display_mode *adjusted_mode)
{
	struct msm_hyp_connector *connector = container_of(drm_bridge,
			struct msm_hyp_connector, bridge);
	struct wfd_connector_info_priv *priv = container_of(connector->info,
			struct wfd_connector_info_priv, base);
	WFDPortMode wfd_port_mode = WFD_INVALID_HANDLE;
	int i;

	for (i = 0; i < priv->mode_count; i++) {
		mode = &priv->modes[i];
		if ((adjusted_mode->hdisplay == mode->hdisplay) &&
		    (adjusted_mode->vdisplay == mode->vdisplay) &&
		    (adjusted_mode->vrefresh == mode->vrefresh)) {
			wfd_port_mode = (WFDPortMode)mode->private;
			break;
		}
	}

	wfdSetPortMode_User(priv->wfd_device, priv->wfd_port,
			wfd_port_mode);
}

static void wfd_kms_bridge_enable(struct drm_bridge *drm_bridge)
{
	struct msm_hyp_connector *connector = container_of(drm_bridge,
			struct msm_hyp_connector, bridge);
	struct wfd_connector_info_priv *priv = container_of(connector->info,
			struct wfd_connector_info_priv, base);

	wfdSetPortAttribi_User(priv->wfd_device,
			priv->wfd_port,
			WFD_PORT_POWER_MODE,
			WFD_POWER_MODE_ON);
}

static void wfd_kms_bridge_disable(struct drm_bridge *drm_bridge)
{
	struct msm_hyp_connector *connector = container_of(drm_bridge,
			struct msm_hyp_connector, bridge);
	struct wfd_connector_info_priv *priv = container_of(connector->info,
			struct wfd_connector_info_priv, base);

	wfdSetPortAttribi_User(priv->wfd_device,
			priv->wfd_port,
			WFD_PORT_POWER_MODE,
			WFD_POWER_MODE_OFF);

	wfdSetPortMode_User(priv->wfd_device,
			priv->wfd_port,
			WFD_INVALID_HANDLE);
}

static const struct drm_bridge_funcs wfd_bridge_ops = {
	.enable       = wfd_kms_bridge_enable,
	.disable      = wfd_kms_bridge_disable,
	.mode_set     = wfd_kms_bridge_mode_set,
};

static int wfd_kms_get_connector_infos(struct msm_hyp_kms *kms,
		struct msm_hyp_connector_info **connector_infos,
		int *connector_num)
{
	struct wfd_kms *wfd_kms = to_wfd_kms(kms);
	struct drm_device *ddev = wfd_kms->dev;
	struct wfd_connector_info_priv *priv;
	struct drm_display_mode *mode;
	WFDint data[4];
	WFDint port_connected;
	WFDint host_cap;
	WFDfloat physical_size[2];
	WFDPortMode port_mode[MAX_PORT_MODES_CNT];
	int num_mode;
	int i, j, ret = 0;

	if (!connector_infos) {
		*connector_num = wfd_kms->port_cnt;
		return 0;
	}

	wfdGetDeviceAttribiv_User(wfd_kms->wfd_device[0],
		WFD_DEVICE_MIN_MAX_WIDTH_HEIGHT, 4, data);

	ddev->mode_config.min_width = data[0];
	ddev->mode_config.max_width = data[1];
	ddev->mode_config.min_height = data[2];
	ddev->mode_config.max_height = data[3];

	host_cap = wfdGetDeviceAttribi_User(wfd_kms->wfd_device[0],
			WFD_DEVICE_HOST_CAPABILITIES);
	wire_user_set_host_capabilities(wfd_kms->wfd_device[0], host_cap);

	for (i = 0; i < wfd_kms->port_cnt; i++) {
		priv = kzalloc(sizeof(*priv), GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		priv->wfd_device = wfd_kms->port_devs[i];
		priv->wfd_port = wfd_kms->ports[i];
		priv->wfd_port_id = wfd_kms->port_ids[i];
		priv->wfd_port_idx = i;

		priv->base.connector_type = _wfd_kms_connector_get_type(
				priv->wfd_device,
				priv->wfd_port, priv->wfd_port_id,
				priv->panel_name);

		port_connected = wfdGetPortAttribi_User(
				priv->wfd_device,
				priv->wfd_port,
				WFD_PORT_ATTACHED);

		priv->connector_status = port_connected ?
				connector_status_connected :
				connector_status_disconnected;

		wfdGetPortAttribfv_User(priv->wfd_device,
					priv->wfd_port,
					WFD_PORT_PHYSICAL_SIZE,
					2, physical_size);

		priv->base.display_info.width_mm =
				(uint32_t)physical_size[0];
		priv->base.display_info.height_mm =
				(uint32_t)physical_size[1];

		priv->base.possible_crtcs = 1 << i;

		num_mode = wfdGetPortModes_User(priv->wfd_device,
						priv->wfd_port,
						0, 0);
		if (!num_mode) {
			ret = -EINVAL;
			break;
		}

		priv->mode_count = num_mode;
		wfdGetPortModes_User(priv->wfd_device,
					priv->wfd_port,
					port_mode,
					num_mode);

		if (num_mode > 0) {
			priv->modes = kcalloc(num_mode,
					sizeof(struct drm_display_mode),
					GFP_KERNEL);
			if (!priv->modes) {
				ret = -ENOMEM;
				break;
			}
		}

		for (j = 0; j < num_mode; j++) {
			mode = &priv->modes[j];
			mode->hdisplay =
					wfdGetPortModeAttribi_User(
						priv->wfd_device,
						priv->wfd_port,
						port_mode[j],
						WFD_PORT_MODE_WIDTH);
			mode->vdisplay =
					wfdGetPortModeAttribi_User(
						priv->wfd_device,
						priv->wfd_port,
						port_mode[j],
						WFD_PORT_MODE_HEIGHT);
			mode->vrefresh =
					wfdGetPortModeAttribi_User(
						priv->wfd_device,
						priv->wfd_port,
						port_mode[j],
						WFD_PORT_MODE_REFRESH_RATE);
			mode->private = (int *)port_mode[j];
			mode->hsync_end = mode->hdisplay;
			mode->htotal = mode->hdisplay;
			mode->hsync_start = mode->hdisplay;
			mode->vsync_end = mode->vdisplay;
			mode->vtotal = mode->vdisplay;
			mode->vsync_start = mode->vdisplay;
			mode->clock = mode->vrefresh *
					mode->vtotal *
					mode->htotal /
					1000LL;
			drm_mode_set_name(mode);
		}

		if (i < ARRAY_SIZE(disp_order_str))
			priv->base.display_type = disp_order_str[i];

		priv->base.connector_funcs = &wfd_connector_helper_funcs;
		priv->base.bridge_funcs = &wfd_bridge_ops;
		connector_infos[i] = &priv->base;
	}

	return ret;
}

static void _wfd_kms_set_crtc_limit(struct wfd_kms *wfd_kms,
		struct wfd_crtc_info_priv *crtc_priv)
{
	struct limit_constraints *constraints = NULL;
	struct limit_val_pair *pair;
	char buf[16];
	int i;

	for (i = 0; i < ARRAY_SIZE(constraints_table); i++) {
		if (constraints_table[i].sdma_width == wfd_kms->max_sdma_width) {
			constraints = &constraints_table[i];
			break;
		}
	}

	if (!constraints)
		return;

	for (i = 0; i < MAX_NUM_LIMIT_PAIRS; i++) {
		pair = &constraints->pairs[i];

		if (!pair->str)
			break;

		snprintf(buf, sizeof(buf), "%d", pair->val);
		msm_hyp_prop_info_add_keystr(&crtc_priv->extra_info,
				pair->str, buf);
	}

	crtc_priv->base.extra_caps = crtc_priv->extra_info.data;
}

static int wfd_kms_get_crtc_infos(struct msm_hyp_kms *kms,
		struct msm_hyp_crtc_info **crtc_infos,
		int *crtc_num)
{
	struct wfd_kms *wfd_kms = to_wfd_kms(kms);
	struct wfd_crtc_info_priv *priv;
	int pipe_cnt = 0;
	int i, j, ret;

	if (!kms || !crtc_num)
		return -EINVAL;

	if (!crtc_infos) {
		*crtc_num = wfd_kms->port_cnt;
		return 0;
	}

	for (i = 0; i < wfd_kms->port_cnt; i++) {
		priv = kzalloc(sizeof(*priv), GFP_KERNEL);
		if (priv == NULL) {
			ret = -ENOMEM;
			break;
		}

		priv->wfd_device = wfd_kms->port_devs[i];
		priv->wfd_port = wfd_kms->ports[i];
		priv->wfd_port_id = wfd_kms->port_ids[i];
		priv->wfd_port_idx = i;

		priv->base.max_blendstages = 0;
		for (j = 0; j < wfd_kms->pipeline_cnt[i]; j++) {
			if (wfd_kms->master_idx[i][j] < 0)
				++priv->base.max_blendstages;
		}

		priv->base.primary_plane_index = pipe_cnt;
		pipe_cnt += wfd_kms->pipeline_cnt[i];

		/* these values should read from host */
		priv->base.max_mdp_clk = 412500000LL;
		priv->base.qseed_type = "qseed3";
		priv->base.smart_dma_rev = "smart_dma_v2p5";
		priv->base.has_hdr = false;
		priv->base.max_bandwidth_low = 9600000000LL;
		priv->base.max_bandwidth_high = 9600000000LL;
		priv->base.has_src_split = true;

		_wfd_kms_set_crtc_limit(wfd_kms, priv);

		crtc_infos[i] = &priv->base;
	}

	return 0;
}

static void wfd_kms_plane_atomic_update(struct drm_plane *plane,
		struct drm_plane_state *old_state)
{
	struct msm_hyp_plane *p = to_msm_hyp_plane(plane);
	struct wfd_plane_info_priv *priv = container_of(p->info,
			struct wfd_plane_info_priv, base);
	struct msm_hyp_framebuffer *fb;
	struct wfd_framebuffer_priv *fb_priv;
	struct msm_hyp_plane_state *old_pstate, *new_pstate;
	WFDint src_rect[4];
	WFDint dst_rect[4];
	WFDint color_space;

	new_pstate = to_msm_hyp_plane_state(plane->state);
	old_pstate = to_msm_hyp_plane_state(old_state);

	if (old_state->crtc != plane->state->crtc) {
		wfdBindPipelineToPort_User(
				priv->wfd_device,
				priv->wfd_port,
				priv->wfd_pipeline);
	}

	if (!plane->state->crtc || !plane->state->fb)
		wfdBindSourceToPipeline_User(
			priv->wfd_device,
			priv->wfd_pipeline,
			WFD_INVALID_HANDLE,
			WFD_TRANSITION_AT_VSYNC,
			NULL);
	else if (old_state->fb != plane->state->fb) {
		fb = to_msm_hyp_fb(plane->state->fb);
		fb_priv = container_of(fb->info,
				struct wfd_framebuffer_priv, base);

		if (!fb_priv->wfd_source) {
			WFDint attrib_list[3] = {WFD_SOURCE_TRANSLATION_MODE,
					WFD_SOURCE_TRANSLATION_UNSECURED,
					WFD_NONE};

			fb_priv->wfd_device = priv->wfd_device;

			if (_wfd_kms_create_image(fb)) {
				pr_err("failed to create wfd image\n");
				return;
			}

			attrib_list[1] = (new_pstate->fb_mode ==
					SDE_DRM_FB_SEC) ?
					WFD_SOURCE_TRANSLATION_SECURED :
					WFD_SOURCE_TRANSLATION_UNSECURED;
			fb_priv->wfd_source = wfdCreateSourceFromImage_User(
					priv->wfd_device,
					priv->wfd_pipeline,
					fb_priv->wfd_image, attrib_list);

			if (!fb_priv->wfd_source) {
				pr_err("failed to create wfd source\n");
				return;
			}
		}

		wfdBindSourceToPipeline_User(
			priv->wfd_device,
			priv->wfd_pipeline,
			fb_priv->wfd_source,
			WFD_TRANSITION_AT_VSYNC,
			NULL);
	}

	if (_wfd_kms_plane_is_rect_changed(
		old_state, plane->state, true)) {
		src_rect[0] = plane->state->src_x >> 16;
		src_rect[1] = plane->state->src_y >> 16;
		src_rect[2] = plane->state->src_w >> 16;
		src_rect[3] = plane->state->src_h >> 16;
		wfdSetPipelineAttribiv_User(
			priv->wfd_device,
			priv->wfd_pipeline,
			WFD_PIPELINE_SOURCE_RECTANGLE,
			4,
			src_rect);
	}

	if (_wfd_kms_plane_is_rect_changed(
		old_state, plane->state, false)) {
		dst_rect[0] = plane->state->crtc_x;
		dst_rect[1] = plane->state->crtc_y;
		dst_rect[2] = plane->state->crtc_w;
		dst_rect[3] = plane->state->crtc_h;
		wfdSetPipelineAttribiv_User(
			priv->wfd_device,
			priv->wfd_pipeline,
			WFD_PIPELINE_DESTINATION_RECTANGLE,
			4,
			dst_rect);
	}

	if (old_pstate->alpha != new_pstate->alpha || !priv->committed) {
		wfdSetPipelineAttribi_User(
			priv->wfd_device,
			priv->wfd_pipeline,
			WFD_PIPELINE_GLOBAL_ALPHA,
			new_pstate->alpha);
	}

	if (_wfd_kms_plane_is_csc_matrix_changed(
		old_pstate, new_pstate, &color_space)) {
		wfdSetPipelineAttribi_User(
			priv->wfd_device,
			priv->wfd_pipeline,
			WFD_PIPELINE_COLOR_SPACE,
			color_space);
	}

	if (old_pstate->blend_op != new_pstate->blend_op || !priv->committed) {
		wfdSetPipelineAttribi_User(
			priv->wfd_device,
			priv->wfd_pipeline,
			WFD_PIPELINE_TRANSPARENCY_ENABLE,
			(new_pstate->blend_op ==
				SDE_DRM_BLEND_OP_OPAQUE) ?
				WFD_TRANSPARENCY_GLOBAL_ALPHA :
				WFD_TRANSPARENCY_SOURCE_ALPHA);
	}

	priv->committed = true;
}

static const struct drm_plane_helper_funcs wfd_plane_helper_funcs = {
	.atomic_update = wfd_kms_plane_atomic_update,
};

static int wfd_kms_get_port_plane_infos(struct msm_hyp_kms *kms,
		WFDint port_idx, int base_idx,
		struct msm_hyp_plane_info **plane_infos)
{
	struct wfd_kms *wfd_kms = to_wfd_kms(kms);
	struct wfd_plane_info_priv *priv;
	int i, ret = 0, master_idx;
	WFDfloat val[2] = {0.0, 0.0};
	WFDint val_i[2] = {0, 0};
	WFDint max_width = wfd_kms->dev->mode_config.max_width;

	for (i = 0; i < wfd_kms->pipeline_cnt[port_idx]; i++) {
		priv = kzalloc(sizeof(*priv), GFP_KERNEL);
		if (priv == NULL) {
			ret = -ENOMEM;
			break;
		}

		priv->wfd_pipeline = wfd_kms->pipelines[port_idx][i];
		priv->wfd_port = wfd_kms->ports[port_idx];
		priv->wfd_device = wfd_kms->port_devs[port_idx];
		priv->wfd_type = wfdGetPipelineAttribi_User(
				priv->wfd_device,
				priv->wfd_pipeline,
				WFD_PIPELINE_TYPE);

		if (i == 0)
			priv->base.plane_type = DRM_PLANE_TYPE_PRIMARY;
		else if (priv->wfd_type ==
				WFD_QDI_LAYER_CURSOR)
			priv->base.plane_type = DRM_PLANE_TYPE_CURSOR;
		else
			priv->base.plane_type = DRM_PLANE_TYPE_OVERLAY;

		ret = _wfd_kms_plane_get_format(priv);
		if (ret)
			break;

		if (priv->wfd_type == WFD_QDI_LAYER_GRAPHICS
				|| priv->wfd_type ==
				WFD_QDI_LAYER_OVERLAY)
			priv->base.support_scale = true;

		if (priv->wfd_type == WFD_QDI_LAYER_OVERLAY)
			priv->base.support_csc = true;

		master_idx = wfd_kms->master_idx[port_idx][i];
		if (master_idx >= 0) {
			priv->base.support_multirect = true;
			priv->base.master_plane_index = master_idx + base_idx;
		}

		priv->base.possible_crtcs = 1 << port_idx;

		wfdGetPipelineAttribfv_User(priv->wfd_device,
				priv->wfd_pipeline,
				WFD_PIPELINE_SCALE_RANGE,
				2,
				val);
		if (val[0] > 0 && val[1] > 0) {
			priv->base.maxdwnscale = (u32)(1.0f / val[0]);
			priv->base.maxupscale = (u32)(val[1]);
		} else {
			priv->base.maxdwnscale = SSPP_UNITY_SCALE;
			priv->base.maxupscale = SSPP_UNITY_SCALE;
		}

		priv->base.maxhdeciexp = MAX_HORZ_DECIMATION;
		priv->base.maxvdeciexp = MAX_VERT_DECIMATION;

		wfdGetPipelineAttribiv_User(priv->wfd_device,
				priv->wfd_pipeline,
				WFD_PIPELINE_MAX_SOURCE_SIZE,
				2,
				val_i);

		priv->base.max_width = (val_i[0] > 0) ? val_i[0] : max_width;
		priv->base.max_bandwidth = 4500000000;

		if (!wfd_kms->max_sdma_width && master_idx >= 0)
			wfd_kms->max_sdma_width = priv->base.max_width;

		priv->base.plane_funcs = &wfd_plane_helper_funcs;

		plane_infos[i + base_idx] = &priv->base;
	}

	return ret;
}

static int wfd_kms_get_plane_infos(struct msm_hyp_kms *kms,
		struct msm_hyp_plane_info **plane_infos,
		int *plane_num)
{
	struct wfd_kms *wfd_kms = to_wfd_kms(kms);
	int i, ret, pipe_cnt = 0;

	if (!kms || !plane_num)
		return -EINVAL;

	if (!plane_infos) {
		*plane_num = 0;
		for (i = 0; i < wfd_kms->port_cnt; i++)
			*plane_num += wfd_kms->pipeline_cnt[i];
		return 0;
	}

	for (i = 0; i < wfd_kms->port_cnt; i++) {
		ret = wfd_kms_get_port_plane_infos(kms, i, pipe_cnt,
				plane_infos);
		if (ret)
			return ret;

		pipe_cnt += wfd_kms->pipeline_cnt[i];
	}

	return 0;
}

static int wfd_kms_get_mode_info(struct msm_hyp_kms *kms,
		const struct drm_display_mode *mode,
		struct msm_hyp_mode_info *modeinfo)
{
	modeinfo->num_lm = (mode->clock > MAX_MDP_CLK_KHZ) ? 2 : 1;
	modeinfo->num_enc = 0;
	modeinfo->num_intf = 1;

	return 0;
}

static int wfd_kms_plane_cmp(const void *a, const void *b)
{
	struct msm_hyp_plane_state *pa = (struct msm_hyp_plane_state *)a;
	struct msm_hyp_plane_state *pb = (struct msm_hyp_plane_state *)b;
	int rc;

	if (pa->zpos != pb->zpos)
		rc = pa->zpos - pb->zpos;
	else
		rc = pa->base.crtc_x - pb->base.crtc_x;

	return rc;
}

static void _wfd_kms_plane_zpos_adj_fe(struct drm_crtc *crtc,
		struct drm_atomic_state *old_state)
{
	struct drm_device *ddev = crtc->dev;
	int cnt = 0;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state;
	struct msm_hyp_plane *p;
	struct msm_hyp_plane_state *old_pstate, *new_pstate;
	struct drm_crtc_state *old_crtc_state;
	bool zpos_update = false;
	struct msm_hyp_plane_state *sorted_pstate[MAX_PIPELINE_CNT];
	struct wfd_plane_info_priv *priv;
	int i;

	drm_for_each_plane_mask(plane, ddev, crtc->state->plane_mask) {
		new_pstate = to_msm_hyp_plane_state(plane->state);
		sorted_pstate[cnt++] = new_pstate;

		if (zpos_update)
			continue;

		old_plane_state = drm_atomic_get_old_plane_state(
				old_state, plane);
		if (old_plane_state) {
			old_pstate = to_msm_hyp_plane_state(old_plane_state);
			if (old_pstate->zpos != new_pstate->zpos)
				zpos_update = true;
		}
	}

	old_crtc_state = drm_atomic_get_old_crtc_state(old_state, crtc);

	if (cnt && (zpos_update || (old_crtc_state->plane_mask !=
			crtc->state->plane_mask))) {
		sort(sorted_pstate, cnt, sizeof(sorted_pstate[0]),
				wfd_kms_plane_cmp, NULL);
		for (i = 0; i < cnt; i++) {
			p = to_msm_hyp_plane(sorted_pstate[i]->base.plane);
			priv = container_of(p->info,
					struct wfd_plane_info_priv, base);
			wfdSetPipelineAttribi_User(
					priv->wfd_device,
					priv->wfd_pipeline,
					WFD_PIPELINE_LAYER,
					i + 1);
		}
	}
}

static void _wfd_kms_req_vblank(struct drm_crtc *crtc);

static void *wfd_kms_complete_handler_cb(enum event_types type,
	union event_info *info, void *params)
{
	struct drm_crtc *crtc = params;
	struct display_event *disp_event = (struct display_event *)info;
	struct msm_hyp_crtc *c = to_msm_hyp_crtc(crtc);
	struct wfd_crtc_info_priv *priv;

	if (type != DISPLAY_EVENT || !info || !params)
		return NULL;

	if (disp_event->type == COMMIT_COMPLETE) {
		msm_hyp_crtc_commit_done(crtc);
	} else if (disp_event->type == VSYNC) {
		msm_hyp_crtc_vblank_done(crtc);

		/* if vblank is still enabled, schedule another one */
		priv = container_of(c->info, struct wfd_crtc_info_priv, base);
		if (priv->vblank_enable)
			_wfd_kms_req_vblank(crtc);
	}

	return NULL;
}

static void _wfd_kms_req_vblank(struct drm_crtc *crtc)
{
	struct msm_hyp_crtc *c = to_msm_hyp_crtc(crtc);
	struct wfd_crtc_info_priv *priv = container_of(c->info,
			struct wfd_crtc_info_priv, base);
	struct display_event disp_event;
	struct cb_info cb_info;

	disp_event.display_id = priv->wfd_port_id;
	disp_event.type = VSYNC;
	cb_info.cb = wfd_kms_complete_handler_cb;
	cb_info.user_data = crtc;

	pr_debug("register vsync event id=%d\n",
			disp_event.display_id);

	wire_user_register_event_listener(priv->wfd_device,
			DISPLAY_EVENT,
			(union event_info *)&disp_event,
			&cb_info);

	wire_user_request_cb(priv->wfd_device,
			DISPLAY_EVENT,
			(union event_info *)&disp_event);
}

static void wfd_kms_commit(struct msm_hyp_kms *kms,
			struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct msm_hyp_crtc *c;
	struct wfd_crtc_info_priv *priv;
	struct display_event disp_event;
	struct cb_info cb_info;
	int i;

	if (!old_state)
		return;

	for_each_new_crtc_in_state(old_state, crtc, crtc_state, i) {
		c = to_msm_hyp_crtc(crtc);
		priv = container_of(c->info, struct wfd_crtc_info_priv, base);

		if (crtc_state->active)
			_wfd_kms_plane_zpos_adj_fe(crtc, old_state);

		disp_event.type = COMMIT_COMPLETE;
		disp_event.display_id = priv->wfd_port_id;
		cb_info.cb = wfd_kms_complete_handler_cb;
		cb_info.user_data = crtc;

		wire_user_register_event_listener(
				priv->wfd_device,
				DISPLAY_EVENT,
				(union event_info *)&disp_event,
				&cb_info);

		wfdDeviceCommitExt_User(
				priv->wfd_device,
				WFD_COMMIT_ENTIRE_PORT,
				priv->wfd_port,
				WFD_COMMIT_ASYNC);
	}
}

static void wfd_kms_enable_vblank(struct msm_hyp_kms *kms,
		struct drm_crtc *crtc)
{
	struct msm_hyp_crtc *c = to_msm_hyp_crtc(crtc);
	struct wfd_crtc_info_priv *priv = container_of(c->info,
			struct wfd_crtc_info_priv, base);

	if (!crtc)
		return;

	priv->vblank_enable = true;

	_wfd_kms_req_vblank(crtc);
}

static void wfd_kms_disable_vblank(struct msm_hyp_kms *kms,
		struct drm_crtc *crtc)
{
	struct msm_hyp_crtc *c = to_msm_hyp_crtc(crtc);
	struct wfd_crtc_info_priv *priv;

	if (!crtc)
		return;

	priv = container_of(c->info, struct wfd_crtc_info_priv, base);

	priv->vblank_enable = false;
}

static const struct msm_hyp_kms_funcs wfd_kms_funcs = {
	.get_connector_infos = wfd_kms_get_connector_infos,
	.get_plane_infos = wfd_kms_get_plane_infos,
	.get_crtc_infos = wfd_kms_get_crtc_infos,
	.get_mode_info = wfd_kms_get_mode_info,
	.get_framebuffer_info = wfd_kms_get_framebuffer_info,
	.commit = wfd_kms_commit,
	.enable_vblank = wfd_kms_enable_vblank,
	.disable_vblank = wfd_kms_disable_vblank,
};

static int wfd_kms_bind(struct device *dev, struct device *master,
		void *data)
{
	struct wfd_kms *kms = dev_get_drvdata(dev);
	struct drm_device *drm_dev = dev_get_drvdata(master);

	kms->dev = drm_dev;
	msm_hyp_set_kms(drm_dev, &kms->base);

	return 0;
}

static void wfd_kms_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct wfd_kms *kms = dev_get_drvdata(dev);

	msm_hyp_set_kms(kms->dev, NULL);
}

static const struct component_ops wfd_kms_comp_ops = {
	.bind = wfd_kms_bind,
	.unbind = wfd_kms_unbind,
};

static int wfd_kms_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct wfd_kms *kms;
	int ret;

	kms = devm_kzalloc(dev, sizeof(*kms), GFP_KERNEL);
	if (!kms)
		return -ENOMEM;

	ret = _wfd_kms_parse_dt(dev->of_node, &kms->client_id);
	if (ret)
		return ret;

	ret = _wfd_kms_hw_init(kms);
	if (ret)
		return ret;

	kms->base.funcs = &wfd_kms_funcs;

	platform_set_drvdata(pdev, kms);

	ret = component_add(&pdev->dev, &wfd_kms_comp_ops);
	if (ret) {
		pr_err("component add failed, rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int wfd_kms_remove(struct platform_device *pdev)
{
	struct wfd_kms *kms = platform_get_drvdata(pdev);
	int i, j;

	for (i = 0; i < kms->port_cnt; i++) {
		for (j = 0; j < kms->pipeline_cnt[i]; j++)
			wfdDestroyPipeline_User(kms->port_devs[i], kms->pipelines[i][j]);
		wfdDestroyPort_User(kms->port_devs[i], kms->ports[i]);
	}

	for (i = 0; i < kms->wfd_device_cnt; i++)
		wfdDestroyDevice_User(kms->wfd_device[i]);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct platform_device_id wfd_kms_id[] = {
	{ "wfd-kms", 0 },
	{ }
};

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,wfd-kms" },
	{}
};

static struct platform_driver wfd_kms_driver = {
	.probe      = wfd_kms_probe,
	.remove     = wfd_kms_remove,
	.driver     = {
		.name   = "wfd_kms",
		.of_match_table = dt_match,
	},
	.id_table   = wfd_kms_id,
};

void wfd_kms_register(void)
{
	platform_driver_register(&wfd_kms_driver);
}

void wfd_kms_unregister(void)
{
	platform_driver_unregister(&wfd_kms_driver);
}
