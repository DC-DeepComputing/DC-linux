/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_PLANE_H__
#define __VS_PLANE_H__

#include <drm/drm_plane_helper.h>

#include "vs_drv.h"

struct vs_plane *vs_plane_primary_create(struct drm_device *drm_dev,
					 struct vs_plane_primary_info *info,
					 unsigned int layer_num,
					 unsigned int possible_crtcs);

struct vs_plane *vs_plane_overlay_create(struct drm_device *drm_dev,
					 struct vs_plane_overlay_info *info,
					 unsigned int layer_num,
					 unsigned int possible_crtcs);

struct vs_plane *vs_plane_cursor_create(struct drm_device *drm_dev,
					struct vs_plane_cursor_info *info,
					unsigned int possible_crtcs);
#endif /* __VS_PLANE_H__ */
