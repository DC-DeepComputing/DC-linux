// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 *    Zheng Yang <zhengyang@rock-chips.com>
 *    Yakir Yang <ykk@rock-chips.com>
 * Copyright (C) 2024 StarFive Technology Co., Ltd.
 */

#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hdmi.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <drm/bridge/inno_hdmi.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>

#include "inno-hdmi.h"

struct inno_hdmi_i2c {
	struct i2c_adapter adap;

	u8 ddc_addr;
	u8 segment_addr;

	struct mutex lock; /* For i2c operation. */
	struct completion cmp;
};

struct inno_hdmi *connector_to_inno_hdmi(struct drm_connector *connector)
{
	return container_of(connector, struct inno_hdmi, connector);
}
EXPORT_SYMBOL_GPL(connector_to_inno_hdmi);

u8 hdmi_readb(struct inno_hdmi *hdmi, u16 offset)
{
	return readl_relaxed(hdmi->regs + (offset) * 0x04);
}
EXPORT_SYMBOL_GPL(hdmi_readb);

void hdmi_writeb(struct inno_hdmi *hdmi, u16 offset, u32 val)
{
	writel_relaxed(val, hdmi->regs + (offset) * 0x04);
}
EXPORT_SYMBOL_GPL(hdmi_writeb);

void hdmi_modb(struct inno_hdmi *hdmi, u16 offset, u32 msk, u32 val)
{
	u8 temp = hdmi_readb(hdmi, offset) & ~msk;

	temp |= val & msk;
	hdmi_writeb(hdmi, offset, temp);
}
EXPORT_SYMBOL_GPL(hdmi_modb);

void inno_hdmi_i2c_init(struct inno_hdmi *hdmi, unsigned long long rate)
{
	unsigned long long ddc_bus_freq = rate >> 2;

	do_div(ddc_bus_freq, HDMI_SCL_RATE);

	hdmi_writeb(hdmi, DDC_BUS_FREQ_L, ddc_bus_freq & 0xFF);
	hdmi_writeb(hdmi, DDC_BUS_FREQ_H, (ddc_bus_freq >> 8) & 0xFF);

	/* Clear the EDID interrupt flag and mute the interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, 0);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);
}
EXPORT_SYMBOL_GPL(inno_hdmi_i2c_init);

void inno_hdmi_sys_power(struct inno_hdmi *hdmi, bool enable)
{
	if (enable)
		hdmi_modb(hdmi, HDMI_SYS_CTRL, m_POWER, v_PWR_ON);
	else
		hdmi_modb(hdmi, HDMI_SYS_CTRL, m_POWER, v_PWR_OFF);
}
EXPORT_SYMBOL_GPL(inno_hdmi_sys_power);

static enum drm_connector_status
inno_hdmi_connector_detect(struct drm_connector *connector, bool force)
{
	struct inno_hdmi *hdmi = connector_to_inno_hdmi(connector);

	return (hdmi_readb(hdmi, HDMI_STATUS) & m_HOTPLUG) ?
		connector_status_connected : connector_status_disconnected;
}

static int inno_hdmi_connector_get_modes(struct drm_connector *connector)
{
	struct inno_hdmi *hdmi = connector_to_inno_hdmi(connector);
	const struct drm_edid *drm_edid;
	int ret = 0;

	if (!hdmi->ddc)
		return 0;

	drm_edid = drm_edid_read_ddc(connector, hdmi->ddc);
	drm_edid_connector_update(connector, drm_edid);
	ret = drm_edid_connector_add_modes(connector);
	drm_edid_free(drm_edid);

	return ret;
}

static enum drm_mode_status
inno_hdmi_connector_mode_valid(struct drm_connector *connector, struct drm_display_mode *mode)
{
	struct inno_hdmi *hdmi = connector_to_inno_hdmi(connector);

	const struct inno_hdmi_plat_data *pdata = hdmi->plat_data;
	enum drm_mode_status mode_status = MODE_OK;

	if (pdata->mode_valid)
		mode_status = pdata->mode_valid(connector, mode);

	return mode_status;
}

static void
inno_hdmi_connector_destroy_state(struct drm_connector *connector,
				  struct drm_connector_state *state)
{
	struct inno_hdmi_connector_state *inno_conn_state =
						to_inno_hdmi_conn_state(state);

	__drm_atomic_helper_connector_destroy_state(&inno_conn_state->base);
	kfree(inno_conn_state);
}

static void inno_hdmi_connector_reset(struct drm_connector *connector)
{
	struct inno_hdmi_connector_state *inno_conn_state;

	if (connector->state) {
		inno_hdmi_connector_destroy_state(connector, connector->state);
		connector->state = NULL;
	}

	inno_conn_state = kzalloc(sizeof(*inno_conn_state), GFP_KERNEL);
	if (!inno_conn_state)
		return;

	__drm_atomic_helper_connector_reset(connector, &inno_conn_state->base);

	inno_conn_state->colorimetry = HDMI_COLORIMETRY_ITU_709;
	inno_conn_state->enc_out_format = HDMI_COLORSPACE_RGB;
	inno_conn_state->rgb_limited_range = false;
}

static struct drm_connector_state *
inno_hdmi_connector_duplicate_state(struct drm_connector *connector)
{
	struct inno_hdmi_connector_state *inno_conn_state;

	if (WARN_ON(!connector->state))
		return NULL;

	inno_conn_state = kmemdup(to_inno_hdmi_conn_state(connector->state),
				  sizeof(*inno_conn_state), GFP_KERNEL);

	if (!inno_conn_state)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector,
						      &inno_conn_state->base);

	return &inno_conn_state->base;
}

static const struct drm_connector_funcs inno_hdmi_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = inno_hdmi_connector_detect,
	.reset = inno_hdmi_connector_reset,
	.atomic_duplicate_state = inno_hdmi_connector_duplicate_state,
	.atomic_destroy_state = inno_hdmi_connector_destroy_state,
};

static struct drm_connector_helper_funcs inno_hdmi_connector_helper_funcs = {
	.get_modes = inno_hdmi_connector_get_modes,
	.mode_valid = inno_hdmi_connector_mode_valid,
};

static int inno_hdmi_register(struct drm_device *drm, struct inno_hdmi *hdmi,
			      struct drm_encoder *encoder)
{
	struct device *dev = hdmi->dev;
	const struct inno_hdmi_plat_data *pdata = hdmi->plat_data;

	if (RK3128_HDMI == pdata->soc_type || RK3036_HDMI == pdata->soc_type)
		drm_simple_encoder_init(drm, encoder, DRM_MODE_ENCODER_TMDS);
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);

	/*
	 * If we failed to find the CRTC(s) which this encoder is
	 * supposed to be connected to, it's because the CRTC has
	 * not been registered yet.  Defer probing, and hope that
	 * the required CRTC is added later.
	 */
	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	drm_encoder_helper_add(encoder, pdata->helper_private);

	hdmi->connector.polled = DRM_CONNECTOR_POLL_HPD;

	drm_connector_helper_add(&hdmi->connector,
				 &inno_hdmi_connector_helper_funcs);

	drmm_connector_init(drm, &hdmi->connector,
			    &inno_hdmi_connector_funcs,
			    DRM_MODE_CONNECTOR_HDMIA,
			    hdmi->ddc);

	drm_connector_attach_encoder(&hdmi->connector, encoder);

	return 0;
}

static irqreturn_t inno_hdmi_i2c_irq(struct inno_hdmi *hdmi)
{
	struct inno_hdmi_i2c *i2c = hdmi->i2c;
	u8 stat;

	stat = hdmi_readb(hdmi, HDMI_INTERRUPT_STATUS1);
	if (!(stat & m_INT_EDID_READY))
		return IRQ_NONE;

	/* Clear HDMI EDID interrupt flag */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);

	complete(&i2c->cmp);

	return IRQ_HANDLED;
}

static irqreturn_t inno_hdmi_hardirq(int irq, void *dev_id)
{
	struct inno_hdmi *hdmi = dev_id;
	irqreturn_t ret = IRQ_NONE;
	u8 interrupt;

	if (hdmi->i2c)
		ret = inno_hdmi_i2c_irq(hdmi);

	interrupt = hdmi_readb(hdmi, HDMI_STATUS);
	if (interrupt & m_INT_HOTPLUG) {
		hdmi_modb(hdmi, HDMI_STATUS, m_INT_HOTPLUG, m_INT_HOTPLUG);
		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

static irqreturn_t inno_hdmi_irq(int irq, void *dev_id)
{
	struct inno_hdmi *hdmi = dev_id;

	drm_helper_hpd_irq_event(hdmi->connector.dev);

	return IRQ_HANDLED;
}

static int inno_hdmi_i2c_read(struct inno_hdmi *hdmi, struct i2c_msg *msgs)
{
	int length = msgs->len;
	u8 *buf = msgs->buf;
	int ret;

	ret = wait_for_completion_timeout(&hdmi->i2c->cmp, HZ / 10);
	if (!ret)
		return -EAGAIN;

	while (length--)
		*buf++ = hdmi_readb(hdmi, HDMI_EDID_FIFO_ADDR);

	return 0;
}

static int inno_hdmi_i2c_write(struct inno_hdmi *hdmi, struct i2c_msg *msgs)
{
	/*
	 * The DDC module only support read EDID message, so
	 * we assume that each word write to this i2c adapter
	 * should be the offset of EDID word address.
	 */
	if (msgs->len != 1 || (msgs->addr != DDC_ADDR && msgs->addr != DDC_SEGMENT_ADDR))
		return -EINVAL;

	reinit_completion(&hdmi->i2c->cmp);

	if (msgs->addr == DDC_SEGMENT_ADDR)
		hdmi->i2c->segment_addr = msgs->buf[0];
	if (msgs->addr == DDC_ADDR)
		hdmi->i2c->ddc_addr = msgs->buf[0];

	/* Set edid fifo first addr */
	hdmi_writeb(hdmi, HDMI_EDID_FIFO_OFFSET, 0x00);

	/* Set edid word address 0x00/0x80 */
	hdmi_writeb(hdmi, HDMI_EDID_WORD_ADDR, hdmi->i2c->ddc_addr);

	/* Set edid segment pointer */
	hdmi_writeb(hdmi, HDMI_EDID_SEGMENT_POINTER, hdmi->i2c->segment_addr);

	return 0;
}

static int inno_hdmi_i2c_xfer(struct i2c_adapter *adap,
			      struct i2c_msg *msgs, int num)
{
	struct inno_hdmi *hdmi = i2c_get_adapdata(adap);
	struct inno_hdmi_i2c *i2c = hdmi->i2c;
	int i, ret = 0;

	mutex_lock(&i2c->lock);

	/* Clear the EDID interrupt flag and unmute the interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, m_INT_EDID_READY);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);

	for (i = 0; i < num; i++) {
		DRM_DEV_DEBUG(hdmi->dev,
			      "xfer: num: %d/%d, len: %d, flags: %#x\n",
			      i + 1, num, msgs[i].len, msgs[i].flags);

		if (msgs[i].flags & I2C_M_RD)
			ret = inno_hdmi_i2c_read(hdmi, &msgs[i]);
		else
			ret = inno_hdmi_i2c_write(hdmi, &msgs[i]);

		if (ret < 0)
			break;
	}

	if (!ret)
		ret = num;

	/* Mute HDMI EDID interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, 0);

	mutex_unlock(&i2c->lock);

	return ret;
}

static u32 inno_hdmi_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm inno_hdmi_algorithm = {
	.master_xfer	= inno_hdmi_i2c_xfer,
	.functionality	= inno_hdmi_i2c_func,
};

static struct i2c_adapter *inno_hdmi_i2c_adapter(struct inno_hdmi *hdmi)
{
	struct i2c_adapter *adap;
	struct inno_hdmi_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(hdmi->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return ERR_PTR(-ENOMEM);

	mutex_init(&i2c->lock);
	init_completion(&i2c->cmp);

	adap = &i2c->adap;
	adap->owner = THIS_MODULE;
	adap->dev.parent = hdmi->dev;
	adap->dev.of_node = hdmi->dev->of_node;
	adap->algo = &inno_hdmi_algorithm;
	strscpy(adap->name, "Inno HDMI", sizeof(adap->name));
	i2c_set_adapdata(adap, hdmi);

	ret = i2c_add_adapter(adap);
	if (ret) {
		dev_warn(hdmi->dev, "cannot add %s I2C adapter\n", adap->name);
		devm_kfree(hdmi->dev, i2c);
		return ERR_PTR(ret);
	}

	hdmi->i2c = i2c;

	DRM_DEV_INFO(hdmi->dev, "registered %s I2C bus driver\n", adap->name);

	return adap;
}

int inno_hdmi_bind(struct drm_device *drm, struct inno_hdmi *hdmi, struct drm_encoder	*encoder)
{
	int ret, irq;
	struct platform_device *pdev = to_platform_device(hdmi->dev);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		return ret;
	}

	hdmi->ddc = inno_hdmi_i2c_adapter(hdmi);
	if (IS_ERR(hdmi->ddc)) {
		ret = PTR_ERR(hdmi->ddc);
		hdmi->ddc = NULL;
		return ret;
	}

	ret = inno_hdmi_register(drm, hdmi, encoder);
	if (ret)
		goto err_put_adapter;

	/* Unmute hotplug interrupt */
	hdmi_modb(hdmi, HDMI_STATUS, m_MASK_INT_HOTPLUG, v_MASK_INT_HOTPLUG(1));

	ret = devm_request_threaded_irq(hdmi->dev, irq, inno_hdmi_hardirq,
					inno_hdmi_irq, IRQF_SHARED,
					dev_name(hdmi->dev), hdmi);
	if (ret)
		goto err_put_adapter;

	return ret;
err_put_adapter:
	i2c_put_adapter(hdmi->ddc);
	return ret;
}
EXPORT_SYMBOL_GPL(inno_hdmi_bind);

static void inno_hdmi_disable_frame(struct inno_hdmi *hdmi,
				    enum hdmi_infoframe_type type)
{
	struct drm_connector *connector = &hdmi->connector;

	if (type != HDMI_INFOFRAME_TYPE_AVI) {
		drm_err(connector->dev,
			"Unsupported infoframe type: %u\n", type);
		return;
	}

	hdmi_writeb(hdmi, HDMI_CONTROL_PACKET_BUF_INDEX, INFOFRAME_AVI);
}

static int inno_hdmi_upload_frame(struct inno_hdmi *hdmi,
				  union hdmi_infoframe *frame, enum hdmi_infoframe_type type)
{
	struct drm_connector *connector = &hdmi->connector;
	u8 packed_frame[HDMI_MAXIMUM_INFO_FRAME_SIZE];
	ssize_t rc, i;

	if (type != HDMI_INFOFRAME_TYPE_AVI) {
		drm_err(connector->dev,
			"Unsupported infoframe type: %u\n", type);
		return 0;
	}

	inno_hdmi_disable_frame(hdmi, type);

	rc = hdmi_infoframe_pack(frame, packed_frame,
				 sizeof(packed_frame));
	if (rc < 0)
		return rc;

	for (i = 0; i < rc; i++)
		hdmi_writeb(hdmi, HDMI_CONTROL_PACKET_ADDR + i,
			    packed_frame[i]);

	return 0;
}

int inno_hdmi_config_video_avi(struct inno_hdmi *hdmi, struct drm_display_mode *mode)
{
	struct drm_connector *connector = &hdmi->connector;
	struct drm_connector_state *conn_state = connector->state;
	struct inno_hdmi_connector_state *inno_conn_state =
					to_inno_hdmi_conn_state(conn_state);
	union hdmi_infoframe frame;
	int rc;

	rc = drm_hdmi_avi_infoframe_from_display_mode(&frame.avi,
						      &hdmi->connector,
						      mode);
	if (rc) {
		inno_hdmi_disable_frame(hdmi, HDMI_INFOFRAME_TYPE_AVI);
		return rc;
	}

	if (inno_conn_state->enc_out_format == HDMI_COLORSPACE_YUV444)
		frame.avi.colorspace = HDMI_COLORSPACE_YUV444;
	else if (inno_conn_state->enc_out_format == HDMI_COLORSPACE_YUV422)
		frame.avi.colorspace = HDMI_COLORSPACE_YUV422;
	else
		frame.avi.colorspace = HDMI_COLORSPACE_RGB;

	if (inno_conn_state->enc_out_format == HDMI_COLORSPACE_RGB) {
		drm_hdmi_avi_infoframe_quant_range(&frame.avi,
						   connector, mode,
						   inno_conn_state->rgb_limited_range ?
						   HDMI_QUANTIZATION_RANGE_LIMITED :
						   HDMI_QUANTIZATION_RANGE_FULL);
	} else {
		frame.avi.quantization_range = HDMI_QUANTIZATION_RANGE_DEFAULT;
		frame.avi.ycc_quantization_range =
			HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
	}

	return inno_hdmi_upload_frame(hdmi, &frame, HDMI_INFOFRAME_TYPE_AVI);
}
EXPORT_SYMBOL_GPL(inno_hdmi_config_video_avi);

int inno_hdmi_config_video_timing(struct inno_hdmi *hdmi, struct drm_display_mode *mode)
{
	int value;

	/* Set detail external video timing polarity and interlace mode */
	value = v_EXTERANL_VIDEO(1);
	if (hdmi->plat_data->soc_type == STARFIVE_HDMI) {
		value |= mode->flags & DRM_MODE_FLAG_PHSYNC ?
			v_HSYNC_POLARITY_SF(1) : v_HSYNC_POLARITY_SF(0);
		value |= mode->flags & DRM_MODE_FLAG_PVSYNC ?
			v_VSYNC_POLARITY_SF(1) : v_VSYNC_POLARITY_SF(0);
	} else {
		value |= mode->flags & DRM_MODE_FLAG_PHSYNC ?
			v_HSYNC_POLARITY(1) : v_HSYNC_POLARITY(0);
		value |= mode->flags & DRM_MODE_FLAG_PVSYNC ?
			v_VSYNC_POLARITY(1) : v_VSYNC_POLARITY(0);
	}
	value |= mode->flags & DRM_MODE_FLAG_INTERLACE ?
	 v_INETLACE(1) : v_INETLACE(0);
	hdmi_writeb(hdmi, HDMI_VIDEO_TIMING_CTL, value);

	/* Set detail external video timing */
	value = mode->htotal;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HTOTAL_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HTOTAL_H, (value >> 8) & 0xFF);

	value = mode->htotal - mode->hdisplay;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HBLANK_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HBLANK_H, (value >> 8) & 0xFF);

	value = mode->htotal - mode->hsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HDELAY_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HDELAY_H, (value >> 8) & 0xFF);

	value = mode->hsync_end - mode->hsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HDURATION_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HDURATION_H, (value >> 8) & 0xFF);

	value = mode->vtotal;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VTOTAL_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VTOTAL_H, (value >> 8) & 0xFF);

	value = mode->vtotal - mode->vdisplay;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VBLANK, value & 0xFF);

	value = mode->vtotal - mode->vsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VDELAY, value & 0xFF);

	value = mode->vsync_end - mode->vsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VDURATION, value & 0xFF);

	if (hdmi->plat_data->soc_type == STARFIVE_HDMI)
		return 0;

	hdmi_writeb(hdmi, HDMI_PHY_PRE_DIV_RATIO, 0x1e);
	hdmi_writeb(hdmi, HDMI_PHY_FEEDBACK_DIV_RATIO_LOW, 0x2c);
	hdmi_writeb(hdmi, HDMI_PHY_FEEDBACK_DIV_RATIO_HIGH, 0x01);

	return 0;
}
EXPORT_SYMBOL_GPL(inno_hdmi_config_video_timing);

MODULE_DESCRIPTION("INNO HDMI transmitter driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:inno-hdmi");
