#include <obs-module.h>
#include <obs-internal.h>
#include <gphoto2/gphoto2-camera.h>

struct gphoto_data {
    /* settings */
    const char *camera_name;
    long long int fps;
    bool autofocus;

    /* internal data */
    obs_source_t *source;
    pthread_t thread;
    os_event_t *event;
    pthread_mutex_t camera_mutex;

    uint32_t width;
    uint32_t height;


    CameraList *cam_list;
    Camera *camera;
    GPContext *gp_context;
};