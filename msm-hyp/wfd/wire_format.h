/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _WIRE_FORMAT_H
#define _WIRE_FORMAT_H

#ifndef u64
#define u64 uint64_t
#endif

#ifndef u32
#define u32 uint32_t
#endif

#ifndef i32
#define i32 int32_t
#endif

#ifndef i64
#define i64 int64_t
#endif

#ifndef uintPtr
#define uintPtr unsigned long long
#endif

#define WIRE_FORMAT_MAGIC				\
	(((unsigned int)'d' << 24) | ((unsigned int)'i'<<16)|	\
	((unsigned int)'s' << 8) | ((unsigned int)'p'))

#define WIRE_FORMAT_VERSION				0x04

#define DISPLAY_SHIM_OPENWFD_CMD_MAJOR_REVISION		0x00000001
#define DISPLAY_SHIM_OPENWFD_CMD_MINOR_REVISION		0x00000000
#define DISPLAY_SHIM_OPENWFD_CMD_SUB_REVISION		0x00000000
#define DISPLAY_SHIM_OPENWFD_CMD_VERSION			\
	(WIRE_FORMAT_VERSION				<< 24 |	\
	DISPLAY_SHIM_OPENWFD_CMD_MAJOR_REVISION		<< 16 |	\
	DISPLAY_SHIM_OPENWFD_CMD_MINOR_REVISION		<<  8 |	\
	DISPLAY_SHIM_OPENWFD_CMD_SUB_REVISION)

#define DISPLAY_SHIM_EVENT_MAJOR_REVISION		0x00000001
#define DISPLAY_SHIM_EVENT_MINOR_REVISION		0x00000000
#define DISPLAY_SHIM_EVENT_SUB_REVISION			0x00000000
#define DISPLAY_SHIM_EVENT_VERSION				\
	(WIRE_FORMAT_VERSION				<< 24 |	\
	DISPLAY_SHIM_EVENT_MAJOR_REVISION		<< 16 |	\
	DISPLAY_SHIM_EVENT_MINOR_REVISION		<<  8 |	\
	DISPLAY_SHIM_EVENT_SUB_REVISION)

#define BIT_SHFT(_x_)	(u32)(1<<(_x_))

#define COMMIT_ASYNC_FLAG        0x1
#define COMMIT_SEND_EVENT_FLAG   0x2

#define CREATE_IMAGE_FROM_HANDLE 0x1

#define WIRE_RESP_NOACK_FLAG     0x1

#pragma pack(push, 1)
enum payload_types {
	OPENWFD_CMD,
	EVENT_REGISTRATION,
	EVENT_NOTIFICATION,
};

struct wire_header {
	u32 magic_num;
	u32 version;
	enum payload_types payload_type;
	u32 id;
	u32 payload_size;
	i64 timestamp;
	u32 flags;
};

enum wire_status {
	WIRE_STS_SUCCESS = 0,
	WIRE_STS_BAD_HANDLE,
	WIRE_STS_BAD_PARAM,
	WIRE_STS_NOT_SUPPORTED,
	WIRE_STS_NO_RESOURCES,
	WIRE_STS_TIMEOUT,
	WIRE_STS_FAILED
};

/*
 * ---------------------------------------------------------------------------
 * Display Core Definitions
 * ---------------------------------------------------------------------------
 */
/*  Defines displays ID */
enum display_types_bitwise {
	DISPLAY_BITWISE_DSI_0	=  BIT_SHFT(0),	/* for first display DSI */
	DISPLAY_BITWISE_DSI_1	=  BIT_SHFT(1),	/* for secondary display DSI */
	DISPLAY_BITWISE_HDMI	=  BIT_SHFT(2),	/* for HDMI display */
	DISPLAY_BITWISE_ROTATOR	=  BIT_SHFT(3),	/* not supported */
	DISPLAY_BITWISE_ALL	= (BIT_SHFT(4) - 1),
};

/*
 * ---------------------------------------------------------------------------
 * OpenWFD Definitions
 * ---------------------------------------------------------------------------
 */

#define MAX_OPENWFD_CMDS			1
#define MAX_DEVICE_CNT				2
#define MAX_CREATE_DEVICE_ATTRIBS		5
#define MAX_DEVICE_ATTRIBS			5
#define MAX_DISPLAY_COMP_INFO_BUFFER_SIZE	1024
#define MAX_PORT_CNT				8
#define MAX_CREATE_PORT_ATTRIBS			20
#define MAX_PORT_MODES_CNT			10
#define MAX_PORT_ATTRIBS			40
#define MAX_PIPELINE_CNT			12
#define MAX_CREATE_PIPELINE_ATTRIBS		20
#define MAX_PIPELINE_ATTRIBS			30
#define MAX_BUFS_CNT				32
#define MAX_CREATE_SOURCE_ATTRIBS		3
#define MAX_PLANES_CNT				4
#define ENABLE_BATCH_COMMIT			1

enum openwfd_cmd_type {
	/* Device Commands */
	DEVICE_CMD_START,
	ENUMERATE_DEVICES = DEVICE_CMD_START,
	CREATE_DEVICE,
	DESTROY_DEVICE,
	DEVICE_COMMIT,
	DEVICE_COMMIT_EXT,
	GET_DEVICE_ATTRIBI,
	SET_DEVICE_ATTRIBI,
	GET_DEVICE_ATTRIBIV,
	DEVICE_CMD_END = GET_DEVICE_ATTRIBIV,

	/* Port Commands */
	PORT_CMD_START,
	ENUMERATE_PORTS = PORT_CMD_START,
	CREATE_PORT,
	DESTROY_PORT,
	GET_PORT_MODES,
	GET_PORT_MODE_ATTRIBI,
	GET_PORT_MODE_ATTRIBF,
	SET_PORT_MODE,
	GET_CURRENT_PORT_MODE,
	GET_PORT_ATTRIBI,
	GET_PORT_ATTRIBF,
	GET_PORT_ATTRIBIV,
	GET_PORT_ATTRIBFV,
	SET_PORT_ATTRIBI,
	SET_PORT_ATTRIBF,
	SET_PORT_ATTRIBIV,
	SET_PORT_ATTRIBFV,
	WAIT_FOR_VSYNC,
	BIND_PIPELINE_TO_PORT,
	PORT_CMD_END = BIND_PIPELINE_TO_PORT,

	/* Pipeline Commands */
	PIPELINE_CMD_START,
	ENUMERATE_PIPELINES = PIPELINE_CMD_START,
	CREATE_PIPELINE,
	DESTROY_PIPELINE,
	GET_PIPELINE_ATTRIBI,
	GET_PIPELINE_ATTRIBF,
	GET_PIPELINE_ATTRIBIV,
	GET_PIPELINE_ATTRIBFV,
	SET_PIPELINE_ATTRIBI,
	SET_PIPELINE_ATTRIBF,
	SET_PIPELINE_ATTRIBIV,
	SET_PIPELINE_ATTRIBFV,
	BIND_SOURCE_TO_PIPELINE,
	GET_PIPELINE_LAYER_ORDER,
	PIPELINE_CMD_END = GET_PIPELINE_LAYER_ORDER,

	/* Source Commands */
	SOURCE_CMD_START,
	CREATE_WFD_EGL_IMAGES = SOURCE_CMD_START,
	CREATE_WFD_EGL_IMAGES_PRE_ALLOC,
	DESTROY_WFD_EGL_IMAGES,
	CREATE_SOURCE_FROM_IMAGE,
	DESTROY_SOURCE,
	SOURCE_CMD_END = DESTROY_SOURCE,

	OPENWFD_CMD_MAX
};

/* Device Commands */
union msg_enumerate_devices {
	struct {
		u32 dev_ids_cnt;
		i32 filter_list[MAX_CREATE_DEVICE_ATTRIBS];
	} req;

	struct {
		u32 dev_ids_cnt;
		u32 dev_ids[MAX_DEVICE_CNT];
	} resp;
};
union msg_create_device {
	struct {
		u32 dev_id;
		i32 attrib_list[MAX_CREATE_DEVICE_ATTRIBS]; /* TODO: correct? */
	} req;

	struct {
		u32 client_dev_hdl; /* WFDDevice */
	} resp;
};
union msg_destroy_device {
	struct {
		u32 dev; /* WFDDevice */
	} req;

	struct {
		u32 sts; /* WFDErrorCode */
	} resp;
};
union msg_device_commit {
	struct {
		u32 dev; /* WFDDevice */
		u32 type; /* WFDCommitType */
		u32 hdl; /* WFDHandle */
	} req;

	struct {
		u32 sts; /* WFDErrorCode */
	} resp;
};
union msg_device_commit_ext {
	struct {
		u32 dev; /* WFDDevice */
		u32 type; /* WFDCommitType */
		u32 hdl; /* WFDHandle */
		u32 flags; /* WFDuint32 */
	} req;

	struct {
		u32 sts; /* WFDErrorCode */
	} resp;
};
union msg_get_device_attribi {
	struct {
		u32 dev; /* WFDDevice */
		u32 attrib; /* WFDDeviceAttrib */
	} req;

	struct {
		i32 val; /* WFDint */
	} resp;
};
union msg_set_device_attribi {
	struct {
		u32 dev; /* WFDDevice */
		u32 attrib; /* WFDDeviceAttrib */
		i32 val; /* WFDint */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_get_device_attribiv {
	struct {
		u32 dev; /* WFDDevice */
		u32 attrib; /* WFDDeviceAttrib */
		u32 attrib_cnt;
	} req;

	struct {
		i32 vals[MAX_DEVICE_ATTRIBS]; /* WFDint */
	} resp;
};

/* Port Commands */
union msg_enumerate_ports {
	struct {
		u32 dev; /* WFDDevice */
		u32 port_ids_cnt;
	} req;

	struct {
		u32 port_ids_cnt;
		u32 port_ids[MAX_PORT_CNT];
	} resp;
};
union msg_create_port {
	struct {
		u32 dev; /* WFDDevice */
		u32 port_id;
	} req;

	struct {
		u32 client_port_hdl; /* WFDPort */
	} resp;
};
union msg_destroy_port {
	struct {
		u32 dev; /* WFDDevice */
		u32  port; /* WFDPort */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_get_port_modes {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 modes_cnt;
	} req;

	struct {
		u32 modes_cnt;
		u32 modes[MAX_PORT_MODES_CNT]; /* WFDPortMode */
	} resp;
};
union msg_get_port_mode_attribi {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 mode; /* WFDPortMode */
		u32 attrib; /* WFDPortModeAttrib */
	} req;

	struct {
		i32 val; /* WFDint */
	} resp;
};
union msg_get_port_mode_attribf {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 mode; /* WFDPortMode */
		u32 attrib; /* WFDPortModeAttrib */
	} req;

	struct {
		float val; /* WFDfloat */
	} resp;
};
union msg_set_port_mode {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 mode; /* WFDPortMode */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_get_current_port_mode {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
	} req;

	struct {
		u32 mode; /* WFDPortMode */
	} resp;
};
union msg_get_port_attribi {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 attrib; /* WFDPortConfigAttrib */
	} req;

	struct {
		i32 val; /* WFDint */
	} resp;
};
union msg_get_port_attribf {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 attrib; /* WFDPortConfigAttrib */
	} req;

	struct {
		float val; /* WFDfloat */
	} resp;
};
union msg_get_port_attribiv {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 attrib; /* WFDPortConfigAttrib */
		u32 attrib_cnt;
	} req;

	struct {
		i32 vals[MAX_PORT_ATTRIBS]; /* WFDint */
	} resp;
};
union msg_get_port_attribfv {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 attrib; /* WFDPortConfigAttrib */
		u32 attrib_cnt;
	} req;

	struct {
		float vals[MAX_PORT_ATTRIBS]; /* WFDfloat */
	} resp;
};
union msg_set_port_attribi {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 attrib; /* WFDPortConfigAttrib */
		i32 val; /* WFDint */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_set_port_attribf {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 attrib; /* WFDPortConfigAttrib */
		float val; /* WFDfloat */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_set_port_attribiv {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 attrib; /* WFDPortConfigAttrib */
		u32 attrib_cnt;
		i32 vals[MAX_PORT_ATTRIBS]; /* WFDint */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_set_port_attribfv {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 attrib; /* WFDPortConfigAttrib */
		u32 attrib_cnt;
		float vals[MAX_PORT_ATTRIBS]; /* WFDfloat */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_wait_for_vsync {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
	} req;

	struct {
		u32 status; /* WFDErrorCode */
	} resp;
};
union msg_bind_pipeline_to_port {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 pipe; /* WFDPipeline */
	} req;

	struct {
		/* void */
	} resp;
};

/* Pipeline commands */
union msg_enumerate_pipelines {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe_ids_cnt;
	} req;

	struct {
		u32 pipe_ids_cnt;
		u32 pipe_ids[MAX_PIPELINE_CNT];
	} resp;
};
union msg_create_pipeline {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe_id;
	} req;

	struct {
		u32 client_pipe_hdl; /* WFDPipeline */
	} resp;
};
union msg_destroy_pipeline {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_get_pipeline_attribi {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u32 attrib; /* WFDPipelineConfigAttrib */
	} req;

	struct {
		i32 val; /* WFDint */
	} resp;
};
union msg_get_pipeline_attribf {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u32 attrib; /* WFDPipelineConfigAttrib */
	} req;

	struct {
		float val; /* WFDfloat */
	} resp;
};
union msg_get_pipeline_attribiv {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u32 attrib; /* WFDPipelineConfigAttrib */
		u32 attrib_cnt;
	} req;

	struct {
		i32 vals[MAX_PIPELINE_ATTRIBS]; /* WFDint */
	} resp;
};
union msg_get_pipeline_attribfv {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u32 attrib; /* WFDPipelineConfigAttrib */
		u32 attrib_cnt;
	} req;

	struct {
		float vals[MAX_PIPELINE_ATTRIBS]; /* WFDfloat */
	} resp;
};
union msg_set_pipeline_attribi {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u32 attrib; /* WFDPipelineConfigAttrib */
		i32 val; /* WFDint */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_set_pipeline_attribf {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u32 attrib; /* WFDPipelineConfigAttrib */
		float val; /* WFDfloat */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_set_pipeline_attribiv {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u32 attrib; /* WFDPipelineConfigAttrib */
		u32 attrib_cnt;
		i32 vals[MAX_PIPELINE_ATTRIBS]; /* WFDint */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_set_pipeline_attribfv {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u32 attrib; /* WFDPipelineConfigAttrib */
		u32 attrib_cnt;
		float vals[MAX_PIPELINE_ATTRIBS]; /* WFDfloat */
	} req;

	struct {
		/* void */
	} resp;
};
struct rect {
	i32 offsetX;
	i32 offsetY;
	i32 width;
	i32 height;
};
union msg_bind_source_to_pipeline {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u64 source; /* WFDSource */
		u32 transition; /* WFDTransition */
		struct rect region; /* TODO: need this? WFDRect * */
	} req;

	struct {
		/* void */
	} resp;
};
union msg_get_pipeline_layer_order {
	struct {
		u32 dev; /* WFDDevice */
		u32 port; /* WFDPort */
		u32 pipe; /* WFDPipeline */
	} req;

	struct {
		i32 order; /* WFDint */
	} resp;
};

/* Source Commands */
enum ShMemType {
	PMEM_ID,
	MMAP_VA,
	HAB_EXPORT_ID,
	PMEM_HDL,
};
enum BufferAllocator {
	WFD_HOST,
	WFD_USER,
};

struct EglImage {
	u32 width;
	u32 height;
	u32 format;
	u32 usage;
	u32 flags;
	u32 fd;
	u64 offset;
	u32 size;
	u64 paddr;
	u32 strides[2];
	u64 vaddr;
	u32 planar_offsets[3];
	u32 addr_alignment;
	u64 image_handle;
	u32 buffer_allocator;
	u64 shmem_id;
	u32 shmem_type;
};

union msg_create_egl_images {
	struct {
		u32 dev; /* WFDDevice */
		u32 width;
		u32 height;
		u32 format;
		u32 usage;
		u32 count;
		u32 flags;
	} req;

	struct {
		struct EglImage eglImage[MAX_BUFS_CNT];
		u32 sts; /* WFDErrorCode */
	} resp;
};

union msg_create_egl_images_pre_alloc {
	struct {
		u32 dev; /* WFDDevice */
		u32 width;
		u32 height;
		u32 format;
		u32 usage;
		u32 count;
		u32 size;
		u64 shmem_ids[MAX_BUFS_CNT];
		u32 shmem_type;
		u32 strides[MAX_PLANES_CNT];
		u32 offsets[MAX_PLANES_CNT];
		u32 flags;
	} req;

	struct {
		struct EglImage eglImage[MAX_BUFS_CNT];
		u32 sts; /* WFDErrorCode */
	} resp;
};

union msg_destroy_egl_images {
	struct {
		u32 dev; /* WFDDevice */
		u32 count;
		u64 image_handle[MAX_BUFS_CNT];
	} req;

	struct {
		u32 sts; /* WFDErrorCode */
	} resp;
};

union msg_create_source_from_image {
	struct {
		u32 dev; /* WFDDevice */
		u32 pipe; /* WFDPipeline */
		u64 image_handle;
		i32 attrib_list[MAX_CREATE_SOURCE_ATTRIBS];
	} req;

	struct {
		u64 source; /* WFDSource */
	} resp;
};

union msg_destroy_source {
	struct {
		u32 dev; /* WFDDevice */
		u64 source; /* WFDSource */
	} req;

	struct {
		/* void */
	} resp;
};

struct openwfd_cmd {
	u32 display_id;
	u32 client_id;
	enum openwfd_cmd_type type;
	union {
		/* Device Commands */
		union msg_enumerate_devices enumerate_devs;
		union msg_create_device create_dev;
		union msg_destroy_device destroy_dev;
		union msg_device_commit dev_commit;
		union msg_device_commit_ext dev_commit_ext;
		union msg_get_device_attribi get_dev_attribi;
		union msg_set_device_attribi set_dev_attribi;
		union msg_get_device_attribiv get_dev_attribiv;

		/* Port Commands */
		union msg_enumerate_ports enumerate_ports;
		union msg_create_port create_port;
		union msg_destroy_port destroy_port;
		union msg_get_port_modes get_port_modes;
		union msg_get_port_mode_attribi get_port_mode_attribi;
		union msg_get_port_mode_attribf get_port_mode_attribf;
		union msg_set_port_mode set_port_mode;
		union msg_get_current_port_mode get_current_port_mode;
		union msg_get_port_attribi get_port_attribi;
		union msg_get_port_attribf get_port_attribf;
		union msg_get_port_attribiv get_port_attribiv;
		union msg_get_port_attribfv get_port_attribfv;
		union msg_set_port_attribi set_port_attribi;
		union msg_set_port_attribf set_port_attribf;
		union msg_set_port_attribiv set_port_attribiv;
		union msg_set_port_attribfv set_port_attribfv;
		union msg_wait_for_vsync wait_for_vsync;
		union msg_bind_pipeline_to_port bind_pipe_to_port;

		/* Pipeline Commands */
		union msg_enumerate_pipelines enumerate_pipes;
		union msg_create_pipeline create_pipe;
		union msg_destroy_pipeline destroy_pipe;
		union msg_get_pipeline_attribi get_pipe_attribi;
		union msg_get_pipeline_attribf get_pipe_attribf;
		union msg_get_pipeline_attribiv get_pipe_attribiv;
		union msg_get_pipeline_attribfv get_pipe_attribfv;
		union msg_set_pipeline_attribi set_pipe_attribi;
		union msg_set_pipeline_attribf set_pipe_attribf;
		union msg_set_pipeline_attribiv set_pipe_attribiv;
		union msg_set_pipeline_attribfv set_pipe_attribfv;
		union msg_bind_source_to_pipeline bind_source_to_pipe;
		union msg_get_pipeline_layer_order get_pipe_layer_order;

		/* Source Commands */
		union msg_create_egl_images create_egl_images;
		union msg_create_egl_images_pre_alloc create_egl_images_pre_alloc;
		union msg_destroy_egl_images destroy_egl_images;
		union msg_create_source_from_image create_src_from_img;
		union msg_destroy_source destroy_src;
	} cmd;
};

struct openwfd_req {
	u32 num_of_cmds;
	struct openwfd_cmd reqs[MAX_OPENWFD_CMDS];
};

struct openwfd_resp {
	u32 num_of_cmds;
	struct openwfd_cmd resps[MAX_OPENWFD_CMDS];
	u32 status;
};

struct openwfd_batch_cmd {
	u32 display_id;
	u32 client_id;
	enum openwfd_cmd_type type;
	u8 cmd[];
};

struct openwfd_batch_req {
	u32 num_of_cmds;
	struct openwfd_batch_cmd reqs[];
};

/*
 * ---------------------------------------------------------------------------
 * Event Definitions
 * ---------------------------------------------------------------------------
 */

enum e_display_types {
	DISP_VSYNC,
	DISP_COMMIT_COMPLETE,
	DISP_HPD,
	DISP_RECOVERY,
	DISP_EVENT_MAX
};
struct e_display {
	enum e_display_types type;
	int display_id;
};

enum e_vm_types {
	VM_EVENT_RESTART,
	VM_EVENT_SHUTDOWN,
	VM_EVENT_TYPE_MAX
};
struct e_vm {
	enum e_vm_types type;
};

enum e_types {
	DISPLAY_EVENT_TYPE,
	VM_EVENT_TYPE,
	EVENT_TYPES_MAX
};
union e_info {
	struct e_display disp_event;
	struct e_vm vm_event;
};

struct event_req {
	enum e_types type;
	union e_info info;
};

struct event_resp {
	u32 status;
};

/*
 * ---------------------------------------------------------------------------
 * Packet Definitions
 * ---------------------------------------------------------------------------
 */

union wire_payload {
	struct openwfd_req wfd_req;
	struct openwfd_resp wfd_resp;
	struct event_req ev_req;
	struct event_resp ev_resp;
};

struct wire_packet {
	struct wire_header hdr;
	union wire_payload payload;
};

struct wire_batch_packet {
	struct wire_header hdr;
	struct openwfd_batch_req wfd_req;
};

#pragma pack(pop)


/*
 * ---------------------------------------------------------------------------
 * Functions
 * ---------------------------------------------------------------------------
 */

#endif /* _WIRE_FORMAT_H */
