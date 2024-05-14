/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_CRTC_H__
#define __VS_CRTC_H__

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "vs_drv.h"

struct vs_crtc *vs_crtc_create(struct drm_device *drm_dev,
			       struct vs_dc_info *info);

#endif /* __VS_CRTC_H__ */
