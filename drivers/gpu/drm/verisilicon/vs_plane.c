// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_plane.h>
#include <drm/drm_plane_helper.h>

#include "vs_plane.h"

static void vs_plane_atomic_destroy_state(struct drm_plane *plane,
					  struct drm_plane_state *state)
{
	struct vs_plane_state *vs_plane_state = to_vs_plane_state(state);

	__drm_atomic_helper_plane_destroy_state(state);
	kfree(vs_plane_state);
}

static void vs_plane_reset(struct drm_plane *plane)
{
	struct vs_plane_state *state;
	struct vs_plane *vs_plane = to_vs_plane(plane);

	if (plane->state)
		vs_plane_atomic_destroy_state(plane, plane->state);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return;

	state->base.zpos = vs_plane->id;
	__drm_atomic_helper_plane_reset(plane, &state->base);
}

static struct drm_plane_state *
vs_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct vs_plane_state *state;

	if (WARN_ON(!plane->state))
		return NULL;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	return &state->base;
}

static bool vs_format_mod_supported(struct drm_plane *plane,
				    u32 format,
				    u64 modifier)
{
	int i;

	/* We always have to allow these modifiers:
	 * 1. Core DRM checks for LINEAR support if userspace does not provide modifiers.
	 * 2. Not passing any modifiers is the same as explicitly passing INVALID.
	 */
	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return true;

	/* Check that the modifier is on the list of the plane's supported modifiers. */
	for (i = 0; i < plane->modifier_count; i++) {
		if (modifier == plane->modifiers[i])
			break;
	}

	if (i == plane->modifier_count)
		return false;

	return true;
}

const struct drm_plane_funcs vs_plane_funcs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.reset			= vs_plane_reset,
	.atomic_duplicate_state = vs_plane_atomic_duplicate_state,
	.atomic_destroy_state	= vs_plane_atomic_destroy_state,
	.format_mod_supported	= vs_format_mod_supported,
};

static unsigned char vs_get_plane_number(struct drm_framebuffer *fb)
{
	const struct drm_format_info *info;

	if (!fb)
		return 0;

	info = drm_format_info(fb->format->format);
	if (!info || info->num_planes > DRM_FORMAT_MAX_PLANES)
		return 0;

	return info->num_planes;
}

static bool vs_dc_mod_supported(const u64 *info_modifiers, u64 modifier)
{
	const u64 *mods;

	if (!info_modifiers)
		return false;

	for (mods = info_modifiers; *mods != DRM_FORMAT_MOD_INVALID; mods++) {
		if (*mods == modifier)
			return true;
	}

	return false;
}

static int vs_primary_plane_atomic_check(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state, plane);
	struct vs_drm_device *priv = to_vs_drm_private(plane->dev);
	struct vs_dc *dc = &priv->dc;
	struct drm_framebuffer *fb = new_state->fb;
	const struct vs_plane_primary_info *primary_info;
	struct drm_crtc_state *crtc_state;
	int i;

	if (!new_state->crtc || !new_state->fb)
		return 0;
	for (i = 0; i < dc->hw.info->primary_num; i++) {
		primary_info = (struct vs_plane_primary_info *)&dc->hw.info->primary[i];
		if (primary_info->id == to_vs_plane(plane)->id)
			break;
	}

	primary_info = &dc->hw.info->primary[i];

	if (fb->width < primary_info->min_width ||
	    fb->width > primary_info->max_width ||
	    fb->height < primary_info->min_height ||
	    fb->height > primary_info->max_height)
		drm_err_once(plane->dev, "buffer size may not support on plane%d.\n",
			     to_vs_plane(plane)->id);

	if (!vs_dc_mod_supported(primary_info->modifiers, fb->modifier)) {
		drm_err(plane->dev, "unsupported modifier on plane%d.\n", to_vs_plane(plane)->id);
		return -EINVAL;
	}

	crtc_state = drm_atomic_get_existing_crtc_state(new_state->state, new_state->crtc);
	return drm_atomic_helper_check_plane_state(new_state, crtc_state,
						   primary_info->min_scale,
						   primary_info->max_scale,
						   true, true);
}

static int vs_overlay_plane_atomic_check(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state, plane);
	struct vs_drm_device *priv = to_vs_drm_private(plane->dev);
	struct vs_dc *dc = &priv->dc;
	struct drm_framebuffer *fb = new_state->fb;
	const struct vs_plane_overlay_info *overlay_info;
	struct drm_crtc_state *crtc_state;
	int i;

	if (!new_state->crtc || !new_state->fb)
		return 0;

	for (i = 0; i < dc->hw.info->overlay_num; i++) {
		overlay_info = (struct vs_plane_overlay_info *)&dc->hw.info->overlay[i];
		if (overlay_info->id == to_vs_plane(plane)->id)
			break;
	}

	overlay_info = &dc->hw.info->overlay[i];

	if (fb->width < overlay_info->min_width ||
	    fb->width > overlay_info->max_width ||
	    fb->height < overlay_info->min_height ||
	    fb->height > overlay_info->max_height)
		drm_err_once(plane->dev, "buffer size may not support on plane%d.\n",
			     to_vs_plane(plane)->id);

	if (!vs_dc_mod_supported(overlay_info->modifiers, fb->modifier)) {
		drm_err(plane->dev, "unsupported modifier on plane%d.\n", to_vs_plane(plane)->id);
		return -EINVAL;
}

	crtc_state = drm_atomic_get_existing_crtc_state(new_state->state, new_state->crtc);
	return drm_atomic_helper_check_plane_state(new_state, crtc_state,
						   overlay_info->min_scale,
						   overlay_info->max_scale,
						   true, true);
}

static int vs_cursor_plane_atomic_check(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state, plane);
	struct vs_drm_device *priv = to_vs_drm_private(plane->dev);
	struct vs_dc *dc = &priv->dc;
	struct drm_framebuffer *fb = new_state->fb;
	const struct vs_plane_cursor_info *cursor_info;
	struct drm_crtc_state *crtc_state;
	int i;

	if (!new_state->crtc || !new_state->fb)
		return 0;

	for (i = 0; i < dc->hw.info->cursor_num; i++) {
		cursor_info = (struct vs_plane_cursor_info *)&dc->hw.info->cursor[i];
		if (cursor_info->id == to_vs_plane(plane)->id)
			break;
	}

	cursor_info = &dc->hw.info->cursor[i];

	if (fb->width < cursor_info->min_width ||
	    fb->width > cursor_info->max_width ||
	    fb->height < cursor_info->min_height ||
	    fb->height > cursor_info->max_height)
		drm_err_once(plane->dev, "buffer size may not support on plane%d.\n",
			     to_vs_plane(plane)->id);

	crtc_state = drm_atomic_get_existing_crtc_state(new_state->state, new_state->crtc);
	return drm_atomic_helper_check_plane_state(new_state, crtc_state,
						   DRM_PLANE_NO_SCALING,
						   DRM_PLANE_NO_SCALING,
						   true, true);
}

static void vs_plane_atomic_update(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state, plane);
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state, plane);

	unsigned char i, num_planes, display_id, id;
	u32 format;
	bool is_yuv;
	struct vs_plane *vs_plane = to_vs_plane(plane);
	struct vs_plane_state *plane_state = to_vs_plane_state(new_state);
	struct vs_drm_device *priv = to_vs_drm_private(plane->dev);
	struct vs_dc *dc = &priv->dc;

	if (!new_state->fb || !new_state->crtc)
		return;

	drm_fb_dma_sync_non_coherent(plane->dev, old_state, new_state);

	num_planes = vs_get_plane_number(new_state->fb);

	for (i = 0; i < num_planes; i++) {
		dma_addr_t dma_addr;

		dma_addr = drm_fb_dma_get_gem_addr(new_state->fb, new_state, i);
		plane_state->dma_addr[i] = dma_addr;
	}

	display_id = to_vs_display_id(dc, new_state->crtc);
	format = new_state->fb->format->format;
	is_yuv = new_state->fb->format->is_yuv;
	id = vs_plane->id;

	plane_hw_update_format_colorspace(dc, format, new_state->color_encoding, id, is_yuv);
	if (new_state->visible)
		plane_hw_update_address(dc, id, format, plane_state->dma_addr,
					new_state->fb, &new_state->src);
	plane_hw_update_format(dc, format, new_state->color_encoding, new_state->rotation,
			       new_state->visible, new_state->zpos, id, display_id);
	plane_hw_update_scale(dc, &new_state->src, &new_state->dst, id,
			      display_id, new_state->rotation);
	plane_hw_update_blend(dc, new_state->alpha, new_state->pixel_blend_mode,
			      id, display_id);
}

static void vs_cursor_plane_atomic_update(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state,
									   plane);
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state,
									   plane);
	struct vs_drm_device *priv = to_vs_drm_private(plane->dev);
	struct vs_dc *dc = &priv->dc;
	unsigned char display_id;
	u32 crtc_w, crtc_x, crtc_y;
	s32 hotspot_x, hotspot_y;
	dma_addr_t dma_addr;

	display_id = to_vs_display_id(dc, new_state->crtc);

	if (!new_state->fb || !new_state->crtc)
		return;

	drm_fb_dma_sync_non_coherent(new_state->fb->dev, old_state, new_state);
	dma_addr = drm_fb_dma_get_gem_addr(new_state->fb, new_state, 0);
	crtc_w = new_state->crtc_w;

	if (new_state->crtc_x > 0) {
		crtc_x = new_state->crtc_x;
		hotspot_x = 0;
	} else {
		hotspot_x = -new_state->crtc_x;
		crtc_x = 0;
	}
	if (new_state->crtc_y > 0) {
		crtc_y = new_state->crtc_y;
		hotspot_y = 0;
	} else {
		hotspot_y = -new_state->crtc_y;
		crtc_y = 0;
	}
	dc_hw_update_cursor(&dc->hw, display_id, dma_addr, crtc_w, crtc_x,
			    crtc_y, hotspot_x, hotspot_y);
}

static void vs_plane_atomic_disable(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct vs_plane *vs_plane = to_vs_plane(plane);
	struct vs_drm_device *priv = to_vs_drm_private(plane->dev);
	struct vs_dc *dc = &priv->dc;

	dc_hw_disable_plane(dc, vs_plane->id);
}

static void vs_cursor_plane_atomic_disable(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state, plane);
	struct vs_drm_device *priv = to_vs_drm_private(plane->dev);
	struct vs_dc *dc = &priv->dc;
	unsigned char display_id;

	display_id = to_vs_display_id(dc, old_state->crtc);
	dc_hw_disable_cursor(&dc->hw, display_id);
}

const struct drm_plane_helper_funcs primary_plane_helpers = {
	.atomic_check	= vs_primary_plane_atomic_check,
	.atomic_update	= vs_plane_atomic_update,
	.atomic_disable = vs_plane_atomic_disable,
};

const struct drm_plane_helper_funcs overlay_plane_helpers = {
	.atomic_check	= vs_overlay_plane_atomic_check,
	.atomic_update	= vs_plane_atomic_update,
	.atomic_disable = vs_plane_atomic_disable,
};

const struct drm_plane_helper_funcs cursor_plane_helpers = {
	.atomic_check	= vs_cursor_plane_atomic_check,
	.atomic_update	= vs_cursor_plane_atomic_update,
	.atomic_disable = vs_cursor_plane_atomic_disable,
};

struct vs_plane *vs_plane_primary_create(struct drm_device *drm_dev,
					 struct vs_plane_primary_info *info,
					 unsigned int layer_num,
					 unsigned int possible_crtcs)
{
	struct vs_plane *plane;
	int ret;

	if (!info)
		return NULL;

	plane = drmm_universal_plane_alloc(drm_dev, struct vs_plane, base,
					   possible_crtcs,
					   &vs_plane_funcs,
					   info->formats, info->num_formats,
					   info->modifiers, DRM_PLANE_TYPE_PRIMARY,
					   NULL);
	if (IS_ERR(plane))
		return ERR_CAST(plane);

	drm_plane_helper_add(&plane->base, &primary_plane_helpers);

	drm_plane_create_alpha_property(&plane->base);
	drm_plane_create_blend_mode_property(&plane->base,
					     BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					     BIT(DRM_MODE_BLEND_PREMULTI) |
					     BIT(DRM_MODE_BLEND_COVERAGE));

	if (info->color_encoding) {
		ret = drm_plane_create_color_properties(&plane->base, info->color_encoding,
							BIT(DRM_COLOR_YCBCR_LIMITED_RANGE),
							DRM_COLOR_YCBCR_BT709,
							DRM_COLOR_YCBCR_LIMITED_RANGE);
		if (ret)
			return NULL;
	}

	if (info->rotation) {
		ret = drm_plane_create_rotation_property(&plane->base,
							 DRM_MODE_ROTATE_0,
							 info->rotation);
		if (ret)
			return NULL;
	}

	ret = drm_plane_create_zpos_property(&plane->base, info->zpos, 0, layer_num - 1);
	if (ret)
		return NULL;

	return plane;
}

struct vs_plane *vs_plane_overlay_create(struct drm_device *drm_dev,
					 struct vs_plane_overlay_info *info,
					 unsigned int layer_num,
					 unsigned int possible_crtcs)
{
	struct vs_plane *plane;
	int ret;

	if (!info)
		return NULL;

	plane = drmm_universal_plane_alloc(drm_dev, struct vs_plane, base,
					   possible_crtcs,
					   &vs_plane_funcs,
					   info->formats, info->num_formats,
					   info->modifiers, DRM_PLANE_TYPE_OVERLAY,
					   NULL);
	if (IS_ERR(plane))
		return ERR_CAST(plane);

	drm_plane_helper_add(&plane->base, &overlay_plane_helpers);

	drm_plane_create_alpha_property(&plane->base);
	drm_plane_create_blend_mode_property(&plane->base,
					     BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					     BIT(DRM_MODE_BLEND_PREMULTI) |
					     BIT(DRM_MODE_BLEND_COVERAGE));

	if (info->color_encoding) {
		ret = drm_plane_create_color_properties(&plane->base, info->color_encoding,
							BIT(DRM_COLOR_YCBCR_LIMITED_RANGE),
							DRM_COLOR_YCBCR_BT709,
							DRM_COLOR_YCBCR_LIMITED_RANGE);
		if (ret)
			return NULL;
	}

	if (info->rotation) {
		ret = drm_plane_create_rotation_property(&plane->base,
							 DRM_MODE_ROTATE_0,
							 info->rotation);
		if (ret)
			return NULL;
	}

	ret = drm_plane_create_zpos_property(&plane->base, info->zpos, 0, layer_num - 1);
	if (ret)
		return NULL;

	return plane;
}

struct vs_plane *vs_plane_cursor_create(struct drm_device *drm_dev,
					struct vs_plane_cursor_info *info,
					unsigned int possible_crtcs)
{
	struct vs_plane *plane;
	int ret;

	if (!info)
		return NULL;

	plane = drmm_universal_plane_alloc(drm_dev, struct vs_plane, base,
					   possible_crtcs,
					   &vs_plane_funcs,
					   info->formats, info->num_formats,
					   NULL, DRM_PLANE_TYPE_CURSOR,
					   NULL);
	if (IS_ERR(plane))
		return ERR_CAST(plane);

	drm_plane_helper_add(&plane->base, &cursor_plane_helpers);

	ret = drm_plane_create_zpos_immutable_property(&plane->base, info->zpos);
	if (ret)
		return NULL;

	return plane;
}
