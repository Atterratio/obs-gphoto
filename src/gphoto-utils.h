#include <obs-module.h>
#include <obs-internal.h>
#include <gphoto2/gphoto2-camera.h>

int gp_camera_by_name(Camera **camera, const char *name, CameraList *cam_list, GPContext *context);
void property_cam_list(CameraList *cam_list, obs_property_t *prop);
void gphoto_capture_preview(Camera *camera, GPContext *context, int width, int height, uint8_t *texture_data);
void gphoto_capture(Camera *camera, GPContext *context, int width, int height, uint8_t *texture_data);
int gphoto_cam_list(CameraList *cam_list, GPContext *context);

int cancel_autofocus(Camera *camera, GPContext *context);

obs_property_t *camera_config_to_obs_property(char *config_name, Camera *camera, GPContext *context, obs_properties_t *props,
                                              const char *prop_name, const char *prop_description);
int create_obs_property_from_camera_config(obs_properties_t *props, obs_data_t *settings, const char *prop_description,
                                           Camera *camera, GPContext *context, char *config_name);
int set_camera_config(obs_data_t *settings, Camera *camera, GPContext *context);

int create_autofocus_property(obs_properties_t *props, obs_data_t *settings, Camera *camera, GPContext *context);
int set_autofocus(Camera *camera, GPContext *context);

int create_manualfocus_property(obs_properties_t *props, obs_data_t *settings, Camera *camera, GPContext *context);
int set_manualfocus(const char *value, Camera *camera, GPContext *context);