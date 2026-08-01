#include <stdarg.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include "config.h"
#include "gphoto2/gphoto2.h"

void gp_log(GPLogLevel level, const char *domain, const char *format, ...) {
	printf("TODO: %s\n", __func__);
}

void gp_log_with_source_location(GPLogLevel level, const char *file, int line, const char *func, const char *format, ...) {
	printf("TODO: %s\n", __func__);
}

void gp_logv(GPLogLevel level, const char *domain, const char *format, va_list args) {
	printf("TODO: %s\n", __func__);
}

void gp_log_data(const char *domain, const char *data, unsigned int size, const char *format, ...) {
	printf("TODO: %s\n", __func__);
}

char *gpi_vsnprintf(const char *format, va_list args) {
	printf("TODO: %s\n", __func__);
	return NULL;
}

struct _GPPortPrivateCore {
	int fd;
};

const char *gp_port_result_as_string(int result) {
	printf("TODO: %s\n", __func__);
	return NULL;
}

int gp_port_read(GPPort *port, char *data, int size) {
	if (port->type == GP_PORT_PTPIP) {
		
	}
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_write(GPPort *port, const char *data, int size) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_get_info(GPPort *port, GPPortInfo *info) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_get_settings(GPPort *port, GPPortSettings *settings) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_get_timeout(GPPort *port, int *timeout) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_set_timeout(GPPort *port, int timeout) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_check_int(GPPort *port, char *data, int size) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_usb_clear_halt(GPPort *port, int ep) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_usb_msg_class_read(GPPort *port, int request, int value, int index, char *bytes, int size) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_usb_msg_class_write(GPPort *port, int request, int value, int index, char *bytes, int size) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_port_info_get_path(GPPortInfo info, char **path) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_new(CameraWidgetType type, const char *label, CameraWidget **widget) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_free(CameraWidget *widget) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_append(CameraWidget *widget, CameraWidget *child) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_count_children(CameraWidget *widget) {
	printf("TODO: %s\n", __func__);
	return 0;
}

int gp_widget_get_child(CameraWidget *widget, int child_number, CameraWidget **child) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_child_by_label(CameraWidget *widget, const char *label, CameraWidget **child) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_child_by_name(CameraWidget *widget, const char *name, CameraWidget **child) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_name(CameraWidget *widget, const char **name) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_set_name(CameraWidget *widget, const char *name) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_value(CameraWidget *widget, void *value) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_set_value(CameraWidget *widget, const void *value) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_type(CameraWidget *widget, CameraWidgetType *type) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_set_range(CameraWidget *range, float min, float max, float increment) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_add_choice(CameraWidget *widget, const char *choice) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_changed(CameraWidget *widget) {
	printf("TODO: %s\n", __func__);
	return 0;
}

int gp_widget_set_changed(CameraWidget *widget, int changed) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_set_readonly(CameraWidget *widget, int readonly) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

struct _CameraFile {
	 struct PakFileHandle *file;
};

int gp_file_new(CameraFile **file) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_free(CameraFile *file) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_unref(CameraFile *file) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_append(CameraFile *file, const char *data, unsigned long int size) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_slurp(CameraFile *file, char *data, size_t size, size_t *readlen) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_get_data_and_size(CameraFile *file, const char **data, unsigned long int *size) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_set_data_and_size(CameraFile *file, char *data, unsigned long int size) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_get_mime_type(CameraFile *file, const char **mime_type) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_set_mime_type(CameraFile *file, const char *mime_type) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_get_mtime(CameraFile *file, time_t *mtime) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_set_mtime(CameraFile *file, time_t mtime) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_file_set_name(CameraFile *file, const char *name) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_list_count(CameraList *list) {
	printf("TODO: %s\n", __func__);
	return 0;
}

int gp_list_append(CameraList *list, const char *name, const char *value) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_list_reset(CameraList *list) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_list_find_by_name(CameraList *list, int *index, const char *name) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

void gp_context_error(GPContext *context, const char *format, ...) {
	printf("TODO: %s\n", __func__);
}

void gp_context_idle(GPContext *context) {
	printf("TODO: %s\n", __func__);
}

GPContextFeedback gp_context_cancel(GPContext *context) {
	printf("TODO: %s\n", __func__);
	return GP_CONTEXT_FEEDBACK_OK;
}

unsigned int gp_context_progress_start(GPContext *context, float target, const char *format, ...) {
	printf("TODO: %s\n", __func__);
	return 0;
}

void gp_context_progress_update(GPContext *context, unsigned int id, float current) {
	printf("TODO: %s\n", __func__);
}

void gp_context_progress_stop(GPContext *context, unsigned int id) {
	printf("TODO: %s\n", __func__);
}

int gp_camera_get_abilities(Camera *camera, CameraAbilities *abilities) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_abilities_list_append(CameraAbilitiesList *list, CameraAbilities abilities) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_filesystem_set_file_noop(CameraFilesystem *fs, const char *folder, const char *filename, CameraFileType type, CameraFile *file, GPContext *context) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_filesystem_set_info_dirty(CameraFilesystem *fs, const char *folder, const char *filename, GPContext *context) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_filesystem_set_funcs(CameraFilesystem *fs, CameraFilesystemFuncs *funcs, void *data) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_filesystem_append(CameraFilesystem *fs, const char *folder, const char *filename, GPContext *context) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_filesystem_set_info_noop(CameraFilesystem *fs, const char *folder, const char *filename, CameraFileInfo info, GPContext *context) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_filesystem_reset(CameraFilesystem *fs) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_setting_set(char *id, char *key, char *value) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_setting_get(char *id, char *key, char *value) {
	printf("TODO: %s\n", __func__);
	return GP_ERROR_NOT_SUPPORTED;
}

const char *gp_result_as_string(int result) {
	printf("TODO: %s\n", __func__);
	return NULL;
}
