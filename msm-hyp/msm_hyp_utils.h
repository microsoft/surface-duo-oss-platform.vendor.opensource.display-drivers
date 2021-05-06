/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_HYP_UTILS_H_
#define _MSM_HYP_UTILS_H_

#include <drm/drm_connector.h>

#define MAX_BLOB_INFO_SIZE          4096

#ifndef DRM_FORMAT_MOD_VENDOR_QTI
#define DRM_FORMAT_MOD_VENDOR_QTI    0x05
#endif
#ifndef DRM_FORMAT_MOD_QTI_COMPRESSED
#define DRM_FORMAT_MOD_QTI_COMPRESSED fourcc_mod_code(QTI, 1)
#endif
#ifndef DRM_FORMAT_MOD_QTI_DX
#define DRM_FORMAT_MOD_QTI_DX fourcc_mod_code(QTI, 2)
#endif
#ifndef DRM_FORMAT_MOD_QTI_TIGHT
#define DRM_FORMAT_MOD_QTI_TIGHT fourcc_mod_code(QTI, 4)
#endif

struct msm_hyp_prop_blob_info {
	char data[MAX_BLOB_INFO_SIZE];
	uint32_t len;
};

void msm_hyp_prop_info_append(
		struct msm_hyp_prop_blob_info *info,
		const char *str);

void msm_hyp_prop_info_add_keystr(
		struct msm_hyp_prop_blob_info *info,
		const char *key, const char *value);

void msm_hyp_prop_info_populate_plane_format(
		struct drm_plane *plane,
		struct msm_hyp_prop_blob_info *info);

int msm_hyp_connector_init_edid(struct drm_connector *connector,
		const char *name);

#endif
