#ifndef _ES_DRM_FBDEV_H_
#define _ES_DRM_FBDEV_H_

#include <drm/drm_fb_helper.h>

#ifdef CONFIG_DRM_FBDEV_EMULATION
int eswin_drm_fbdev_client_setup(struct drm_device *dev, const struct drm_format_info *format);
int eswin_drm_fbdev_create(struct drm_fb_helper *fb_helper,
			   struct drm_fb_helper_surface_size *sizes);
#else
static inline int eswin_drm_fbdev_client_setup(struct drm_device *dev, const struct drm_format_info *format)
{
	return 0;
}

static inline int eswin_drm_fbdev_create(struct drm_fb_helper *fb_helper,
					 struct drm_fb_helper_surface_size *sizes)
{
	return 0;
}
#endif

#endif
