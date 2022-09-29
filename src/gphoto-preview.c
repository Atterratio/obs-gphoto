#include <magick/MagickCore.h>

#include "gphoto-preview.h"
#include "gphoto-utils.h"
#if HAVE_UDEV
#include "gphoto-udev.h"
#endif



static const char *capture_getname(void *vptr) {
    UNUSED_PARAMETER(vptr);
    return obs_module_text("gPhoto live preview capture");
}

static void capture_defaults(obs_data_t *settings) {
    obs_data_set_default_int(settings, "fps", 30);
}

static bool capture_camera_selected(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings){
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(prop);
    obs_data_set_string(settings, "changed", "camera");

    return true;
}

static bool capture_fps_selected(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings){
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(prop);
    obs_data_set_string(settings, "changed", "fps");

    return true;
}

static obs_properties_t *capture_properties(void *vptr){
    struct preview_data *data = vptr;

    obs_properties_t *props = obs_properties_create();
    obs_data_t *settings = obs_source_get_settings(data->source);
    int cam_count = gp_list_count(data->cam_list);
    if(cam_count > 0) {
        obs_property_t *cam_list = obs_properties_add_list(props, "camera_name", obs_module_text("Camera"),
                                                           OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
        property_cam_list(data->cam_list, cam_list);
        obs_property_set_modified_callback(cam_list, capture_camera_selected);


        obs_property_t *fps_list = obs_properties_add_list(props, "fps", obs_module_text("FPS"),
                                                           OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
        obs_property_list_add_int(fps_list, "25", 25);
        obs_property_list_add_int(fps_list, "30", 30);
        obs_property_list_add_int(fps_list, "60", 60);
        obs_property_set_modified_callback(fps_list, capture_fps_selected);

        if (data->camera) {
            pthread_mutex_lock(&data->camera_mutex);
            create_autofocus_property(props, settings, data->camera, data->gp_context);
            create_manualfocus_property(props, settings, data->camera, data->gp_context);
            create_obs_property_from_camera_config(props, settings, obs_module_text("Shutter Speed"),
                                                   data->camera, data->gp_context, "shutterspeed");
            create_obs_property_from_camera_config(props, settings, obs_module_text("Aperture"),
                                                   data->camera, data->gp_context, "aperture");
            create_obs_property_from_camera_config(props, settings, obs_module_text("ISO"),
                                                   data->camera, data->gp_context, "iso");
            create_obs_property_from_camera_config(props, settings, obs_module_text("White balance"),
                                                   data->camera, data->gp_context, "whitebalance");
            create_obs_property_from_camera_config(props, settings, obs_module_text("Picture style"),
                                                   data->camera, data->gp_context, "picturestyle");
            pthread_mutex_unlock(&data->camera_mutex);
        }
    }

    obs_data_release(settings);

    return props;
}

static void *capture_thread(void *vptr){
    struct preview_data *data = vptr;
    uint8_t *texture_data = malloc(data->width * data->height * 4);
    uint64_t cur_time = os_gettime_ns();

    struct obs_source_frame frame = {
            .data     = {[0] = texture_data},
            .linesize = {[0] = data->width*4},
            .width    = data->width,
            .height   = data->height,
            .format   = VIDEO_FORMAT_BGRX
    };

    while (os_event_try(data->event) == EAGAIN){
        frame.timestamp = cur_time;
        pthread_mutex_lock(&data->camera_mutex);
        gphoto_capture_preview(data->camera, data->gp_context, data->width, data->height, texture_data);
        pthread_mutex_unlock(&data->camera_mutex);
        obs_source_output_video(data->source, &frame);
        switch (data->fps){
            case 60:
                os_sleepto_ns(cur_time += 15000000);
                break;
            case 30:
                os_sleepto_ns(cur_time += 30000000);
                break;
            case 25:
                os_sleepto_ns(cur_time += 40000000);
                break;
            default:
                os_sleepto_ns(cur_time += 50000000);
                break;
        }
    }

    free(texture_data);

    return NULL;
}

static void capture_init(void *vptr){
    struct preview_data *data = vptr;
    CameraFile *cam_file = NULL;
    const char *image_data = NULL;
    unsigned long data_size = NULL;
    Image *image = NULL;
    ImageInfo *image_info = AcquireImageInfo();
    ExceptionInfo *exception = AcquireExceptionInfo();

    if (gp_file_new(&cam_file) < GP_OK) {
            blog(LOG_WARNING, "What???\n");
        } else {
        if (gp_camera_by_name(&data->camera, data->camera_name, data->cam_list, data->gp_context) < GP_OK) {
            blog(LOG_WARNING, "Can't get camera.\n");
        } else {
            if (gp_camera_init(data->camera, data->gp_context) < GP_OK) {
                blog(LOG_WARNING, "Can't init camera.\n");
            } else {
                if (gp_camera_capture_preview(data->camera, cam_file, data->gp_context) < GP_OK) {
                    blog(LOG_WARNING, "Can't capture preview.\n");
                } else {
                    if (gp_file_get_data_and_size(cam_file, &image_data, &data_size) < GP_OK) {
                        blog(LOG_WARNING, "Can't get image data.\n");
                    } else {
                        image = BlobToImage(image_info, image_data, data_size, exception);
                        if (exception->severity != UndefinedException) {
                            CatchException(exception);
                            blog(LOG_WARNING, "ImageMagic error: %s.\n", (char *)exception->severity);
                            exception->severity = UndefinedException;
                        } else {
                            data->width = (uint32_t)image->magick_columns;
                            data->height = (uint32_t)image->magick_rows;

                            os_event_init(&data->event, OS_EVENT_TYPE_MANUAL);
                            pthread_create(&data->thread, NULL, capture_thread, data);
                        }
                    }
                }
            }
        }
    }

    if(image_data){
        free(image_data);
    }
    if(image_info){
        DestroyImageInfo(image_info);
    }
    if(image){
        DestroyImageList(image);
    }
    if(exception){
        DestroyExceptionInfo(exception);
    }
}

static void capture_terminate(void *vptr){
    struct preview_data *data = vptr;

    if (!data->camera)
      return;

    if(data->event) {
        os_event_signal(data->event);
        if(data->thread != 0){
            pthread_join(data->thread, NULL);
        }
        os_event_destroy(data->event);
    }

    gp_camera_exit(data->camera, data->gp_context);
    gp_camera_free(data->camera);
    data->camera = NULL;
}

static void capture_update(void *vptr, obs_data_t *settings){
    struct preview_data *data = vptr;

    const char *changed = obs_data_get_string(settings, "changed");

    if (strcmp(changed, "camera") == 0) {
        data->camera_name = obs_data_get_string(settings, "camera_name");
        if (data->source->active) {
            capture_terminate(data);
            pthread_mutex_lock(&data->camera_mutex);
            capture_init(data);
            pthread_mutex_unlock(&data->camera_mutex);
            obs_source_update_properties(data->source);
            if(data->autofocus) {
                pthread_mutex_lock(&data->camera_mutex);
                set_autofocus(data->camera, data->gp_context);
                pthread_mutex_unlock(&data->camera_mutex);
            }
        }
    }

    if(strcmp(changed, "fps") == 0){
        data->fps = obs_data_get_int(settings, "fps");
    }

    if (strcmp(changed, "autofocus") == 0) {
        data->autofocus = obs_data_get_bool(settings, "autofocusdrive");
        if(data->autofocus) {
            pthread_mutex_lock(&data->camera_mutex);
            set_autofocus(data->camera, data->gp_context);
            pthread_mutex_unlock(&data->camera_mutex);
        }else{
            pthread_mutex_lock(&data->camera_mutex);
            cancel_autofocus(data->camera, data->gp_context);
            pthread_mutex_unlock(&data->camera_mutex);
        }
    }

    if (strcmp(changed, "manualfocus") == 0) {
        const char *manual_focus = obs_data_get_string(settings, "manualfocus");
        pthread_mutex_lock(&data->camera_mutex);
        set_manualfocus(manual_focus, data->camera, data->gp_context);
        pthread_mutex_unlock(&data->camera_mutex);
    }

    if (strcmp(changed, "auto_prop") == 0) {
        pthread_mutex_lock(&data->camera_mutex);
        set_camera_config(settings, data->camera, data->gp_context);
        pthread_mutex_unlock(&data->camera_mutex);
    }
}

#if HAVE_UDEV
static void capture_camera_added(void *vptr, calldata_t *calldata) {
    UNUSED_PARAMETER(calldata);
    struct preview_data *data = vptr;
    int i, count;
    const char *camera_name;
    if(!data->camera){
        pthread_mutex_lock(&data->camera_mutex);
        gphoto_cam_list(data->cam_list, data->gp_context);
        count = gp_list_count(data->cam_list);
        for(i=0; i<count; i++){
            gp_list_get_name(data->cam_list, i, &camera_name);
            if (strcmp(camera_name, data->camera_name) == 0) {
                capture_init(data);
                obs_source_update_properties(data->source);
                if(data->autofocus) {
                    set_autofocus(data->camera, data->gp_context);
                }
            }
        }
        pthread_mutex_unlock(&data->camera_mutex);
    }
}

static void capture_camera_removed(void *vptr, calldata_t *calldata) {
    UNUSED_PARAMETER(calldata);
    struct preview_data *data = vptr;
    int i, count;
    const char *camera_name;
    if(data->camera){
        pthread_mutex_lock(&data->camera_mutex);
        gphoto_cam_list(data->cam_list, data->gp_context);
        pthread_mutex_unlock(&data->camera_mutex);
        count = gp_list_count(data->cam_list);
        for(i=0; i<count; i++){
            gp_list_get_name(data->cam_list, i, &camera_name);
            if (strcmp(camera_name, data->camera_name) == 0) {
                return;
            }
        }
        capture_terminate(data);
        obs_source_update_properties(data->source);
    }
}
#endif

static void capture_show(void *vptr) {
    struct preview_data *data = vptr;
    if (strcmp(data->camera_name, "") != 0) {
        if (!data->source->active && !data->camera) {
            capture_terminate(data);
            pthread_mutex_lock(&data->camera_mutex);
            capture_init(data);
            pthread_mutex_unlock(&data->camera_mutex);
            obs_source_update_properties(data->source);
            if(data->autofocus) {
                pthread_mutex_lock(&data->camera_mutex);
                set_autofocus(data->camera, data->gp_context);
                pthread_mutex_unlock(&data->camera_mutex);
            }
        }
    }
}

static void capture_hide(void *vptr) {
    struct preview_data *data = vptr;
    if(data->source->active) {
        capture_terminate(data);
    }
}

static void *capture_create(obs_data_t *settings, obs_source_t *source){
    struct preview_data *data = bzalloc(sizeof(struct preview_data));

    pthread_mutex_init(&data->camera_mutex, NULL);

    data->source = source;
    data->gp_context = gp_context_new();

    gp_list_new(&data->cam_list);
    gphoto_cam_list(data->cam_list, data->gp_context);

    data->camera_name = obs_data_get_string(settings, "camera_name");
    data->fps = obs_data_get_int(settings, "fps");
    data->autofocus = obs_data_get_bool(settings, "autofocusdrive");

    #if HAVE_UDEV
    gphoto_init_udev();
    signal_handler_t *sh = gphoto_get_udev_signalhandler();

    signal_handler_connect(sh, "device_added", &capture_camera_added, data);
    signal_handler_connect(sh, "device_removed", &capture_camera_removed, data);
    #endif

    return data;
}

static void capture_destroy(void *vptr) {
    struct preview_data *data = vptr;

    if(data->source->active){
        capture_terminate(data);
    }

    pthread_mutex_destroy(&data->camera_mutex);
    gp_context_unref(data->gp_context);
    gp_list_free(data->cam_list);

    signal_handler_t *sh = gphoto_get_udev_signalhandler();

    signal_handler_disconnect(sh, "device_added", capture_camera_added, data);
    signal_handler_disconnect(sh, "device_removed", capture_camera_removed, data);

    gphoto_unref_udev();

    bfree(vptr);
}

static uint32_t capture_getwidth(void *vptr) {
    struct preview_data *data = vptr;
    return data->width;
}

static uint32_t capture_getheight(void *vptr) {
    struct preview_data *data = vptr;
    return data->height;
}

struct obs_source_info capture_preview_info = {
    .id             = "gphoto-capture-preview",
    .type           = OBS_SOURCE_TYPE_INPUT,
    .output_flags   = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE,

    .get_name       = capture_getname,
    .get_defaults   = capture_defaults,
    .get_properties = capture_properties,
    .create         = capture_create,
    .destroy        = capture_destroy,
    .update         = capture_update,
    .show           = capture_show,
    .hide           = capture_hide,
    .get_width      = capture_getwidth,
    .get_height     = capture_getheight,
};
