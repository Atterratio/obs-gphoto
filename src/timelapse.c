#include <MagickCore/MagickCore.h>

#include "timelapse.h"
#include "gphoto-utils.h"
#if HAVE_UDEV
#include "gphoto-udev.h"
#endif



static const char *timelapse_getname(void *vptr) {
    UNUSED_PARAMETER(vptr);
    return obs_module_text("Timelapse photo capture");
}

static void timelapse_defaults(obs_data_t *settings) {
    obs_data_set_default_int(settings, "interval", 30);
}

static bool test_capture_callback(obs_properties_t *props, obs_property_t *prop, void *vptr){
    UNUSED_PARAMETER(prop);
    UNUSED_PARAMETER(props);
    struct timelapse_data *data = vptr;

    pthread_mutex_lock(&data->camera_mutex);
    gphoto_capture(data->camera, data->gp_context, data->width, data->height, data->texture_data);
    pthread_mutex_unlock(&data->camera_mutex);

    obs_enter_graphics();
    gs_texture_set_image(data->texture, data->texture_data, data->width * 4, false);
    obs_leave_graphics();

    return TRUE;
}

static void capture_hotkey_pressed(void *vptr, obs_hotkey_id id, obs_hotkey_t *key, bool pressed){
    UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(key);
    struct timelapse_data *data = vptr;
    uint64_t delta_time = os_gettime_ns() - data->last_capture_time;
    if(pressed && delta_time >= 500000000 && obs_source_active(data->source)) {
        pthread_mutex_lock(&data->camera_mutex);
        gphoto_capture(data->camera, data->gp_context, data->width, data->height, data->texture_data);
        pthread_mutex_unlock(&data->camera_mutex);

        obs_enter_graphics();
        gs_texture_set_image(data->texture, data->texture_data, data->width * 4, false);
        obs_leave_graphics();
        data->last_capture_time = os_gettime_ns();
    }
}

static bool timelapse_camera_selected(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings){
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(prop);
    obs_data_set_string(settings, "changed", "camera");

    return true;
}

static bool timelapse_interval_changed(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings){
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(prop);
    obs_data_set_string(settings, "changed", "interval");

    return true;
}

static obs_properties_t *timelapse_properties(void *vptr){
    struct timelapse_data *data = vptr;

    obs_properties_t *props = obs_properties_create();
    obs_data_t *settings = obs_source_get_settings(data->source);

    int cam_count = gp_list_count(data->cam_list);
    if(cam_count > 0) {
        obs_property_t *cam_list = obs_properties_add_list(props, "camera_name", obs_module_text("Camera"),
                                                           OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
        property_cam_list(data->cam_list, cam_list);
        obs_property_set_modified_callback(cam_list, timelapse_camera_selected);


        obs_property_t *interval = obs_properties_add_int(props, "interval", obs_module_text("Interval(in seconds)"),
                                                          0, 100000, 1);
        obs_property_set_modified_callback(interval, timelapse_interval_changed);

        if (data->camera) {
            obs_properties_add_button(props, "test_capture", obs_module_text("Test Capture"), test_capture_callback);
            pthread_mutex_lock(&data->camera_mutex);
            create_obs_property_from_camera_config(props, settings, obs_module_text("Image Format"),
                                                   data->camera, data->gp_context, "imageformat");
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

static void timelapse_init(void *vptr) {
    struct timelapse_data *data = vptr;
    CameraFile *cam_file = NULL;
    CameraFilePath camera_file_path;
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
                if (gp_camera_capture(data->camera, GP_CAPTURE_IMAGE, &camera_file_path, data->gp_context) < GP_OK) {
                    blog(LOG_WARNING, "Can't capture photo.\n");
                } else {
                    if (gp_camera_file_get(data->camera, camera_file_path.folder, camera_file_path.name,
                                           GP_FILE_TYPE_NORMAL, cam_file, data->gp_context) < GP_OK) {
                        blog(LOG_WARNING, "Can't get photo from camera.\n");
                    } else {
                        if (gp_file_get_data_and_size(cam_file, &image_data, &data_size) < GP_OK) {
                            blog(LOG_WARNING, "Can't get image data.\n");
                        } else {
                            gp_camera_file_delete(data->camera, camera_file_path.folder, camera_file_path.name,
                                                  data->gp_context);
                            image = BlobToImage(image_info, image_data, data_size, exception);
                            if (exception->severity != UndefinedException) {
                                CatchException(exception);
                                blog(LOG_WARNING, "ImageMagic error: %s.\n", (char *) exception->severity);
                                exception->severity = UndefinedException;
                            } else {
                                data->width = (uint32_t) image->magick_columns;
                                data->height = (uint32_t) image->magick_rows;

                                data->texture_data = malloc(data->width * data->height * 4);

                                ExportImagePixels(image, 0, 0, data->width, data->height, "BGRA", CharPixel,
                                                  data->texture_data,
                                                  exception);
                                if (exception->severity != UndefinedException) {
                                    CatchException(exception);
                                    blog(LOG_WARNING, "ImageMagic error: %s.\n", (char *) exception->severity);
                                    exception->severity = UndefinedException;
                                } else {
                                    obs_enter_graphics();
                                    data->texture = gs_texture_create(data->width, data->height, GS_BGRA, 1,
                                                                      &data->texture_data,
                                                                      GS_DYNAMIC);
                                    obs_leave_graphics();
                                    goto exit;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    data->width = 0;
    data->height = 0;
    obs_enter_graphics();
    data->texture = gs_texture_create(data->width, data->height, GS_BGRA, 1, NULL, GS_DYNAMIC);
    obs_leave_graphics();

    exit:
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

static void timelapse_terminate(void *vptr){
    struct timelapse_data *data = vptr;

    gp_camera_exit(data->camera, data->gp_context);
    gp_camera_free(data->camera);
    data->camera = NULL;
    free(data->texture_data);
}

static void timelapse_update(void *vptr, obs_data_t *settings){
    struct timelapse_data *data = vptr;

    const char *changed = obs_data_get_string(settings, "changed");

    if (strcmp(changed, "camera") == 0) {
        data->camera_name = obs_data_get_string(settings, "camera_name");
        if (data->source->active) {
            timelapse_terminate(data);
            pthread_mutex_lock(&data->camera_mutex);
            timelapse_init(data);
            pthread_mutex_unlock(&data->camera_mutex);
            obs_source_update_properties(data->source);
            if(data->autofocus) {
                pthread_mutex_lock(&data->camera_mutex);
                set_autofocus(data->camera, data->gp_context);
                pthread_mutex_unlock(&data->camera_mutex);
            }
        }
    }

    if(strcmp(changed, "interval") == 0){
        data->interval = obs_data_get_int(settings, "interval");
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
static void timelapse_camera_added(void *vptr, calldata_t *calldata) {
    UNUSED_PARAMETER(calldata);
    struct timelapse_data *data = vptr;
    int i, count;
    const char *camera_name;
    if(!data->camera){
        pthread_mutex_lock(&data->camera_mutex);
        gphoto_cam_list(data->cam_list, data->gp_context);
        count = gp_list_count(data->cam_list);
        for(i=0; i<count; i++){
            gp_list_get_name(data->cam_list, i, &camera_name);
            if (strcmp(camera_name, data->camera_name) == 0) {
                timelapse_init(data);
                pthread_mutex_unlock(&data->camera_mutex);
                obs_source_update_properties(data->source);
                if(data->autofocus) {
                    pthread_mutex_lock(&data->camera_mutex);
                    set_autofocus(data->camera, data->gp_context);
                    pthread_mutex_unlock(&data->camera_mutex);
                }
                return;
            }
        }
        pthread_mutex_unlock(&data->camera_mutex);
    }
}

static void timelapse_camera_removed(void *vptr, calldata_t *calldata) {
    UNUSED_PARAMETER(calldata);
    struct timelapse_data *data = vptr;
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
                pthread_mutex_unlock(&data->camera_mutex);
                return;
            }
        }
        timelapse_terminate(data);
        obs_source_update_properties(data->source);
    }
}
#endif

static void timelapse_show(void *vptr) {
    struct timelapse_data *data = vptr;
    if (strcmp(data->camera_name, "") != 0) {
        if (!data->source->active && !data->camera) {
            timelapse_terminate(data);
            pthread_mutex_lock(&data->camera_mutex);
            timelapse_init(data);
            pthread_mutex_unlock(&data->camera_mutex);
            obs_source_update_properties(data->source);
            if(data->autofocus) {
                pthread_mutex_lock(&data->camera_mutex);
                set_autofocus(data->camera, data->gp_context);
                pthread_mutex_unlock(&data->camera_mutex);
            }
        }
    }else{
        data->width = 0;
        data->height = 0;
        obs_enter_graphics();
        data->texture = gs_texture_create(data->width, data->height, GS_BGRA, 1, NULL, GS_DYNAMIC);
        obs_leave_graphics();
    }
}

static void timelapse_hide(void *vptr) {
    struct timelapse_data *data = vptr;
    if(data->source->active) {
        timelapse_terminate(data);
    }
}

static void *timelapse_create(obs_data_t *settings, obs_source_t *source){
    struct timelapse_data *data = bzalloc(sizeof(struct timelapse_data));

    pthread_mutex_init(&data->camera_mutex, NULL);

    data->source = source;
    data->gp_context = gp_context_new();

    gp_list_new(&data->cam_list);
    gphoto_cam_list(data->cam_list, data->gp_context);

    data->camera_name = obs_data_get_string(settings, "camera_name");
    data->interval = obs_data_get_int(settings, "interval");
    data->autofocus = obs_data_get_bool(settings, "autofocusdrive");
    data->time_elapsed = 0;

    data->capture_key = obs_hotkey_register_source(source, "timelapse.capture",
                                                   obs_module_text("Capture hotkey"), capture_hotkey_pressed, data);
    data->last_capture_time = os_gettime_ns();

#if HAVE_UDEV
    gphoto_init_udev();
    signal_handler_t *sh = gphoto_get_udev_signalhandler();

    signal_handler_connect(sh, "device_added", &timelapse_camera_added, data);
    signal_handler_connect(sh, "device_removed", &timelapse_camera_removed, data);
#endif

    return data;
}

static void timelapse_destroy(void *vptr) {
    struct timelapse_data *data = vptr;

    if(data->source->active){
        timelapse_terminate(data);
    }

    pthread_mutex_destroy(&data->camera_mutex);
    gp_context_unref(data->gp_context);
    gp_list_free(data->cam_list);

    obs_enter_graphics();
    gs_texture_destroy(data->texture);
    data->texture = NULL;
    obs_leave_graphics();

    signal_handler_t *sh = gphoto_get_udev_signalhandler();

    signal_handler_disconnect(sh, "device_added", timelapse_camera_added, data);
    signal_handler_disconnect(sh, "device_removed", timelapse_camera_removed, data);

    gphoto_unref_udev();

    bfree(vptr);
}

static uint32_t timelapse_getwidth(void *vptr) {
    struct timelapse_data *data = vptr;
    return data->width;
}

static uint32_t timelapse_getheight(void *vptr) {
    struct timelapse_data *data = vptr;
    return data->height;
}

static void timelapse_render(void *vptr, gs_effect_t *effect) {
    struct timelapse_data *data = vptr;

    gs_reset_blend_state();
    gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), data->texture);
    gs_draw_sprite(data->texture, 0, data->width, data->height);
}

static void timelapse_tick(void *vptr, float seconds) {
    struct timelapse_data *data = vptr;
    void *event_data;
    CameraEventType evtype;
    CameraFilePath *path;
    CameraFile *cam_file = NULL;
    const char *image_data = NULL;
    unsigned long data_size = NULL;
    Image *image = NULL;
    ImageInfo *image_info = AcquireImageInfo();
    ExceptionInfo *exception = AcquireExceptionInfo();

    if(data->camera){
        pthread_mutex_lock(&data->camera_mutex);
        data->time_elapsed += seconds;
        if (data->time_elapsed >= data->interval && data->interval > 0) {
            gphoto_capture(data->camera, data->gp_context, data->width, data->height, data->texture_data);

            obs_enter_graphics();
            gs_texture_set_image(data->texture, data->texture_data, data->width * 4, false);
            obs_leave_graphics();

            data->time_elapsed = 0;
        } else {
            gp_camera_wait_for_event(data->camera, 100, &evtype, &event_data, data->gp_context);
            path = event_data;
            if (evtype == GP_EVENT_FILE_ADDED) {
                if (gp_file_new(&cam_file) < GP_OK) {
                    blog(LOG_WARNING, "What???\n");
                } else {
                    if (gp_camera_file_get(data->camera, path->folder, path->name,
                                           GP_FILE_TYPE_NORMAL, cam_file, data->gp_context) < GP_OK) {
                        blog(LOG_WARNING, "Can't get photo from camera.\n");
                    } else {
                        if (gp_file_get_data_and_size(cam_file, &image_data, &data_size) < GP_OK) {
                            blog(LOG_WARNING, "Can't get image data.\n");
                        } else {
                            gp_camera_file_delete(data->camera, path->folder, path->name, data->gp_context);
                            image = BlobToImage(image_info, image_data, data_size, exception);
                            if (exception->severity != UndefinedException) {
                                CatchException(exception);
                                blog(LOG_WARNING, "ImageMagic error: %s.\n", (char *) exception->severity);
                                exception->severity = UndefinedException;
                            } else {
                                ExportImagePixels(image, 0, 0, (const size_t) data->width,
                                                  (const size_t) data->height,
                                                  "BGRA", CharPixel, data->texture_data,
                                                  exception);
                                if (exception->severity != UndefinedException) {
                                    CatchException(exception);
                                    blog(LOG_WARNING, "ImageMagic error: %s.\n", (char *) exception->severity);
                                    exception->severity = UndefinedException;
                                } else {
                                    obs_enter_graphics();
                                    gs_texture_set_image(data->texture, data->texture_data, data->width * 4, false);
                                    obs_leave_graphics();
                                }
                            }
                        }
                    }
                }
            }
        }
        pthread_mutex_unlock(&data->camera_mutex);
    }

    if (image_data) {
        free(image_data);
    }
    if (image_info) {
        DestroyImageInfo(image_info);
    }
    if (image) {
        DestroyImageList(image);
    }
    if (exception) {
        DestroyExceptionInfo(exception);
    }
    if (cam_file) {
        //TODO: SIGSEGV here, can't understand why!
        //gp_file_free(cam_file);
    }
}

struct obs_source_info timelapse_capture_info = {
        .id             = "timelapse-capture",
        .type           = OBS_SOURCE_TYPE_INPUT,
        .output_flags   = OBS_SOURCE_VIDEO,

        .get_name       = timelapse_getname,
        .get_defaults   = timelapse_defaults,
        .get_properties = timelapse_properties,
        .create         = timelapse_create,
        .destroy        = timelapse_destroy,
        .update         = timelapse_update,
        .show           = timelapse_show,
        .hide           = timelapse_hide,
        .get_width      = timelapse_getwidth,
        .get_height     = timelapse_getheight,
        .video_render   = timelapse_render,
        .video_tick     = timelapse_tick,
};