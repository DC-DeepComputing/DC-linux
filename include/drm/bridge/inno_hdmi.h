/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 */

#ifndef __INNO_COMMON_HDMI__
#define __INNO_COMMON_HDMI__

#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>

struct inno_hdmi_connector_state {
	struct drm_connector_state	base;
	unsigned int			enc_out_format;
	unsigned int			colorimetry;
	bool				rgb_limited_range;
};

enum inno_hdmi_devtype {
	RK3036_HDMI,
	RK3128_HDMI,
	STARFIVE_HDMI,
};

#define to_inno_hdmi_conn_state(conn_state) \
	container_of_const(conn_state, struct inno_hdmi_connector_state, base)

struct inno_hdmi_phy_config {
	unsigned long pixelclock;
	u8 pre_emphasis;
	u8 voltage_level_control;
};

struct inno_hdmi_plat_data {
	enum inno_hdmi_devtype soc_type;

	/* Platform-specific mode validation*/
	enum drm_mode_status (*mode_valid)(struct drm_connector *connector,
					   struct drm_display_mode *mode);
	/* Platform-specific encoder helper funcs*/
	const struct drm_encoder_helper_funcs *helper_private;

	/* Platform-specific phy_configs Optional*/
	struct inno_hdmi_phy_config *phy_configs;
	struct inno_hdmi_phy_config *default_phy_config;
};

struct inno_hdmi {
	struct device *dev;
	void __iomem *regs;

	struct drm_connector	connector;
	struct inno_hdmi_i2c	*i2c;
	struct i2c_adapter	*ddc;
	const struct inno_hdmi_plat_data *plat_data;
};

u8 hdmi_readb(struct inno_hdmi *hdmi, u16 offset);
void hdmi_writeb(struct inno_hdmi *hdmi, u16 offset, u32 val);
void hdmi_modb(struct inno_hdmi *hdmi, u16 offset, u32 msk, u32 val);

struct inno_hdmi *connector_to_inno_hdmi(struct drm_connector *connector);
void inno_hdmi_i2c_init(struct inno_hdmi *hdmi, unsigned long long rate);
int inno_hdmi_bind(struct drm_device *drm, struct inno_hdmi *hdmi, struct drm_encoder *encoder);
void inno_hdmi_sys_power(struct inno_hdmi *hdmi, bool enable);
int inno_hdmi_config_video_avi(struct inno_hdmi *hdmi, struct drm_display_mode *mode);
int inno_hdmi_config_video_timing(struct inno_hdmi *hdmi, struct drm_display_mode *mode);

#endif /* __INNO_COMMON_HDMI__ */
