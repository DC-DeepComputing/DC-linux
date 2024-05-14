/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_DRV_H__
#define __VS_DRV_H__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/reset.h>

#include <drm/drm_drv.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem.h>
#include <drm/drm_managed.h>

#include "vs_dc_hw.h"
#include "vs_simple_enc.h"

/*@pitch_alignment: buffer pitch alignment required by sub-devices.*/
struct vs_drm_device {
	struct drm_device base;
	unsigned int pitch_alignment;
	/* clocks */
	unsigned int clk_count;
	struct clk_bulk_data	*clks;
	struct reset_control	*rsts;
	struct vs_dc dc;
	int irq;
};

static inline struct vs_drm_device *
to_vs_drm_private(const struct drm_device *dev)
{
	return container_of(dev, struct vs_drm_device, base);
}

struct vs_crtc_state {
	struct drm_crtc_state base;

	u32 output_fmt;
	u8 encoder_type;
	u8 bpp;
};

struct vs_crtc {
	struct drm_crtc base;
	struct device *dev;
};

static inline u8 to_vs_display_id(struct vs_dc *dc, struct drm_crtc *crtc)
{
	u8 panel_num = dc->hw.info->panel_num;
	u32 index = drm_crtc_index(crtc);
	int i;

	for (i = 0; i < panel_num; i++) {
		if (index == dc->crtc[i]->base.index)
			return i;
	}

	return 0;
}

static inline struct vs_crtc_state *
to_vs_crtc_state(struct drm_crtc_state *state)
{
	return container_of(state, struct vs_crtc_state, base);
}

struct vs_plane_state {
	struct drm_plane_state base;
	dma_addr_t dma_addr[DRM_FORMAT_MAX_PLANES];
};

struct vs_plane {
	struct drm_plane base;
	u8 id;
};

static inline struct vs_plane *to_vs_plane(struct drm_plane *plane)
{
	return container_of(plane, struct vs_plane, base);
}

static inline struct vs_plane_state *
to_vs_plane_state(struct drm_plane_state *state)
{
	return container_of(state, struct vs_plane_state, base);
}

#ifdef CONFIG_DRM_INNO_STARFIVE_HDMI
extern struct platform_driver starfive_hdmi_driver;
#endif

#endif /* __VS_DRV_H__ */
