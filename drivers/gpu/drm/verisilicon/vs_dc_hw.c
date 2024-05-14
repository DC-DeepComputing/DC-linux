// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/bits.h>
#include <linux/io.h>
#include <linux/media-bus-format.h>
//#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_fb_dma_helper.h>

#include "vs_dc_hw.h"

static const u32 horkernel[] = {
	0x00000000, 0x20000000, 0x00002000, 0x00000000,
	0x00000000, 0x00000000, 0x23fd1c03, 0x00000000,
	0x00000000, 0x00000000, 0x181f0000, 0x000027e1,
	0x00000000, 0x00000000, 0x00000000, 0x2b981468,
	0x00000000, 0x00000000, 0x00000000, 0x10f00000,
	0x00002f10, 0x00000000, 0x00000000, 0x00000000,
	0x32390dc7, 0x00000000, 0x00000000, 0x00000000,
	0x0af50000, 0x0000350b, 0x00000000, 0x00000000,
	0x00000000, 0x3781087f, 0x00000000, 0x00000000,
	0x00000000, 0x06660000, 0x0000399a, 0x00000000,
	0x00000000, 0x00000000, 0x3b5904a7, 0x00000000,
	0x00000000, 0x00000000, 0x033c0000, 0x00003cc4,
	0x00000000, 0x00000000, 0x00000000, 0x3de1021f,
	0x00000000, 0x00000000, 0x00000000, 0x01470000,
	0x00003eb9, 0x00000000, 0x00000000, 0x00000000,
	0x3f5300ad, 0x00000000, 0x00000000, 0x00000000,
	0x00480000, 0x00003fb8, 0x00000000, 0x00000000,
	0x00000000, 0x3fef0011, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00004000, 0x00000000,
	0x00000000, 0x00000000, 0x20002000, 0x00000000,
	0x00000000, 0x00000000, 0x1c030000, 0x000023fd,
	0x00000000, 0x00000000, 0x00000000, 0x27e1181f,
	0x00000000, 0x00000000, 0x00000000, 0x14680000,
	0x00002b98, 0x00000000, 0x00000000, 0x00000000,
	0x2f1010f0, 0x00000000, 0x00000000, 0x00000000,
	0x0dc70000, 0x00003239, 0x00000000, 0x00000000,
	0x00000000, 0x350b0af5, 0x00000000, 0x00000000,
	0x00000000, 0x087f0000, 0x00003781, 0x00000000,
	0x00000000, 0x00000000, 0x399a0666, 0x00000000,
	0x00000000, 0x00000000, 0x04a70000, 0x00003b59,
	0x00000000, 0x00000000, 0x00000000, 0x3cc4033c,
	0x00000000, 0x00000000, 0x00000000, 0x021f0000,
};

#define H_COEF_SIZE ARRAY_SIZE(horkernel)

static const u32 verkernel[] = {
	0x00000000, 0x20000000, 0x00002000, 0x00000000,
	0x00000000, 0x00000000, 0x23fd1c03, 0x00000000,
	0x00000000, 0x00000000, 0x181f0000, 0x000027e1,
	0x00000000, 0x00000000, 0x00000000, 0x2b981468,
	0x00000000, 0x00000000, 0x00000000, 0x10f00000,
	0x00002f10, 0x00000000, 0x00000000, 0x00000000,
	0x32390dc7, 0x00000000, 0x00000000, 0x00000000,
	0x0af50000, 0x0000350b, 0x00000000, 0x00000000,
	0x00000000, 0x3781087f, 0x00000000, 0x00000000,
	0x00000000, 0x06660000, 0x0000399a, 0x00000000,
	0x00000000, 0x00000000, 0x3b5904a7, 0x00000000,
	0x00000000, 0x00000000, 0x033c0000, 0x00003cc4,
	0x00000000, 0x00000000, 0x00000000, 0x3de1021f,
	0x00000000, 0x00000000, 0x00000000, 0x01470000,
	0x00003eb9, 0x00000000, 0x00000000, 0x00000000,
	0x3f5300ad, 0x00000000, 0x00000000, 0x00000000,
	0x00480000, 0x00003fb8, 0x00000000, 0x00000000,
	0x00000000, 0x3fef0011, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00004000, 0x00000000,
	0xcdcd0000, 0xfdfdfdfd, 0xabababab, 0xabababab,
	0x00000000, 0x00000000, 0x5ff5f456, 0x000f5f58,
	0x02cc6c78, 0x02cc0c28, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
};

#define V_COEF_SIZE ARRAY_SIZE(verkernel)

/*
 * RGB 709->2020 conversion parameters
 */
static const u16 RGB2RGB[RGB_TO_RGB_TABLE_SIZE] = {
	10279,	5395,	709,
	1132,	15065,	187,
	269,	1442,	14674
};

/*
 * YUV601 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static const s32 YUV601_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196,	0,		1640,	1196,
	-404,	-836,		1196,	2076,
	0,	-916224,	558336,	-1202944,
	64,	940,		64,	960
};

/*
 * YUV709 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static s32 YUV709_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196,		0,		1844,	1196,
	-220,		-548,	1196,	2172,
	0,			-1020672, 316672,  -1188608,
	64,			940,		64,		960
};

/*
 * YUV2020 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static s32 YUV2020_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196, 0, 1724, 1196,
	-192, -668, 1196, 2200,
	0, -959232, 363776, -1202944,
	64, 940, 64, 960
};

/*
 * RGB to YUV2020 conversion parameters
 * RGB2YUV[0] - [8] : C0 - C8;
 * RGB2YUV[9] - [11]: D0 - D2;
 */
static s16 RGB2YUV[RGB_TO_YUV_TABLE_SIZE] = {
	230,	594,	52,
	-125,  -323,	448,
	448,   -412,   -36,
	64,		512,	512
};

/* one is for primary plane and the other is for all overlay planes */
static const struct dc_hw_plane_reg dc_plane_reg[] = {
	{
		.y_address		= DC_FRAMEBUFFER_ADDRESS,
		.u_address		= DC_FRAMEBUFFER_U_ADDRESS,
		.v_address		= DC_FRAMEBUFFER_V_ADDRESS,
		.y_stride		= DC_FRAMEBUFFER_STRIDE,
		.u_stride		= DC_FRAMEBUFFER_U_STRIDE,
		.v_stride		= DC_FRAMEBUFFER_V_STRIDE,
		.size			= DC_FRAMEBUFFER_SIZE,
		.top_left		= DC_FRAMEBUFFER_TOP_LEFT,
		.bottom_right	= DC_FRAMEBUFFER_BOTTOM_RIGHT,
		.scale_factor_x			= DC_FRAMEBUFFER_SCALE_FACTOR_X,
		.scale_factor_y			= DC_FRAMEBUFFER_SCALE_FACTOR_Y,
		.h_filter_coef_index	= DC_FRAMEBUFFER_H_FILTER_COEF_INDEX,
		.h_filter_coef_data		= DC_FRAMEBUFFER_H_FILTER_COEF_DATA,
		.v_filter_coef_index	= DC_FRAMEBUFFER_V_FILTER_COEF_INDEX,
		.v_filter_coef_data		= DC_FRAMEBUFFER_V_FILTER_COEF_DATA,
		.init_offset			= DC_FRAMEBUFFER_INIT_OFFSET,
		.color_key				= DC_FRAMEBUFFER_COLOR_KEY,
		.color_key_high			= DC_FRAMEBUFFER_COLOR_KEY_HIGH,
		.clear_value			= DC_FRAMEBUFFER_CLEAR_VALUE,
		.color_table_index		= DC_FRAMEBUFFER_COLOR_TABLE_INDEX,
		.color_table_data		= DC_FRAMEBUFFER_COLOR_TABLE_DATA,
		.scale_config			= DC_FRAMEBUFFER_SCALE_CONFIG,
		.water_mark				= DC_FRAMEBUFFER_WATER_MARK,
		.degamma_index			= DC_FRAMEBUFFER_DEGAMMA_INDEX,
		.degamma_data			= DC_FRAMEBUFFER_DEGAMMA_DATA,
		.degamma_ex_data		= DC_FRAMEBUFFER_DEGAMMA_EX_DATA,
		.src_global_color		= DC_FRAMEBUFFER_SRC_GLOBAL_COLOR,
		.dst_global_color		= DC_FRAMEBUFFER_DST_GLOBAL_COLOR,
		.blend_config			= DC_FRAMEBUFFER_BLEND_CONFIG,
		.roi_origin				= DC_FRAMEBUFFER_ROI_ORIGIN,
		.roi_size				= DC_FRAMEBUFFER_ROI_SIZE,
		.yuv_to_rgb_coef0			= DC_FRAMEBUFFER_YUVTORGB_COEF0,
		.yuv_to_rgb_coef1			= DC_FRAMEBUFFER_YUVTORGB_COEF1,
		.yuv_to_rgb_coef2			= DC_FRAMEBUFFER_YUVTORGB_COEF2,
		.yuv_to_rgb_coef3			= DC_FRAMEBUFFER_YUVTORGB_COEF3,
		.yuv_to_rgb_coef4			= DC_FRAMEBUFFER_YUVTORGB_COEF4,
		.yuv_to_rgb_coefd0			= DC_FRAMEBUFFER_YUVTORGB_COEFD0,
		.yuv_to_rgb_coefd1			= DC_FRAMEBUFFER_YUVTORGB_COEFD1,
		.yuv_to_rgb_coefd2			= DC_FRAMEBUFFER_YUVTORGB_COEFD2,
		.y_clamp_bound				= DC_FRAMEBUFFER_Y_CLAMP_BOUND,
		.uv_clamp_bound				= DC_FRAMEBUFFER_UV_CLAMP_BOUND,
		.rgb_to_rgb_coef0			= DC_FRAMEBUFFER_RGBTORGB_COEF0,
		.rgb_to_rgb_coef1			= DC_FRAMEBUFFER_RGBTORGB_COEF1,
		.rgb_to_rgb_coef2			= DC_FRAMEBUFFER_RGBTORGB_COEF2,
		.rgb_to_rgb_coef3			= DC_FRAMEBUFFER_RGBTORGB_COEF3,
		.rgb_to_rgb_coef4			= DC_FRAMEBUFFER_RGBTORGB_COEF4,
	},
	{
		.y_address		= DC_OVERLAY_ADDRESS,
		.u_address		= DC_OVERLAY_U_ADDRESS,
		.v_address		= DC_OVERLAY_V_ADDRESS,
		.y_stride		= DC_OVERLAY_STRIDE,
		.u_stride		= DC_OVERLAY_U_STRIDE,
		.v_stride		= DC_OVERLAY_V_STRIDE,
		.size			= DC_OVERLAY_SIZE,
		.top_left		= DC_OVERLAY_TOP_LEFT,
		.bottom_right	= DC_OVERLAY_BOTTOM_RIGHT,
		.scale_factor_x	= DC_OVERLAY_SCALE_FACTOR_X,
		.scale_factor_y	= DC_OVERLAY_SCALE_FACTOR_Y,
		.h_filter_coef_index = DC_OVERLAY_H_FILTER_COEF_INDEX,
		.h_filter_coef_data  = DC_OVERLAY_H_FILTER_COEF_DATA,
		.v_filter_coef_index = DC_OVERLAY_V_FILTER_COEF_INDEX,
		.v_filter_coef_data  = DC_OVERLAY_V_FILTER_COEF_DATA,
		.init_offset		 = DC_OVERLAY_INIT_OFFSET,
		.color_key			 = DC_OVERLAY_COLOR_KEY,
		.color_key_high			= DC_OVERLAY_COLOR_KEY_HIGH,
		.clear_value		 = DC_OVERLAY_CLEAR_VALUE,
		.color_table_index	 = DC_OVERLAY_COLOR_TABLE_INDEX,
		.color_table_data	 = DC_OVERLAY_COLOR_TABLE_DATA,
		.scale_config		 = DC_OVERLAY_SCALE_CONFIG,
		.water_mark				= DC_OVERLAY_WATER_MARK,
		.degamma_index		 = DC_OVERLAY_DEGAMMA_INDEX,
		.degamma_data		 = DC_OVERLAY_DEGAMMA_DATA,
		.degamma_ex_data	 = DC_OVERLAY_DEGAMMA_EX_DATA,
		.src_global_color	 = DC_OVERLAY_SRC_GLOBAL_COLOR,
		.dst_global_color	 = DC_OVERLAY_DST_GLOBAL_COLOR,
		.blend_config		 = DC_OVERLAY_BLEND_CONFIG,
		.roi_origin				= DC_OVERLAY_ROI_ORIGIN,
		.roi_size				= DC_OVERLAY_ROI_SIZE,
		.yuv_to_rgb_coef0		 = DC_OVERLAY_YUVTORGB_COEF0,
		.yuv_to_rgb_coef1		 = DC_OVERLAY_YUVTORGB_COEF1,
		.yuv_to_rgb_coef2		 = DC_OVERLAY_YUVTORGB_COEF2,
		.yuv_to_rgb_coef3		 = DC_OVERLAY_YUVTORGB_COEF3,
		.yuv_to_rgb_coef4			= DC_OVERLAY_YUVTORGB_COEF4,
		.yuv_to_rgb_coefd0			= DC_OVERLAY_YUVTORGB_COEFD0,
		.yuv_to_rgb_coefd1			= DC_OVERLAY_YUVTORGB_COEFD1,
		.yuv_to_rgb_coefd2			= DC_OVERLAY_YUVTORGB_COEFD2,
		.y_clamp_bound		 = DC_OVERLAY_Y_CLAMP_BOUND,
		.uv_clamp_bound		 = DC_OVERLAY_UV_CLAMP_BOUND,
		.rgb_to_rgb_coef0		 = DC_OVERLAY_RGBTORGB_COEF0,
		.rgb_to_rgb_coef1		 = DC_OVERLAY_RGBTORGB_COEF1,
		.rgb_to_rgb_coef2		 = DC_OVERLAY_RGBTORGB_COEF2,
		.rgb_to_rgb_coef3		 = DC_OVERLAY_RGBTORGB_COEF3,
		.rgb_to_rgb_coef4		 = DC_OVERLAY_RGBTORGB_COEF4,
	},
};

static inline u32 hi_read(struct dc_hw *hw, u32 reg)
{
	return readl(hw->hi_base + reg);
}

static inline void hi_write(struct dc_hw *hw, u32 reg, u32 value)
{
	writel(value, hw->hi_base + reg);
}

static inline void dc_write(struct dc_hw *hw, u32 reg, u32 value)
{
	writel(value, hw->reg_base + reg - DC_REG_BASE);
}

static inline u32 dc_read(struct dc_hw *hw, u32 reg)
{
	u32 value = readl(hw->reg_base + reg - DC_REG_BASE);

	return value;
}

static inline void dc_set_clear(struct dc_hw *hw, u32 reg, u32 set, u32 clear)
{
	u32 value = dc_read(hw, reg);

	value &= ~clear;
	value |= set;
	dc_write(hw, reg, value);
}

static void load_default_filter(struct dc_hw *hw,
				const struct dc_hw_plane_reg *reg, u32 offset)
{
	u8 i;

	dc_write(hw, reg->scale_config + offset, 0x33);
	dc_write(hw, reg->init_offset + offset, 0x80008000);
	dc_write(hw, reg->h_filter_coef_index + offset, 0x00);
	for (i = 0; i < H_COEF_SIZE; i++)
		dc_write(hw, reg->h_filter_coef_data + offset, horkernel[i]);

	dc_write(hw, reg->v_filter_coef_index + offset, 0x00);
	for (i = 0; i < V_COEF_SIZE; i++)
		dc_write(hw, reg->v_filter_coef_data + offset, verkernel[i]);
}

static void load_rgb_to_rgb(struct dc_hw *hw, const struct dc_hw_plane_reg *reg,
			    u32 offset, const u16 *table)
{
	dc_write(hw, reg->rgb_to_rgb_coef0 + offset, table[0] | (table[1] << 16));
	dc_write(hw, reg->rgb_to_rgb_coef1 + offset, table[2] | (table[3] << 16));
	dc_write(hw, reg->rgb_to_rgb_coef2 + offset, table[4] | (table[5] << 16));
	dc_write(hw, reg->rgb_to_rgb_coef3 + offset, table[6] | (table[7] << 16));
	dc_write(hw, reg->rgb_to_rgb_coef4 + offset, table[8]);
}

static void load_yuv_to_rgb(struct dc_hw *hw, const struct dc_hw_plane_reg *reg,
			    u32 offset, const s32 *table)
{
	dc_write(hw, reg->yuv_to_rgb_coef0 + offset,
		 (0xFFFF & table[0]) | (table[1] << 16));
	dc_write(hw, reg->yuv_to_rgb_coef1 + offset,
		 (0xFFFF & table[2]) | (table[3] << 16));
	dc_write(hw, reg->yuv_to_rgb_coef2 + offset,
		 (0xFFFF & table[4]) | (table[5] << 16));
	dc_write(hw, reg->yuv_to_rgb_coef3 + offset,
		 (0xFFFF & table[6]) | (table[7] << 16));
	dc_write(hw, reg->yuv_to_rgb_coef4 + offset, table[8]);
	dc_write(hw, reg->yuv_to_rgb_coefd0 + offset, table[9]);
	dc_write(hw, reg->yuv_to_rgb_coefd1 + offset, table[10]);
	dc_write(hw, reg->yuv_to_rgb_coefd2 + offset, table[11]);
	dc_write(hw, reg->y_clamp_bound + offset, table[12] | (table[13] << 16));
	dc_write(hw, reg->uv_clamp_bound + offset, table[14] | (table[15] << 16));
}

static void load_rgb_to_yuv(struct dc_hw *hw, u32 offset, s16 *table)
{
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF0 + offset,
		 table[0] | (table[1] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF1 + offset,
		 table[2] | (table[3] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF2 + offset,
		 table[4] | (table[5] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF3 + offset,
		 table[6] | (table[7] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF4 + offset, table[8]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD0 + offset, table[9]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD1 + offset, table[10]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD2 + offset, table[11]);
}

static int update_vs_format(u32 drm_format)
{
	u8 f = FORMAT_A8R8G8B8;

	switch (drm_format) {
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_BGRX4444:
		f = FORMAT_X4R4G4B4;
		break;
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_BGRA4444:
		f = FORMAT_A4R4G4B4;
		break;
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_BGRX5551:
		f = FORMAT_X1R5G5B5;
		break;
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_BGRA5551:
		f = FORMAT_A1R5G5B5;
		break;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		f = FORMAT_R5G6B5;
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_BGRX8888:
		f = FORMAT_X8R8G8B8;
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_BGRA8888:
		f = FORMAT_A8R8G8B8;
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		f = FORMAT_YUY2;
		break;
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		f = FORMAT_UYVY;
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		f = FORMAT_YV12;
		break;
	case DRM_FORMAT_NV21:
		f = FORMAT_NV12;
		break;
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		f = FORMAT_NV16;
		break;
	case DRM_FORMAT_P010:
		f = FORMAT_P010;
		break;
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_BGRA1010102:
		f = FORMAT_A2R10G10B10;
		break;
	case DRM_FORMAT_NV12:
		f = FORMAT_NV12;
		break;
	case DRM_FORMAT_YUV444:
		f = FORMAT_YUV444;
		break;
	default:
		break;
	}

	return f;
}

int dc_hw_init(struct vs_dc *dc)
{
	u8 i, id, panel_num, layer_num;
	struct dc_hw *hw = &dc->hw;
	u32 offset;

	layer_num = hw->info->layer_num;
	for (i = 0; i < layer_num; i++) {
		id = dc->planes[i].id;
		if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
			hw->reg[i] = dc_plane_reg[0];
		else
			hw->reg[i] = dc_plane_reg[1];

		load_default_filter(hw, &hw->reg[i], dc->planes[i].offset);
		load_rgb_to_rgb(hw, &hw->reg[i], dc->planes[i].offset, RGB2RGB);
	}

	panel_num = hw->info->panel_num;
	for (i = 0; i < panel_num; i++) {
		offset = i << 2;

		load_rgb_to_yuv(hw, offset, RGB2YUV);
		dc_write(hw, DC_DISPLAY_PANEL_CONFIG + offset, 0x111);

		offset = i ? DC_CURSOR_OFFSET : 0;
		dc_write(hw, DC_CURSOR_BACKGROUND + offset, 0x00FFFFFF);
		dc_write(hw, DC_CURSOR_FOREGROUND + offset, 0x00AAAAAA);
	}

	return 0;
}

void dc_hw_disable_plane(struct vs_dc *dc, u8 id)
{
	struct dc_hw *hw = &dc->hw;

	if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + dc->planes[id].offset,
			     PRIMARY_EN(false), PRIMARY_EN_MASK);
	else
		dc_set_clear(hw, DC_OVERLAY_CONFIG + dc->planes[id].offset,
			     OVERLAY_FB_EN(false), OVERLAY_FB_EN_MASK);
}

static int update_cursor_size(uint32_t crtc_w)
{
	u8 size_type;

	switch (crtc_w) {
	case 32:
		size_type = CURSOR_SIZE_32X32;
		break;
	case 64:
		size_type = CURSOR_SIZE_64X64;
		break;
	default:
		size_type = CURSOR_SIZE_32X32;
		break;
	}

	return size_type;
}

void dc_hw_update_cursor(struct dc_hw *hw, u8 id, dma_addr_t dma_addr,
			 u32 crtc_w, u32 crtc_x, u32 crtc_y,
			 s32 hotspot_x, s32 hotspot_y)
{
	u32 offset, size;

	offset = id ? DC_CURSOR_OFFSET : 0;
	size = update_cursor_size(crtc_w);

	dc_write(hw, DC_CURSOR_ADDRESS + offset,
		 dma_addr);
	dc_write(hw, DC_CURSOR_LOCATION + offset,
		 X_LCOTION(crtc_x) |
		 Y_LCOTION(crtc_y));
	dc_set_clear(hw, DC_CURSOR_CONFIG + offset,
		     CURSOR_HOT_X(hotspot_x) |
		     CURSOR_HOT_y(hotspot_y) |
		     CURSOR_SIZE(size) |
		     CURSOR_VALID(1) |
		     CURSOR_TRIG_FETCH(1) |
		     CURSOR_FORMAT(CURSOR_FORMAT_A8R8G8B8),
		     CURSOR_HOT_X_MASK |
		     CURSOR_HOT_y_MASK |
		     CURSOR_SIZE_MASK |
		     CURSOR_VALID_MASK |
		     CURSOR_TRIG_FETCH_MASK |
		     CURSOR_FORMAT_MASK);
}

void dc_hw_disable_cursor(struct dc_hw *hw, u8 id)
{
	u32 offset = 0;

	offset = id ? DC_CURSOR_OFFSET : 0;
	dc_set_clear(hw, DC_CURSOR_CONFIG + offset, CURSOR_VALID(1), CURSOR_FORMAT_MASK);
}

void dc_hw_update_gamma(struct dc_hw *hw, u8 id, u16 index,
			u16 r, u16 g, u16 b)
{
	if (index >= hw->info->gamma_size)
		return;

	hw->gamma[id].gamma[index][0] = r;
	hw->gamma[id].gamma[index][1] = g;
	hw->gamma[id].gamma[index][2] = b;
}

void dc_hw_enable_gamma(struct dc_hw *hw, u8 id, bool enable)
{
	u32 value;

	if (enable) {
		dc_write(hw, DC_DISPLAY_GAMMA_EX_INDEX + (id << 2), 0x00);
		for (int i = 0; i < GAMMA_EX_SIZE; i++) {
			value = hw->gamma[id].gamma[i][2] |
				(hw->gamma[id].gamma[i][1] << 12);
			dc_write(hw, DC_DISPLAY_GAMMA_EX_DATA + (id << 2), value);
			dc_write(hw, DC_DISPLAY_GAMMA_EX_ONE_DATA + (id << 2),
				 hw->gamma[id].gamma[i][0]);
		}
		dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + (id << 2), PANEL_GAMMA_EN, 0);
	} else {
		dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + (id << 2), 0, PANEL_GAMMA_EN);
	}
}

void dc_hw_enable(struct dc_hw *hw, int id, struct drm_display_mode *mode,
		  u8 encoder_type, u32 output_fmt)
{
	u32 dp_cfg, dpi_cfg, offset = id << 2;
	bool is_yuv = false;

	if (encoder_type != DRM_MODE_ENCODER_DSI) {
		switch (output_fmt) {
		case MEDIA_BUS_FMT_RGB565_1X16:
			dp_cfg = 0;
			break;
		case MEDIA_BUS_FMT_RGB666_1X18:
			dp_cfg = 1;
			break;
		case MEDIA_BUS_FMT_RGB888_1X24:
			dp_cfg = 2;
			break;
		case MEDIA_BUS_FMT_RGB101010_1X30:
			dp_cfg = 3;
			break;
		case MEDIA_BUS_FMT_UYVY8_1X16:
			dp_cfg = 2 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_YUV8_1X24:
			dp_cfg = 4 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYVY10_1X20:
			dp_cfg = 8 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_YUV10_1X30:
			dp_cfg = 10 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
			dp_cfg = 12 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
			dp_cfg = 13 << 4;
			is_yuv = true;
			break;
		default:
			dp_cfg = 2;
			break;
		}
		if (is_yuv)
			dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + offset, PANEL_RGB2YUV_EN, 0);
		else
			dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + offset, 0, PANEL_RGB2YUV_EN);
		dc_write(hw, DC_DISPLAY_DP_CONFIG + offset, dp_cfg | DP_SELECT);
	}

	if (hw->out[id] == OUT_DPI)
		dc_set_clear(hw, DC_DISPLAY_DP_CONFIG + offset, 0, DP_SELECT);

	switch (output_fmt) {
	case MEDIA_BUS_FMT_RGB565_1X16:
		dpi_cfg = 0;
		break;
	case MEDIA_BUS_FMT_RGB666_1X18:
		dpi_cfg = 3;
		break;
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		dpi_cfg = 4;
		break;
	case MEDIA_BUS_FMT_RGB888_1X24:
		dpi_cfg = 5;
		break;
	case MEDIA_BUS_FMT_RGB101010_1X30:
		dpi_cfg = 6;
		break;
	default:
		dpi_cfg = 5;
		break;
	}
	dc_write(hw, DC_DISPLAY_DPI_CONFIG + offset, dpi_cfg);

	if (id == 0)
		dc_set_clear(hw, DC_DISPLAY_PANEL_START, 0, PANEL0_EN | TWO_PANEL_EN);
	else
		dc_set_clear(hw, DC_DISPLAY_PANEL_START, 0, PANEL1_EN | TWO_PANEL_EN);

	dc_write(hw, DC_DISPLAY_H + offset,
		 H_ACTIVE_LEN(mode->hdisplay) |
		 H_TOTAL_LEN(mode->htotal));

	dc_write(hw, DC_DISPLAY_H_SYNC + offset,
		 H_SYNC_START_LEN(mode->hsync_start) |
		 H_SYNC_END_LEN(mode->hsync_end) |
		 H_POLARITY_LEN(mode->flags & DRM_MODE_FLAG_PHSYNC ? 0 : 1) |
		 H_PLUS_LEN(1));

	dc_write(hw, DC_DISPLAY_V + offset,
		 V_ACTIVE_LEN(mode->vdisplay) |
		 V_TOTAL_LEN(mode->vtotal));

	dc_write(hw, DC_DISPLAY_V_SYNC + offset,
		 V_SYNC_START_LEN(mode->vsync_start) |
		 V_SYNC_END_LEN(mode->vsync_end) |
		 V_POLARITY_LEN(mode->flags & DRM_MODE_FLAG_PVSYNC ? 0 : 1) |
		 V_PLUS_LEN(1));

	dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + offset, PANEL_OUTPUT_EN, 0);
	dc_set_clear(hw, DC_DISPLAY_PANEL_START, BIT(id), SYNC_EN);
}

void dc_hw_disable(struct dc_hw *hw, int id)
{
	u32 offset = id << 2;

	if (hw->out[id] == OUT_DPI)
		dc_set_clear(hw, DC_DISPLAY_DP_CONFIG + offset, 0, DP_SELECT);
	dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + offset, 0, PANEL_OUTPUT_EN);
	dc_set_clear(hw, DC_DISPLAY_PANEL_START, 0, BIT(id) | TWO_PANEL_EN);
}

void dc_hw_enable_interrupt(struct dc_hw *hw)
{
	hi_write(hw, AQ_INTR_ENBL, 0xFFFFFFFF);
}

void dc_hw_disable_interrupt(struct dc_hw *hw)
{
	hi_write(hw, AQ_INTR_ENBL, 0);
}

u32 dc_hw_get_interrupt(struct dc_hw *hw)
{
	return hi_read(hw, AQ_INTR_ACKNOWLEDGE);
}

void dc_hw_enable_shadow_register(struct vs_dc *dc, bool enable)
{
	u32 i, offset;
	struct dc_hw *hw = &dc->hw;
	u8 id, layer_num = hw->info->layer_num;
	u8 panel_num = hw->info->panel_num;

	for (i = 0; i < layer_num; i++) {
		id = dc->planes[i].id;
		offset = dc->planes[i].offset;
		if (enable) {
			if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + offset,
					     PRIMARY_SHADOW_EN, 0);
			else
				dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
					     OVERLAY_SHADOW_EN, 0);
		} else {
			if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + offset,
					     0, PRIMARY_SHADOW_EN);
			else
				dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
					     0, OVERLAY_SHADOW_EN);
		}
	}

	for (i = 0; i < panel_num; i++) {
		offset = i << 2;
		if (enable)
			dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG_EX + offset, 0, PANEL_SHADOW_EN);
		else
			dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG_EX + offset, PANEL_SHADOW_EN, 0);
	}
}

void dc_hw_set_out(struct dc_hw *hw, enum dc_hw_out out, u8 id)
{
	if (out <= OUT_DP)
		hw->out[id] = out;
}

static inline u8 to_vs_yuv_color_space(u32 color_space)
{
	u8 cs;

	switch (color_space) {
	case DRM_COLOR_YCBCR_BT601:
		cs = COLOR_SPACE_601;
		break;
	case DRM_COLOR_YCBCR_BT709:
		cs = COLOR_SPACE_709;
		break;
	case DRM_COLOR_YCBCR_BT2020:
		cs = COLOR_SPACE_2020;
		break;
	default:
		cs = COLOR_SPACE_601;
		break;
	}

	return cs;
}

static inline u8 update_uv_swizzle(u32 format)
{
	u8 uv_swizzle = 0;

	switch (format) {
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
		uv_swizzle = 1;
		break;
	default:
		break;
	}

	return uv_swizzle;
}

static inline u8 update_swizzle(u32 format)
{
	u8 swizzle = SWIZZLE_ARGB;

	switch (format) {
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBA1010102:
		swizzle = SWIZZLE_RGBA;
		break;
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_ABGR2101010:
		swizzle = SWIZZLE_ABGR;
		break;
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRA1010102:
		swizzle = SWIZZLE_BGRA;
		break;
	default:
		break;
	}

	return swizzle;
}

static inline u8 to_vs_rotation(unsigned int rotation)
{
	u8 rot;

	switch (rotation & DRM_MODE_REFLECT_MASK) {
	case DRM_MODE_REFLECT_X:
		rot = FLIP_X;
		return rot;
	case DRM_MODE_REFLECT_Y:
		rot = FLIP_Y;
		return rot;
	case DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y:
		rot = FLIP_XY;
		return rot;
	default:
		break;
	}

	switch (rotation & DRM_MODE_ROTATE_MASK) {
	case DRM_MODE_ROTATE_0:
		rot = ROT_0;
		break;
	case DRM_MODE_ROTATE_90:
		rot = ROT_90;
		break;
	case DRM_MODE_ROTATE_180:
		rot = ROT_180;
		break;
	case DRM_MODE_ROTATE_270:
		rot = ROT_270;
		break;
	default:
		rot = ROT_0;
		break;
	}

	return rot;
}

void plane_hw_update_format_colorspace(struct vs_dc *dc, u32 format,
				       enum drm_color_encoding encoding, u8 id, bool is_yuv)
{
	u32 offset = dc->planes[id].offset;
	struct dc_hw *hw = &dc->hw;

	if (is_yuv) {
		if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + offset,
				     PRIMARY_YUVCLAMP_EN, PRIMARY_RGB2RGB_EN);
		else
			dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
				     OVERLAY_CLAMP_EN, OVERLAY_RGB2RGB_EN);

		switch (to_vs_yuv_color_space(encoding)) {
		case COLOR_SPACE_601:
			load_yuv_to_rgb(hw, &hw->reg[id], offset, YUV601_2RGB);
			break;
		case COLOR_SPACE_709:
			load_yuv_to_rgb(hw, &hw->reg[id], offset, YUV709_2RGB);
			break;
		case COLOR_SPACE_2020:
			load_yuv_to_rgb(hw, &hw->reg[id], offset, YUV2020_2RGB);
			break;
		default:
			break;
		}
	} else {
		if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + offset,
				     PRIMARY_RGB2RGB_EN, PRIMARY_YUVCLAMP_EN);
		else
			dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
				     OVERLAY_RGB2RGB_EN, OVERLAY_CLAMP_EN);
	}
}

void plane_hw_update_address(struct vs_dc *dc, u8 id, u32 format, dma_addr_t *dma_addr,
			     struct drm_framebuffer *drm_fb, struct drm_rect *src)
{
	u32 offset = dc->planes[id].offset;
	struct dc_hw *hw = &dc->hw;

	dc_write(hw, hw->reg[id].y_address + offset, dma_addr[0]);
	dc_write(hw, hw->reg[id].u_address + offset,
		 format == DRM_FORMAT_YVU420 ?
		 dma_addr[2] : dma_addr[1]);
	dc_write(hw, hw->reg[id].v_address + offset,
		 format == DRM_FORMAT_YVU420 ?
		 dma_addr[1] : dma_addr[2]);
	dc_write(hw, hw->reg[id].y_stride + offset, drm_fb->pitches[0]);
	dc_write(hw, hw->reg[id].u_stride + offset,
		 format == DRM_FORMAT_YVU420 ?
		 drm_fb->pitches[2] : drm_fb->pitches[1]);
	dc_write(hw, hw->reg[id].v_stride + offset,
		 format == DRM_FORMAT_YVU420 ?
		 drm_fb->pitches[1] : drm_fb->pitches[2]);
	dc_write(hw, hw->reg[id].size + offset,
		 FB_SIZE(drm_rect_width(src) >> 16, drm_rect_height(src) >> 16));
}

void plane_hw_update_format(struct vs_dc *dc, u32 format, enum drm_color_encoding encoding,
			    unsigned int rotation, bool visible, unsigned int zpos,
			    u8 id, u8 display_id)
{
	u32 offset = dc->planes[id].offset;
	struct dc_hw *hw = &dc->hw;

	if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1) {
		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG + offset,
			     PRIMARY_FORMAT(update_vs_format(format)) |
			     PRIMARY_UV_SWIZ(update_uv_swizzle(format)) |
			     PRIMARY_SWIZ(update_swizzle(format)) |
			     PRIMARY_TILE(DRM_FORMAT_MOD_LINEAR) |
			     PRIMARY_YUV_COLOR(to_vs_yuv_color_space(encoding)) |
			     PRIMARY_ROTATION(to_vs_rotation(rotation)),
			     PRIMARY_FORMAT_MASK |
			     PRIMARY_UV_SWIZ_MASK |
			     PRIMARY_SWIZ_MASK |
			     PRIMARY_TILE_MASK |
			     PRIMARY_YUV_COLOR_MASK |
			     PRIMARY_ROTATION_MASK |
			     PRIMARY_CLEAR_EN_MASK);
		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + offset,
			     PRIMARY_DECODER_EN(false) |
			     PRIMARY_EN(visible) |
			     PRIMARY_ZPOS(zpos) |
			     PRIMARY_CHANNEL(display_id),
			     PRIMARY_DECODER_EN_EN_MASK |
			     PRIMARY_EN_MASK |
			     PRIMARY_ZPOS_MASK |
			     PRIMARY_CHANNEL_MASK);
	} else {
		dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
			     OVERLAY_FB_EN(visible) |
			     OVERLAY_FORMAT(update_vs_format(format)) |
			     OVERLAY_UV_SWIZ(update_uv_swizzle(format)) |
			     OVERLAY_SWIZ(update_swizzle(format)) |
			     OVERLAY_TILE(DRM_FORMAT_MOD_LINEAR) |
			     OVERLAY_YUV_COLOR(to_vs_yuv_color_space(encoding)) |
			     OVERLAY_ROTATION(to_vs_rotation(rotation)),
			     OVERLAY_DEC_EN_MASK |
			     OVERLAY_CLEAR_EN_MASK |
			     OVERLAY_FB_EN_MASK |
			     OVERLAY_FORMAT_MASK |
			     OVERLAY_UV_SWIZ_MASK |
			     OVERLAY_SWIZ_MASK |
			     OVERLAY_TILE_MASK |
			     OVERLAY_YUV_COLOR_MASK |
			     OVERLAY_ROTATION_MASK);

	 dc_set_clear(hw, DC_OVERLAY_CONFIG_EX + offset,
		      OVERLAY_LAYER_SEL(zpos) |
		      OVERLAY_PANEL_SEL(display_id),
		      OVERLAY_LAYER_SEL_MASK |
		      OVERLAY_PANEL_SEL_MASK);
	}
}

static u32 calc_factor(u32 src, u32 dest)
{
	u32 factor = 1 << 16;

	if (src > 1 && dest > 1)
		factor = ((src - 1) << 16) / (dest - 1);

	return factor;
}

void plane_hw_update_scale(struct vs_dc *dc, struct drm_rect *src, struct drm_rect *dst,
			   u8 id, u8 display_id, unsigned int rotation)
{
	u32 offset = dc->planes[id].offset;
	struct dc_hw *hw = &dc->hw;

	int dst_w = drm_rect_width(dst);
	int dst_h = drm_rect_height(dst);
	int src_w, src_h, temp;
	u32 scale_factor_x;
	u32 scale_factor_y;
	bool enable_scale = false;

	src_w = drm_rect_width(src) >> 16;
	src_h = drm_rect_height(src) >> 16;

	if (drm_rotation_90_or_270(rotation)) {
		temp = src_w;
		src_w = src_h;
		src_h = temp;
	}

	if (src_w != dst_w) {
		scale_factor_x = calc_factor(src_w, dst_w);
		enable_scale = true;
	} else {
		scale_factor_x = 1 << 16;
	}
	if (src_h != dst_h) {
		scale_factor_y = calc_factor(src_h, dst_h);
		enable_scale = true;
	} else {
		scale_factor_y = 1 << 16;
	}
	if (enable_scale) {
		dc_write(hw, hw->reg[id].scale_factor_x + offset, scale_factor_x);
		dc_write(hw, hw->reg[id].scale_factor_y + offset, scale_factor_y);
		if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG + offset, PRIMARY_SCALE_EN, 0);
		else
			dc_set_clear(hw, DC_OVERLAY_SCALE_CONFIG + offset, OVERLAY_SCALE_EN, 0);
	} else {
		if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG + offset, 0, PRIMARY_SCALE_EN);
		else
			dc_set_clear(hw, DC_OVERLAY_SCALE_CONFIG + offset, 0, OVERLAY_SCALE_EN);
	}

	dc_write(hw, hw->reg[id].top_left + offset, X_POS(dst->x1) | Y_POS(dst->y1));
	dc_write(hw, hw->reg[id].bottom_right + offset, X_POS(dst->x2) | Y_POS(dst->y2));
}

void plane_hw_update_blend(struct vs_dc *dc, u16 alpha,
			   u16 pixel_blend_mode, u8 id, u8 display_id)
{
	u32 offset = dc->planes[id].offset;
	struct dc_hw *hw = &dc->hw;

	dc_write(hw, hw->reg[id].src_global_color + offset, PRIMARY_ALPHA_LEN(alpha >> 8));
	dc_write(hw, hw->reg[id].dst_global_color + offset, PRIMARY_ALPHA_LEN(alpha >> 8));
	switch (pixel_blend_mode) {
	case DRM_MODE_BLEND_PREMULTI:
		dc_write(hw, hw->reg[id].blend_config + offset, BLEND_PREMULTI);
		break;
	case DRM_MODE_BLEND_COVERAGE:
		dc_write(hw, hw->reg[id].blend_config + offset, BLEND_COVERAGE);
		break;
	case DRM_MODE_BLEND_PIXEL_NONE:
		dc_write(hw, hw->reg[id].blend_config + offset, BLEND_PIXEL_NONE);
		break;
	default:
		break;
	}
}
