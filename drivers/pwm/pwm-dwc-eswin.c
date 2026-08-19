// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN platform glue for the Synopsys DWC PWM block
 * (compatible "eswin,pwm-eswin").
 *
 * pwm-dwc-core maps duty onto the low half of the cycle. The EIC770x
 * backlight/fan path (and the old pwm-dwc-eswin driver) put duty on
 * the high half when polarity is inverted. Override apply/get_state
 * so larger PWM duty is brighter, and allow 0%/100% (pwm-fan).
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pwm.h>
#include <linux/reset.h>

#include "pwm-dwc.h"

#ifndef DWC_TIM_CTRL_0N100PWM_EN
#define DWC_TIM_CTRL_0N100PWM_EN	BIT(4)
#endif

static void dwc_pwm_eswin_set_enable(struct dwc_pwm *dwc, int pwm, int enabled)
{
	u32 reg = dwc_pwm_readl(dwc, DWC_TIM_CTRL(pwm));

	if (enabled)
		reg |= DWC_TIM_CTRL_EN;
	else
		reg &= ~DWC_TIM_CTRL_EN;
	dwc_pwm_writel(dwc, reg, DWC_TIM_CTRL(pwm));
}

static int dwc_pwm_eswin_configure(struct dwc_pwm *dwc, struct pwm_device *pwm,
				   const struct pwm_state *state)
{
	u64 tmp;
	u32 high = 0, low = 0, ctrl;

	tmp = DIV_ROUND_CLOSEST_ULL(state->duty_cycle, dwc->clk_ns);
	if (tmp > (1ULL << 32))
		return -ERANGE;
	if (pwm->args.polarity == PWM_POLARITY_INVERSED)
		high = tmp;
	else
		low = tmp;

	tmp = DIV_ROUND_CLOSEST_ULL(state->period - state->duty_cycle,
				    dwc->clk_ns);
	if (tmp > (1ULL << 32))
		return -ERANGE;
	if (pwm->args.polarity == PWM_POLARITY_INVERSED)
		low = tmp;
	else
		high = tmp;

	dwc_pwm_eswin_set_enable(dwc, pwm->hwpwm, false);
	dwc_pwm_writel(dwc, low, DWC_TIM_LD_CNT(pwm->hwpwm));
	dwc_pwm_writel(dwc, high, DWC_TIM_LD_CNT2(pwm->hwpwm));

	ctrl = DWC_TIM_CTRL_MODE_USER | DWC_TIM_CTRL_PWM |
	       DWC_TIM_CTRL_0N100PWM_EN;
	dwc_pwm_writel(dwc, ctrl, DWC_TIM_CTRL(pwm->hwpwm));
	dwc_pwm_eswin_set_enable(dwc, pwm->hwpwm, state->enabled);
	return 0;
}

static int dwc_pwm_eswin_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			       const struct pwm_state *state)
{
	struct dwc_pwm *dwc = to_dwc_pwm(chip);

	if (state->polarity != PWM_POLARITY_INVERSED)
		return -EINVAL;

	if (state->enabled) {
		if (!pwm->state.enabled)
			pm_runtime_get_sync(pwmchip_parent(chip));
		return dwc_pwm_eswin_configure(dwc, pwm, state);
	}

	if (pwm->state.enabled) {
		dwc_pwm_eswin_set_enable(dwc, pwm->hwpwm, false);
		pm_runtime_put_sync(pwmchip_parent(chip));
	}
	return 0;
}

static int dwc_pwm_eswin_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				   struct pwm_state *state)
{
	struct dwc_pwm *dwc = to_dwc_pwm(chip);
	u64 low, high;

	pm_runtime_get_sync(pwmchip_parent(chip));

	state->enabled = !!(dwc_pwm_readl(dwc, DWC_TIM_CTRL(pwm->hwpwm)) &
			    DWC_TIM_CTRL_EN);

	low = dwc_pwm_readl(dwc, DWC_TIM_LD_CNT(pwm->hwpwm));
	low *= dwc->clk_ns;
	high = dwc_pwm_readl(dwc, DWC_TIM_LD_CNT2(pwm->hwpwm));
	high *= dwc->clk_ns;

	if (pwm->args.polarity == PWM_POLARITY_INVERSED)
		state->duty_cycle = high;
	else
		state->duty_cycle = low;
	state->period = low + high;
	state->polarity = PWM_POLARITY_INVERSED;

	pm_runtime_put_sync(pwmchip_parent(chip));
	return 0;
}

static const struct pwm_ops dwc_pwm_eswin_ops = {
	.apply = dwc_pwm_eswin_apply,
	.get_state = dwc_pwm_eswin_get_state,
};

static int dwc_pwm_eswin_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pwm_chip *chip;
	struct dwc_pwm *dwc;
	struct clk *clk;
	struct reset_control *rst;
	void __iomem *base;
	unsigned long rate;
	int ret;

	chip = dwc_pwm_alloc(dev);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	dwc = to_dwc_pwm(chip);
	chip->ops = &dwc_pwm_eswin_ops;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);
	dwc->base = base;

	clk = devm_clk_get_enabled(dev, "pclk");
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk), "failed to get pclk\n");

	rate = clk_get_rate(clk);
	if (rate)
		dwc->clk_ns = DIV_ROUND_CLOSEST(NSEC_PER_SEC, rate);

	rst = devm_reset_control_get_optional_exclusive(dev, "rst");
	if (IS_ERR(rst))
		return dev_err_probe(dev, PTR_ERR(rst), "failed to get reset\n");

	ret = reset_control_deassert(rst);
	if (ret)
		return dev_err_probe(dev, ret, "failed to deassert reset\n");

	ret = devm_pwmchip_add(dev, chip);
	if (ret) {
		reset_control_assert(rst);
		return dev_err_probe(dev, ret, "failed to add pwmchip\n");
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	dev_info(dev, "ESWIN DWC PWM registered (%u channels, clk_ns=%u)\n",
		 chip->npwm, dwc->clk_ns);
	return 0;
}

static const struct of_device_id dwc_pwm_eswin_of_match[] = {
	{ .compatible = "eswin,pwm-eswin" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dwc_pwm_eswin_of_match);

static struct platform_driver dwc_pwm_eswin_driver = {
	.probe = dwc_pwm_eswin_probe,
	.driver = {
		.name = "pwm-dwc-eswin",
		.of_match_table = dwc_pwm_eswin_of_match,
	},
};
module_platform_driver(dwc_pwm_eswin_driver);

MODULE_AUTHOR("ESWIN / DeepComputing");
MODULE_DESCRIPTION("ESWIN DesignWare PWM platform driver");
MODULE_LICENSE("GPL");
