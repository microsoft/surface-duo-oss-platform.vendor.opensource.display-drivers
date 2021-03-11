/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _USER_OS_UTILS_H
#define _USER_OS_UTILS_H

/*
 * ---------------------------------------------------------------------------
 * Includes
 * ---------------------------------------------------------------------------
 */
#include "wire_format.h"

/*
 * ---------------------------------------------------------------------------
 * Defines
 * ---------------------------------------------------------------------------
 */
#define LOG_SEVERITY_ERROR		0x00000001
#define LOG_SEVERITY_WARNING		0x00000002
#define LOG_SEVERITY_CRITICAL_INFO	0x00000004
#define LOG_SEVERITY_INFO		0x00000008

#define LOG_SEVERITY	(LOG_SEVERITY_ERROR		|	\
			 LOG_SEVERITY_WARNING		|	\
			 LOG_SEVERITY_CRITICAL_INFO)

/*
 * ---------------------------------------------------------------------------
 * Structure/Enumeration/Union Definitions
 * ---------------------------------------------------------------------------
 */
struct user_os_utils_init_info {
	u32 enable_event_handling;
	u32 clock_id;
	u32 client_id;
	void *context;
};

struct user_os_utils_mem_info {
	void *buffer;
	u32 size;
	u64 shmem_id;
	u32 shmem_type;
	u32 fd;
	u64 offset;
	u32 id;
};

/*
 * ---------------------------------------------------------------------------
 * Function Prototypes
 * ---------------------------------------------------------------------------
 */
int
user_os_utils_init(
	struct user_os_utils_init_info *init_info,
	u32 flags);

int
user_os_utils_deinit(
	void *context,
	u32 flags);

void
user_os_utils_get_id(
	void *context,
	u32 *id,
	u32 flags);

int
user_os_utils_send_recv(
	void *context,
	struct wire_packet *req,
	struct wire_packet *resp,
	u32 flags);

int
user_os_utils_recv(
	void *context,
	struct wire_packet *req,
	u32 flags);

int
user_os_utils_shmem_export(
	void *context,
	struct user_os_utils_mem_info *mem,
	u32 flags);

int
user_os_utils_shmem_import(
	void *context,
	struct user_os_utils_mem_info *mem,
	u32 flags);

int
user_os_utils_shmem_unexport(
	void *context,
	struct user_os_utils_mem_info *mem,
	u32 flags);

int
user_os_utils_shmem_unimport(
	void *context,
	struct user_os_utils_mem_info *mem,
	u32 flags);

#define user_os_utils_log_print pr_info

#define USER_OS_UTILS_LOG_ERROR(module_name, fmt, ...)			\
	do {								\
		if (LOG_SEVERITY & LOG_SEVERITY_ERROR) {		\
			user_os_utils_log_print(			\
				"%s [%s:%d] ERROR " fmt,		\
				module_name, __func__, __LINE__,	\
				##__VA_ARGS__);				\
		}							\
	} while (0)

#define USER_OS_UTILS_LOG_WARNING(module_name, fmt, ...)		\
	do {								\
		if (LOG_SEVERITY & LOG_SEVERITY_WARNING) {		\
			user_os_utils_log_print(			\
				"%s [%s:%d] " fmt,		\
				module_name, __func__, __LINE__,	\
				##__VA_ARGS__);				\
		}							\
	} while (0)

#define USER_OS_UTILS_LOG_CRITICAL_INFO(module_name, fmt, ...)		\
	do {								\
		if (LOG_SEVERITY & LOG_SEVERITY_CRITICAL_INFO) {	\
			user_os_utils_log_print(			\
				"%s [%s:%d] " fmt,	\
				module_name, __func__, __LINE__,	\
				##__VA_ARGS__);				\
		}							\
	} while (0)

#define USER_OS_UTILS_LOG_INFO(module_name, fmt, ...)			\
	do {								\
		if (LOG_SEVERITY & LOG_SEVERITY_INFO) {			\
			user_os_utils_log_print(			\
				"%s [%s:%d] " fmt,		\
				module_name, __func__, __LINE__,	\
				##__VA_ARGS__);				\
		}							\
	} while (0)


#endif /* _USER_OS_UTILS_H */
