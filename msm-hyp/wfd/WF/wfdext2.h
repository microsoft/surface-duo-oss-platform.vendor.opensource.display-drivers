/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __wfdext2_h_
#define __wfdext2_h_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__QNXNTO__)
#include <WF/wfdplatform.h>
#else
#include "wfdplatform.h"
#endif

#define WFD_WFDEXT_VERSION 1

#ifndef WFD_error_timeout
/**
 * This extension provides a way to return the timeout error to the
 * application in case of any.
 */
#define WFD_error_timeout 1
/* Error extension */
#define WFD_ERROR_TIMEOUT   0x7518
#endif

#ifndef WFD_multi_client
#define WFD_multi_client 1

#define WFD_DEVICE_CLIENT_TYPE  0x7561
enum WFDClientIdType {
	WFD_CLIENT_ID_CLUSTER     = 0x7810,
	WFD_CLIENT_ID_MONITOR     = 0x7811,
	WFD_CLIENT_ID_TELLTALE    = 0x7812,
	WFD_CLIENT_ID_RVC         = 0x7813,
	WFD_CLIENT_ID_QNX_GVM     = 0x7814,
	WFD_CLIENT_ID_LA_GVM      = 0x7815,
	WFD_CLIENT_ID_LV_GVM      = 0x7816,
	WFD_CLIENT_ID_FORCE_32BIT = 0x7FFFFFFF
};
#endif

#ifndef WFD_device_get_attribiv
#define WFD_device_get_attribiv 1
enum WFDDeviceAttribEXT {
	WFD_DEVICE_MIN_MAX_WIDTH_HEIGHT   = 0x7561,
	WFD_DEVICE_ATTRIB_EXT_FORCE_32BIT = 0x7FFFFFFF
};
#endif

#ifndef WFD_device_hdcp_min_enc_level
#define WFD_device_hdcp_min_enc_level 1
#define WFD_DEVICE_HDCP_MIN_ENC_LEVEL  0x7562
#endif

#ifndef WFD_device_oem_secure
#define WFD_device_oem_secure  1
#define WFD_DEVICE_OEM_SECURE  0x7563
#endif

#ifndef WFD_device_host_capabilities
#define WFD_device_host_capabilities  1
#define WFD_DEVICE_HOST_CAPABILITIES  0x7564
#define WFD_DEVICE_HOST_CAP_BATCH_MODE 0x00000001
#endif

#ifndef WFD_cbabc
#define WFD_cbabc 1
#define WFD_PORT_CBABC_MODE 0x7670
enum WFDPortCBABCMode {
	WFD_PORT_CBABC_MODE_NONE  = 0x7671,
	WFD_PORT_CBABC_MODE_VIDEO = 0x7672,
	WFD_PORT_CBABC_MODE_UI    = 0x7673,
	WFD_PORT_CBABC_MODE_PHOTO = 0x7674,
	WFD_PORT_CBABC_MODE_32BIT = 0x7FFFFFFF
};
#endif

#ifndef WFD_port_types_ext
#define WFD_port_types_ext  1
#define WFD_PORT_TYPE_DSI   0x766A
#endif

#ifndef WFD_port_brightness
/**
 * This extension provides a way of changing the perceived brightess of a port.
 *
 * WFD_PORT_CURRENT_BRIGHTNESS is a read-only property that returns the current
 * brightness as a normalized value between 0.0 and 1.0.
 *
 * WFD_PORT_BRIGHTNESS is a read/write property that is used to set the current
 * brightness. Values can be:
 *   -1.0       Indicates that the system has automatic control over
 *              the brightness
 *   [0.0,1.0]  An absolute brightness ranging from off to maximum.
 *
 * @note The use of 'brightness' in this extension is inconsistent with the
 *       'brightness' in the bchs extension, where brightness affects the
 *       color, not the way the color is displayed. In other words, pipeline
 *       brightness changes the color in the WFDDestination when writeback
 *       is enabled, whereas the port brightness doesn't.
 */
#define WFD_port_brightness 1
#define WFD_PORT_CURRENT_BRIGHTNESS   0x7650
#define WFD_PORT_BRIGHTNESS           0x7651
#endif

#ifndef WFD_port_color_profile
/**
 * This extension provides a way of querying a port's current color profile.
 *
 * WFD_PORT_COLOR_POINT_#### is a read-only property that returns the requested
 * x,y,Y values after any transformations by a HW CMU block (if present)
 *
 */
#define WFD_port_color_profile 1
#define WFD_PORT_COLOR_POINT_RED    0x7652
#define WFD_PORT_COLOR_POINT_GREEN  0x7653
#define WFD_PORT_COLOR_POINT_BLUE   0x7654
#define WFD_PORT_COLOR_POINT_WHITE  0x7655
#endif

#ifndef WFD_port_gamma_curve
/**
 * WFD_PORT_###_GAMMA_CURVE is a port property for getting/setting gamma curve
 * values. Each R/G/B gamma curve is defined as an array of 256 elements of
 * integers, ranging from 0 to 65535.
 */
#define WFD_port_gamma_curve 1
#define WFD_PORT_RED_GAMMA_CURVE    0x7690
#define WFD_PORT_GREEN_GAMMA_CURVE  0x7691
#define WFD_PORT_BLUE_GAMMA_CURVE   0x7692
#endif

#ifndef WFD_port_mode_info
#define WFD_port_mode_info 1
#define WFD_PORT_MODE_ASPECT_RATIO  0x7606
#define WFD_PORT_MODE_PREFERRED     0x7607
#endif

#ifndef WFD_vsync
#define WFD_vsync 1
#ifdef WFD_WFDEXT_PROTOTYPES
WFD_API_CALL WFDErrorCode WFD_APIENTRY
wfdWaitForVSync(WFDDevice device, WFDPort port) WFD_APIEXIT;
#endif /* WFD_WFDEXT_PROTOTYPES */
#endif

#ifndef WFD_write_back
#define WFD_write_back 1
#define WFD_PORT_WRITEBACK_SUPPORT                0x7640
#define WFD_PORT_WRITEBACK_SCALE_RANGE            0x7641
#define WFD_PORT_WRITEBACK_SOURCE_RECTANGLE       0x7642
#define WFD_PORT_WRITEBACK_DESTINATION_RECTANGLE  0x7643
#define WFD_EVENT_PORT_BIND_DESTINATION_COMPLETE  0x7587
#define WFD_EVENT_PORT_BIND_PORT_ID               0x75C9
#define WFD_EVENT_PORT_BIND_DESTINATION           0x75CA
#define WFD_EVENT_PORT_BIND_QUEUE_OVERFLOW        0x75CB
#endif

#ifndef WFD_pipeline_crc_capture
/**
 * This extension provides a way to capture the pipeline CRC.
 *
 * WFD_PIPELINE_CRC is a read-only property which returns the CRC of
 * the requested pipeline.
 */
#define WFD_pipeline_crc_capture 1
#define WFD_PIPELINE_CRC               0x7730
#endif

#ifndef WFD_pipeline_attrib_ext
#define WFD_pipeline_attrib_ext 1
#define WFD_PIPELINE_TYPE                   0x7740
#define WFD_PIPELINE_POSSIBLE_PORTS         0x7741 /* list of port ids */
#define WFD_PIPELINE_PIXEL_FORMATS          0x7742
#define WFD_PIPELINE_VIRTUAL_PIPE_ID        0x7745
#define WFD_PIPELINE_PIXEL_FORMATS_COUNT    0x7746
#endif

#ifndef WFD_bchs_filter
#define WFD_bchs_filter 1
#define WFD_PIPELINE_BRIGHTNESS        0x7750
#define WFD_PIPELINE_CONTRAST          0x7751
#define WFD_PIPELINE_HUE               0x7752
#define WFD_PIPELINE_SATURATION        0x7753
#endif

#ifndef WFD_pipeline_color_space
#define WFD_pipeline_color_space 1
#define WFD_PIPELINE_COLOR_SPACE       0x77A0

#define WFD_COLOR_SPACE_UNCORRECTED    0x0
#define WFD_COLOR_SPACE_SRGB           0x1
#define WFD_COLOR_SPACE_LRGB           0x2
#define WFD_COLOR_SPACE_BT601          0x3
#define WFD_COLOR_SPACE_BT601_FULL     0x4
#define WFD_COLOR_SPACE_BT709          0x5
#define WFD_COLOR_SPACE_BT709_FULL     0x6
#endif

#ifndef WFD_source_translation_mode
#define WFD_source_translation_mode 1
#define WFD_SOURCE_TRANSLATION_MODE    0x7900
enum WFDTranslationMode {
	WFD_SOURCE_TRANSLATION_UNSECURED,
	WFD_SOURCE_TRANSLATION_SECURED,
	WFD_SOURCE_TRANSLATION_DIRECT_UNSECURED,
	WFD_SOURCE_TRANSLATION_DIRECT_SECURED,
	WFD_SOURCE_TRANSLATION_MAX,
	WFD_SOURCE_TRANSLATION_FORCE_32BIT = 0x7FFFFFFF
};
#endif

#ifndef WFD_egl_images
#define WFD_egl_images 1
#define WFD_USAGE_DISPLAY             (1 << 0)
#define WFD_USAGE_READ                (1 << 1)
#define WFD_USAGE_WRITE               (1 << 2)
#define WFD_USAGE_NATIVE              (1 << 3)
#define WFD_USAGE_OPENGL_ES1          (1 << 4)
#define WFD_USAGE_OPENGL_ES2          (1 << 5)
#define WFD_USAGE_OPENGL_ES3          (1 << 11)
#define WFD_USAGE_OPENVG              (1 << 6)
#define WFD_USAGE_VIDEO               (1 << 7)
#define WFD_USAGE_CAPTURE             (1 << 8)
#define WFD_USAGE_ROTATION            (1 << 9)
#define WFD_USAGE_OVERLAY             (1 << 10)
#define WFD_USAGE_COMPRESSION         (1 << 12)
#define WFD_USAGE_WRITEBACK           (1 << 31)
#define WFD_FORMAT_BYTE               1
#define WFD_FORMAT_RGBA4444           2
#define WFD_FORMAT_RGBX4444           3
#define WFD_FORMAT_RGBA5551           4
#define WFD_FORMAT_RGBX5551           5
#define WFD_FORMAT_RGB565             6
#define WFD_FORMAT_RGB888             7
#define WFD_FORMAT_RGBA8888           8
#define WFD_FORMAT_RGBX8888           9
#define WFD_FORMAT_YVU9               10
#define WFD_FORMAT_YUV420             11
#define WFD_FORMAT_NV12               12
#define WFD_FORMAT_YV12               13
#define WFD_FORMAT_UYVY               14
#define WFD_FORMAT_YUY2               15
#define WFD_FORMAT_YVYU               16
#define WFD_FORMAT_V422               17
#define WFD_FORMAT_AYUV               18
#define WFD_FORMAT_NV16               19
#define WFD_FORMAT_RGBX1010102        20
#define WFD_FORMAT_RGBA1010102        21
#define WFD_FORMAT_BGRX1010102        22
#define WFD_FORMAT_BGRA1010102        23
#define WFD_FORMAT_XBGR2101010        24
#define WFD_FORMAT_XRGB2101010        25
#define WFD_FORMAT_ARGB2101010        26
#define WFD_FORMAT_ABGR2101010        27
#define WFD_FORMAT_P010               28
#define WFD_FORMAT_TP10               29

#ifndef WFD_FORMAT_NV12_QC_SUPERTILE
#define WFD_FORMAT_NV12_QC_SUPERTILE  ((1 << 16) | WFD_FORMAT_NV12)
#endif
#ifndef WFD_FORMAT_NV12_QC_32M4KA
#define WFD_FORMAT_NV12_QC_32M4KA     ((2 << 16) | WFD_FORMAT_NV12)
#endif

/* QTI extension definitions - start at 50 */
#define WFD_FORMAT_BGRA8888           50
#define WFD_FORMAT_BGRX8888           51
#define WFD_FORMAT_BGR565             52

struct WFD_EGLImageType {
	WFDuint32 width;
	WFDuint32 height;
	WFDuint32 format;
	WFDuint32 usage;
	WFDuint32 flags;
	WFDuint32 fd;
	WFDuint64 offset;
	WFDuint32 size;
	WFDuint32 padding;
	WFDuint64 paddr;
	WFDuint32 strides[2];
	WFDuint64 vaddr;
	WFDuint64 cvaddr;
	WFDuint64 dvaddr;
	WFDuint32 planar_offsets[3];
	WFDuint64 pages;
	WFDuint32 addr_alignment;
	WFDuint64 image_handle;
	WFDuint32 buffer_allocator;
	WFDuint64 shmem_id;
	WFDuint32 shmem_type;
};
#endif

#ifdef __cplusplus
}
#endif

#endif
