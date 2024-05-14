// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 */

#include <linux/component.h>
#include <linux/i2c.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_managed.h>
#include <drm/drm_of.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/bridge/inno_hdmi.h>
#include <sound/hdmi-codec.h>

#include "vs_drv.h"
#include "inno_hdmi-starfive.h"

enum hdmi_clk {
	CLK_SYS = 0,
	CLK_M,
	CLK_B,
	CLK_HDMI_NUM
};

struct pre_pll_config {
	unsigned long pixclock;
	unsigned long tmdsclock;
	u8 prediv;
	u16 fbdiv;
	u8 tmds_div_a;
	u8 tmds_div_b;
	u8 tmds_div_c;
	u8 pclk_div_a;
	u8 pclk_div_b;
	u8 pclk_div_c;
	u8 pclk_div_d;
	u8 vco_div_5_en;
	u32 fracdiv;
};

struct post_pll_config {
	unsigned long tmdsclock;
	u8 prediv;
	u16 fbdiv;
	u8 postdiv;
	u8 post_div_en;
	u8 version;
};

struct stf_inno_hdmi {
	struct drm_encoder	encoder;
	struct inno_hdmi inno_hdmi;
	struct clk_bulk_data clk_hdmi[CLK_HDMI_NUM];
	struct reset_control *tx_rst;
	int	nclks;
	unsigned long tmds_rate;
	struct pre_pll_config pre_cfg;
	const struct post_pll_config *post_cfg;
};

static inline struct stf_inno_hdmi *to_starfive_inno_hdmi(struct drm_encoder *encoder)
{
	return container_of(encoder, struct stf_inno_hdmi, encoder);
}

static struct inno_hdmi *encoder_to_inno_hdmi(struct drm_encoder *encoder)
{
	struct stf_inno_hdmi *stf_hdmi = to_starfive_inno_hdmi(encoder);

	return &stf_hdmi->inno_hdmi;
}

static const struct post_pll_config post_pll_cfg_table[] = {
	{25200000, 1, 80, 13, 3, 1},
	{27000000, 1, 40, 11, 3, 1},
	{33750000, 1, 40, 11, 3, 1},
	{49000000, 1, 20, 1, 3, 3},
	{241700000, 1, 20, 1, 3, 3},
	{297000000, 4, 20, 0, 0, 3},
	{ /* sentinel */ }
};

static int starfive_hdmi_enable_clk_rst(struct device *dev, struct stf_inno_hdmi *stf_inno_hdmi)
{
	int ret;

	ret = clk_bulk_prepare_enable(stf_inno_hdmi->nclks, stf_inno_hdmi->clk_hdmi);
	if (ret) {
		dev_err(dev, "failed to enable clocks\n");
		return ret;
	}

	ret = reset_control_deassert(stf_inno_hdmi->tx_rst);
	if (ret < 0) {
		dev_err(dev, "failed to deassert tx_rst\n");
		return ret;
	}
	return 0;
}

static void starfive_hdmi_disable_clk_rst(struct device *dev, struct stf_inno_hdmi *stf_inno_hdmi)
{
	int ret;

	ret = reset_control_assert(stf_inno_hdmi->tx_rst);
	if (ret < 0)
		dev_err(dev, "failed to assert tx_rst\n");

	clk_bulk_disable_unprepare(stf_inno_hdmi->nclks, stf_inno_hdmi->clk_hdmi);
}

static void starfive_hdmi_config_pll(struct stf_inno_hdmi *stf_inno_hdmi)
{
	u32 val;
	struct inno_hdmi *hdmi;

	hdmi = &stf_inno_hdmi->inno_hdmi;
	u8 reg_1ad_value = stf_inno_hdmi->post_cfg->post_div_en ?
			   stf_inno_hdmi->post_cfg->postdiv : 0x00;
	u8 reg_1aa_value = stf_inno_hdmi->post_cfg->post_div_en ?
			   0x0e : 0x02;

	hdmi_writeb(hdmi, STF_INNO_PRE_PLL_CONTROL, STF_INNO_PRE_PLL_POWER_DOWN);
	hdmi_writeb(hdmi, STF_INNO_POST_PLL_DIV_1,
		    STF_INNO_POST_PLL_POST_DIV_ENABLE |
		    STF_INNO_POST_PLL_REFCLK_SEL_TMDS |
		    STF_INNO_POST_PLL_POWER_DOWN);
	hdmi_writeb(hdmi, STF_INNO_PRE_PLL_DIV_1,
		    STF_INNO_PRE_PLL_PRE_DIV(stf_inno_hdmi->pre_cfg.prediv));

	val = STF_INNO_SPREAD_SPECTRUM_MOD_DISABLE | STF_INNO_SPREAD_SPECTRUM_MOD_DOWN;
	if (!stf_inno_hdmi->pre_cfg.fracdiv)
		val |= STF_INNO_PRE_PLL_FRAC_DIV_DISABLE;
	hdmi_writeb(hdmi, STF_INNO_PRE_PLL_DIV_2,
		    STF_INNO_PRE_PLL_FB_DIV_11_8(stf_inno_hdmi->pre_cfg.fbdiv) | val);
	hdmi_writeb(hdmi, STF_INNO_PRE_PLL_DIV_3,
		    STF_INNO_PRE_PLL_FB_DIV_7_0(stf_inno_hdmi->pre_cfg.fbdiv));
	hdmi_writeb(hdmi, STF_INNO_PRE_PLL_DIV_4,
		    STF_INNO_PRE_PLL_TMDSCLK_DIV_C(stf_inno_hdmi->pre_cfg.tmds_div_c) |
		    STF_INNO_PRE_PLL_TMDSCLK_DIV_A(stf_inno_hdmi->pre_cfg.tmds_div_a) |
		    STF_INNO_PRE_PLL_TMDSCLK_DIV_B(stf_inno_hdmi->pre_cfg.tmds_div_b));

	if (stf_inno_hdmi->pre_cfg.fracdiv) {
		hdmi_writeb(hdmi, STF_INNO_PRE_PLL_FRAC_DIV_L,
			    STF_INNO_PRE_PLL_FRAC_DIV_7_0(stf_inno_hdmi->pre_cfg.fracdiv));
		hdmi_writeb(hdmi, STF_INNO_PRE_PLL_FRAC_DIV_M,
			    STF_INNO_PRE_PLL_FRAC_DIV_15_8(stf_inno_hdmi->pre_cfg.fracdiv));
		hdmi_writeb(hdmi, STF_INNO_PRE_PLL_FRAC_DIV_H,
			    STF_INNO_PRE_PLL_FRAC_DIV_23_16(stf_inno_hdmi->pre_cfg.fracdiv));
	}

	hdmi_writeb(hdmi, STF_INNO_PRE_PLL_DIV_5,
		    STF_INNO_PRE_PLL_PCLK_DIV_A(stf_inno_hdmi->pre_cfg.pclk_div_a) |
		    STF_INNO_PRE_PLL_PCLK_DIV_B(stf_inno_hdmi->pre_cfg.pclk_div_b));
	hdmi_writeb(hdmi, STF_INNO_PRE_PLL_DIV_6,
		    STF_INNO_PRE_PLL_PCLK_DIV_C(stf_inno_hdmi->pre_cfg.pclk_div_c) |
		    STF_INNO_PRE_PLL_PCLK_DIV_D(stf_inno_hdmi->pre_cfg.pclk_div_d));

	/*pre-pll power down*/
	hdmi_modb(hdmi, STF_INNO_PRE_PLL_CONTROL, STF_INNO_PRE_PLL_POWER_DOWN, 0);

	hdmi_modb(hdmi, STF_INNO_POST_PLL_DIV_2, STF_INNO_POST_PLL_Pre_DIV_MASK,
		  STF_INNO_POST_PLL_PRE_DIV(stf_inno_hdmi->post_cfg->prediv));
	hdmi_writeb(hdmi, STF_INNO_POST_PLL_DIV_3, stf_inno_hdmi->post_cfg->fbdiv & 0xff);
	hdmi_writeb(hdmi, STF_INNO_POST_PLL_DIV_4, reg_1ad_value);
	hdmi_writeb(hdmi, STF_INNO_POST_PLL_DIV_1, reg_1aa_value);
}

static void starfive_hdmi_tmds_driver_on(struct inno_hdmi *hdmi)
{
	hdmi_modb(hdmi, STF_INNO_TMDS_CONTROL,
		  STF_INNO_TMDS_DRIVER_ENABLE, STF_INNO_TMDS_DRIVER_ENABLE);
}

static void starfive_hdmi_sync_tmds(struct inno_hdmi *hdmi)
{
	/*first send 0 to this bit, then send 1 and keep 1 into this bit*/
	hdmi_writeb(hdmi, HDMI_SYNC, 0x0);
	hdmi_writeb(hdmi, HDMI_SYNC, 0x1);
}

static void starfive_hdmi_phy_get_pre_pll_cfg(struct stf_inno_hdmi *hdmi)
{
	if (hdmi->tmds_rate > 30000000) {
		hdmi->pre_cfg.pixclock = hdmi->tmds_rate;
		hdmi->pre_cfg.tmdsclock = hdmi->tmds_rate;
		hdmi->pre_cfg.prediv = 1;
		hdmi->pre_cfg.fbdiv = hdmi->tmds_rate / 3000000;
		hdmi->pre_cfg.tmds_div_a = 0;
		hdmi->pre_cfg.tmds_div_b = 1;
		hdmi->pre_cfg.tmds_div_c = 1;
		hdmi->pre_cfg.pclk_div_a = 1;
		hdmi->pre_cfg.pclk_div_b = 0;
		hdmi->pre_cfg.pclk_div_c = 2;
		hdmi->pre_cfg.pclk_div_d = 2;
		hdmi->pre_cfg.vco_div_5_en = hdmi->tmds_rate % 3000000 ? 1 : 0;

		if (hdmi->pre_cfg.vco_div_5_en) {
			hdmi->pre_cfg.fracdiv = (hdmi->tmds_rate % 3000000) *
						 0xffffff / 1000000;
		}
	} else {
		hdmi->pre_cfg.pixclock = hdmi->tmds_rate;
		hdmi->pre_cfg.tmdsclock = hdmi->tmds_rate;
		hdmi->pre_cfg.prediv = 1;
		hdmi->pre_cfg.fbdiv = hdmi->tmds_rate / 1000000;
		hdmi->pre_cfg.tmds_div_a = 2;
		hdmi->pre_cfg.tmds_div_b = 1;
		hdmi->pre_cfg.tmds_div_c = 1;
		hdmi->pre_cfg.pclk_div_a = 3;
		hdmi->pre_cfg.pclk_div_b = 0;
		hdmi->pre_cfg.pclk_div_c = 3;
		hdmi->pre_cfg.pclk_div_d = 4;
		hdmi->pre_cfg.vco_div_5_en = hdmi->tmds_rate % 1000000 ? 1 : 0;

		if (hdmi->pre_cfg.vco_div_5_en) {
			hdmi->pre_cfg.fracdiv = (hdmi->tmds_rate % 1000000) *
						 0xffffff / 1000000;
		}
	}
}

static int starfive_hdmi_phy_clk_set_rate(struct stf_inno_hdmi *stf_inno_hdmi)
{
	stf_inno_hdmi->post_cfg = post_pll_cfg_table;

	starfive_hdmi_phy_get_pre_pll_cfg(stf_inno_hdmi);

	for (; stf_inno_hdmi->post_cfg->tmdsclock != 0; stf_inno_hdmi->post_cfg++)
		if (stf_inno_hdmi->tmds_rate <= stf_inno_hdmi->post_cfg->tmdsclock)
			break;

	starfive_hdmi_config_pll(stf_inno_hdmi);

	return 0;
}

static int starfive_hdmi_setup(struct inno_hdmi *hdmi,
			       struct drm_display_mode *mode)
{
	struct stf_inno_hdmi *stf_inno_hdmi = dev_get_drvdata(hdmi->dev);
	struct drm_display_info *display = &hdmi->connector.display_info;
	int ret;
	u32 val;

	hdmi_modb(hdmi, STF_INNO_BIAS_CONTROL, STF_INNO_BIAS_ENABLE, STF_INNO_BIAS_ENABLE);
	hdmi_writeb(hdmi, STF_INNO_RX_CONTROL, STF_INNO_RX_ENABLE);

	stf_inno_hdmi->tmds_rate = mode->clock * 1000;
	starfive_hdmi_phy_clk_set_rate(stf_inno_hdmi);

	ret = readx_poll_timeout(readl_relaxed,
				 hdmi->regs + (STF_INNO_PRE_PLL_LOCK_STATUS) * 0x04,
				 val, val & 0x1, 1000, 100000);
	if (ret < 0) {
		dev_err(hdmi->dev, "failed to wait pre-pll lock\n");
		return ret;
	}

	ret = readx_poll_timeout(readl_relaxed,
				 hdmi->regs + (STF_INNO_POST_PLL_LOCK_STATUS) * 0x04,
				 val, val & 0x1, 1000, 100000);
	if (ret < 0) {
		dev_err(hdmi->dev, "failed to wait post-pll lock\n");
		return ret;
	}

	/*turn on LDO*/
	hdmi_writeb(hdmi, STF_INNO_LDO_CONTROL, STF_INNO_LDO_ENABLE);
	/*turn on serializer*/
	hdmi_writeb(hdmi, STF_INNO_SERIALIER_CONTROL, STF_INNO_SERIALIER_ENABLE);

	if (display->is_hdmi)
		inno_hdmi_config_video_avi(hdmi, mode);

	inno_hdmi_sys_power(hdmi, false);
	inno_hdmi_config_video_timing(hdmi, mode);
	inno_hdmi_sys_power(hdmi, true);

	starfive_hdmi_tmds_driver_on(hdmi);
	starfive_hdmi_sync_tmds(hdmi);

	return 0;
}

static enum drm_mode_status
starfive_hdmi_connector_mode_valid(struct drm_connector *connector, struct drm_display_mode *mode)
{
	int pclk = mode->clock * 1000;
	bool valid = false;

	if (pclk <= PIXCLOCK_4K_30FPS)
		valid = true;

	return (valid) ? MODE_OK : MODE_BAD;
}

static void starfive_hdmi_encoder_enable(struct drm_encoder *encoder,
					 struct drm_atomic_state *state)
{
	struct inno_hdmi *hdmi = encoder_to_inno_hdmi(encoder);
	struct drm_connector_state *conn_state;
	struct drm_crtc_state *crtc_state;

	conn_state = drm_atomic_get_new_connector_state(state, &hdmi->connector);
	if (WARN_ON(!conn_state))
		return;

	crtc_state = drm_atomic_get_new_crtc_state(state, conn_state->crtc);
	if (WARN_ON(!crtc_state))
		return;

	starfive_hdmi_setup(hdmi, &crtc_state->adjusted_mode);
}

static void starfive_hdmi_encoder_disable(struct drm_encoder *encoder,
					  struct drm_atomic_state *state)
{
	struct inno_hdmi *hdmi = encoder_to_inno_hdmi(encoder);

	inno_hdmi_sys_power(hdmi, false);
}

static int
starfive_hdmi_encoder_atomic_check(struct drm_encoder *encoder,
				   struct drm_crtc_state *crtc_state,
				   struct drm_connector_state *conn_state)
{
	struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	u8 vic = drm_match_cea_mode(mode);
	struct inno_hdmi_connector_state *inno_conn_state = to_inno_hdmi_conn_state(conn_state);

	struct vs_crtc_state *vs_crtc_state = to_vs_crtc_state(crtc_state);

	vs_crtc_state->encoder_type = encoder->encoder_type;
	vs_crtc_state->output_fmt = MEDIA_BUS_FMT_RGB888_1X24;

	if (vic == 6 || vic == 7 || vic == 21 || vic == 22 ||
	    vic == 2 || vic == 3 || vic == 17 || vic == 18)
		inno_conn_state->colorimetry = HDMI_COLORIMETRY_ITU_601;
	else
		inno_conn_state->colorimetry = HDMI_COLORIMETRY_ITU_709;

	inno_conn_state->enc_out_format = HDMI_COLORSPACE_RGB;
	inno_conn_state->rgb_limited_range =
		drm_default_rgb_quant_range(mode) == HDMI_QUANTIZATION_RANGE_LIMITED;

	return  starfive_hdmi_connector_mode_valid(conn_state->connector,
			&crtc_state->adjusted_mode) == MODE_OK ? 0 : -EINVAL;
}

static const struct drm_encoder_helper_funcs stf_inno_encoder_helper_funcs = {
	.atomic_check   = starfive_hdmi_encoder_atomic_check,
	.atomic_enable  = starfive_hdmi_encoder_enable,
	.atomic_disable = starfive_hdmi_encoder_disable,
};

static int starfive_hdmi_get_clk_rst(struct device *dev, struct stf_inno_hdmi *stf_hdmi)
{
	int ret;

	stf_hdmi->nclks = ARRAY_SIZE(stf_hdmi->clk_hdmi);

	ret = devm_clk_bulk_get(dev, stf_hdmi->nclks, stf_hdmi->clk_hdmi);
	if (ret) {
		dev_err(dev, "Failed to get clk controls\n");
		return ret;
	}

	stf_hdmi->tx_rst = devm_reset_control_get_by_index(dev, 0);
	if (IS_ERR(stf_hdmi->tx_rst)) {
		dev_err(dev, "failed to get tx_rst reset\n");
		return PTR_ERR(stf_hdmi->tx_rst);
	}

	return 0;
}

static int starfive_hdmi_bind(struct device *dev, struct device *master,
			      void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = dev_get_drvdata(master);
	struct stf_inno_hdmi *stf_hdmi;
	struct inno_hdmi *hdmi;
	struct resource *iores;

	int ret;
	unsigned long long rate;

	stf_hdmi = drmm_simple_encoder_alloc(drm, struct stf_inno_hdmi,
					     encoder, DRM_MODE_ENCODER_TMDS);
	hdmi = &stf_hdmi->inno_hdmi;

	hdmi->dev = dev;
	hdmi->plat_data = (struct inno_hdmi_plat_data *)of_device_get_match_data(dev);

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hdmi->regs = devm_ioremap_resource(dev, iores);
	if (IS_ERR(hdmi->regs))
		return PTR_ERR(hdmi->regs);

	ret = starfive_hdmi_get_clk_rst(dev, stf_hdmi);
	if (ret < 0)
		return ret;

	ret = starfive_hdmi_enable_clk_rst(dev, stf_hdmi);
	if (ret)
		return ret;

	rate = clk_get_rate(stf_hdmi->clk_hdmi[CLK_SYS].clk);
	inno_hdmi_i2c_init(hdmi, rate);

	ret = inno_hdmi_bind(drm, hdmi, &stf_hdmi->encoder);
	if (ret)
		goto err_disable_clk;

	dev_set_drvdata(dev, stf_hdmi);

	return 0;

err_disable_clk:
	starfive_hdmi_disable_clk_rst(dev, stf_hdmi);
	return ret;
}

static void starfive_hdmi_unbind(struct device *dev, struct device *master, void *data)
{
	struct stf_inno_hdmi *stf_hdmi = dev_get_drvdata(dev);

	struct inno_hdmi *hdmi = &stf_hdmi->inno_hdmi;

	i2c_put_adapter(hdmi->ddc);
	starfive_hdmi_disable_clk_rst(dev, stf_hdmi);
}

static const struct component_ops starfive_hdmi_ops = {
	.bind	= starfive_hdmi_bind,
	.unbind	= starfive_hdmi_unbind,
};

static int starfive_hdmi_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &starfive_hdmi_ops);
}

static int starfive_hdmi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &starfive_hdmi_ops);

	return 0;
}

static const struct inno_hdmi_plat_data stf_inno_info = {
	.soc_type = STARFIVE_HDMI,
	.mode_valid = starfive_hdmi_connector_mode_valid,
	.helper_private = &stf_inno_encoder_helper_funcs,
};

static const struct of_device_id starfive_hdmi_dt_ids[] = {
	{ .compatible = "starfive,jh7110-inno-hdmi", .data = &stf_inno_info,},
	{},
};
MODULE_DEVICE_TABLE(of, starfive_hdmi_dt_ids);

struct platform_driver starfive_hdmi_driver = {
	.probe  = starfive_hdmi_probe,
	.remove = starfive_hdmi_remove,
	.driver = {
		.name = "starfive-hdmi",
		.of_match_table = starfive_hdmi_dt_ids,
	},
};

MODULE_AUTHOR("StarFive Corporation");
MODULE_DESCRIPTION("Starfive Specific INNO-HDMI Driver");
