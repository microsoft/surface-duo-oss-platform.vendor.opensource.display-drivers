/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef WIRE_USER_H
#define WIRE_USER_H

/*
 * ---------------------------------------------------------------------------
 * Includes
 * ---------------------------------------------------------------------------
 */
#if defined(__linux__)
#include "WF/wfd.h"
#include "WF/wfdext2.h"
#elif defined(__QNXNTO__)
#include <WF/wfd.h>
#include <WF/wfdext2.h>
#else
#include "wfd.h"
#include "wfdext2.h"
#endif

#ifdef ANDROID
#define ATRACE_TAG ATRACE_TAG_ALWAYS
#include <cutils/trace.h>
#else
#define ATRACE_TAG
#define ATRACE_BEGIN(_x_)
#define ATRACE_END(_x_)
#endif
/*
 * ---------------------------------------------------------------------------
 * Defines
 * ---------------------------------------------------------------------------
 */
#define WFD_COMMIT_ASYNC        0x1
#define WFD_COMMIT_SEND_EVENT   0x2

#define WIRE_INIT_EVENT_SUPPORT 0x1

/*
 * ---------------------------------------------------------------------------
 * Structure Definitions
 * ---------------------------------------------------------------------------
 */
struct event_notification {
	unsigned int type;
	unsigned long user_data;
};

enum display_event_types {
	VSYNC,
	COMMIT_COMPLETE,
	HPD,
	RECOVERY,
	DISPLAY_EVENT_MAX
};

struct display_event {
	enum display_event_types type;
	int display_id;
};

enum vm_event_types {
	VM_RESTART,
	VM_SHUTDOWN,
	VM_EVENT_MAX
};

struct vm_event {
	enum vm_event_types type;
};

enum event_types {
	DISPLAY_EVENT,
	VM_EVENT,
	EVENT_TYPE_MAX
};

union event_info {
	struct display_event disp_event;
	struct vm_event vm_event;
};

struct cb_info {
	void *(*cb)(enum event_types, union event_info *info, void *user_data);
	void *user_data;
};

/*
 * ---------------------------------------------------------------------------
 * Wire User APIs
 * ---------------------------------------------------------------------------
 */
int wire_user_init(uint32_t client_id,
		uint32_t flags);

int wire_user_deinit(uint32_t client_id,
		uint32_t flags);

int wire_user_set_host_capabilities(WFDDevice device,
		uint32_t capabilities);

/* ========== OPENWFD ========== */

/* Device */
WFDint wfdEnumerateDevices_User(
		WFDint *deviceIds,
		WFDint deviceIdsCount,
		const WFDint *filterList);

WFDDevice wfdCreateDevice_User(
		WFDint deviceId,
		const WFDint *attribList);

WFDErrorCode wfdDestroyDevice_User(
		WFDDevice device);

void wfdDeviceCommit_User(
		WFDDevice device,
		WFDCommitType type,
		WFDHandle handle);

void wfdDeviceCommitExt_User(
		WFDDevice device,
		WFDCommitType type,
		WFDHandle handle,
		WFDint flags);

WFDint wfdGetDeviceAttribi_User(
		WFDDevice device,
		WFDDeviceAttrib attrib);

void wfdSetDeviceAttribi_User(
		WFDDevice device,
		WFDDeviceAttrib attrib,
		WFDint value);

void wfdGetDeviceAttribiv_User(
		WFDDevice device,
		enum WFDDeviceAttribEXT attrib,
		WFDint count,
		WFDint *value);

/* Port */
WFDint wfdEnumeratePorts_User(
		WFDDevice device,
		WFDint *portIds,
		WFDint portIdsCount,
		const WFDint *filterList);

WFDPort wfdCreatePort_User(
		WFDDevice device,
		WFDint portId,
		const WFDint *attribList);

void wfdDestroyPort_User(
		WFDDevice device,
		WFDPort port);

WFDint wfdGetPortModes_User(
		WFDDevice device,
		WFDPort port,
		WFDPortMode *modes,
		WFDint modesCount);

WFDint wfdGetPortModeAttribi_User(
		WFDDevice device,
		WFDPort port,
		WFDPortMode mode,
		WFDPortModeAttrib attrib);

WFDfloat wfdGetPortModeAttribf_User(
		WFDDevice device,
		WFDPort port,
		WFDPortMode mode,
		WFDPortModeAttrib attrib);

void wfdSetPortMode_User(
		WFDDevice device,
		WFDPort port,
		WFDPortMode mode);

WFDPortMode wfdGetCurrentPortMode_User(
		WFDDevice device,
		WFDPort port);

WFDint wfdGetPortAttribi_User(
		WFDDevice device,
		WFDPort port,
		WFDPortConfigAttrib attrib);

WFDfloat wfdGetPortAttribf_User(
		WFDDevice device,
		WFDPort port,
		WFDPortConfigAttrib attrib);

void wfdGetPortAttribiv_User(
		WFDDevice device,
		WFDPort port,
		WFDPortConfigAttrib attrib,
		WFDint count,
		WFDint *value);

void wfdGetPortAttribfv_User(
		WFDDevice device,
		WFDPort port,
		WFDPortConfigAttrib attrib,
		WFDint count,
		WFDfloat *value);

void wfdSetPortAttribi_User(
		WFDDevice device,
		WFDPort port,
		WFDPortConfigAttrib attrib,
		WFDint value);

void wfdSetPortAttribf_User(
		WFDDevice device,
		WFDPort port,
		WFDPortConfigAttrib attrib,
		WFDfloat value);

void wfdSetPortAttribiv_User(
		WFDDevice device,
		WFDPort port,
		WFDPortConfigAttrib attrib,
		WFDint count,
		const WFDint *value);

void wfdSetPortAttribfv_User(
		WFDDevice device,
		WFDPort port,
		WFDPortConfigAttrib attrib,
		WFDint count,
		const WFDfloat *value);

WFDErrorCode wfdWaitForVSync_User(
		WFDDevice device,
		WFDPort port);

void wfdBindPipelineToPort_User(
		WFDDevice device,
		WFDPort port,
		WFDPipeline pipeline);

/* Pipeline */
WFDint wfdEnumeratePipelines_User(
		WFDDevice device,
		WFDint *pipelineIds,
		WFDint pipelineIdsCount,
		const WFDint *filterList);

WFDPipeline wfdCreatePipeline_User(
		WFDDevice device,
		WFDint pipelineId,
		const WFDint *attribList);

void wfdDestroyPipeline_User(
		WFDDevice device,
		WFDPipeline pipeline);

void wfdBindSourceToPipeline_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDSource source,
		WFDTransition transition,
		const WFDRect *region);

WFDint wfdGetPipelineAttribi_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDPipelineConfigAttrib attrib);

WFDfloat wfdGetPipelineAttribf_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDPipelineConfigAttrib attrib);

void wfdGetPipelineAttribiv_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDPipelineConfigAttrib attrib,
		WFDint count,
		WFDint *value);

void wfdGetPipelineAttribfv_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDPipelineConfigAttrib attrib,
		WFDint count,
		WFDfloat *value);

void wfdSetPipelineAttribi_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDPipelineConfigAttrib attrib,
		WFDint value);

void wfdSetPipelineAttribf_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDPipelineConfigAttrib attrib,
		WFDfloat value);

void wfdSetPipelineAttribiv_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDPipelineConfigAttrib attrib,
		WFDint count,
		const WFDint *value);

void wfdSetPipelineAttribfv_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDPipelineConfigAttrib attrib,
		WFDint count,
		const WFDfloat *value);

WFDint wfdGetPipelineLayerOrder_User(
		WFDDevice device,
		WFDPort port,
		WFDPipeline pipeline);

/* Source */
WFDErrorCode wfdCreateWFDEGLImagesPreAlloc_User(
		WFDDevice device,
		WFDint width,
		WFDint height,
		WFDint format,
		WFDint usage,
		WFDint count,
		WFDint size,
		WFDEGLImage *images,
		void **buffers,
		WFDint *strides,
		WFDint *offsets,
		WFDint flags);

WFDErrorCode wfdDestroyWFDEGLImages_User(
		WFDDevice device,
		WFDint count,
		WFDEGLImage *images,
		void **vaddrs);

WFDSource wfdCreateSourceFromImage_User(
		WFDDevice device,
		WFDPipeline pipeline,
		WFDEGLImage image,
		const WFDint *attribList);

void wfdDestroySource_User(
		WFDDevice device,
		WFDSource source);

/* ========== EVENT ========== */
int wire_user_register_event_listener(
		WFDDevice device,
		enum event_types type,
		union event_info *info,
		struct cb_info *cb_info);

int wire_user_request_cb(
		WFDDevice device,
		enum event_types type,
		union event_info *info);

#endif /* WIRE_USER_H */
