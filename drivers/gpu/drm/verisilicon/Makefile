# SPDX-License-Identifier: GPL-2.0

vs_drm-objs := vs_dc_hw.o \
		vs_modeset.o \
		vs_plane.o \
		vs_crtc.o \
		vs_drv.o

vs_drm-$(CONFIG_DRM_INNO_STARFIVE_HDMI) += inno_hdmi-starfive.o
obj-$(CONFIG_DRM_VERISILICON_DC8200) += vs_drm.o
