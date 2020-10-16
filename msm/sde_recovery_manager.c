/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
 */

#include "sde_recovery_manager.h"
#include "sde_crtc.h"

#define EVENTS_LIST_ENTRIES	64

/*
 * struct recovery_event_db - event database.
 * @event: event that client reports.
 * @crtc: crtc pointer.
 * @list: link list
 */
struct recovery_event_db {
	u32 event;
	struct drm_crtc *crtc;
	struct list_head list;
};

/*
 * struct recovery_work - event recovery work info.
 * @work: work for event handling
 * @crtc_index: crtc index.
 * @rec_mgr: recovery manager
 */
struct recovery_work {
	struct kthread_work work;
	int crtc_index;
	struct recovery_mgr_info *rec_mgr;
};

/*
 * struct recovery_event_context - Event processing context for a CRTC
 * @event_lock: spinlock for protecting event list.
 * @free_events: event entries.
 * @free_event_list: free event entry list.
 * @event_list: list of reported events.
 * @event_work: work for event handling.
 * @event_worker: queue for scheduling the event work.
 * @event_thread: worker thread.
 */
struct recovery_event_context {
	spinlock_t event_lock;
	struct recovery_event_db free_events[EVENTS_LIST_ENTRIES];
	struct list_head free_event_list;
	struct list_head event_list;

	struct recovery_work event_work;
	struct kthread_worker event_worker;
	struct task_struct *event_thread;
};

/*
 * struct recovery_mgr_info - Recovery manager information
 * @dev: drm device.
 * @client_lock: mutex lock for protecting client list.
 * @client_list: list of registered clients.
 * @event_contexts: event processing context for each CRTC.
 * @sysfs_created: if the sysfs node has been created.
 * @master_client: the default client.
 * @list: link list
 */
struct recovery_mgr_info {
	struct drm_device *dev;
	struct mutex client_lock;
	struct list_head client_list;
	struct recovery_event_context event_contexts[MAX_CRTCS];

	bool sysfs_created;

	struct sde_recovery_client master_client;

	struct list_head list;
};

static int sde_recovery_manager_client_callback(void *handle, u32 event,
		struct drm_crtc *crtc);

static int sde_crtc_null_interrupt_handler(struct drm_crtc *crtc_drm,
	bool en, struct sde_irq_callback *idle_irq);

static const struct sde_recovery_client master_client_info = {
	.name = "master_client",
	.recovery_cb = sde_recovery_manager_client_callback,
	.events = (u32[1]) { DRM_EVENT_BRIDGE_ERROR },
	.num_events = 1,
	.handle = NULL,
};

struct sde_crtc_custom_events {
	u32 event;
	int (*func)(struct drm_crtc *crtc, bool en,
			struct sde_irq_callback *irq);
};

static struct sde_crtc_custom_events custom_events[] = {
	{DRM_EVENT_SDE_UNDERRUN, sde_crtc_null_interrupt_handler},
	{DRM_EVENT_SDE_SMMUFAULT, sde_crtc_null_interrupt_handler},
	{DRM_EVENT_SDE_VSYNC_MISS, sde_crtc_null_interrupt_handler},
	{DRM_EVENT_RECOVERY_SUCCESS, sde_crtc_null_interrupt_handler},
	{DRM_EVENT_RECOVERY_FAILURE, sde_crtc_null_interrupt_handler},
	{DRM_EVENT_BRIDGE_ERROR, sde_crtc_null_interrupt_handler},
};

static ssize_t sde_recovery_mgr_rda_clients_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct drm_minor *primary;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct recovery_mgr_info *rec_mgr;
	struct list_head *pos;
	ssize_t len = 0;
	struct sde_recovery_client *client;

	primary = dev_get_drvdata(dev);
	priv = primary->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	rec_mgr = sde_kms->recovery_mgr;
	if (rec_mgr == NULL)
		return 0;

	len = snprintf(buf, PAGE_SIZE, "Clients:\n");

	mutex_lock(&rec_mgr->client_lock);

	list_for_each(pos, &rec_mgr->client_list) {
		client = list_entry(pos, struct sde_recovery_client, list);
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n",
					client->name);
	}

	mutex_unlock(&rec_mgr->client_lock);

	return len;
}

static DEVICE_ATTR(recovery_clients, 00444, sde_recovery_mgr_rda_clients_attr,
		NULL);

static struct attribute *recovery_attrs[] = {
	&dev_attr_recovery_clients.attr,
	NULL,
};

static struct attribute_group recovery_mgr_attr_group = {
	.attrs = recovery_attrs,
};

static int _sde_recovery_manager_client_null_commit(struct drm_device *dev,
		struct drm_crtc *crtc)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state = NULL;
	struct drm_crtc_state *crtc_state = NULL;
	int ret = 0;

	state = drm_atomic_state_alloc(dev);

	if (!state) {
		SDE_ERROR("%s failed to allocate atomic state, %d\n", __func__,
				ret);
		return -ENOMEM;
	}
	drm_modeset_acquire_init(&ctx, 0);
	state->acquire_ctx = &ctx;
retry:
	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto end;
	}

	if (!crtc_state->active)
		goto end;

	/* perform full mode set */
	crtc_state->mode_changed = true;
	ret = drm_atomic_commit(state);

end:
	if (ret == -EDEADLK) {
		drm_atomic_state_clear(state);
		drm_modeset_backoff(&ctx);
		goto retry;
	}
	drm_atomic_state_put(state);

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return ret;
}

static int sde_recovery_manager_client_callback(void *handle, u32 event,
		struct drm_crtc *crtc)
{
	struct drm_device *dev = handle;
	int rc = 0;

	switch (event) {
	case DRM_EVENT_BRIDGE_ERROR:
		rc = _sde_recovery_manager_client_null_commit(
			dev, crtc);
		break;

	default:
		SDE_DEBUG("Event %x unsupported\n", event);
	}
	return rc;
}

static int sde_crtc_null_interrupt_handler(struct drm_crtc *crtc_drm,
	bool en, struct sde_irq_callback *irq)
{
	return 0;
}

static void sde_recovery_mgr_notify(struct recovery_mgr_info *rec_mgr,
		u32 event, u32 payload, struct drm_crtc *crtc)
{
	struct drm_event evt;

	evt.type = event;
	evt.length = sizeof(u32);
	msm_mode_object_event_notify(&crtc->base, crtc->dev, &evt,
				(u8 *)&payload);
}

static bool sde_recovery_mgr_recover(struct recovery_mgr_info *rec_mgr,
		u32 event, struct drm_crtc *crtc)
{
	struct list_head *pos;
	struct sde_recovery_client *client;
	int rc, i;
	static bool rec_flag = true;

	mutex_lock(&rec_mgr->client_lock);
	list_for_each(pos, &rec_mgr->client_list) {
		client = list_entry(pos, struct sde_recovery_client, list);

		for (i = 0; i < client->num_events; i++) {
			if (client->events[i] != event)
				continue;

			if (client->recovery_cb) {
				rc = client->recovery_cb(client->handle,
						event, crtc);
				if (rc) {
					SDE_ERROR("%s %s recovery failed %x\n",
							__func__, client->name,
							event);
					rec_flag = false;
				} else {
					SDE_DEBUG("%s %s recovery done %x\n",
							__func__, client->name,
							event);
				}
			}

			break;
		}
	}

	mutex_unlock(&rec_mgr->client_lock);
	return rec_flag;
}

static void sde_recovery_mgr_event_work(struct kthread_work *work)
{
	struct recovery_mgr_info *rec_mgr;
	struct recovery_work *work_info;
	struct list_head *pos, *q;
	struct list_head event_list;
	struct recovery_event_db *temp_event;
	u32 event;
	bool ret;
	struct drm_crtc *crtc;
	unsigned long flags;
	int crtc_index;
	struct recovery_event_context *event_context;

	work_info = container_of(work, struct recovery_work, work);
	rec_mgr = work_info->rec_mgr;
	crtc_index = work_info->crtc_index;
	event_context = &rec_mgr->event_contexts[crtc_index];

	INIT_LIST_HEAD(&event_list);

	spin_lock_irqsave(&event_context->event_lock, flags);
	list_for_each_safe(pos, q, &event_context->event_list) {
		list_del(pos);
		list_add_tail(pos, &event_list);
	}
	spin_unlock_irqrestore(&event_context->event_lock, flags);

	list_for_each_entry(temp_event, &event_list, list) {
		event = temp_event->event;
		crtc = temp_event->crtc;

		/* notify error */
		sde_recovery_mgr_notify(rec_mgr, event, event, crtc);

		/* recover error */
		ret = sde_recovery_mgr_recover(rec_mgr, event, crtc);

		/* notify error recovery result */
		sde_recovery_mgr_notify(rec_mgr,
				ret ? DRM_EVENT_RECOVERY_SUCCESS
				: DRM_EVENT_RECOVERY_FAILURE, event, crtc);
	}

	spin_lock_irqsave(&event_context->event_lock, flags);
	list_for_each_safe(pos, q, &event_list) {
		list_del(pos);
		list_add_tail(pos, &event_context->free_event_list);
	}
	spin_unlock_irqrestore(&event_context->event_lock, flags);
}

static int sde_recovery_queue_event(struct recovery_mgr_info *rec_mgr,
		u32 event, struct drm_crtc *crtc)
{
	int rc = 0;
	int crtc_index;
	struct recovery_event_db *temp;
	unsigned long flags;
	struct recovery_event_context *event_context;

	crtc_index = drm_crtc_index(crtc);
	if (crtc_index >= MAX_CRTCS)
		return -EINVAL;
	event_context = &rec_mgr->event_contexts[crtc_index];

	spin_lock_irqsave(&event_context->event_lock, flags);

	/* check if there is same event in the list */
	list_for_each_entry(temp, &event_context->event_list, list) {
		if (event == temp->event && crtc == temp->crtc) {
			SDE_INFO("Event %x crtc %d already exist\n",
					event, crtc->base.id);
			rc = -EEXIST;
			goto out;
		}
	}

	temp = list_first_entry_or_null(&event_context->free_event_list,
			struct recovery_event_db, list);
	if (!temp) {
		SDE_ERROR("Run out of free event entry\n");
		rc = -ENOMEM;
		goto out;
	}
	list_del(&temp->list);
	temp->event = event;
	temp->crtc = crtc;
	list_add_tail(&temp->list, &event_context->event_list);
	kthread_queue_work(&event_context->event_worker,
			&event_context->event_work.work);
	SDE_DEBUG("queue event %x crtc %d id %d\n",
			event, crtc_index, crtc->base.id);

out:
	spin_unlock_irqrestore(&event_context->event_lock, flags);

	return rc;
}

int sde_recovery_set_event(struct drm_device *dev, u32 event,
		struct drm_crtc *crtc)
{
	int rc = 0;
	bool queued = false;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct recovery_mgr_info *rec_mgr;

	priv = dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	rec_mgr = sde_kms->recovery_mgr;
	if (rec_mgr == NULL)
		return -ENOENT;

	if (crtc) {
		rc = sde_recovery_queue_event(rec_mgr, event, crtc);
	} else {
		/* Populate event to all CRTCs */
		drm_for_each_crtc(crtc, dev) {
			if (crtc->enabled) {
				rc = sde_recovery_queue_event(rec_mgr, event,
					crtc);
				if (rc && rc != -EEXIST)
					break;
				if (rc == 0)
					queued = true;
			}
		}
		/*
		 * If queuing event succeed at least once,
		 * without any other errors except -EEXIST,
		 * consider as success overall.
		 */
		if (queued && rc == -EEXIST)
			rc = 0;
	}

	return rc;
}

struct sde_crtc_irq_info *sde_recovery_get_event_handler(
		struct drm_device *dev, u32 event)
{
	struct sde_crtc_irq_info *node = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(custom_events); i++) {
		if (custom_events[i].event == event &&
			custom_events[i].func) {
			node = kzalloc(sizeof(*node), GFP_KERNEL);
			if (!node)
				break;
			INIT_LIST_HEAD(&node->list);
			node->func = custom_events[i].func;
			node->event = event;
			node->state = IRQ_NOINIT;
			spin_lock_init(&node->state_lock);
			break;
		}
	}

	return node;
}

int sde_recovery_client_register(struct drm_device *dev,
		struct sde_recovery_client *client)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct recovery_mgr_info *rec_mgr;
	struct list_head *pos;
	struct sde_recovery_client *c;
	bool found = false;
	int rc = 0;

	if (!strlen(client->name)) {
		SDE_ERROR("Client name is empty\n");
		return -EINVAL;
	}

	priv = dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	rec_mgr = sde_kms->recovery_mgr;
	if (rec_mgr == NULL)
		return -ENOENT;

	mutex_lock(&rec_mgr->client_lock);

	/* check if client has been registered */
	list_for_each(pos, &rec_mgr->client_list) {
		c = list_entry(pos, struct sde_recovery_client, list);
		if (c == client) {
			found = true;
			break;
		}
	}

	if (found)
		SDE_DEBUG("Client = %s is already registered\n",
				client->name);
	else
		list_add_tail(&client->list, &rec_mgr->client_list);

	mutex_unlock(&rec_mgr->client_lock);

	return rc;
}

int sde_recovery_client_unregister(struct drm_device *dev,
		struct sde_recovery_client *client)
{
	struct recovery_mgr_info *rec_mgr;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int rc = 0;
	int i;

	if (!client) {
		SDE_ERROR("Client handle is NULL\n");
		return -EINVAL;
	}

	priv = dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	rec_mgr = sde_kms->recovery_mgr;
	if (rec_mgr == NULL)
		return -ENOENT;

	// Wait all events are processed
	for (i = 0; i < priv->num_crtcs; i++)
		kthread_flush_worker(&rec_mgr->event_contexts[i].event_worker);

	mutex_lock(&rec_mgr->client_lock);

	list_del_init(&client->list);

	mutex_unlock(&rec_mgr->client_lock);

	return rc;
}

int sde_init_recovery_mgr(struct drm_device *dev)
{
	struct recovery_mgr_info *rec = NULL;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct recovery_event_context *event_context;
	int i, j;
	int rc = 0;


	if (!dev || !dev->dev_private) {
		SDE_ERROR("drm device node invalid\n");
		return -EINVAL;
	}

	priv = dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	if (sde_kms->recovery_mgr) {
		SDE_DEBUG("recovrey manager already inited");
		return 0;
	}

	rec = kzalloc(sizeof(struct recovery_mgr_info), GFP_KERNEL);
	if (!rec)
		return -ENOMEM;

	sde_kms->recovery_mgr = rec;

	mutex_init(&rec->client_lock);

	rec->dev = dev;
	rc = sysfs_create_group(&dev->primary->kdev->kobj,
				&recovery_mgr_attr_group);
	if (rc) {
		SDE_DEBUG("Sysfs_create_group fails=%d", rc);
		rec->sysfs_created = false;
	} else {
		rec->sysfs_created = true;
	}

	INIT_LIST_HEAD(&rec->client_list);

	for (i = 0; i < priv->num_crtcs; i++) {
		event_context = &rec->event_contexts[i];

		spin_lock_init(&event_context->event_lock);

		INIT_LIST_HEAD(&event_context->event_list);

		INIT_LIST_HEAD(&event_context->free_event_list);
		for (j = 0; j < EVENTS_LIST_ENTRIES; j++) {
			list_add(&event_context->free_events[j].list,
					&event_context->free_event_list);
		}

		kthread_init_work(&event_context->event_work.work,
				sde_recovery_mgr_event_work);
		event_context->event_work.crtc_index = i;
		event_context->event_work.rec_mgr = rec;

		kthread_init_worker(&event_context->event_worker);
		event_context->event_thread = kthread_run(kthread_worker_fn,
				&event_context->event_worker,
				"recovery_event:%d", priv->crtcs[i]->base.id);
	}

	memcpy(&rec->master_client, &master_client_info,
			sizeof(struct sde_recovery_client));
	rec->master_client.handle = dev;
	rc = sde_recovery_client_register(dev, &rec->master_client);
	if (rc)
		SDE_ERROR("recovery mgr register failed %d\n", rc);

	return rc;
}

int sde_deinit_recovery_mgr(struct drm_device *dev)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct recovery_mgr_info *rec_mgr;
	struct list_head *pos, *q;
	struct sde_recovery_client *client;
	unsigned long flags;
	struct recovery_event_context *event_context;
	int i;

	priv = dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	rec_mgr = sde_kms->recovery_mgr;
	if (rec_mgr == NULL)
		return -ENOENT;

	/* Clear pending events, block new events */
	for (i = 0; i < priv->num_crtcs; i++) {
		event_context = &rec_mgr->event_contexts[i];
		spin_lock_irqsave(&event_context->event_lock, flags);
		INIT_LIST_HEAD(&event_context->event_list);
		INIT_LIST_HEAD(&event_context->free_event_list);
		spin_unlock_irqrestore(&event_context->event_lock, flags);
	}

	/* Flush work thread */
	for (i = 0; i < priv->num_crtcs; i++) {
		event_context = &rec_mgr->event_contexts[i];
		kthread_destroy_worker(&event_context->event_worker);
		kthread_stop(event_context->event_thread);
	}

	/* Remove all clients */
	mutex_lock(&rec_mgr->client_lock);
	list_for_each_safe(pos, q, &rec_mgr->client_list) {
		client = list_entry(pos, struct sde_recovery_client, list);
		list_del(&client->list);
	}
	mutex_unlock(&rec_mgr->client_lock);

	mutex_destroy(&rec_mgr->client_lock);

	if (rec_mgr->sysfs_created)
		sysfs_remove_group(&rec_mgr->dev->primary->kdev->kobj,
				&recovery_mgr_attr_group);

	kfree(rec_mgr);

	return 0;
}
