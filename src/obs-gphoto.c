#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-gphoto", "en-US")

extern struct obs_source_info capture_preview_info;

bool obs_module_load(void) {
    obs_register_source(&capture_preview_info);
    return true;
}