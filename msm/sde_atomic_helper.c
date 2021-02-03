/*
 * Copyright (c) 2015-2021 The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sde_atomic_helper.h"
#include "sde_kms.h"

void sde_atomic_helper_crtc_install_properties(
		const struct sde_mdss_cfg *catalog,
		bool custom_client,
		u64 max_core_clk_rate,
		struct msm_property_info *property_info,
		struct drm_property_blob **blob_info)
{
	struct sde_kms_info *info;
	int i, j;

	static const struct drm_prop_enum_list e_secure_level[] = {
		{SDE_DRM_SEC_NON_SEC, "sec_and_non_sec"},
		{SDE_DRM_SEC_ONLY, "sec_only"},
	};

	static const struct drm_prop_enum_list e_cwb_data_points[] = {
		{CAPTURE_MIXER_OUT, "capture_mixer_out"},
		{CAPTURE_DSPP_OUT, "capture_pp_out"},
	};

	static const struct drm_prop_enum_list e_idle_pc_state[] = {
		{IDLE_PC_NONE, "idle_pc_none"},
		{IDLE_PC_ENABLE, "idle_pc_enable"},
		{IDLE_PC_DISABLE, "idle_pc_disable"},
	};

	SDE_DEBUG("\n");

	if (!catalog) {
		SDE_ERROR("invalid crtc or catalog\n");
		return;
	}

	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (!info) {
		SDE_ERROR("failed to allocate info memory\n");
		return;
	}

	/* range properties */
	msm_property_install_range(property_info,
		"input_fence_timeout", 0x0, 0, SDE_CRTC_MAX_INPUT_FENCE_TIMEOUT,
		SDE_CRTC_INPUT_FENCE_TIMEOUT, CRTC_PROP_INPUT_FENCE_TIMEOUT);

	msm_property_install_volatile_range(property_info,
		"output_fence", 0x0, 0, ~0, 0, CRTC_PROP_OUTPUT_FENCE);

	msm_property_install_range(property_info,
			"output_fence_offset", 0x0, 0, 1, 0,
			CRTC_PROP_OUTPUT_FENCE_OFFSET);

	msm_property_install_volatile_range(property_info,
			"roi_misr", 0x0, 0, ~0, 0, CRTC_PROP_ROI_MISR);

	msm_property_install_range(property_info,
			"core_clk", 0x0, 0, U64_MAX,
			max_core_clk_rate,
			CRTC_PROP_CORE_CLK);
	msm_property_install_range(property_info,
			"core_ab", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_CORE_AB);
	msm_property_install_range(property_info,
			"core_ib", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_CORE_IB);
	msm_property_install_range(property_info,
			"llcc_ab", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_LLCC_AB);
	msm_property_install_range(property_info,
			"llcc_ib", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_LLCC_IB);
	msm_property_install_range(property_info,
			"dram_ab", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_DRAM_AB);
	msm_property_install_range(property_info,
			"dram_ib", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_DRAM_IB);
	msm_property_install_range(property_info,
			"rot_prefill_bw", 0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_ROT_PREFILL_BW);
	msm_property_install_range(property_info,
			"rot_clk", 0, 0, U64_MAX,
			max_core_clk_rate,
			CRTC_PROP_ROT_CLK);

	msm_property_install_range(property_info,
		"idle_time", 0, 0, U64_MAX, 0,
		CRTC_PROP_IDLE_TIMEOUT);

	if (catalog->has_idle_pc)
		msm_property_install_enum(property_info,
			"idle_pc_state", 0x0, 0, e_idle_pc_state,
			ARRAY_SIZE(e_idle_pc_state),
			CRTC_PROP_IDLE_PC_STATE);

	if (catalog->has_cwb_support)
		msm_property_install_enum(property_info,
				"capture_mode", 0, 0, e_cwb_data_points,
				ARRAY_SIZE(e_cwb_data_points),
				CRTC_PROP_CAPTURE_OUTPUT);

	msm_property_install_blob(property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, CRTC_PROP_INFO);

	msm_property_install_volatile_range(property_info,
		"sde_drm_roi_v1", 0x0, 0, ~0, 0, CRTC_PROP_ROI_V1);

	msm_property_install_enum(property_info, "security_level",
			0x0, 0, e_secure_level,
			ARRAY_SIZE(e_secure_level),
			CRTC_PROP_SECURITY_LEVEL);

	sde_kms_info_reset(info);

	if (catalog->has_dim_layer) {
		msm_property_install_volatile_range(property_info,
			"dim_layer_v1", 0x0, 0, ~0, 0, CRTC_PROP_DIM_LAYER_V1);
		sde_kms_info_add_keyint(info, "dim_layer_v1_max_layers",
				SDE_MAX_DIM_LAYERS);
	}

	sde_kms_info_add_keyint(info, "hw_version", catalog->hwversion);
	sde_kms_info_add_keyint(info, "max_linewidth",
			catalog->max_mixer_width);
	sde_kms_info_add_keyint(info, "max_blendstages",
			catalog->max_mixer_blendstages);
	if (catalog->qseed_type == SDE_SSPP_SCALER_QSEED2)
		sde_kms_info_add_keystr(info, "qseed_type", "qseed2");
	if (catalog->qseed_type == SDE_SSPP_SCALER_QSEED3)
		sde_kms_info_add_keystr(info, "qseed_type", "qseed3");
	if (catalog->qseed_type == SDE_SSPP_SCALER_QSEED3LITE)
		sde_kms_info_add_keystr(info, "qseed_type", "qseed3lite");

	sde_kms_info_add_keyint(info, "UBWC version", catalog->ubwc_version);
	sde_kms_info_add_keyint(info, "UBWC macrotile_mode",
				catalog->macrotile_mode);
	sde_kms_info_add_keyint(info, "UBWC highest banking bit",
				catalog->mdp[0].highest_bank_bit);
	sde_kms_info_add_keyint(info, "UBWC swizzle",
				catalog->mdp[0].ubwc_swizzle);

	if (custom_client) {
		/* No support for SMART_DMA_V1 yet */
		if (catalog->smart_dma_rev == SDE_SSPP_SMART_DMA_V2)
			sde_kms_info_add_keystr(info,
					"smart_dma_rev", "smart_dma_v2");
		else if (catalog->smart_dma_rev == SDE_SSPP_SMART_DMA_V2p5)
			sde_kms_info_add_keystr(info,
					"smart_dma_rev", "smart_dma_v2p5");
	}

	if (catalog->mdp[0].has_dest_scaler) {
		sde_kms_info_add_keyint(info, "has_dest_scaler",
				catalog->mdp[0].has_dest_scaler);
		sde_kms_info_add_keyint(info, "dest_scaler_count",
					catalog->ds_count);

		if (catalog->ds[0].top) {
			sde_kms_info_add_keyint(info,
					"max_dest_scaler_input_width",
					catalog->ds[0].top->maxinputwidth);
			sde_kms_info_add_keyint(info,
					"max_dest_scaler_output_width",
					catalog->ds[0].top->maxinputwidth);
			sde_kms_info_add_keyint(info, "max_dest_scale_up",
					catalog->ds[0].top->maxupscale);
		}

		if (catalog->ds[0].features & BIT(SDE_SSPP_SCALER_QSEED3)) {
			msm_property_install_volatile_range(
					property_info, "dest_scaler",
					0x0, 0, ~0, 0, CRTC_PROP_DEST_SCALER);
			msm_property_install_blob(property_info,
					"ds_lut_ed", 0,
					CRTC_PROP_DEST_SCALER_LUT_ED);
			msm_property_install_blob(property_info,
					"ds_lut_cir", 0,
					CRTC_PROP_DEST_SCALER_LUT_CIR);
			msm_property_install_blob(property_info,
					"ds_lut_sep", 0,
					CRTC_PROP_DEST_SCALER_LUT_SEP);
		} else if (catalog->ds[0].features
				& BIT(SDE_SSPP_SCALER_QSEED3LITE)) {
			msm_property_install_volatile_range(
					property_info, "dest_scaler",
					0x0, 0, ~0, 0, CRTC_PROP_DEST_SCALER);
		}
	}

	sde_kms_info_add_keyint(info, "has_src_split", catalog->has_src_split);
	sde_kms_info_add_keyint(info, "has_hdr", catalog->has_hdr);
	if (catalog->perf.max_bw_low)
		sde_kms_info_add_keyint(info, "max_bandwidth_low",
				catalog->perf.max_bw_low * 1000LL);
	if (catalog->perf.max_bw_high)
		sde_kms_info_add_keyint(info, "max_bandwidth_high",
				catalog->perf.max_bw_high * 1000LL);
	if (catalog->perf.min_core_ib)
		sde_kms_info_add_keyint(info, "min_core_ib",
				catalog->perf.min_core_ib * 1000LL);
	if (catalog->perf.min_llcc_ib)
		sde_kms_info_add_keyint(info, "min_llcc_ib",
				catalog->perf.min_llcc_ib * 1000LL);
	if (catalog->perf.min_dram_ib)
		sde_kms_info_add_keyint(info, "min_dram_ib",
				catalog->perf.min_dram_ib * 1000LL);
	if (max_core_clk_rate)
		sde_kms_info_add_keyint(info, "max_mdp_clk",
				max_core_clk_rate);

	for (i = 0; i < catalog->limit_count; i++) {
		sde_kms_info_add_keyint(info,
			catalog->limit_cfg[i].name,
			catalog->limit_cfg[i].lmt_case_cnt);

		for (j = 0; j < catalog->limit_cfg[i].lmt_case_cnt; j++) {
			sde_kms_info_add_keyint(info,
				catalog->limit_cfg[i].vector_cfg[j].usecase,
				catalog->limit_cfg[i].vector_cfg[j].value);
		}

		if (!strcmp(catalog->limit_cfg[i].name,
			"sspp_linewidth_usecases"))
			sde_kms_info_add_keyint(info,
				"sspp_linewidth_values",
				catalog->limit_cfg[i].lmt_vec_cnt);
		else if (!strcmp(catalog->limit_cfg[i].name,
				"sde_bwlimit_usecases"))
			sde_kms_info_add_keyint(info,
				"sde_bwlimit_values",
				catalog->limit_cfg[i].lmt_vec_cnt);

		for (j = 0; j < catalog->limit_cfg[i].lmt_vec_cnt; j++) {
			sde_kms_info_add_keyint(info, "limit_usecase",
				catalog->limit_cfg[i].value_cfg[j].use_concur);
			sde_kms_info_add_keyint(info, "limit_value",
				catalog->limit_cfg[i].value_cfg[j].value);
		}
	}

	sde_kms_info_add_keystr(info, "core_ib_ff",
			catalog->perf.core_ib_ff);
	sde_kms_info_add_keystr(info, "core_clk_ff",
			catalog->perf.core_clk_ff);
	sde_kms_info_add_keystr(info, "comp_ratio_rt",
			catalog->perf.comp_ratio_rt);
	sde_kms_info_add_keystr(info, "comp_ratio_nrt",
			catalog->perf.comp_ratio_nrt);
	sde_kms_info_add_keyint(info, "dest_scale_prefill_lines",
			catalog->perf.dest_scale_prefill_lines);
	sde_kms_info_add_keyint(info, "undersized_prefill_lines",
			catalog->perf.undersized_prefill_lines);
	sde_kms_info_add_keyint(info, "macrotile_prefill_lines",
			catalog->perf.macrotile_prefill_lines);
	sde_kms_info_add_keyint(info, "yuv_nv12_prefill_lines",
			catalog->perf.yuv_nv12_prefill_lines);
	sde_kms_info_add_keyint(info, "linear_prefill_lines",
			catalog->perf.linear_prefill_lines);
	sde_kms_info_add_keyint(info, "downscaling_prefill_lines",
			catalog->perf.downscaling_prefill_lines);
	sde_kms_info_add_keyint(info, "xtra_prefill_lines",
			catalog->perf.xtra_prefill_lines);
	sde_kms_info_add_keyint(info, "amortizable_threshold",
			catalog->perf.amortizable_threshold);
	sde_kms_info_add_keyint(info, "min_prefill_lines",
			catalog->perf.min_prefill_lines);
	sde_kms_info_add_keyint(info, "num_mnoc_ports",
			catalog->perf.num_mnoc_ports);
	sde_kms_info_add_keyint(info, "axi_bus_width",
			catalog->perf.axi_bus_width);
	sde_kms_info_add_keyint(info, "sec_ui_blendstage",
			catalog->sui_supported_blendstage);

	if (catalog->ubwc_bw_calc_version)
		sde_kms_info_add_keyint(info, "ubwc_bw_calc_ver",
				catalog->ubwc_bw_calc_version);

	sde_kms_info_add_keyint(info, "use_baselayer_for_stage",
				catalog->has_base_layer);

	msm_property_set_blob(property_info, &blob_info,
			info->data, SDE_KMS_INFO_DATALEN(info), CRTC_PROP_INFO);

	kfree(info);
}

void sde_atomic_helper_plane_install_properties(
		const struct sde_mdss_cfg *catalog,
		const struct sde_sspp_sub_blks *pipe_sblk,
		bool custom_client,
		bool support_solidfill,
		unsigned long features,
		int zpos_def,
		u32 scaler_ver,
		u32 master_plane_id,
		struct msm_property_info *property_info,
		struct drm_property_blob **blob_info)
{
	static const struct drm_prop_enum_list e_blend_op[] = {
		{SDE_DRM_BLEND_OP_NOT_DEFINED,    "not_defined"},
		{SDE_DRM_BLEND_OP_OPAQUE,         "opaque"},
		{SDE_DRM_BLEND_OP_PREMULTIPLIED,  "premultiplied"},
		{SDE_DRM_BLEND_OP_COVERAGE,       "coverage"}
	};
	static const struct drm_prop_enum_list e_src_config[] = {
		{SDE_DRM_DEINTERLACE, "deinterlace"}
	};
	static const struct drm_prop_enum_list e_fb_translation_mode[] = {
		{SDE_DRM_FB_NON_SEC, "non_sec"},
		{SDE_DRM_FB_SEC, "sec"},
		{SDE_DRM_FB_NON_SEC_DIR_TRANS, "non_sec_direct_translation"},
		{SDE_DRM_FB_SEC_DIR_TRANS, "sec_direct_translation"},
	};
	static const struct drm_prop_enum_list e_multirect_mode[] = {
		{SDE_SSPP_MULTIRECT_NONE, "none"},
		{SDE_SSPP_MULTIRECT_PARALLEL, "parallel"},
		{SDE_SSPP_MULTIRECT_TIME_MX,  "serial"},
	};
	const struct sde_format_extended *format_list;
	struct sde_kms_info *info;
	int zpos_max = 255;
	char feature_name[256];

	if (!catalog) {
		SDE_ERROR("invalid catalog\n");
		return;
	}

	if (custom_client) {
		if (catalog->mixer_count &&
				catalog->mixer[0].sblk->maxblendstages) {
			zpos_max = catalog->mixer[0].sblk->maxblendstages - 1;

			if (catalog->has_base_layer &&
					(zpos_max > SDE_STAGE_MAX - 1))
				zpos_max = SDE_STAGE_MAX - 1;
			else if (zpos_max > SDE_STAGE_MAX - SDE_STAGE_0 - 1)
				zpos_max = SDE_STAGE_MAX - SDE_STAGE_0 - 1;
		}
	}

	msm_property_install_range(property_info, "zpos",
		0x0, 0, zpos_max, zpos_def, PLANE_PROP_ZPOS);

	msm_property_install_range(property_info, "alpha",
		0x0, 0, 255, 255, PLANE_PROP_ALPHA);

	/* linux default file descriptor range on each process */
	msm_property_install_range(property_info, "input_fence",
		0x0, 0, INR_OPEN_MAX, 0, PLANE_PROP_INPUT_FENCE);

	if (!master_plane_id) {
		if (pipe_sblk->maxhdeciexp) {
			msm_property_install_range(property_info,
					"h_decimate", 0x0, 0,
					pipe_sblk->maxhdeciexp, 0,
					PLANE_PROP_H_DECIMATE);
		}

		if (pipe_sblk->maxvdeciexp) {
			msm_property_install_range(property_info,
					"v_decimate", 0x0, 0,
					pipe_sblk->maxvdeciexp, 0,
					PLANE_PROP_V_DECIMATE);
		}

		if (features & BIT(SDE_SSPP_SCALER_QSEED3)) {
			msm_property_install_range(
					property_info, "scaler_v2",
					0x0, 0, ~0, 0, PLANE_PROP_SCALER_V2);
			msm_property_install_blob(property_info,
					"lut_ed", 0, PLANE_PROP_SCALER_LUT_ED);
			msm_property_install_blob(property_info,
					"lut_cir", 0,
					PLANE_PROP_SCALER_LUT_CIR);
			msm_property_install_blob(property_info,
					"lut_sep", 0,
					PLANE_PROP_SCALER_LUT_SEP);
		} else if (features & BIT(SDE_SSPP_SCALER_QSEED3LITE)) {
			msm_property_install_range(
					property_info, "scaler_v2",
					0x0, 0, ~0, 0, PLANE_PROP_SCALER_V2);
			msm_property_install_blob(property_info,
					"lut_sep", 0,
					PLANE_PROP_SCALER_LUT_SEP);
		} else if (features & SDE_SSPP_SCALER) {
			msm_property_install_range(
					property_info, "scaler_v1", 0x0,
					0, ~0, 0, PLANE_PROP_SCALER_V1);
		}

		if (features & BIT(SDE_SSPP_CSC) ||
		    features & BIT(SDE_SSPP_CSC_10BIT))
			msm_property_install_volatile_range(
					property_info, "csc_v1", 0x0,
					0, ~0, 0, PLANE_PROP_CSC_V1);

		if (features & BIT(SDE_SSPP_HSIC)) {
			snprintf(feature_name, sizeof(feature_name), "%s%d",
				"SDE_SSPP_HUE_V",
				pipe_sblk->hsic_blk.version >> 16);
			msm_property_install_range(property_info,
				feature_name, 0, 0, 0xFFFFFFFF, 0,
				PLANE_PROP_HUE_ADJUST);
			snprintf(feature_name, sizeof(feature_name), "%s%d",
				"SDE_SSPP_SATURATION_V",
				pipe_sblk->hsic_blk.version >> 16);
			msm_property_install_range(property_info,
				feature_name, 0, 0, 0xFFFFFFFF, 0,
				PLANE_PROP_SATURATION_ADJUST);
			snprintf(feature_name, sizeof(feature_name), "%s%d",
				"SDE_SSPP_VALUE_V",
				pipe_sblk->hsic_blk.version >> 16);
			msm_property_install_range(property_info,
				feature_name, 0, 0, 0xFFFFFFFF, 0,
				PLANE_PROP_VALUE_ADJUST);
			snprintf(feature_name, sizeof(feature_name), "%s%d",
				"SDE_SSPP_CONTRAST_V",
				pipe_sblk->hsic_blk.version >> 16);
			msm_property_install_range(property_info,
				feature_name, 0, 0, 0xFFFFFFFF, 0,
				PLANE_PROP_CONTRAST_ADJUST);
		}
	}

	if (features & BIT(SDE_SSPP_EXCL_RECT))
		msm_property_install_volatile_range(property_info,
			"excl_rect_v1", 0x0, 0, ~0, 0, PLANE_PROP_EXCL_RECT_V1);

	msm_property_install_enum(property_info, "blend_op", 0x0, 0,
		e_blend_op, ARRAY_SIZE(e_blend_op), PLANE_PROP_BLEND_OP);

	msm_property_install_enum(property_info, "src_config", 0x0, 1,
		e_src_config, ARRAY_SIZE(e_src_config), PLANE_PROP_SRC_CONFIG);

	if (support_solidfill)
		msm_property_install_range(property_info, "color_fill",
				0, 0, 0xFFFFFFFF, 0, PLANE_PROP_COLOR_FILL);

	msm_property_install_range(property_info,
			"prefill_size", 0x0, 0, ~0, 0,
			PLANE_PROP_PREFILL_SIZE);
	msm_property_install_range(property_info,
			"prefill_time", 0x0, 0, ~0, 0,
			PLANE_PROP_PREFILL_TIME);

	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (!info) {
		SDE_ERROR("failed to allocate info memory\n");
		return;
	}

	msm_property_install_blob(property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, PLANE_PROP_INFO);
	sde_kms_info_reset(info);

	if (!master_plane_id) {
		format_list = pipe_sblk->format_list;
	} else {
		format_list = pipe_sblk->virt_format_list;
		sde_kms_info_add_keyint(info, "primary_smart_plane_id",
						master_plane_id);
		msm_property_install_enum(property_info,
			"multirect_mode", 0x0, 0, e_multirect_mode,
			ARRAY_SIZE(e_multirect_mode),
			PLANE_PROP_MULTIRECT_MODE);
	}

	if (format_list) {
		sde_kms_info_start(info, "pixel_formats");
		while (format_list->fourcc_format) {
			sde_kms_info_append_format(info,
					format_list->fourcc_format,
					format_list->modifier);
			++format_list;
		}
		sde_kms_info_stop(info);
	}

	if (scaler_ver)
		sde_kms_info_add_keyint(info, "scaler_step_ver",
			scaler_ver);

	sde_kms_info_add_keyint(info, "max_linewidth",
			pipe_sblk->maxlinewidth);
	sde_kms_info_add_keyint(info, "max_upscale",
			pipe_sblk->maxupscale);
	sde_kms_info_add_keyint(info, "max_downscale",
			pipe_sblk->maxdwnscale);
	sde_kms_info_add_keyint(info, "max_horizontal_deci",
			pipe_sblk->maxhdeciexp);
	sde_kms_info_add_keyint(info, "max_vertical_deci",
			pipe_sblk->maxvdeciexp);
	sde_kms_info_add_keyint(info, "max_per_pipe_bw",
			pipe_sblk->max_per_pipe_bw * 1000LL);
	sde_kms_info_add_keyint(info, "max_per_pipe_bw_high",
			pipe_sblk->max_per_pipe_bw_high * 1000LL);

	if ((!master_plane_id &&
		(features & BIT(SDE_SSPP_INVERSE_PMA))) ||
		(features & BIT(SDE_SSPP_DGM_INVERSE_PMA))) {
		msm_property_install_range(property_info,
			"inverse_pma", 0x0, 0, 1, 0, PLANE_PROP_INVERSE_PMA);
		sde_kms_info_add_keyint(info, "inverse_pma", 1);
	}

	if (features & BIT(SDE_SSPP_DGM_CSC)) {
		msm_property_install_volatile_range(
			property_info, "csc_dma_v1", 0x0,
			0, ~0, 0, PLANE_PROP_CSC_DMA_V1);
		sde_kms_info_add_keyint(info, "csc_dma_v1", 1);
	}

	if (features & BIT(SDE_SSPP_SEC_UI_ALLOWED))
		sde_kms_info_add_keyint(info, "sec_ui_allowed", 1);
	if (features & BIT(SDE_SSPP_BLOCK_SEC_UI))
		sde_kms_info_add_keyint(info, "block_sec_ui", 1);

	msm_property_set_blob(property_info, &blob_info,
			info->data, SDE_KMS_INFO_DATALEN(info),
			PLANE_PROP_INFO);

	kfree(info);

	if (features & BIT(SDE_SSPP_MEMCOLOR)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_SKIN_COLOR_V",
			pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(property_info, feature_name, 0,
			PLANE_PROP_SKIN_COLOR);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_SKY_COLOR_V",
			pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(property_info, feature_name, 0,
			PLANE_PROP_SKY_COLOR);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_FOLIAGE_COLOR_V",
			pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(property_info, feature_name, 0,
			PLANE_PROP_FOLIAGE_COLOR);
	}

	if (features & BIT(SDE_SSPP_VIG_GAMUT)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_VIG_3D_LUT_GAMUT_V",
			pipe_sblk->gamut_blk.version >> 16);
		msm_property_install_blob(property_info, feature_name, 0,
			PLANE_PROP_VIG_GAMUT);
	}

	if (features & BIT(SDE_SSPP_VIG_IGC)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_VIG_1D_LUT_IGC_V",
			pipe_sblk->igc_blk[0].version >> 16);
		msm_property_install_blob(property_info, feature_name, 0,
			PLANE_PROP_VIG_IGC);
	}

	if (features & BIT(SDE_SSPP_DMA_IGC)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_DGM_1D_LUT_IGC_V",
			pipe_sblk->igc_blk[0].version >> 16);
		msm_property_install_blob(property_info, feature_name, 0,
			PLANE_PROP_DMA_IGC);
	}

	if (features & BIT(SDE_SSPP_DMA_GC)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_DGM_1D_LUT_GC_V",
			pipe_sblk->gc_blk[0].version >> 16);
		msm_property_install_blob(property_info, feature_name, 0,
			PLANE_PROP_DMA_GC);
	}

	msm_property_install_enum(property_info, "fb_translation_mode",
			0x0,
			0, e_fb_translation_mode,
			ARRAY_SIZE(e_fb_translation_mode),
			PLANE_PROP_FB_TRANSLATION_MODE);
}

void sde_atomic_helper_connector_install_properties(
		const struct sde_mdss_cfg *sde_cfg,
		struct msm_property_info *property_info,
		struct msm_property_data *property_data);
void sde_atomic_helper_connector_reset_properties(
		struct msm_property_state *property_state,
		struct msm_property_value **property_values);
void sde_atomic_helper_connector_populate_edid(
		struct drm_connector *connector);
void sde_atomic_helper_connector_populate_mode_info(
		struct drm_connector *connector,
		struct msm_mode_info *mode_info);

void sde_atomic_helper_thread_install(struct drm_device *dev,
		struct msm_drm_thread_data *thread_data);
void sde_atomic_helper_thread_commit(struct drm_device *dev,
		struct msm_drm_thread_data *thread_data,
		struct drm_atomic_state *state);
void sde_atomic_helper_thread_destroy(struct drm_device *dev,
		struct msm_drm_thread_data *thread_data);
