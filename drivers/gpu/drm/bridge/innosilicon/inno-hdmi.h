/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 *    Zheng Yang <zhengyang@rock-chips.com>
 *    Yakir Yang <ykk@rock-chips.com>
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 */

#ifndef __INNO_H__
#define __INNO_H__

#define DDC_SEGMENT_ADDR		0x30

#define HDMI_SCL_RATE			(100 * 1000)
#define DDC_BUS_FREQ_L			0x4b
#define DDC_BUS_FREQ_H			0x4c

#define HDMI_SYS_CTRL			0x00
#define m_RST_ANALOG			(1 << 6)
#define v_RST_ANALOG			(0 << 6)
#define v_NOT_RST_ANALOG		(1 << 6)
#define m_RST_DIGITAL			(1 << 5)
#define v_RST_DIGITAL			(0 << 5)
#define v_NOT_RST_DIGITAL		(1 << 5)
#define m_REG_CLK_INV			(1 << 4)
#define v_REG_CLK_NOT_INV		(0 << 4)
#define v_REG_CLK_INV			(1 << 4)
#define m_VCLK_INV			(1 << 3)
#define v_VCLK_NOT_INV			(0 << 3)
#define v_VCLK_INV			(1 << 3)
#define m_REG_CLK_SOURCE		(1 << 2)
#define v_REG_CLK_SOURCE_TMDS		(0 << 2)
#define v_REG_CLK_SOURCE_SYS		(1 << 2)
#define m_POWER				(1 << 1)
#define v_PWR_ON			(0 << 1)
#define v_PWR_OFF			(1 << 1)
#define m_INT_POL			(1 << 0)
#define v_INT_POL_HIGH			1
#define v_INT_POL_LOW			0

#define HDMI_VIDEO_TIMING_CTL		0x08
#define v_HSYNC_POLARITY(n)		((n) << 3)
#define v_VSYNC_POLARITY(n)		((n) << 2)
#define v_VSYNC_POLARITY_SF(n)		((n) << 3)
#define v_HSYNC_POLARITY_SF(n)		((n) << 2)
#define v_INETLACE(n)			((n) << 1)
#define v_EXTERANL_VIDEO(n)		((n) << 0)

#define HDMI_VIDEO_EXT_HTOTAL_L		0x09
#define HDMI_VIDEO_EXT_HTOTAL_H		0x0a
#define HDMI_VIDEO_EXT_HBLANK_L		0x0b
#define HDMI_VIDEO_EXT_HBLANK_H		0x0c
#define HDMI_VIDEO_EXT_HDELAY_L		0x0d
#define HDMI_VIDEO_EXT_HDELAY_H		0x0e
#define HDMI_VIDEO_EXT_HDURATION_L	0x0f
#define HDMI_VIDEO_EXT_HDURATION_H	0x10
#define HDMI_VIDEO_EXT_VTOTAL_L		0x11
#define HDMI_VIDEO_EXT_VTOTAL_H		0x12
#define HDMI_VIDEO_EXT_VBLANK		0x13
#define HDMI_VIDEO_EXT_VDELAY		0x14
#define HDMI_VIDEO_EXT_VDURATION	0x15

#define HDMI_EDID_SEGMENT_POINTER	0x4d
#define HDMI_EDID_WORD_ADDR		0x4e
#define HDMI_EDID_FIFO_OFFSET		0x4f
#define HDMI_EDID_FIFO_ADDR		0x50

#define HDMI_INTERRUPT_MASK1		0xc0
#define HDMI_INTERRUPT_STATUS1		0xc1
#define	m_INT_ACTIVE_VSYNC		(1 << 5)
#define m_INT_EDID_READY		(1 << 2)

#define HDMI_CONTROL_PACKET_BUF_INDEX	0x9f
enum {
	INFOFRAME_VSI = 0x05,
	INFOFRAME_AVI = 0x06,
	INFOFRAME_AAI = 0x08,
};

#define HDMI_CONTROL_PACKET_ADDR	0xa0
#define HDMI_MAXIMUM_INFO_FRAME_SIZE	0x11

#define HDMI_STATUS			0xc8
#define m_HOTPLUG			(1 << 7)
#define m_MASK_INT_HOTPLUG		(1 << 5)
#define m_INT_HOTPLUG			(1 << 1)
#define v_MASK_INT_HOTPLUG(n)		(((n) & 0x1) << 5)

#define HDMI_PHY_FEEDBACK_DIV_RATIO_LOW		0xe7
#define v_FEEDBACK_DIV_LOW(n)			((n) & 0xff)
#define HDMI_PHY_FEEDBACK_DIV_RATIO_HIGH	0xe8
#define v_FEEDBACK_DIV_HIGH(n)			((n) & 1)

#define HDMI_PHY_PRE_DIV_RATIO		0xed
#define v_PRE_DIV_RATIO(n)		((n) & 0x1f)

#endif /* __INNO_H__ */
