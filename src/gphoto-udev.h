#include <libudev.h>

void gphoto_init_udev(void);
void gphoto_unref_udev(void);
signal_handler_t *gphoto_get_udev_signalhandler(void);