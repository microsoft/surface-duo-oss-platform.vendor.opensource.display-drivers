// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/habmm.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/ktime.h>

#include "wire_user.h"
#include "wire_format.h"
#include "user_os_utils.h"

/*
 * ---------------------------------------------------------------------------
 * Defines
 * ---------------------------------------------------------------------------
 */
#define WIRE_USER_LOG_MODULE_NAME		"WireUser"

#define WIRE_LOG_ERROR(fmt, ...)		\
	USER_OS_UTILS_LOG_ERROR(		\
		WIRE_USER_LOG_MODULE_NAME,	\
		fmt, ##__VA_ARGS__)

#define WIRE_LOG_WARNING(fmt, ...)		\
	USER_OS_UTILS_LOG_WARNING(		\
		WIRE_USER_LOG_MODULE_NAME,	\
		fmt, ##__VA_ARGS__)

#define WIRE_LOG_CRITICAL_INFO(fmt, ...)	\
	USER_OS_UTILS_LOG_CRITICAL_INFO(	\
		WIRE_USER_LOG_MODULE_NAME,	\
		fmt, ##__VA_ARGS__)

#define WIRE_LOG_INFO(fmt, ...)			\
	USER_OS_UTILS_LOG_INFO(			\
		WIRE_USER_LOG_MODULE_NAME,	\
		fmt, ##__VA_ARGS__)

/*
 * ---------------------------------------------------------------------------
 * Structure Definitions
 * ---------------------------------------------------------------------------
 */
struct cb_info_node {
	enum event_types type;
	union event_info info;
	struct cb_info cb_info;
	struct list_head head;
};

struct wire_context {
	struct list_head head;
	struct user_os_utils_init_info init_info;
	bool wire_isr_enable;
	bool wire_isr_stop;
	struct list_head _cb_info_ctx;
	struct mutex _event_cb_lock;
	struct task_struct *listener_thread;
	bool support_batch_mode;
};

struct wire_commit {
	struct wire_batch_packet *packet;
	u32 alloc_size;
	u32 size;
};

struct wire_device {
	WFDDevice device;
	struct wire_context *ctx;
};

struct wire_port {
	WFDPort port;
	struct wire_commit commit;
};

struct wire_pipeline {
	WFDPipeline pipeline;
	struct wire_port *port;
};

/*
 * ---------------------------------------------------------------------------
 * Global Variables
 * ---------------------------------------------------------------------------
 */

static LIST_HEAD(g_context_list);

static int event_listener(void *param);

/*
 * ---------------------------------------------------------------------------
 * Clock Utilities
 * ---------------------------------------------------------------------------
 */

static void get_timestamp(
	i64 *timestamp)
{
	ktime_t ts = ktime_get();

	*timestamp = ktime_to_us(ts);
}

/*
 * ---------------------------------------------------------------------------
 * Command Sizes
 * ---------------------------------------------------------------------------
 */
const static u32 wire_user_cmd_size[OPENWFD_CMD_MAX] = {
	[ENUMERATE_DEVICES]     = sizeof(union msg_enumerate_devices),
	[CREATE_DEVICE]         = sizeof(union msg_create_device),
	[DESTROY_DEVICE]        = sizeof(union msg_destroy_device),
	[DEVICE_COMMIT]         = sizeof(union msg_device_commit),
	[DEVICE_COMMIT_EXT]     = sizeof(union msg_device_commit_ext),
	[GET_DEVICE_ATTRIBI]    = sizeof(union msg_get_device_attribi),
	[SET_DEVICE_ATTRIBI]    = sizeof(union msg_set_device_attribi),
	[GET_DEVICE_ATTRIBIV]   = sizeof(union msg_get_device_attribiv),
	[ENUMERATE_PORTS]       = sizeof(union msg_enumerate_ports),
	[CREATE_PORT]           = sizeof(union msg_create_port),
	[DESTROY_PORT]          = sizeof(union msg_destroy_port),
	[GET_PORT_MODES]        = sizeof(union msg_get_port_modes),
	[GET_PORT_MODE_ATTRIBI] = sizeof(union msg_get_port_mode_attribi),
	[GET_PORT_MODE_ATTRIBF] = sizeof(union msg_get_port_mode_attribf),
	[SET_PORT_MODE]         = sizeof(union msg_set_port_mode),
	[GET_CURRENT_PORT_MODE] = sizeof(union msg_get_current_port_mode),
	[GET_PORT_ATTRIBI]      = sizeof(union msg_get_port_attribi),
	[GET_PORT_ATTRIBF]      = sizeof(union msg_get_port_attribf),
	[GET_PORT_ATTRIBIV]     = sizeof(union msg_get_port_attribiv),
	[GET_PORT_ATTRIBFV]     = sizeof(union msg_get_port_attribfv),
	[SET_PORT_ATTRIBI]      = sizeof(union msg_set_port_attribi),
	[SET_PORT_ATTRIBF]      = sizeof(union msg_set_port_attribf),
	[SET_PORT_ATTRIBIV]     = sizeof(union msg_set_port_attribiv),
	[SET_PORT_ATTRIBFV]     = sizeof(union msg_set_port_attribfv),
	[WAIT_FOR_VSYNC]        = sizeof(union msg_wait_for_vsync),
	[BIND_PIPELINE_TO_PORT] = sizeof(union msg_bind_pipeline_to_port),
	[ENUMERATE_PIPELINES]   = sizeof(union msg_enumerate_pipelines),
	[CREATE_PIPELINE]       = sizeof(union msg_create_pipeline),
	[DESTROY_PIPELINE]      = sizeof(union msg_destroy_pipeline),
	[GET_PIPELINE_ATTRIBI]  = sizeof(union msg_get_pipeline_attribi),
	[GET_PIPELINE_ATTRIBF]  = sizeof(union msg_get_pipeline_attribf),
	[GET_PIPELINE_ATTRIBIV] = sizeof(union msg_get_pipeline_attribiv),
	[GET_PIPELINE_ATTRIBFV] = sizeof(union msg_get_pipeline_attribfv),
	[SET_PIPELINE_ATTRIBI]  = sizeof(union msg_set_pipeline_attribi),
	[SET_PIPELINE_ATTRIBF]  = sizeof(union msg_set_pipeline_attribf),
	[SET_PIPELINE_ATTRIBIV] = sizeof(union msg_set_pipeline_attribiv),
	[SET_PIPELINE_ATTRIBFV] = sizeof(union msg_set_pipeline_attribfv),
	[BIND_SOURCE_TO_PIPELINE] = sizeof(union msg_bind_source_to_pipeline),
	[GET_PIPELINE_LAYER_ORDER] = sizeof(union msg_get_pipeline_layer_order),
	[CREATE_WFD_EGL_IMAGES] = sizeof(union msg_create_egl_images),
	[CREATE_WFD_EGL_IMAGES_PRE_ALLOC] = sizeof(union msg_create_egl_images_pre_alloc),
	[DESTROY_WFD_EGL_IMAGES] = sizeof(union msg_destroy_egl_images),
	[CREATE_SOURCE_FROM_IMAGE] = sizeof(union msg_create_source_from_image),
	[DESTROY_SOURCE]        = sizeof(union msg_destroy_source),
};

/*
 * ---------------------------------------------------------------------------
 * Performance Profile Utilities
 * ---------------------------------------------------------------------------
 */

//#define WIRE_USER_DEBUG_BATCH

//#define WIRE_USER_DEBUG_BATCH_SIM

//#define WIRE_USER_PROFILING_ENABLE

#define PROFILING_MAX					50
#define WIRE_USER_INIT_PROFILING			0
#define WFD_ENUMERATE_DEVICES_PROFILING			1
#define WFD_CREATE_DEVICE_PROFILING			2
#define WFD_DESTROY_DEVICE_PROFILING			3
#define WFD_DEVICE_COMMIT_PROFILING			4
#define WFD_DEVICE_COMMIT_EXT_PROFILING			5
#define WFD_GET_DEVICE_ATTRIBI_PROFILING		6
#define WFD_SET_DEVICE_ATTRIBI_PROFILING		7
#define WFD_GET_DEVICE_ATTRIBIV_PROFILING		8
#define WFD_ENUMERATE_PORTS_PROFILING			9
#define WFD_CREATE_PORT_PROFILING			10
#define WFD_DESTROY_PORT_PROFILING			11
#define WFD_GET_PORT_MODES_PROFILING			12
#define WFD_GET_PORT_MODE_ATTRIBI_PROFILING		13
#define WFD_GET_PORT_MODE_ATTRIBF_PROFILING		14
#define WFD_SET_PORT_MODE_PROFILING			15
#define WFD_GET_CURRENT_PORT_MODE_PROFILING		16
#define WFD_GET_PORT_ATTRIBI_PROFILING			17
#define WFD_GET_PORT_ATTRIBF_PROFILING			18
#define WFD_GET_PORT_ATTRIBIV_PROFILING			19
#define WFD_GET_PORT_ATTRIBFV_PROFILING			20
#define WFD_SET_PORT_ATTRIBI_PROFILING			21
#define WFD_SET_PORT_ATTRIBF_PROFILING			22
#define WFD_SET_PORT_ATTRIBIV_PROFILING			23
#define WFD_SET_PORT_ATTRIBFV_PROFILING			24
#define WFD_WAIT_FOR_VSYNC_PROFILING			25
#define WFD_BIND_PIPELINE_TO_PORT_PROFILING		26
#define WFD_ENUMERATE_PIPELINE_PROFILING		27
#define WFD_CREATE_PIPELINE_PROFILING			28
#define WFD_DESTROY_PIPELINE_PROFILING			29
#define WFD_BIND_SOURCE_TO_PIPELINE_PROFILING		30
#define WFD_GET_PIPELINE_ATTRIBI_PROFILING		31
#define WFD_GET_PIPELINE_ATTRIBF_PROFILING		32
#define WFD_GET_PIPELINE_ATTRIBIV_PROFILING		33
#define WFD_GET_PIPELINE_ATTRIBFV_PROFILING		34
#define WFD_SET_PIPELINE_ATTRIBI_PROFILING		35
#define WFD_SET_PIPELINE_ATTRIBF_PROFILING		36
#define WFD_SET_PIPELINE_ATTRIBIV_PROFILING		37
#define WFD_SET_PIPELINE_ATTRIBFV_PROFILING		38
#define WFD_GET_PIPELINE_LAYER_ORDER_PROFILING		39
#define WFD_CREATE_EGL_IMAGES_PROFILING			40
#define WFD_CREATE_EGL_IMAGES_PRE_ALLOC_PROFILING	41
#define WFD_DESTROY_EGL_IMAGES_PROFILING		42
#define WFD_CREATE_SOURCE_FROM_IMAGE_PROFILING		43
#define WFD_DESTROY_SOURCE_PROFILING			44

/*
 * ---------------------------------------------------------------------------
 * Head size optimization
 * ---------------------------------------------------------------------------
 */

#if (MAX_BUFS_CNT > 1) || defined(WIRE_USER_PROFILING_ENABLE)
#define WIRE_HEAP static
static struct mutex _heap_mutex[PROFILING_MAX + 1];
static bool _heap_inited;
static inline void wire_user_heap_init(void)
{
	int i;

	if (_heap_inited)
		return;

	for (i = 0; i < PROFILING_MAX + 1; i++)
		mutex_init(&_heap_mutex[i]);

	_heap_inited = true;
}
static inline void wire_user_heap_begin(u32 index)
{
	if (index >= PROFILING_MAX)
		mutex_lock(&_heap_mutex[PROFILING_MAX]);
	else
		mutex_lock(&_heap_mutex[index]);
}
static inline void wire_user_heap_end(u32 index)
{
	if (index >= PROFILING_MAX)
		mutex_unlock(&_heap_mutex[PROFILING_MAX]);
	else
		mutex_unlock(&_heap_mutex[index]);
}
#else
#define WIRE_HEAP
static inline void wire_user_heap_init(void)
{
}
static inline void wire_user_heap_begin(u32 index)
{
}
static inline void wire_user_heap_end(u32 index)
{
}
#endif

#ifdef WIRE_USER_PROFILING_ENABLE

static char func_name[PROFILING_MAX][50] = {
	"wire_user_init",
	"wfdEnumerateDevices_User",
	"wfdCreateDevice_User",
	"wfdDestroyDevice_User",
	"wfdDeviceCommit_User",
	"wfdDeviceCommitExt_User",
	"wfdGetDeviceAttribi_User",
	"wfdSetDeviceAttribi_User",
	"wfdGetDeviceAttribiv_User",
	"wfdEnumeratePorts_User",
	"wfdCreatePort_User",
	"wfdDestroyPort_User",
	"wfdGetPortModes_User",
	"wfdGetPortModeAttribi_User",
	"wfdGetPortModeAttribf_User",
	"wfdSetPortMode_User",
	"wfdGetCurrentPortMode_User",
	"wfdGetPortAttribi_User",
	"wfdGetPortAttribf_User",
	"wfdGetPortAttribiv_User",
	"wfdGetPortAttribfv_User",
	"wfdSetPortAttribi_User",
	"wfdSetPortAttribf_User",
	"wfdSetPortAttribiv_User",
	"wfdSetPortAttribfv_User",
	"wfdWaitForVSync_User",
	"wfdBindPipelineToPort_User",
	"wfdEnumeratePipelines_User",
	"wfdCreatePipeline_User",
	"wfdDestroyPipeline_User",
	"wfdBindSourceToPipeline_User",
	"wfdGetPipelineAttribi_User",
	"wfdGetPipelineAttribf_User",
	"wfdGetPipelineAttribiv_User",
	"wfdGetPipelineAttribfv_User",
	"wfdSetPipelineAttribi_User",
	"wfdSetPipelineAttribf_User",
	"wfdSetPipelineAttribiv_User",
	"wfdSetPipelineAttribfv_User",
	"wfdGetPipelineLayerOrder_User",
	"wfdCreateWFDEGLImages_User",
	"wfdCreateWFDEGLImagesPreAlloc_User",
	"wfdDestroyWFDEGLImages_User",
	"wfdCreateSourceFromImage_User",
	"wfdDestroySource_User"
};

static ktime_t time_start[PROFILING_MAX];
static ktime_t time_end[PROFILING_MAX];

static int
wire_user_profile_begin(
	u32 index)
{
	wire_user_heap_begin(index);

	if (index >= PROFILING_MAX)
		return -EINVAL;

	time_start[index] = ktime_get();

	return 0;
}

static int
wire_user_profile_end(
	u32 index,
	bool bprint)
{
	i64 time_exec;

	wire_user_heap_end(index);

	if (index >= PROFILING_MAX)
		return -EINVAL;

	time_end[index] = ktime_get();

	if (bprint) {
		time_exec = ktime_to_us(ktime_sub(time_end[index], time_start[index]));
		WIRE_LOG_INFO("%s execution time: %lld us",
			func_name[index], (long long)time_exec);
	}

	return 0;
}

#else

static int
wire_user_profile_begin(
	u32 index)
{
	wire_user_heap_begin(index);
	return 0;
}

static int
wire_user_profile_end(
	u32 index,
	bool bprint)
{
	wire_user_heap_end(index);
	return 0;
}

#endif

/*
 * ---------------------------------------------------------------------------
 * RPC Utilities
 * ---------------------------------------------------------------------------
 */

static int
prep_hdr(
	enum payload_types payload_type,
	struct wire_packet *req)
{
	struct wire_header *req_hdr = NULL;
	u32 version = 0;
	u32 payload_size = 0;
	i64 timestamp = 0;

	if (payload_type == OPENWFD_CMD) {
		version = DISPLAY_SHIM_OPENWFD_CMD_VERSION;
		payload_size = sizeof(struct openwfd_req);

	} else if ((payload_type == EVENT_REGISTRATION) ||
			(payload_type == EVENT_NOTIFICATION)) {
		version = DISPLAY_SHIM_EVENT_VERSION;
		payload_size = sizeof(struct event_req);
	} else {
		WIRE_LOG_ERROR("invalid payload_type");
		return -EINVAL;
	}

	get_timestamp(&timestamp);

	req_hdr = &req->hdr;
	req_hdr->magic_num = WIRE_FORMAT_MAGIC;
	req_hdr->version = version;
	req_hdr->payload_type = payload_type;
	req_hdr->payload_size = payload_size;
	req_hdr->timestamp = timestamp;

	return 0;
}

static int
prep_batch_hdr(
	struct wire_commit *commit)
{
	struct wire_header *req_hdr;

	req_hdr = &commit->packet->hdr;
	req_hdr->magic_num = WIRE_FORMAT_MAGIC;
	req_hdr->version = DISPLAY_SHIM_OPENWFD_CMD_VERSION;
	req_hdr->payload_type = OPENWFD_CMD;
	req_hdr->payload_size = commit->size + sizeof(struct openwfd_batch_req);
	get_timestamp(&req_hdr->timestamp);

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * Batch mode
 * ---------------------------------------------------------------------------
 */

static int
wire_port_send_recv(
	struct wire_device *device,
	struct wire_port *port,
	struct wire_packet *req,
	struct wire_packet *resp,
	u32 flags)
{
#if ENABLE_BATCH_COMMIT
	struct wire_commit *commit;
	struct wire_batch_packet *p;
	u32 type;
	u32 size;
	u8 *payload;

	if (port && device->ctx->support_batch_mode) {
		commit = &port->commit;

		if (commit->size + sizeof(struct wire_batch_packet) >= commit->alloc_size) {
			size = commit->alloc_size + SZ_4K;

			p = krealloc(commit->packet, size, GFP_KERNEL);
			if (!p)
				return -ENOMEM;

			if (!commit->alloc_size)
				memset(p, 0, sizeof(struct wire_batch_packet));

			commit->packet = p;
			commit->alloc_size = size;
		}

		type = req->payload.wfd_req.reqs[0].type;
		if (type >= OPENWFD_CMD_MAX) {
			WIRE_LOG_ERROR("invalid req type");
			return -EINVAL;
		}

		size = wire_user_cmd_size[type] + sizeof(struct openwfd_batch_cmd);
		payload = (u8 *)commit->packet->wfd_req.reqs;
		memcpy(&payload[commit->size], &req->payload.wfd_req.reqs[0], size);
		commit->size += size;
		commit->packet->wfd_req.num_of_cmds++;

#ifdef WIRE_USER_DEBUG_BATCH
		pr_info("command %d size %d total %d/%d\n",
			req->payload.wfd_req.reqs[0].type, size, commit->size, commit->alloc_size);
		print_hex_dump(KERN_INFO, "hdr: ", DUMP_PREFIX_NONE, 16, 1,
			&req->hdr, sizeof(req->hdr), false);
		print_hex_dump(KERN_INFO, "req: ", DUMP_PREFIX_NONE, 16, 1,
			&req->payload, size + sizeof(struct openwfd_batch_req), false);
#endif

#ifndef WIRE_USER_DEBUG_BATCH_SIM
		return 0;
#endif
	}
#endif
	return user_os_utils_send_recv(device->ctx->init_info.context, req, resp, flags);
}

/*
 * ---------------------------------------------------------------------------
 * Wire User APIs
 * ---------------------------------------------------------------------------
 */

int
wire_user_init(u32 client_id,
	u32 flags)
{
	struct wire_context *ctx;
	int rc = 0;

	wire_user_heap_init();

	list_for_each_entry(ctx, &g_context_list, head) {
		if (ctx->init_info.client_id == client_id) {
			WIRE_LOG_ERROR("client %d already inited\n", client_id);
			return -EINVAL;
		}
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->init_info.client_id = client_id;

	rc = user_os_utils_init(&ctx->init_info, flags);
	if (rc) {
		WIRE_LOG_ERROR("user_os_utils_init failed");
		goto fail;
	}

	/* event handling initialization */
	if ((rc == 0) && (ctx->init_info.enable_event_handling)) {
		ctx->wire_isr_enable = true;
		ctx->wire_isr_stop = false;
		/* init event callback lock */
		mutex_init(&ctx->_event_cb_lock);

		/* create event listener thread */
		ctx->listener_thread = kthread_run(event_listener, ctx,
				"wfd event listener");

		INIT_LIST_HEAD(&ctx->_cb_info_ctx);
	}

	list_add_tail(&ctx->head, &g_context_list);

	return 0;
fail:
	kfree(ctx);
	return rc;
}

int
wire_user_deinit(
	u32 client_id,
	u32 flags)
{
	struct wire_context *p, *ctx = NULL;
	int rc = 0;

	list_for_each_entry(p, &g_context_list, head) {
		if (p->init_info.client_id == client_id) {
			ctx = p;
			break;
		}
	}

	if (!ctx) {
		WIRE_LOG_ERROR("failed to find client %d\n", client_id);
		return -EINVAL;
	}

	if (ctx->init_info.enable_event_handling) {
		ctx->wire_isr_stop = true;
		ctx->wire_isr_enable = false;
	}

	rc = user_os_utils_deinit(ctx->init_info.context, 0x00);
	if (rc) {
		WIRE_LOG_ERROR("user_os_utils_deinit failed");
		goto fail;
	}

	/* event handling de-initialization */
	if (ctx->init_info.enable_event_handling) {
		mutex_destroy(&ctx->_event_cb_lock);
		kthread_stop(ctx->listener_thread);
	}

	list_del(&ctx->head);
	kfree(ctx);

fail:
	return rc;
}

int wire_user_set_host_capabilities(WFDDevice device,
		uint32_t capabilities)
{
	struct wire_device *wire_dev = device;

	if (capabilities & WFD_DEVICE_HOST_CAP_BATCH_MODE)
		wire_dev->ctx->support_batch_mode = true;

	return 0;
}

/* ========== OPENWFD ========== */

/* Device */
WFDint
wfdEnumerateDevices_User(
	WFDint *deviceIds,
	WFDint deviceIdsCount,
	const WFDint *filterList)
{
	struct wire_context *p, *ctx = NULL;
	WFDint *tmp = (WFDint *)filterList;
	u32 client_id = 0;
	void *handle;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_enumerate_devices *enum_devs;
	int i = 0;
	WFDint dev_ids_cnt = 0;

	wire_user_profile_begin(WFD_ENUMERATE_DEVICES_PROFILING);

	while ((tmp) && (*tmp != WFD_NONE) && (i < MAX_CREATE_DEVICE_ATTRIBS - 1)) {
		if (*tmp == WFD_DEVICE_CLIENT_TYPE) {
			client_id = tmp[1];
			break;
		}
		i++;
		tmp++;
	}

	if (!client_id) {
		WIRE_LOG_ERROR("can't find client id in filter list\n");
		goto end;
	}

	list_for_each_entry(p, &g_context_list, head) {
		if (p->init_info.client_id == client_id) {
			ctx = p;
			break;
		}
	}

	if (!ctx) {
		WIRE_LOG_ERROR("can't find client id %d\n", client_id);
		goto end;
	}

	handle = ctx->init_info.context;

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	req.payload.wfd_req.num_of_cmds = 1;
	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	wfd_req_cmd->type = ENUMERATE_DEVICES;

	enum_devs = (union msg_enumerate_devices *)
			&wfd_req_cmd->cmd.enumerate_devs;

	enum_devs->req.dev_ids_cnt = (deviceIds) ? (u32)deviceIdsCount : 0;

	/* loop through attribList and copy items, one at a time, until WFD_NONE
	 * or NULL is found
	 */
	tmp = (WFDint *)filterList;
	i = 0;
	while ((tmp) && (*tmp != WFD_NONE) && (i < MAX_CREATE_DEVICE_ATTRIBS - 1)) {
		enum_devs->req.filter_list[i] = *tmp;
		i++; tmp++;
	}
	/* server expects last item to be WFD_NONE */
	enum_devs->req.filter_list[i] = WFD_NONE;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	dev_ids_cnt = wfd_resp_cmd->cmd.enumerate_devs.resp.dev_ids_cnt;

	if (deviceIds) {
		dev_ids_cnt = (dev_ids_cnt > deviceIdsCount) ?
				deviceIdsCount : dev_ids_cnt;

		for (i = 0; i < dev_ids_cnt; i++) {
			*deviceIds = (WFDint)
				wfd_resp_cmd->cmd.enumerate_devs.resp.dev_ids[i];
			deviceIds++;
		}
	}

end:
	wire_user_profile_end(WFD_ENUMERATE_DEVICES_PROFILING, true);

	return dev_ids_cnt;
}

WFDDevice
wfdCreateDevice_User(
	WFDint deviceId,
	const WFDint *attribList)
{
	struct wire_device *wire_dev = NULL;
	struct wire_context *p, *ctx = NULL;
	u32 client_id = 0;
	void *handle;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	int i = 0;
	union msg_create_device *create_dev = NULL;
	WFDint *tmp = (WFDint *)attribList;
	WFDDevice dev_hdl = 0;

	wire_user_profile_begin(WFD_CREATE_DEVICE_PROFILING);

	while ((tmp) && (*tmp != WFD_NONE) && (i < MAX_CREATE_DEVICE_ATTRIBS - 1)) {
		if (*tmp == WFD_DEVICE_CLIENT_TYPE) {
			client_id = tmp[1];
			break;
		}
		i++;
		tmp++;
	}

	i = 0;
	tmp = (WFDint *)attribList;

	if (!client_id) {
		WIRE_LOG_ERROR("can't find client id in attrib list\n");
		goto end;
	}

	list_for_each_entry(p, &g_context_list, head) {
		if (p->init_info.client_id == client_id) {
			ctx = p;
			break;
		}
	}

	if (!ctx) {
		WIRE_LOG_ERROR("can't find client id %d\n", client_id);
		goto end;
	}

	handle = ctx->init_info.context;

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = CREATE_DEVICE;

	create_dev = (union msg_create_device *)&(wfd_req_cmd->cmd.create_dev);
	create_dev->req.dev_id = deviceId;

	/*
	 * loop through attribList and copy items, one at a time, until WFD_NONE
	 * or NULL is found
	 */
	while ((tmp) && (*tmp != WFD_NONE) && (i < MAX_CREATE_DEVICE_ATTRIBS - 1)) {
		create_dev->req.attrib_list[i] = *tmp;
		i++;
		tmp++;
	}
	/* server expects last item to be WFD_NONE */
	create_dev->req.attrib_list[i] = WFD_NONE;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	dev_hdl = (WFDDevice)(uintptr_t)(wfd_resp_cmd->cmd.create_dev.resp.client_dev_hdl);
	if (dev_hdl == WFD_INVALID_HANDLE)
		goto end;

	wire_dev = kzalloc(sizeof(*wire_dev), GFP_KERNEL);
	if (!wire_dev)
		goto end;

	wire_dev->device = dev_hdl;
	wire_dev->ctx = ctx;
end:

	wire_user_profile_end(WFD_CREATE_DEVICE_PROFILING, true);

	return wire_dev;
}

WFDErrorCode
wfdDestroyDevice_User(
	WFDDevice device)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_destroy_device *destroy_dev = NULL;
	WFDErrorCode sts = WFD_ERROR_NONE;

	wire_user_profile_begin(WFD_DESTROY_DEVICE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = DESTROY_DEVICE;

	destroy_dev = (union msg_destroy_device *)
			&(wfd_req_cmd->cmd.destroy_dev);
	destroy_dev->req.dev = (u32)(uintptr_t)wire_dev->device;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	sts = (WFDErrorCode)wfd_resp_cmd->cmd.destroy_dev.resp.sts;

	if (sts == WFD_ERROR_NONE)
		kfree(wire_dev);

end:

	wire_user_profile_end(WFD_DESTROY_DEVICE_PROFILING, true);

	return sts;
}

void
wfdDeviceCommitExt_User(
	WFDDevice device,
	WFDCommitType type,
	WFDHandle hdl,
	WFDint flags)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = hdl;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_device_commit_ext *dev_commit_ext = NULL;
	WFDErrorCode sts = WFD_ERROR_NONE;

	wire_user_profile_begin(WFD_DEVICE_COMMIT_EXT_PROFILING);

	if (type != WFD_COMMIT_ENTIRE_PORT) {
		WIRE_LOG_ERROR("unsupported type %d\n", type);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	req.hdr.flags |= WIRE_RESP_NOACK_FLAG;

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = DEVICE_COMMIT_EXT;

	dev_commit_ext = (union msg_device_commit_ext *)
					&(wfd_req_cmd->cmd.dev_commit_ext);
	dev_commit_ext->req.dev = (u32)(uintptr_t)wire_dev->device;
	dev_commit_ext->req.type = (u32)type;
	dev_commit_ext->req.hdl = (u32)(uintptr_t)wire_port->port;
	dev_commit_ext->req.flags = (u32)flags;

	if (wire_port_send_recv(wire_dev, wire_port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	/* reset batch commit */
	if (wire_port->commit.size) {
		prep_batch_hdr(&wire_port->commit);

#ifdef WIRE_USER_DEBUG_BATCH
		pr_info("batch size=%d\n", wire_port->commit.size);
		print_hex_dump(KERN_INFO, "hdr: ", DUMP_PREFIX_NONE, 16, 1,
			&wire_port->commit.packet->hdr, sizeof(req.hdr), false);
		print_hex_dump(KERN_INFO, "req: ", DUMP_PREFIX_NONE, 16, 1,
			&wire_port->commit.packet->wfd_req.num_of_cmds,
			wire_port->commit.size + sizeof(struct openwfd_batch_req), false);
#endif

#ifndef WIRE_USER_DEBUG_BATCH_SIM
		if (user_os_utils_send_recv(handle, (struct wire_packet *)wire_port->commit.packet,
				&resp, 0x00)) {
			WIRE_LOG_ERROR("RPC call failed");
			goto end;
		}
#endif
		wire_port->commit.size = 0;
		wire_port->commit.packet->wfd_req.num_of_cmds = 0;
	}

	sts = (WFDErrorCode)wfd_resp_cmd->cmd.dev_commit_ext.resp.sts;
	WIRE_LOG_INFO("Device commit Async returned %d from BE",
			(unsigned int)sts);

end:

	wire_user_profile_end(WFD_DEVICE_COMMIT_EXT_PROFILING, true);
}

WFDint
wfdGetDeviceAttribi_User(
	WFDDevice device,
	WFDDeviceAttrib attrib)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_device_attribi *get_dev_attribi = NULL;
	WFDint val = 0;

	wire_user_profile_begin(WFD_GET_DEVICE_ATTRIBI_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_DEVICE_ATTRIBI;

	get_dev_attribi = (union msg_get_device_attribi *)
				&(wfd_req_cmd->cmd.get_dev_attribi);
	get_dev_attribi->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_dev_attribi->req.attrib = (u32)attrib;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	val = (WFDint)wfd_resp_cmd->cmd.get_dev_attribi.resp.val;

end:

	wire_user_profile_end(WFD_GET_DEVICE_ATTRIBI_PROFILING, true);

	return val;
}

void
wfdSetDeviceAttribi_User(
	WFDDevice device,
	WFDDeviceAttrib attrib,
	WFDint value)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_device_attribi *set_dev_attribi = NULL;

	wire_user_profile_begin(WFD_SET_DEVICE_ATTRIBI_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_DEVICE_ATTRIBI;

	set_dev_attribi = (union msg_set_device_attribi *)
				&(wfd_req_cmd->cmd.set_dev_attribi);
	set_dev_attribi->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_dev_attribi->req.attrib = (u32)attrib;
	set_dev_attribi->req.val = (i32)value;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_DEVICE_ATTRIBI_PROFILING, true);
}

void
wfdGetDeviceAttribiv_User(
	WFDDevice device,
	enum WFDDeviceAttribEXT attrib,
	WFDint count,
	WFDint *value)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	int i = 0;
	union msg_get_device_attribiv *get_dev_attribiv = NULL;

	wire_user_profile_begin(WFD_GET_DEVICE_ATTRIBIV_PROFILING);

	if (count > MAX_DEVICE_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_DEVICE_ATTRIBIV;

	get_dev_attribiv = (union msg_get_device_attribiv *)
				&(wfd_req_cmd->cmd.get_dev_attribiv);
	get_dev_attribiv->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_dev_attribiv->req.attrib = (u32)attrib;
	get_dev_attribiv->req.attrib_cnt = (u32)count;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	for (i = 0; (i < count) && (i < MAX_DEVICE_ATTRIBS); i++) {
		value[i] =
			(WFDint)wfd_resp_cmd->cmd.get_dev_attribiv.resp.vals[i];
	}

end:

	wire_user_profile_end(WFD_GET_DEVICE_ATTRIBIV_PROFILING, true);
}

/* Port */

WFDint
wfdEnumeratePorts_User(
	WFDDevice device,
	WFDint *portIds,
	WFDint portIdsCount,
	const WFDint *filterList)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_enumerate_ports *enum_ports = NULL;
	int i = 0;
	WFDint port_ids_cnt = 0;

	wire_user_profile_begin(WFD_ENUMERATE_PORTS_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = ENUMERATE_PORTS;

	enum_ports = (union msg_enumerate_ports *)
			&wfd_req_cmd->cmd.enumerate_ports;

	enum_ports->req.dev = (u32)(uintptr_t)wire_dev->device;
	enum_ports->req.port_ids_cnt = (portIds) ? (u32)portIdsCount : 0;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	port_ids_cnt = wfd_resp_cmd->cmd.enumerate_ports.resp.port_ids_cnt;

	if (portIds) {
		port_ids_cnt = (port_ids_cnt > portIdsCount) ?
				portIdsCount : port_ids_cnt;

		for (i = 0; i < port_ids_cnt; i++) {
			*portIds = (WFDint)
				wfd_resp_cmd->cmd.enumerate_ports.resp.port_ids[i];
			portIds++;
		}
	}

end:

	wire_user_profile_end(WFD_ENUMERATE_PORTS_PROFILING, true);

	return port_ids_cnt;
}

WFDPort
wfdCreatePort_User(
	WFDDevice device,
	WFDint portId,
	const WFDint *attribList)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = NULL;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_create_port *create_port;
	WFDPort port_hdl = 0;

	wire_user_profile_begin(WFD_CREATE_PORT_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = CREATE_PORT;

	create_port = (union msg_create_port *)
			&wfd_req_cmd->cmd.create_port;
	create_port->req.dev = (u32)(uintptr_t)wire_dev->device;
	create_port->req.port_id = (u32)portId;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	port_hdl = (WFDPort)(uintptr_t)wfd_resp_cmd->cmd.create_port.resp.client_port_hdl;
	if (port_hdl == WFD_INVALID_HANDLE)
		goto end;

	wire_port = kzalloc(sizeof(*wire_port), GFP_KERNEL);
	if (!wire_port)
		goto end;

	wire_port->port = port_hdl;

end:

	wire_user_profile_end(WFD_CREATE_PORT_PROFILING, true);

	return wire_port;
}

void
wfdDestroyPort_User(
	WFDDevice device,
	WFDPort port)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_destroy_port *destroy_port;

	wire_user_profile_begin(WFD_DESTROY_PORT_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = DESTROY_PORT;

	destroy_port = (union msg_destroy_port *)
			&wfd_req_cmd->cmd.destroy_port;
	destroy_port->req.dev = (u32)(uintptr_t)wire_dev->device;
	destroy_port->req.port = (u32)(uintptr_t)wire_port->port;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	kfree(wire_port->commit.packet);
	kfree(wire_port);

end:

	wire_user_profile_end(WFD_DESTROY_PORT_PROFILING, true);
}

WFDint
wfdGetPortModes_User(
	WFDDevice device,
	WFDPort port,
	WFDPortMode *modes,
	WFDint modesCount)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_port_modes *get_port_mode = NULL;
	int i = 0;
	WFDint modes_cnt = 0;

	wire_user_profile_begin(WFD_GET_PORT_MODES_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PORT_MODES;

	get_port_mode = (union msg_get_port_modes *)
			&(wfd_req_cmd->cmd.get_port_modes);

	get_port_mode->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_port_mode->req.port = (u32)(uintptr_t)wire_port->port;
	get_port_mode->req.modes_cnt = (modes) ? (u32)modesCount : 0;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	modes_cnt = (WFDint)wfd_resp_cmd->cmd.get_port_modes.resp.modes_cnt;

	if (modes) {
		modes_cnt = (modes_cnt > modesCount) ?
				modesCount : modes_cnt;

		for (i = 0; i < modes_cnt; i++)
			modes[i] = (WFDPortMode)(uintptr_t)
				wfd_resp_cmd->cmd.get_port_modes.resp.modes[i];
	}

end:

	wire_user_profile_end(WFD_GET_PORT_MODES_PROFILING, true);

	return modes_cnt;
}

WFDint
wfdGetPortModeAttribi_User(
	WFDDevice device,
	WFDPort port,
	WFDPortMode mode,
	WFDPortModeAttrib attrib)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_port_mode_attribi *get_port_mode_attrib = NULL;
	WFDint val = 0;

	wire_user_profile_begin(WFD_GET_PORT_MODE_ATTRIBI_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PORT_MODE_ATTRIBI;

	get_port_mode_attrib = (union msg_get_port_mode_attribi *)
				&(wfd_req_cmd->cmd.get_port_mode_attribi);
	get_port_mode_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_port_mode_attrib->req.port = (u32)(uintptr_t)wire_port->port;
	get_port_mode_attrib->req.mode = (u32)(uintptr_t)mode;
	get_port_mode_attrib->req.attrib = (u32)attrib;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	val = (WFDint)wfd_resp_cmd->cmd.get_port_mode_attribi.resp.val;

end:

	wire_user_profile_end(WFD_GET_PORT_MODE_ATTRIBI_PROFILING, true);

	return val;
}

WFDfloat
wfdGetPortModeAttribf_User(
	WFDDevice device,
	WFDPort port,
	WFDPortMode mode,
	WFDPortModeAttrib attrib)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_port_mode_attribf *get_port_mode_attrib = NULL;
	WFDfloat val = 0;

	wire_user_profile_begin(WFD_GET_PORT_MODE_ATTRIBF_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PORT_MODE_ATTRIBF;

	get_port_mode_attrib = (union msg_get_port_mode_attribf *)
				&(wfd_req_cmd->cmd.get_port_mode_attribf);
	get_port_mode_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_port_mode_attrib->req.port = (u32)(uintptr_t)wire_port->port;
	get_port_mode_attrib->req.mode = (u32)(uintptr_t)mode;
	get_port_mode_attrib->req.attrib = (u32)attrib;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	val = (WFDfloat)wfd_resp_cmd->cmd.get_port_mode_attribi.resp.val;

end:

	wire_user_profile_end(WFD_GET_PORT_MODE_ATTRIBF_PROFILING, true);

	return val;
}

void
wfdSetPortMode_User(
	WFDDevice device,
	WFDPort port,
	WFDPortMode mode)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_port_mode *set_port_mode = NULL;

	wire_user_profile_begin(WFD_SET_PORT_MODE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PORT_MODE;

	set_port_mode = (union msg_set_port_mode *)
			&(wfd_req_cmd->cmd.set_port_mode);
	set_port_mode->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_port_mode->req.port = (u32)(uintptr_t)wire_port->port;
	set_port_mode->req.mode = (u32)(uintptr_t)mode;

	if (wire_port_send_recv(wire_dev, wire_port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PORT_MODE_PROFILING, true);
}

WFDPortMode
wfdGetCurrentPortMode_User(
	WFDDevice device,
	WFDPort port)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_current_port_mode *get_port_mode = NULL;
	WFDPortMode mode = 0;

	wire_user_profile_begin(WFD_GET_CURRENT_PORT_MODE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PORT_MODES;

	get_port_mode = (union msg_get_current_port_mode *)
			&(wfd_req_cmd->cmd.get_current_port_mode);
	get_port_mode->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_port_mode->req.port = (u32)(uintptr_t)wire_port->port;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	mode = (WFDPortMode)(uintptr_t)wfd_resp_cmd->cmd.get_current_port_mode.resp.mode;

end:

	wire_user_profile_end(WFD_GET_CURRENT_PORT_MODE_PROFILING, true);

	return mode;
}

WFDint
wfdGetPortAttribi_User(
	WFDDevice device,
	WFDPort port,
	WFDPortConfigAttrib attrib)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_port_attribi *get_port_attrib = NULL;
	WFDint val = 0;

	wire_user_profile_begin(WFD_GET_PORT_ATTRIBI_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PORT_ATTRIBI;

	get_port_attrib = (union msg_get_port_attribi *)
			&(wfd_req_cmd->cmd.get_port_attribi);
	get_port_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_port_attrib->req.port = (u32)(uintptr_t)wire_port->port;
	get_port_attrib->req.attrib = (u32)attrib;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	val = (WFDint)wfd_resp_cmd->cmd.get_port_attribi.resp.val;

end:

	wire_user_profile_end(WFD_GET_PORT_ATTRIBI_PROFILING, true);

	return val;
}

WFDfloat
wfdGetPortAttribf_User(
	WFDDevice device,
	WFDPort port,
	WFDPortConfigAttrib attrib)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_port_attribf *get_port_attrib = NULL;
	WFDfloat val = 0;

	wire_user_profile_begin(WFD_GET_PORT_ATTRIBF_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PORT_ATTRIBF;

	get_port_attrib = (union msg_get_port_attribf *)
				&(wfd_req_cmd->cmd.get_port_attribf);
	get_port_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_port_attrib->req.port = (u32)(uintptr_t)wire_port->port;
	get_port_attrib->req.attrib = (u32)attrib;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	val = (WFDint)wfd_resp_cmd->cmd.get_port_attribf.resp.val;

end:

	wire_user_profile_end(WFD_GET_PORT_ATTRIBF_PROFILING, true);

	return val;

}

void
wfdGetPortAttribiv_User(
	WFDDevice device,
	WFDPort port,
	WFDPortConfigAttrib attrib,
	WFDint count,
	WFDint *value)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_port_attribiv *get_port_attribiv = NULL;

	int i = 0;

	wire_user_profile_begin(WFD_GET_PORT_ATTRIBIV_PROFILING);

	if (count > MAX_PORT_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PORT_ATTRIBIV;

	get_port_attribiv = (union msg_get_port_attribiv *)
			&(wfd_req_cmd->cmd.get_port_attribiv);
	get_port_attribiv->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_port_attribiv->req.port = (u32)(uintptr_t)wire_port->port;
	get_port_attribiv->req.attrib = (u32)attrib;
	get_port_attribiv->req.attrib_cnt = (u32)count;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	for (i = 0; i < count; i++) {
		value[i] =
			(WFDint)wfd_resp_cmd->cmd.get_port_attribiv.resp.vals[i];
	}

end:

	wire_user_profile_end(WFD_GET_PORT_ATTRIBIV_PROFILING, true);
}

void
wfdGetPortAttribfv_User(
	WFDDevice device,
	WFDPort port,
	WFDPortConfigAttrib attrib,
	WFDint count,
	WFDfloat *value)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_port_attribfv *get_port_attribfv = NULL;

	int i = 0;

	wire_user_profile_begin(WFD_GET_PORT_ATTRIBFV_PROFILING);

	if (count > MAX_PORT_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PORT_ATTRIBFV;

	get_port_attribfv = (union msg_get_port_attribfv *)
				&(wfd_req_cmd->cmd.get_port_attribfv);
	get_port_attribfv->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_port_attribfv->req.port = (u32)(uintptr_t)wire_port->port;
	get_port_attribfv->req.attrib = (u32)attrib;
	get_port_attribfv->req.attrib_cnt = (u32)count;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	for (i = 0; i < count; i++)
		value[i] = (WFDfloat)wfd_resp_cmd->cmd.get_port_attribfv.resp.vals[i];

end:

	wire_user_profile_end(WFD_GET_PORT_ATTRIBFV_PROFILING, true);

}

void
wfdSetPortAttribi_User(
	WFDDevice device,
	WFDPort port,
	WFDPortConfigAttrib attrib,
	WFDint value)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	/* Command specific */
	union msg_set_port_attribi *set_port_attrib = NULL;

	wire_user_profile_begin(WFD_SET_PORT_ATTRIBI_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PORT_ATTRIBI;

	set_port_attrib = (union msg_set_port_attribi *)
			&(wfd_req_cmd->cmd.set_port_attribi);
	set_port_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_port_attrib->req.port = (u32)(uintptr_t)wire_port->port;
	set_port_attrib->req.attrib = (u32)attrib;
	set_port_attrib->req.val = (i32)value;

	if (wire_port_send_recv(wire_dev, wire_port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PORT_ATTRIBI_PROFILING, true);
}

void
wfdSetPortAttribf_User(
	WFDDevice device,
	WFDPort port,
	WFDPortConfigAttrib attrib,
	WFDfloat value)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_port_attribf *set_port_attrib = NULL;

	wire_user_profile_begin(WFD_SET_PORT_ATTRIBF_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PORT_ATTRIBF;

	set_port_attrib = (union msg_set_port_attribf *)
			&(wfd_req_cmd->cmd.set_port_attribf);
	set_port_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_port_attrib->req.port = (u32)(uintptr_t)wire_port->port;
	set_port_attrib->req.attrib = (u32)attrib;
	set_port_attrib->req.val = (float)value;

	if (wire_port_send_recv(wire_dev, wire_port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PORT_ATTRIBF_PROFILING, true);
}

void
wfdSetPortAttribiv_User(
	WFDDevice device,
	WFDPort port,
	WFDPortConfigAttrib attrib,
	WFDint count,
	const WFDint *value)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_port_attribiv *set_port_attrib = NULL;

	int i = 0;

	wire_user_profile_begin(WFD_SET_PORT_ATTRIBIV_PROFILING);

	if (count > MAX_PORT_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PORT_ATTRIBIV;

	set_port_attrib = (union msg_set_port_attribiv *)
				&(wfd_req_cmd->cmd.set_port_attribiv);
	set_port_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_port_attrib->req.port = (u32)(uintptr_t)wire_port->port;
	set_port_attrib->req.attrib = (u32)attrib;
	set_port_attrib->req.attrib_cnt = (u32)count;
	for (i = 0; i < count; i++)
		set_port_attrib->req.vals[i] = (i32)value[i];

	if (wire_port_send_recv(wire_dev, wire_port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PORT_ATTRIBIV_PROFILING, true);
}

void
wfdSetPortAttribfv_User(
	WFDDevice device,
	WFDPort port,
	WFDPortConfigAttrib attrib,
	WFDint count,
	const WFDfloat *value)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_port_attribfv *set_port_attrib = NULL;

	int i = 0;

	wire_user_profile_begin(WFD_SET_PORT_ATTRIBFV_PROFILING);

	if (count > MAX_PORT_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PORT_ATTRIBFV;

	set_port_attrib = (union msg_set_port_attribfv *)
				&(wfd_req_cmd->cmd.set_port_attribfv);
	set_port_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_port_attrib->req.port = (u32)(uintptr_t)wire_port->port;
	set_port_attrib->req.attrib = (u32)attrib;
	set_port_attrib->req.attrib_cnt = (u32)count;
	for (i = 0; i < count; i++)
		set_port_attrib->req.vals[i] = (i32)value[i];

	if (wire_port_send_recv(wire_dev, wire_port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PORT_ATTRIBFV_PROFILING, true);
}

WFDErrorCode
wfdWaitForVSync_User(
	WFDDevice device,
	WFDPort port)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_wait_for_vsync *wait_for_vsync = NULL;
	WFDErrorCode sts = WFD_ERROR_NONE;

	wire_user_profile_begin(WFD_WAIT_FOR_VSYNC_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		sts = WFD_ERROR_INCONSISTENCY;
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = WAIT_FOR_VSYNC;

	wait_for_vsync = (union msg_wait_for_vsync *)
			&(wfd_req_cmd->cmd.wait_for_vsync);
	wait_for_vsync->req.dev = (u32)(uintptr_t)wire_dev->device;
	wait_for_vsync->req.port = (u32)(uintptr_t)wire_port->port;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		sts = WFD_ERROR_INCONSISTENCY;
		goto end;
	}

	sts = (WFDErrorCode)wfd_resp_cmd->cmd.wait_for_vsync.resp.status;

end:

	wire_user_profile_end(WFD_WAIT_FOR_VSYNC_PROFILING, true);

	return sts;
}

void
wfdBindPipelineToPort_User(
	WFDDevice device,
	WFDPort port,
	WFDPipeline pipeline)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_bind_pipeline_to_port *bind_pipeline_to_port = NULL;

	wire_user_profile_begin(WFD_BIND_PIPELINE_TO_PORT_PROFILING);

	wire_pipeline->port = wire_port;

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	req.hdr.flags |= WIRE_RESP_NOACK_FLAG;

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = BIND_PIPELINE_TO_PORT;

	bind_pipeline_to_port = (union msg_bind_pipeline_to_port *)
			&(wfd_req_cmd->cmd.bind_pipe_to_port);
	bind_pipeline_to_port->req.dev = (u32)(uintptr_t)wire_dev->device;
	bind_pipeline_to_port->req.port = (u32)(uintptr_t)wire_port->port;
	bind_pipeline_to_port->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;

	if (wire_port_send_recv(wire_dev, wire_port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_BIND_PIPELINE_TO_PORT_PROFILING, true);
}

/* Pipeline */

WFDint
wfdEnumeratePipelines_User(
	WFDDevice device,
	WFDint *pipelineIds,
	WFDint pipelineIdsCount,
	const WFDint *filterList)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_enumerate_pipelines *enumerate_pipe = NULL;
	int i = 0;
	WFDint pipe_ids_cnt = 0;

	wire_user_profile_begin(WFD_ENUMERATE_PIPELINE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = ENUMERATE_PIPELINES;

	enumerate_pipe = (union msg_enumerate_pipelines *)
				&(wfd_req_cmd->cmd.enumerate_pipes);

	enumerate_pipe->req.dev = (u32)(uintptr_t)wire_dev->device;
	enumerate_pipe->req.pipe_ids_cnt = (pipelineIds) ?
						(u32)pipelineIdsCount : 0;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	pipe_ids_cnt =
		(WFDint)wfd_resp_cmd->cmd.enumerate_pipes.resp.pipe_ids_cnt;

	if (pipelineIds) {
		pipe_ids_cnt = (pipe_ids_cnt > pipelineIdsCount) ?
				pipelineIdsCount : pipe_ids_cnt;

		for (i = 0; i < pipe_ids_cnt; i++) {
			*pipelineIds = (WFDint)
				wfd_resp_cmd->cmd.enumerate_pipes.resp.pipe_ids[i];
			pipelineIds++;
		}
	}

end:

	wire_user_profile_end(WFD_ENUMERATE_PIPELINE_PROFILING, true);

	return pipe_ids_cnt;
}

WFDPipeline
wfdCreatePipeline_User(
	WFDDevice device,
	WFDint pipelineId,
	const WFDint *attribList)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = NULL;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_create_pipeline *create_pipe = NULL;
	WFDPipeline pipe_hdl = 0;

	wire_user_profile_begin(WFD_CREATE_PIPELINE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = CREATE_PIPELINE;

	create_pipe = (union msg_create_pipeline *)&(wfd_req_cmd->cmd.create_pipe);
	create_pipe->req.dev = (u32)(uintptr_t)wire_dev->device;
	create_pipe->req.pipe_id = (u32)pipelineId;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	pipe_hdl = (WFDPipeline)(uintptr_t)wfd_resp_cmd->cmd.create_pipe.resp.client_pipe_hdl;
	if (pipe_hdl == WFD_INVALID_HANDLE)
		goto end;

	wire_pipeline = kzalloc(sizeof(*wire_pipeline), GFP_KERNEL);
	if (!wire_pipeline)
		goto end;

	wire_pipeline->pipeline = pipe_hdl;

end:

	wire_user_profile_end(WFD_CREATE_PIPELINE_PROFILING, true);

	return wire_pipeline;
}

void
wfdDestroyPipeline_User(
	WFDDevice device,
	WFDPipeline pipeline)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_destroy_pipeline *destroy_pipe = NULL;

	wire_user_profile_begin(WFD_DESTROY_PIPELINE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = DESTROY_PIPELINE;

	destroy_pipe = (union msg_destroy_pipeline *)
			&(wfd_req_cmd->cmd.destroy_pipe);
	destroy_pipe->req.dev = (u32)(uintptr_t)wire_dev->device;
	destroy_pipe->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	kfree(wire_pipeline);

end:

	wire_user_profile_end(WFD_DESTROY_PIPELINE_PROFILING, true);
}

void
wfdBindSourceToPipeline_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDSource source,
	WFDTransition transition,
	const WFDRect *region)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_bind_source_to_pipeline *bind_source_to_pipe = NULL;

	wire_user_profile_begin(WFD_BIND_SOURCE_TO_PIPELINE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	req.hdr.flags |= WIRE_RESP_NOACK_FLAG;

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = BIND_SOURCE_TO_PIPELINE;

	bind_source_to_pipe = (union msg_bind_source_to_pipeline *)
				&(wfd_req_cmd->cmd.bind_source_to_pipe);
	bind_source_to_pipe->req.dev = (u32)(uintptr_t)wire_dev->device;
	bind_source_to_pipe->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	bind_source_to_pipe->req.source = (u64)(uintptr_t)source;
	bind_source_to_pipe->req.transition = (u32)transition;
	/* TODO: CHECK if this is needed: Based on union definition */
	//bind_source_to_pipe->req.region. = (struct rect *)region;
	//memcpy(&(bind_source_to_pipe->req.region), region, sizeof(struct rect));

	if (wire_port_send_recv(wire_dev, wire_pipeline->port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_BIND_SOURCE_TO_PIPELINE_PROFILING, true);
}

WFDint
wfdGetPipelineAttribi_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDPipelineConfigAttrib attrib)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_pipeline_attribi *get_pipe_attrib = NULL;
	WFDint val = 0;

	wire_user_profile_begin(WFD_GET_PIPELINE_ATTRIBI_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PIPELINE_ATTRIBI;

	get_pipe_attrib = (union msg_get_pipeline_attribi *)
			&(wfd_req_cmd->cmd.get_pipe_attribi);
	get_pipe_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_pipe_attrib->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	get_pipe_attrib->req.attrib = (u32)attrib;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	val = (WFDint)wfd_resp_cmd->cmd.get_pipe_attribi.resp.val;

end:

	wire_user_profile_end(WFD_GET_PIPELINE_ATTRIBI_PROFILING, true);

	return val;
}

WFDfloat
wfdGetPipelineAttribf_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDPipelineConfigAttrib attrib)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_pipeline_attribf *get_pipe_attrib = NULL;
	WFDfloat val = 0;

	wire_user_profile_begin(WFD_GET_PIPELINE_ATTRIBF_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PIPELINE_ATTRIBF;

	get_pipe_attrib = (union msg_get_pipeline_attribf *)
				&(wfd_req_cmd->cmd.get_pipe_attribf);
	get_pipe_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_pipe_attrib->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	get_pipe_attrib->req.attrib = (u32)attrib;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	val = (WFDfloat)wfd_resp_cmd->cmd.get_pipe_attribf.resp.val;

end:

	wire_user_profile_end(WFD_GET_PIPELINE_ATTRIBF_PROFILING, true);

	return val;
}

void
wfdGetPipelineAttribiv_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDPipelineConfigAttrib attrib,
	WFDint count,
	WFDint *value)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_pipeline_attribiv *get_pipe_attrib = NULL;

	int i = 0;

	wire_user_profile_begin(WFD_GET_PIPELINE_ATTRIBIV_PROFILING);

	if (count > MAX_PIPELINE_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PIPELINE_ATTRIBIV;

	get_pipe_attrib = (union msg_get_pipeline_attribiv *)
			&(wfd_req_cmd->cmd.get_pipe_attribiv);
	get_pipe_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_pipe_attrib->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	get_pipe_attrib->req.attrib = (u32)attrib;
	get_pipe_attrib->req.attrib_cnt = (u32)count;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	for (i = 0; i < count; i++) {
		value[i] =
			(WFDint)wfd_resp_cmd->cmd.get_pipe_attribiv.resp.vals[i];
	}

end:

	wire_user_profile_end(WFD_GET_PIPELINE_ATTRIBIV_PROFILING, true);
}

void
wfdGetPipelineAttribfv_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDPipelineConfigAttrib attrib,
	WFDint count,
	WFDfloat *value)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_pipeline_attribfv *get_pipe_attrib = NULL;

	int i = 0;

	wire_user_profile_begin(WFD_GET_PIPELINE_ATTRIBFV_PROFILING);

	if (count > MAX_PIPELINE_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PIPELINE_ATTRIBFV;

	get_pipe_attrib = (union msg_get_pipeline_attribfv *)
			&(wfd_req_cmd->cmd.get_pipe_attribfv);
	get_pipe_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_pipe_attrib->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	get_pipe_attrib->req.attrib = (u32)attrib;
	get_pipe_attrib->req.attrib_cnt = (u32)count;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	for (i = 0; i < count; i++) {
		value[i] =
			(WFDfloat)wfd_resp_cmd->cmd.get_pipe_attribfv.resp.vals[i];
	}

end:

	wire_user_profile_end(WFD_GET_PIPELINE_ATTRIBFV_PROFILING, true);
}

void
wfdSetPipelineAttribi_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDPipelineConfigAttrib attrib,
	WFDint value)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_pipeline_attribi *set_pipe_attrib = NULL;

	wire_user_profile_begin(WFD_SET_PIPELINE_ATTRIBI_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	req.hdr.flags |= WIRE_RESP_NOACK_FLAG;

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PIPELINE_ATTRIBI;

	set_pipe_attrib = (union msg_set_pipeline_attribi *)
			&(wfd_req_cmd->cmd.set_pipe_attribi);
	set_pipe_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_pipe_attrib->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	set_pipe_attrib->req.attrib = (u32)attrib;
	set_pipe_attrib->req.val = (i32)value;

	if (wire_port_send_recv(wire_dev, wire_pipeline->port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PIPELINE_ATTRIBI_PROFILING, true);
}

void
wfdSetPipelineAttribf_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDPipelineConfigAttrib attrib,
	WFDfloat value)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_pipeline_attribf *set_pipe_attrib = NULL;

	wire_user_profile_begin(WFD_SET_PIPELINE_ATTRIBF_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PIPELINE_ATTRIBF;

	set_pipe_attrib = (union msg_set_pipeline_attribf *)
			&(wfd_req_cmd->cmd.set_pipe_attribf);
	set_pipe_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_pipe_attrib->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	set_pipe_attrib->req.attrib = (u32)attrib;
	set_pipe_attrib->req.val = (float)value;

	if (wire_port_send_recv(wire_dev, wire_pipeline->port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PIPELINE_ATTRIBF_PROFILING, true);
}

void
wfdSetPipelineAttribiv_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDPipelineConfigAttrib attrib,
	WFDint count,
	const WFDint *value)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_pipeline_attribiv *set_pipe_attrib = NULL;

	int i = 0;

	wire_user_profile_begin(WFD_SET_PIPELINE_ATTRIBIV_PROFILING);

	if (count > MAX_PIPELINE_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	req.hdr.flags |= WIRE_RESP_NOACK_FLAG;

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PIPELINE_ATTRIBIV;

	set_pipe_attrib = (union msg_set_pipeline_attribiv *)
			&(wfd_req_cmd->cmd.set_pipe_attribiv);
	set_pipe_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_pipe_attrib->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	set_pipe_attrib->req.attrib = (u32)attrib;
	set_pipe_attrib->req.attrib_cnt = (u32)count;
	for (i = 0; i < count; i++)
		set_pipe_attrib->req.vals[i] = (i32)value[i];

	if (wire_port_send_recv(wire_dev, wire_pipeline->port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PIPELINE_ATTRIBIV_PROFILING, true);
}

void
wfdSetPipelineAttribfv_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDPipelineConfigAttrib attrib,
	WFDint count,
	const WFDfloat *value)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_set_pipeline_attribfv *set_pipe_attrib = NULL;

	int i = 0;

	wire_user_profile_begin(WFD_SET_PIPELINE_ATTRIBFV_PROFILING);

	if (count > MAX_PIPELINE_ATTRIBS) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = SET_PIPELINE_ATTRIBFV;

	set_pipe_attrib = (union msg_set_pipeline_attribfv *)
				&(wfd_req_cmd->cmd.set_pipe_attribfv);
	set_pipe_attrib->req.dev = (u32)(uintptr_t)wire_dev->device;
	set_pipe_attrib->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;
	set_pipe_attrib->req.attrib = (u32)attrib;
	set_pipe_attrib->req.attrib_cnt = (u32)count;
	for (i = 0; i < count; i++)
		set_pipe_attrib->req.vals[i] = (i32)value[i];

	if (wire_port_send_recv(wire_dev, wire_pipeline->port, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_SET_PIPELINE_ATTRIBFV_PROFILING, true);
}

WFDint
wfdGetPipelineLayerOrder_User(
	WFDDevice device,
	WFDPort port,
	WFDPipeline pipeline)
{
	struct wire_device *wire_dev = device;
	struct wire_port *wire_port = port;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_get_pipeline_layer_order *get_pipe_layer_order = NULL;
	WFDint val = 0;

	wire_user_profile_begin(WFD_GET_PIPELINE_LAYER_ORDER_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = GET_PIPELINE_LAYER_ORDER;

	get_pipe_layer_order = (union msg_get_pipeline_layer_order *)
			&(wfd_req_cmd->cmd.get_pipe_layer_order);
	get_pipe_layer_order->req.dev = (u32)(uintptr_t)wire_dev->device;
	get_pipe_layer_order->req.port = (u32)(uintptr_t)wire_port->port;
	get_pipe_layer_order->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	val = (WFDint)wfd_resp_cmd->cmd.get_pipe_layer_order.resp.order;

end:

	wire_user_profile_end(WFD_GET_PIPELINE_LAYER_ORDER_PROFILING, true);

	return val;
}

/* Source */

WFDErrorCode
wfdCreateWFDEGLImagesPreAlloc_User(
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
	WFDint flags)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_create_egl_images_pre_alloc *create_egl_images_pre_alloc = NULL;
	WFDErrorCode sts = WFD_ERROR_NONE;

	int i = 0;
	struct EglImage *wire_eglimage = NULL;
	struct WFD_EGLImageType *wfd_eglimage = NULL;
	struct user_os_utils_mem_info mem;

	wire_user_profile_begin(WFD_CREATE_EGL_IMAGES_PRE_ALLOC_PROFILING);

	if ((count > MAX_BUFS_CNT) || (count <= 0)) {
		WIRE_LOG_ERROR("count: %d invalid count value", count);
		sts = WFD_ERROR_ILLEGAL_ARGUMENT;
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		sts = WFD_ERROR_INCONSISTENCY;
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = CREATE_WFD_EGL_IMAGES_PRE_ALLOC;

	create_egl_images_pre_alloc = (union msg_create_egl_images_pre_alloc *)
				&(wfd_req_cmd->cmd.create_egl_images_pre_alloc);
	create_egl_images_pre_alloc->req.dev    = (u32)(uintptr_t)wire_dev->device;
	create_egl_images_pre_alloc->req.width  = (u32)width;
	create_egl_images_pre_alloc->req.height = (u32)height;
	create_egl_images_pre_alloc->req.format = (u32)format;
	create_egl_images_pre_alloc->req.usage  = (u32)usage;
	create_egl_images_pre_alloc->req.count  = (u32)count;
	create_egl_images_pre_alloc->req.size   = (u32)size;

	for (i = 0; i < count; i++) {
		/* this API is exclusive for buffers allocated in the user.
		 * these image buffers need to be exported
		 */
		memset((char *)&mem,
			0x00,
			sizeof(struct user_os_utils_mem_info));
		mem.size	= (u32)size;
		mem.buffer	= buffers[i];
		if (user_os_utils_shmem_export(handle, &mem, 0x00) != 0) {
			WIRE_LOG_ERROR("shmem export failed");
			sts = WFD_ERROR_INCONSISTENCY;
			goto end;
		}
		create_egl_images_pre_alloc->req.shmem_ids[i] = mem.shmem_id;
	}

	create_egl_images_pre_alloc->req.shmem_type = mem.shmem_type;

	if ((strides != NULL) && (offsets != NULL)) {
		for (i = 0; i < MAX_PLANES_CNT; i++) {
			create_egl_images_pre_alloc->req.strides[i] = strides[i];
			create_egl_images_pre_alloc->req.offsets[i] = offsets[i];
		}
	}

	create_egl_images_pre_alloc->req.flags      = (i32)flags;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		sts = WFD_ERROR_INCONSISTENCY;
		goto end;
	}

	for (i = 0; i < count; i++) {
		wire_eglimage = &wfd_resp_cmd->cmd.create_egl_images_pre_alloc.resp.eglImage[i];

		images[i] = kzalloc(sizeof(struct WFD_EGLImageType), GFP_KERNEL);
		if (images[i] == NULL) {
			WIRE_LOG_ERROR("malloc failed");
			sts = WFD_ERROR_OUT_OF_MEMORY;
			goto end;
		}

		wfd_eglimage = (struct WFD_EGLImageType *)images[i];
		wfd_eglimage->width		= width;
		wfd_eglimage->height		= height;
		wfd_eglimage->format		= format;
		wfd_eglimage->usage		= (WFDuint32)wire_eglimage->usage;
		wfd_eglimage->flags		= flags;
		wfd_eglimage->size		= (WFDuint32)size;
		wfd_eglimage->paddr		= (WFDuint64)wire_eglimage->paddr;
		wfd_eglimage->strides[0]	= (WFDuint32)wire_eglimage->strides[0];
		wfd_eglimage->strides[1]	= (WFDuint32)wire_eglimage->strides[1];
		wfd_eglimage->vaddr		= (WFDuint64)(uintptr_t)buffers[i];
		wfd_eglimage->planar_offsets[0] = (WFDuint32)wire_eglimage->planar_offsets[0];
		wfd_eglimage->planar_offsets[1]	= (WFDuint32)wire_eglimage->planar_offsets[1];
		wfd_eglimage->planar_offsets[2]	= (WFDuint32)wire_eglimage->planar_offsets[2];
		wfd_eglimage->addr_alignment	= (WFDuint32)wire_eglimage->addr_alignment;
		wfd_eglimage->image_handle	= (WFDuint64)wire_eglimage->image_handle;
		wfd_eglimage->buffer_allocator	= (WFDuint32)wire_eglimage->buffer_allocator;
		wfd_eglimage->shmem_id		= (WFDuint64)wire_eglimage->shmem_id;
		wfd_eglimage->shmem_type	= (WFDuint32)wire_eglimage->shmem_type;

		/*
		 * NOTE:
		 *   Screen is passing us a copy of WFDEGLImage rather than the
		 *   original one we allocated. This forces us to use dvaddr field to
		 *   retrieve the Host side image_handle.
		 */
		wfd_eglimage->dvaddr		= (WFDuint64)wire_eglimage->image_handle;

		WIRE_LOG_INFO("img: 0x%p %dx%d vaddr:0x%lx size:%d usage=0x%x 0x%lx dvaddr=0x%lx",
			wfd_eglimage, width, height, wfd_eglimage->vaddr,
			wfd_eglimage->size, wfd_eglimage->usage,
			wfd_eglimage->image_handle, wfd_eglimage->dvaddr);
	}

	sts = (WFDErrorCode)
		wfd_resp_cmd->cmd.create_egl_images_pre_alloc.resp.sts;

end:

	wire_user_profile_end(WFD_CREATE_EGL_IMAGES_PRE_ALLOC_PROFILING, true);

	return sts;
}

WFDErrorCode
wfdDestroyWFDEGLImages_User(
	WFDDevice device,
	WFDint count,
	WFDEGLImage *images,
	void **vaddrs)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_destroy_egl_images *destroy_egl_images = NULL;
	WFDErrorCode sts = WFD_ERROR_NONE;

	int i = 0;
	struct WFD_EGLImageType *wfd_eglimage = NULL;
	struct user_os_utils_mem_info mem;

	wire_user_profile_begin(WFD_DESTROY_EGL_IMAGES_PROFILING);

	if (count > MAX_BUFS_CNT) {
		WIRE_LOG_ERROR("count: %d exceed max count", count);
		sts = WFD_ERROR_ILLEGAL_ARGUMENT;
		goto end;
	}

	if (vaddrs)
		memset((char *)vaddrs, 0x00, sizeof(void *) * count);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		sts = WFD_ERROR_INCONSISTENCY;
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = DESTROY_WFD_EGL_IMAGES;

	destroy_egl_images = (union msg_destroy_egl_images *)
				&(wfd_req_cmd->cmd.destroy_egl_images);
	destroy_egl_images->req.dev = (u32)(uintptr_t)wire_dev->device;
	destroy_egl_images->req.count = (u32)count;
	for (i = 0; i < count; i++) {
		wfd_eglimage = (struct WFD_EGLImageType *)images[i];

		WIRE_LOG_INFO("wfd_eglimage=0x%p dvaddr=0x%lx",
				wfd_eglimage, wfd_eglimage->dvaddr);

		/* NOTE:
		 *   Screen is passing us a copy of WFDEGLImage rather than the
		 *   original one we allocated. This forces us to use dvaddr
		 *   field to retrieve the Host side image_handle.
		 */
		destroy_egl_images->req.image_handle[i] = wfd_eglimage->dvaddr;

		/* unimport buffers allocated by the host */
		if (wfd_eglimage->buffer_allocator == WFD_HOST) {
			memset((char *)&mem, 0x00,
				sizeof(struct user_os_utils_mem_info));
			mem.buffer	= (void *)(uintptr_t)
				wfd_eglimage->vaddr;
			mem.shmem_id	= (u64)wfd_eglimage->shmem_id;
			mem.shmem_type	= (u32)wfd_eglimage->shmem_type;
			if (user_os_utils_shmem_unimport(handle, &mem, 0x00) != 0) {
				WIRE_LOG_ERROR("shmem unimport failed");
				sts = WFD_ERROR_INCONSISTENCY;
				goto end;
			}
			wfd_eglimage->vaddr		= 0;
			wfd_eglimage->fd		= 0;
			wfd_eglimage->offset		= 0;
			wfd_eglimage->shmem_id		= 0;
			wfd_eglimage->shmem_type	= 0;
		}
	}

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		sts = WFD_ERROR_INCONSISTENCY;
		goto end;
	}

	for (i = 0; i < count; i++) {
		wfd_eglimage = (struct WFD_EGLImageType *)images[i];

		/* unexport buffers allocated by the user */
		if (wfd_eglimage->buffer_allocator == WFD_USER) {
			memset((char *)&mem, 0x00,
				sizeof(struct user_os_utils_mem_info));
			mem.buffer	= (void *)(uintptr_t)
				wfd_eglimage->vaddr;
			mem.shmem_id	= (u64)wfd_eglimage->shmem_id;
			mem.shmem_type	= (u32)wfd_eglimage->shmem_type;
			if (user_os_utils_shmem_unexport(handle, &mem, 0x00) != 0) {
				WIRE_LOG_ERROR("shmem unexport failed");
				sts = WFD_ERROR_INCONSISTENCY;
				goto end;
			}
			if (vaddrs) {
				vaddrs[i] = (void *)(uintptr_t)
						wfd_eglimage->vaddr;
			}
			wfd_eglimage->vaddr		= 0;
			wfd_eglimage->fd		= 0;
			wfd_eglimage->offset		= 0;
			wfd_eglimage->shmem_id		= 0;
			wfd_eglimage->shmem_type	= 0;
		}

		kfree(images[i]);
		images[i] = NULL;
	}

	sts = (WFDErrorCode)wfd_resp_cmd->cmd.destroy_egl_images.resp.sts;

end:

	wire_user_profile_end(WFD_DESTROY_EGL_IMAGES_PROFILING, true);

	return sts;
}

WFDSource
wfdCreateSourceFromImage_User(
	WFDDevice device,
	WFDPipeline pipeline,
	WFDEGLImage image,
	const WFDint *attribList)
{
	struct wire_device *wire_dev = device;
	struct wire_pipeline *wire_pipeline = pipeline;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];
	struct openwfd_cmd *wfd_resp_cmd = &resp.payload.wfd_resp.resps[0];

	/* Command specific */
	union msg_create_source_from_image *create_source_from_image = NULL;
	WFDSource src_hdl = 0;
	struct WFD_EGLImageType *wfd_eglimage = NULL;
	int i = 0;
	WFDint *tmp = (WFDint *)attribList;

	wire_user_profile_begin(WFD_CREATE_SOURCE_FROM_IMAGE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = CREATE_SOURCE_FROM_IMAGE;

	create_source_from_image = (union msg_create_source_from_image *)
			&(wfd_req_cmd->cmd.create_src_from_img);
	create_source_from_image->req.dev = (u32)(uintptr_t)wire_dev->device;
	create_source_from_image->req.pipe = (u32)(uintptr_t)wire_pipeline->pipeline;

	/* loop through attribList and copy items, one at a time, until
	 * WFD_NONE or NULL is found
	 */
	while ((tmp) && (*tmp != WFD_NONE) &&
			(i < MAX_CREATE_SOURCE_ATTRIBS - 1)) {
		create_source_from_image->req.attrib_list[i] = *tmp;
		i++; tmp++;
	}
	/* server expects last item to be WFD_NONE */
	create_source_from_image->req.attrib_list[i] = WFD_NONE;

	wfd_eglimage = (struct WFD_EGLImageType *)image;

	WIRE_LOG_INFO("wfd_eglimage=0x%p dvaddr=0x%lx",
		wfd_eglimage, wfd_eglimage->dvaddr);

	/* NOTE:
	 *   Screen is passing us a copy of WFDEGLImage rather than the
	 *   original one we allocated. This forces us to use dvaddr field to
	 *   retrieve the Host side image_handle.
	 */
	create_source_from_image->req.image_handle = (u64)wfd_eglimage->dvaddr;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	src_hdl = (WFDSource)(uintptr_t)
		wfd_resp_cmd->cmd.create_src_from_img.resp.source;

end:

	wire_user_profile_end(WFD_CREATE_SOURCE_FROM_IMAGE_PROFILING, true);

	return src_hdl;
}

void
wfdDestroySource_User(
	WFDDevice device,
	WFDSource source)
{
	struct wire_device *wire_dev = device;
	void *handle = wire_dev->ctx->init_info.context;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req, resp;
	struct openwfd_cmd *wfd_req_cmd = &req.payload.wfd_req.reqs[0];

	/* Command specific */
	union msg_destroy_source *destroy_source = NULL;

	wire_user_profile_begin(WFD_DESTROY_SOURCE_PROFILING);

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if (prep_hdr(OPENWFD_CMD, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	user_os_utils_get_id(handle, &wfd_req_cmd->client_id, 0x00);
	req.payload.wfd_req.num_of_cmds = 1;
	wfd_req_cmd->type = DESTROY_SOURCE;

	destroy_source = (union msg_destroy_source *)
				&(wfd_req_cmd->cmd.destroy_src);
	destroy_source->req.dev = (u32)(uintptr_t)wire_dev->device;
	destroy_source->req.source = (u64)(uintptr_t)source;

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

end:

	wire_user_profile_end(WFD_DESTROY_SOURCE_PROFILING, true);
}

/* ========== EVENT ========== */

static struct cb_info_node *
find_node_locked(
	enum event_types type,
	union event_info *info,
	struct list_head *list)
{
	struct cb_info_node *tmp, *node = NULL;
	struct display_event *_disp_event;
	struct vm_event *_vm_event;

	list_for_each_entry(tmp, list, head) {
		if (tmp->type == type) {
			/* if DISPLAY event, match subtype and display id */
			if (type == DISPLAY_EVENT) {
				_disp_event = (struct display_event *)info;

				if ((tmp->info.disp_event.type ==
					_disp_event->type) &&
					(tmp->info.disp_event.display_id ==
					_disp_event->display_id)) {
					node = tmp;
					break;
				}
			}

			/* if VM event, match subtype */
			if (type == VM_EVENT) {
				_vm_event = (struct vm_event *)info;

				if (tmp->info.vm_event.type ==
					_vm_event->type) {
					node = tmp;
					break;
				}
			}
		}
	}

	return node;
}

static void
event_handler(
	struct wire_context *ctx,
	struct event_req *e_req)
{
	struct cb_info_node *node;
	enum event_types type;
	union event_info info;

	if (!e_req)
		return;

	type = (enum event_types)e_req->type;

	if (type == DISPLAY_EVENT) {
		info.disp_event.type = (enum display_event_types)
					e_req->info.disp_event.type;
		info.disp_event.display_id = e_req->info.disp_event.display_id;
	} else if (type == VM_EVENT) {
		info.vm_event.type = (enum vm_event_types)
					e_req->info.vm_event.type;
	}

	mutex_lock(&ctx->_event_cb_lock);
	node = find_node_locked(type, &info, &ctx->_cb_info_ctx);
	mutex_unlock(&ctx->_event_cb_lock);

	if (node && node->cb_info.cb)
		node->cb_info.cb(type, &info, node->cb_info.user_data);
}

static int event_listener(void *param)
{
	struct wire_context *ctx = param;
	void *handle = ctx->init_info.context;
	WIRE_HEAP struct wire_packet req;
	int rc;

	while (ctx->wire_isr_enable) {
		memset((char *)&req, 0x00, sizeof(struct wire_packet));

		if (prep_hdr(EVENT_NOTIFICATION, &req)) {
			WIRE_LOG_ERROR("prep_hdr failed");
			continue;
		}

		rc = user_os_utils_recv(handle, &req, 0x00);
		if (rc && !ctx->wire_isr_stop) {
			WIRE_LOG_ERROR("user_os_utils_recv (EVENT_NOTIFICATION) failed");
			break;
		}

		/* validate packet */
		if (req.hdr.magic_num != WIRE_FORMAT_MAGIC) {
			WIRE_LOG_ERROR("Invalid magic_num=0x%x", req.hdr.magic_num);
			rc = -1;
		}
		if (req.hdr.version != DISPLAY_SHIM_EVENT_VERSION) {
			WIRE_LOG_ERROR("version mismatch should_be=0x%x req=0x%x",
				DISPLAY_SHIM_EVENT_VERSION, req.hdr.version);
			rc = -1;
		}
		if (req.hdr.payload_type != EVENT_NOTIFICATION) {
			WIRE_LOG_ERROR("wrong payload type %d",
				req.hdr.payload_type);
			rc = -1;
		}

		if (!rc) {
			/* Need to handle event callbacks outside of channel lock */
			event_handler(ctx, &req.payload.ev_req);
		}
	}

	return 0;
}

int
wire_user_register_event_listener(
	WFDDevice device,
	enum event_types type,
	union event_info *info,
	struct cb_info *cb_info)
{
	struct wire_device *wire_dev = device;
	struct wire_context *ctx = wire_dev->ctx;
	struct cb_info_node *node = NULL;
	int rc = 0;

	if (!ctx->init_info.enable_event_handling) {
		WIRE_LOG_ERROR("not supported");
		rc = -1;
		goto end;
	}

	if ((type >= EVENT_TYPE_MAX) || !info) {
		rc = -1;
		goto end;
	}

	if (((type == DISPLAY_EVENT) &&
		(info->disp_event.type >= DISPLAY_EVENT_MAX)) ||
		((type == VM_EVENT) &&
			(info->vm_event.type >= VM_EVENT_MAX))) {
		rc = -1;
		goto end;
	}

	/*
	 * for type and info, check if cb_info exists
	 * if it exists, then
	 *   a) if cb_info is valid, copy it to list entry
	 *   b) if cb_info is NULL, free list entry
	 * else
	 *   a) if cb_info is valid, malloc list entry
	 *   b) error case
	 */

	mutex_lock(&ctx->_event_cb_lock);

	node = find_node_locked(type, info, &ctx->_cb_info_ctx);
	if (node) {
		if (cb_info) {
			memcpy(&node->cb_info, cb_info, sizeof(struct cb_info));
		} else {
			list_del(&node->head);
			kfree(node);
		}
	} else if (cb_info) {
		node = kzalloc(sizeof(struct cb_info_node), GFP_KERNEL);
		if (node) {
			node->type = type;
			node->info = *info;
			memcpy(&node->cb_info, cb_info, sizeof(struct cb_info));
			list_add_tail(&node->head, &ctx->_cb_info_ctx);
		} else {
			WIRE_LOG_ERROR("malloc failed, out of memory");
			rc = -1;
		}
	} else {
		WIRE_LOG_ERROR("cb_info is NULL");
		rc = -1;
	}

	mutex_unlock(&ctx->_event_cb_lock);

end:

	return rc;
}

int
wire_user_request_cb(
	WFDDevice device,
	enum event_types type,
	union event_info *info)
{
	struct wire_device *wire_dev = device;
	struct wire_context *ctx = wire_dev->ctx;
	void *handle = ctx->init_info.context;
	int rc = 0;

	/* Request/Response */
	WIRE_HEAP struct wire_packet req;
	WIRE_HEAP struct wire_packet resp;
	struct event_req *ev_req = &req.payload.ev_req;
	struct event_resp *ev_resp = &resp.payload.ev_resp;

	wire_user_heap_begin(0);

	if (!ctx->init_info.enable_event_handling) {
		WIRE_LOG_ERROR("not supported");
		rc = -1;
		goto end;
	}

	memset((char *)&req, 0x00, sizeof(struct wire_packet));
	memset((char *)&resp, 0x00, sizeof(struct wire_packet));

	if ((type >= EVENT_TYPE_MAX) || !info) {
		rc = -1;
		goto end;
	}

	if (((type == DISPLAY_EVENT) &&
		(info->disp_event.type >= DISPLAY_EVENT_MAX)) ||
		((type == VM_EVENT) &&
			(info->vm_event.type >= VM_EVENT_MAX))) {
		rc = -1;
		goto end;
	}

	if (prep_hdr(EVENT_REGISTRATION, &req)) {
		WIRE_LOG_ERROR("prep_hdr failed");
		goto end;
	}

	req.hdr.flags |= WIRE_RESP_NOACK_FLAG;

	ev_req->type = (enum e_types)type;

	if (type == DISPLAY_EVENT) {
		ev_req->info.disp_event.type = (enum e_display_types)
						info->disp_event.type;
		ev_req->info.disp_event.display_id =
				info->disp_event.display_id;
	} else if (type == VM_EVENT) {
		ev_req->info.vm_event.type = (enum e_vm_types)
						info->vm_event.type;
	}

	if (user_os_utils_send_recv(handle, &req, &resp, 0x00)) {
		WIRE_LOG_ERROR("RPC call failed");
		goto end;
	}

	rc = ev_resp->status;

end:
	wire_user_heap_end(0);

	return rc;
}
