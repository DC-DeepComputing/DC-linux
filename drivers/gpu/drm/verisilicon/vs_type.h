/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_TYPE_H__
#define __VS_TYPE_H__

#include <drm/drm_plane.h>

struct vs_plane_primary_info {
	u8 id;
	unsigned int num_formats;
	const u32 *formats;
	u8 num_modifiers;
	const u64 *modifiers;
	unsigned int min_width;
	unsigned int min_height;
	unsigned int max_width;
	unsigned int max_height;
	unsigned int rotation;
	unsigned int color_encoding;

	int min_scale; /* 16.16 fixed point */
	int max_scale; /* 16.16 fixed point */

	u8	 zpos;

};

struct vs_plane_overlay_info {
	u8 id;
	unsigned int num_formats;
	const u32 *formats;
	u8 num_modifiers;
	const u64 *modifiers;
	unsigned int min_width;
	unsigned int min_height;
	unsigned int max_width;
	unsigned int max_height;
	unsigned int rotation;
	unsigned int color_encoding;

	int min_scale; /* 16.16 fixed point */
	int max_scale; /* 16.16 fixed point */

	u8	 zpos;

};

struct vs_plane_cursor_info {
	u8 id;
	unsigned int num_formats;
	const u32 *formats;
	unsigned int min_width;
	unsigned int min_height;
	unsigned int max_width;
	unsigned int max_height;
	u8	 zpos;

};

struct vs_dc_info {
	const char *name;

	u8 panel_num;

	/* planes */
	u8 layer_num;
	u8 primary_num;
	u8 overlay_num;
	u8 cursor_num;
	const struct vs_plane_primary_info *primary;
	const struct vs_plane_overlay_info *overlay;
	const struct vs_plane_cursor_info *cursor;

	/* 0 means no gamma LUT */
	u16 gamma_size;
	u8 gamma_bits;

	u16 pitch_alignment;
};

#endif /* __VS_TYPE_H__ */
