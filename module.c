#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include "config.h"
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-locking.h>

#include <runtime.h>
#include <runtime_ext.h>
#include <wifi.h>

struct _GPContext {
	int foo;
};

struct ModulePriv {
	GPContext *context;
	Camera *camera;
};

// Private struct in Camera
struct _CameraPrivateCore {
	struct PakModule *mod;
};

GPPort *gp_alloc_port(struct PakModule *mod);

static int init(struct PakModule *mod) {
	pak_debug_log(mod, "Hello from libgphoto2 module");
	mod->priv = (struct ModulePriv *)calloc(sizeof(struct ModulePriv), 1);
	mod->priv->context = calloc(1, sizeof(GPContext));

	pak_rt_set_screen_supported(mod, PAK_SCREEN_DASHBOARD, 1);
	pak_rt_set_screen_supported(mod, PAK_SCREEN_FILE_GALLERY, 1);
	pak_rt_set_screen_supported(mod, PAK_SCREEN_FILE_VIEWER, 1);
	pak_rt_set_tick_interval(mod, 1000 * 100);
	return 0;
}

static int on_free(struct PakModule *mod) {
	return 0;
}

static Camera *alloc_camera(struct PakModule *mod) {
	Camera *camera = calloc(1, sizeof(Camera));
	camera->functions = calloc(1, sizeof(CameraFunctions));
	camera->pc = calloc(1, sizeof(CameraPrivateCore));
	camera->port = gp_alloc_port(mod);
	camera->port->type = GP_PORT_PTPIP;
	mod->priv->camera = camera;

	camera_init(camera, mod->priv->context);
	return camera;
}

static int on_try_connect_wifi(struct PakModule *mod, struct PakWiFiAdapter *handle, struct PakSavedConnection *saved, int job) {
	return 0;
}

static int on_run_test(struct PakModule *mod, int job) {
	Camera *cam = alloc_camera(mod);
	return 0;
}

static int on_idle_tick(struct PakModule *mod, unsigned int us_since_last_tick) {
	return 0;
}

static int on_disconnect(struct PakModule *mod) {
	return 0;
}

static int on_switch_screen(struct PakModule *mod, int old_screen, int new_screen, int job) {
	return 0;
}

static int on_request_file_contents(struct PakModule *mod, int job, struct PakFileHandle *file) {
	return 0;
}

static int on_request_thumbnail(struct PakModule *mod, int job, struct PakFileHandle *file) {
	return -1;
}

static int on_request_file_metadata(struct PakModule *mod, int job, struct PakFileHandle *file) {
	return 0;
}

static int on_custom_command(struct PakModule *mod, int job, int argc, const char * const *argv) {
	return 0;
}

static int on_prop_changed(struct PakModule *mod, int job, struct PakWidget *prop) {
	return 0;
}

int get_module(struct PakModule *mod) {
	mod->init = init;
	mod->free = on_free;
	mod->on_request_file_thumbnail = on_request_thumbnail;
	mod->on_request_file_metadata = on_request_file_metadata;
	mod->on_try_connect_wifi = on_try_connect_wifi;
	mod->on_request_file_contents = on_request_file_contents;
	mod->on_idle_tick = on_idle_tick;
	mod->on_disconnect = on_disconnect;
	mod->on_switch_screen = on_switch_screen;
	mod->on_custom_command = on_custom_command;
	mod->on_setting_changed = on_prop_changed;
	mod->on_run_test = on_run_test;
	return 0;
}
