#include <obs-module.h>
#include <obs-internal.h>
#include <gphoto2/gphoto2-camera.h>

struct timelapse_data {
    /* settings */
    const char *camera_name;
    long long int interval;
    bool autofocus;

    /* internal data */
    obs_source_t *source;
    pthread_mutex_t camera_mutex;

    uint32_t width;
    uint32_t height;
    uint8_t *texture_data;
    gs_texture_t *texture;
    float time_elapsed;

    CameraList *cam_list;
    Camera *camera;
    GPContext *gp_context;
};