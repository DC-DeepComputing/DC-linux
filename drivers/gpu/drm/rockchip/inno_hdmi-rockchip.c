// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 *    Zheng Yang <zhengyang@rock-chips.com>
 *    Yakir Yang <ykk@rock-chips.com>
 */

#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hdmi.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <drm/bridge/inno_hdmi.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>

#include "rockchip_drm_drv.h"

#include "inno_hdmi-rockchip.h"

#define INNO_HDMI_MIN_TMDS_CLOCK  25000000U

struct rk_inno_hdmi {
	struct rockchip_encoder encoder;
	struct inno_hdmi inno_hdmi;
	struct clk *pclk;
	struct clk *refclk;
};

static struct inno_hdmi *rk_encoder_to_inno_hdmi(struct drm_encoder *encoder)
{
	struct rockchip_encoder *rkencoder = to_rockchip_encoder(encoder);
	struct rk_inno_hdmi *rk_hdmi = container_of(rkencoder, struct rk_inno_hdmi, encoder);

	return &rk_hdmi->inno_hdmi;
}

enum {
	CSC_RGB_0_255_TO_ITU601_16_235_8BIT,
	CSC_RGB_0_255_TO_ITU709_16_235_8BIT,
	CSC_RGB_0_255_TO_RGB_16_235_8BIT,
};

static const char coeff_csc[][24] = {
	/*
	 * RGB2YUV:601 SD mode:
	 *   Cb = -0.291G - 0.148R + 0.439B + 128
	 *   Y  = 0.504G  + 0.257R + 0.098B + 16
	 *   Cr = -0.368G + 0.439R - 0.071B + 128
	 */
	{
		0x11, 0x5f, 0x01, 0x82, 0x10, 0x23, 0x00, 0x80,
		0x02, 0x1c, 0x00, 0xa1, 0x00, 0x36, 0x00, 0x1e,
		0x11, 0x29, 0x10, 0x59, 0x01, 0x82, 0x00, 0x80
	},
	/*
	 * RGB2YUV:709 HD mode:
	 *   Cb = - 0.338G - 0.101R + 0.439B + 128
	 *   Y  = 0.614G   + 0.183R + 0.062B + 16
	 *   Cr = - 0.399G + 0.439R - 0.040B + 128
	 */
	{
		0x11, 0x98, 0x01, 0xc1, 0x10, 0x28, 0x00, 0x80,
		0x02, 0x74, 0x00, 0xbb, 0x00, 0x3f, 0x00, 0x10,
		0x11, 0x5a, 0x10, 0x67, 0x01, 0xc1, 0x00, 0x80
	},
	/*
	 * RGB[0:255]2RGB[16:235]:
	 *   R' = R x (235-16)/255 + 16;
	 *   G' = G x (235-16)/255 + 16;
	 *   B' = B x (235-16)/255 + 16;
	 */
	{
		0x00, 0x00, 0x03, 0x6F, 0x00, 0x00, 0x00, 0x10,
		0x03, 0x6F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
		0x00, 0x00, 0x00, 0x00, 0x03, 0x6F, 0x00, 0x10
	},
};

static struct inno_hdmi_phy_config rk3036_hdmi_phy_configs[] = {
	{  74250000, 0x3f, 0xbb },
	{ 165000000, 0x6f, 0xbb },
	{      ~0UL, 0x00, 0x00 }
};

static struct inno_hdmi_phy_config rk3128_hdmi_phy_configs[] = {
	{  74250000, 0x3f, 0xaa },
	{ 165000000, 0x5f, 0xaa },
	{      ~0UL, 0x00, 0x00 }
};

static int inno_hdmi_find_phy_config(struct inno_hdmi *hdmi,
				     unsigned long pixelclk)
{
	const struct inno_hdmi_phy_config *phy_configs = hdmi->plat_data->phy_configs;
	int i;

	for (i = 0; phy_configs[i].pixelclock != ~0UL; i++) {
		if (pixelclk <= phy_configs[i].pixelclock)
			return i;
	}

	DRM_DEV_DEBUG(hdmi->dev, "No phy configuration for pixelclock %lu\n",
		      pixelclk);

	return -EINVAL;
}

static void inno_hdmi_standby(struct inno_hdmi *hdmi)
{
	inno_hdmi_sys_power(hdmi, false);

	hdmi_writeb(hdmi, HDMI_PHY_DRIVER, 0x00);
	hdmi_writeb(hdmi, HDMI_PHY_PRE_EMPHASIS, 0x00);
	hdmi_writeb(hdmi, HDMI_PHY_CHG_PWR, 0x00);
	hdmi_writeb(hdmi, HDMI_PHY_SYS_CTL, 0x15);
};

static void inno_hdmi_power_up(struct inno_hdmi *hdmi,
			       unsigned long mpixelclock)
{
	struct inno_hdmi_phy_config *phy_config;
	int ret = inno_hdmi_find_phy_config(hdmi, mpixelclock);

	if (ret < 0) {
		phy_config = hdmi->plat_data->default_phy_config;
		DRM_DEV_ERROR(hdmi->dev,
			      "Using default phy configuration for TMDS rate %lu",
			      mpixelclock);
	} else {
		phy_config = &hdmi->plat_data->phy_configs[ret];
	}

	inno_hdmi_sys_power(hdmi, false);

	hdmi_writeb(hdmi, HDMI_PHY_PRE_EMPHASIS, phy_config->pre_emphasis);
	hdmi_writeb(hdmi, HDMI_PHY_DRIVER, phy_config->voltage_level_control);
	hdmi_writeb(hdmi, HDMI_PHY_SYS_CTL, 0x15);
	hdmi_writeb(hdmi, HDMI_PHY_SYS_CTL, 0x14);
	hdmi_writeb(hdmi, HDMI_PHY_SYS_CTL, 0x10);
	hdmi_writeb(hdmi, HDMI_PHY_CHG_PWR, 0x0f);
	hdmi_writeb(hdmi, HDMI_PHY_SYNC, 0x00);
	hdmi_writeb(hdmi, HDMI_PHY_SYNC, 0x01);

	inno_hdmi_sys_power(hdmi, true);
};

static void inno_hdmi_reset(struct inno_hdmi *hdmi)
{
	u32 val;
	u32 msk;

	hdmi_modb(hdmi, HDMI_SYS_CTRL, m_RST_DIGITAL, v_NOT_RST_DIGITAL);
	udelay(100);

	hdmi_modb(hdmi, HDMI_SYS_CTRL, m_RST_ANALOG, v_NOT_RST_ANALOG);
	udelay(100);

	msk = m_REG_CLK_INV | m_REG_CLK_SOURCE | m_POWER | m_INT_POL;
	val = v_REG_CLK_INV | v_REG_CLK_SOURCE_SYS | v_PWR_ON | v_INT_POL_HIGH;
	hdmi_modb(hdmi, HDMI_SYS_CTRL, msk, val);

	inno_hdmi_standby(hdmi);
}

static int inno_hdmi_config_video_csc(struct inno_hdmi *hdmi)
{
	struct drm_connector *connector = &hdmi->connector;
	struct drm_connector_state *conn_state = connector->state;
	struct inno_hdmi_connector_state *inno_conn_state =
					to_inno_hdmi_conn_state(conn_state);
	int c0_c2_change = 0;
	int csc_enable = 0;
	int csc_mode = 0;
	int auto_csc = 0;
	int value;
	int i;

	/* Input video mode is SDR RGB24bit, data enable signal from external */
	hdmi_writeb(hdmi, HDMI_VIDEO_CONTRL1, v_DE_EXTERNAL |
		    v_VIDEO_INPUT_FORMAT(VIDEO_INPUT_SDR_RGB444));

	/* Input color hardcode to RGB, and output color hardcode to RGB888 */
	value = v_VIDEO_INPUT_BITS(VIDEO_INPUT_8BITS) |
		v_VIDEO_OUTPUT_COLOR(0) |
		v_VIDEO_INPUT_CSP(0);
	hdmi_writeb(hdmi, HDMI_VIDEO_CONTRL2, value);

	if (inno_conn_state->enc_out_format == HDMI_COLORSPACE_RGB) {
		if (inno_conn_state->rgb_limited_range) {
			csc_mode = CSC_RGB_0_255_TO_RGB_16_235_8BIT;
			auto_csc = AUTO_CSC_DISABLE;
			c0_c2_change = C0_C2_CHANGE_DISABLE;
			csc_enable = v_CSC_ENABLE;

		} else {
			value = v_SOF_DISABLE | v_COLOR_DEPTH_NOT_INDICATED(1);
			hdmi_writeb(hdmi, HDMI_VIDEO_CONTRL3, value);

			hdmi_modb(hdmi, HDMI_VIDEO_CONTRL,
				  m_VIDEO_AUTO_CSC | m_VIDEO_C0_C2_SWAP,
				  v_VIDEO_AUTO_CSC(AUTO_CSC_DISABLE) |
				  v_VIDEO_C0_C2_SWAP(C0_C2_CHANGE_DISABLE));
			return 0;
		}
	} else {
		if (inno_conn_state->colorimetry == HDMI_COLORIMETRY_ITU_601) {
			if (inno_conn_state->enc_out_format == HDMI_COLORSPACE_YUV444) {
				csc_mode = CSC_RGB_0_255_TO_ITU601_16_235_8BIT;
				auto_csc = AUTO_CSC_DISABLE;
				c0_c2_change = C0_C2_CHANGE_DISABLE;
				csc_enable = v_CSC_ENABLE;
			}
		} else {
			if (inno_conn_state->enc_out_format == HDMI_COLORSPACE_YUV444) {
				csc_mode = CSC_RGB_0_255_TO_ITU709_16_235_8BIT;
				auto_csc = AUTO_CSC_DISABLE;
				c0_c2_change = C0_C2_CHANGE_DISABLE;
				csc_enable = v_CSC_ENABLE;
			}
		}
	}

	for (i = 0; i < 24; i++)
		hdmi_writeb(hdmi, HDMI_VIDEO_CSC_COEF + i,
			    coeff_csc[csc_mode][i]);

	value = v_SOF_DISABLE | csc_enable | v_COLOR_DEPTH_NOT_INDICATED(1);
	hdmi_writeb(hdmi, HDMI_VIDEO_CONTRL3, value);
	hdmi_modb(hdmi, HDMI_VIDEO_CONTRL, m_VIDEO_AUTO_CSC |
		  m_VIDEO_C0_C2_SWAP, v_VIDEO_AUTO_CSC(auto_csc) |
		  v_VIDEO_C0_C2_SWAP(c0_c2_change));

	return 0;
}

static int inno_hdmi_setup(struct inno_hdmi *hdmi,
			   struct drm_display_mode *mode)
{
	struct drm_display_info *display = &hdmi->connector.display_info;
	unsigned long mpixelclock = mode->clock * 1000;

	/* Mute video and audio output */
	hdmi_modb(hdmi, HDMI_AV_MUTE, m_AUDIO_MUTE | m_VIDEO_BLACK,
		  v_AUDIO_MUTE(1) | v_VIDEO_MUTE(1));

	/* Set HDMI Mode */
	hdmi_writeb(hdmi, HDMI_HDCP_CTRL,
		    v_HDMI_DVI(display->is_hdmi));

	inno_hdmi_config_video_timing(hdmi, mode);

	inno_hdmi_config_video_csc(hdmi);

	if (display->is_hdmi)
		inno_hdmi_config_video_avi(hdmi, mode);

	/*
	 * When IP controller have configured to an accurate video
	 * timing, then the TMDS clock source would be switched to
	 * DCLK_LCDC, so we need to init the TMDS rate to mode pixel
	 * clock rate, and reconfigure the DDC clock.
	 */
	inno_hdmi_i2c_init(hdmi, mpixelclock);

	/* Unmute video and audio output */
	hdmi_modb(hdmi, HDMI_AV_MUTE, m_AUDIO_MUTE | m_VIDEO_BLACK,
		  v_AUDIO_MUTE(0) | v_VIDEO_MUTE(0));

	inno_hdmi_power_up(hdmi, mpixelclock);

	return 0;
}

static enum drm_mode_status rk_inno_hdmi_connector_mode_valid(struct drm_connector *connector,
							      struct drm_display_mode *mode)
{
	struct inno_hdmi *hdmi = connector_to_inno_hdmi(connector);
	struct rk_inno_hdmi *rk_hdmi = dev_get_drvdata(hdmi->dev);

	unsigned long mpixelclk, max_tolerance;
	long rounded_refclk;

	/* No support for double-clock modes */
	if (mode->flags & DRM_MODE_FLAG_DBLCLK)
		return MODE_BAD;

	mpixelclk = mode->clock * 1000;

	if (mpixelclk < INNO_HDMI_MIN_TMDS_CLOCK)
		return MODE_CLOCK_LOW;

	if (inno_hdmi_find_phy_config(hdmi, mpixelclk) < 0)
		return MODE_CLOCK_HIGH;

	if (rk_hdmi->refclk) {
		rounded_refclk = clk_round_rate(rk_hdmi->refclk, mpixelclk);
		if (rounded_refclk < 0)
			return MODE_BAD;

		/* Vesa DMT standard mentions +/- 0.5% max tolerance */
		max_tolerance = mpixelclk / 200;
		if (abs_diff((unsigned long)rounded_refclk, mpixelclk) > max_tolerance)
			return MODE_NOCLOCK;
	}

	return MODE_OK;
}

static void rk_inno_hdmi_encoder_enable(struct drm_encoder *encoder,
					struct drm_atomic_state *state)
{
	struct inno_hdmi *hdmi = rk_encoder_to_inno_hdmi(encoder);
	struct drm_connector_state *conn_state;
	struct drm_crtc_state *crtc_state;

	conn_state = drm_atomic_get_new_connector_state(state, &hdmi->connector);
	if (WARN_ON(!conn_state))
		return;

	crtc_state = drm_atomic_get_new_crtc_state(state, conn_state->crtc);
	if (WARN_ON(!crtc_state))
		return;

	inno_hdmi_setup(hdmi, &crtc_state->adjusted_mode);
}

static void rk_inno_hdmi_encoder_disable(struct drm_encoder *encoder,
					 struct drm_atomic_state *state)
{
	struct inno_hdmi *hdmi = rk_encoder_to_inno_hdmi(encoder);

	inno_hdmi_standby(hdmi);
}

static int
rk_inno_hdmi_encoder_atomic_check(struct drm_encoder *encoder,
				  struct drm_crtc_state *crtc_state,
				  struct drm_connector_state *conn_state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	u8 vic = drm_match_cea_mode(mode);
	struct inno_hdmi_connector_state *inno_conn_state =
				   to_inno_hdmi_conn_state(conn_state);

	s->output_mode = ROCKCHIP_OUT_MODE_P888;
	s->output_type = DRM_MODE_CONNECTOR_HDMIA;

	if (vic == 6 || vic == 7 ||
	    vic == 21 || vic == 22 ||
	    vic == 2 || vic == 3 ||
	    vic == 17 || vic == 18)
		inno_conn_state->colorimetry = HDMI_COLORIMETRY_ITU_601;
	else
		inno_conn_state->colorimetry = HDMI_COLORIMETRY_ITU_709;

	inno_conn_state->enc_out_format = HDMI_COLORSPACE_RGB;
	inno_conn_state->rgb_limited_range =
	   drm_default_rgb_quant_range(mode) == HDMI_QUANTIZATION_RANGE_LIMITED;

	return  rk_inno_hdmi_connector_mode_valid(conn_state->connector,
			   &crtc_state->adjusted_mode) == MODE_OK ? 0 : -EINVAL;
}

static const struct drm_encoder_helper_funcs rk_inno_encoder_helper_funcs = {
	.atomic_check   = rk_inno_hdmi_encoder_atomic_check,
	.atomic_enable  = rk_inno_hdmi_encoder_enable,
	.atomic_disable = rk_inno_hdmi_encoder_disable,
};

static int rk_inno_hdmi_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = data;
	struct inno_hdmi *hdmi;
	int ret;
	struct rk_inno_hdmi *rk_hdmi;

	rk_hdmi = devm_kzalloc(dev, sizeof(*rk_hdmi), GFP_KERNEL);
	if (!rk_hdmi)
		return -ENOMEM;
	hdmi = &rk_hdmi->inno_hdmi;

	hdmi->dev = dev;
	hdmi->plat_data = (struct inno_hdmi_plat_data *)of_device_get_match_data(dev);

	hdmi->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(hdmi->regs))
		return PTR_ERR(hdmi->regs);

	rk_hdmi->pclk = devm_clk_get(hdmi->dev, "pclk");
	if (IS_ERR(rk_hdmi->pclk)) {
		DRM_DEV_ERROR(hdmi->dev, "Unable to get HDMI pclk clk\n");
		return PTR_ERR(rk_hdmi->pclk);
	}

	ret = clk_prepare_enable(rk_hdmi->pclk);
	if (ret) {
		DRM_DEV_ERROR(hdmi->dev,
			      "Cannot enable HDMI pclk clock: %d\n", ret);
		return ret;
	}

	rk_hdmi->refclk = devm_clk_get_optional(hdmi->dev, "ref");
	if (IS_ERR(rk_hdmi->refclk)) {
		DRM_DEV_ERROR(hdmi->dev, "Unable to get HDMI reference clock\n");
		ret = PTR_ERR(rk_hdmi->refclk);
		goto err_disable_pclk;
	}

	ret = clk_prepare_enable(rk_hdmi->refclk);
	if (ret) {
		DRM_DEV_ERROR(hdmi->dev,
			      "Cannot enable HDMI reference clock: %d\n", ret);
		goto err_disable_pclk;
	}

	inno_hdmi_reset(hdmi);
	/*
	 * When the controller isn't configured to an accurate
	 * video timing and there is no reference clock available,
	 * then the TMDS clock source would be switched to PCLK_HDMI,
	 * so we need to init the TMDS rate to PCLK rate, and
	 * reconfigure the DDC clock.
	 */
	if (rk_hdmi->refclk)
		inno_hdmi_i2c_init(hdmi, clk_get_rate(rk_hdmi->refclk));
	else
		inno_hdmi_i2c_init(hdmi, clk_get_rate(rk_hdmi->pclk));

	ret = inno_hdmi_bind(drm, hdmi, &rk_hdmi->encoder.encoder);
	if (ret)
		goto err_cleanup_hdmi;

	dev_set_drvdata(dev, rk_hdmi);

	return 0;

err_cleanup_hdmi:
	rk_hdmi->encoder.encoder.funcs->destroy(&rk_hdmi->encoder.encoder);
	clk_disable_unprepare(rk_hdmi->refclk);
err_disable_pclk:
	clk_disable_unprepare(rk_hdmi->pclk);
	return ret;
}

static void rk_inno_hdmi_unbind(struct device *dev, struct device *master, void *data)
{
	struct rk_inno_hdmi *rk_hdmi = dev_get_drvdata(dev);
	struct inno_hdmi *hdmi = &rk_hdmi->inno_hdmi;

	rk_hdmi->encoder.encoder.funcs->destroy(&rk_hdmi->encoder.encoder);
	i2c_put_adapter(hdmi->ddc);
	clk_disable_unprepare(rk_hdmi->refclk);
	clk_disable_unprepare(rk_hdmi->pclk);
}

static const struct component_ops inno_hdmi_ops = {
	.bind	= rk_inno_hdmi_bind,
	.unbind	= rk_inno_hdmi_unbind,
};

static int inno_hdmi_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &inno_hdmi_ops);
}

static void inno_hdmi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &inno_hdmi_ops);
}

static const struct inno_hdmi_plat_data rk3036_inno_info = {
	.soc_type = RK3036_HDMI,
	.mode_valid = rk_inno_hdmi_connector_mode_valid,
	.helper_private = &rk_inno_encoder_helper_funcs,
	.phy_configs = rk3036_hdmi_phy_configs,
	.default_phy_config = &rk3036_hdmi_phy_configs[1],
};

static const struct inno_hdmi_plat_data rk3128_inno_info = {
	.soc_type = RK3128_HDMI,
	.mode_valid = rk_inno_hdmi_connector_mode_valid,
	.helper_private = &rk_inno_encoder_helper_funcs,
	.phy_configs = rk3128_hdmi_phy_configs,
	.default_phy_config = &rk3128_hdmi_phy_configs[1],
};

static const struct of_device_id inno_hdmi_dt_ids[] = {
	{ .compatible = "rockchip,rk3036-inno-hdmi",
	  .data = &rk3036_inno_info,
	},
	{ .compatible = "rockchip,rk3128-inno-hdmi",
	  .data = &rk3128_inno_info,
	},
	{},
};
MODULE_DEVICE_TABLE(of, inno_hdmi_dt_ids);

struct platform_driver inno_hdmi_driver = {
	.probe  = inno_hdmi_probe,
	.remove_new = inno_hdmi_remove,
	.driver = {
		.name = "innohdmi-rockchip",
		.of_match_table = inno_hdmi_dt_ids,
	},
};
