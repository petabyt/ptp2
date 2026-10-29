/*
 * libgphoto2 core compatibility layer over the libpak runtime.
 *
 * Implements the subset of libgphoto2/libgphoto2_port that ptp2_upstream/
 * links against (see funcs.txt history), backed by pak_rt_* calls.
 *
 * The ptp2 camlib is used unmodified; this file is the "core" side:
 *  - logging goes to pak_debug_log
 *  - CameraFile/CameraList are plain memory implementations
 *  - GPContext maps error/progress/cancel onto pak_rt_*
 *  - GPPort only carries the PTPIP path (ptpip.c does its own sockets)
 *  - CameraFilesystem just stores the camlib's CameraFilesystemFuncs so
 *    module.c can dispatch them directly
 *
 * Keep struct _GPContext in sync with module.c.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-context.h>
#include <gphoto2/gphoto2-list.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2-setting.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-widget.h>

#include <runtime.h>
#include <wifi.h>
#include <libusb.h>

static struct PakModule *g_mod;

void gp_set_module(struct PakModule *mod) {
	g_mod = mod;
}

static void pak_log(const char *fmt, ...) {
	char buf[2048];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (g_mod)
		pak_debug_log(g_mod, "%s", buf);
	else
		printf("ptp2: %s\n", buf);
}

/* ============================== logging ============================== */

char *gpi_vsnprintf(const char *format, va_list args) {
	va_list args2;
	int n;
	char *buf;

	va_copy(args2, args);
	n = vsnprintf(NULL, 0, format, args2);
	va_end(args2);
	if (n < 0)
		return NULL;

	buf = malloc(n + 1);
	if (!buf)
		return NULL;
	vsnprintf(buf, n + 1, format, args);
	return buf;
}

void gp_log(GPLogLevel level, const char *domain, const char *format, ...) {
	char *msg;
	va_list ap;

	(void)level;
	va_start(ap, format);
	msg = gpi_vsnprintf(format, ap);
	va_end(ap);
	if (msg) {
		pak_log("%s: %s", domain ? domain : "", msg);
		free(msg);
	}
}

void gp_logv(GPLogLevel level, const char *domain, const char *format, va_list args) {
	char *msg = gpi_vsnprintf(format, args);

	(void)level;
	if (msg) {
		pak_log("%s: %s", domain ? domain : "", msg);
		free(msg);
	}
}

void gp_log_with_source_location(GPLogLevel level, const char *file, int line,
				 const char *func, const char *format, ...) {
	char *msg;
	va_list ap;

	(void)level;
	va_start(ap, format);
	msg = gpi_vsnprintf(format, ap);
	va_end(ap);
	if (msg) {
		pak_log("%s:%d %s: %s", file, line, func, msg);
		free(msg);
	}
}

void gp_log_data(const char *domain, const char *data, unsigned int size,
		 const char *format, ...) {
	char *msg;
	va_list ap;
	unsigned int i, printed;

	va_start(ap, format);
	msg = gpi_vsnprintf(format, ap);
	va_end(ap);
	pak_log("%s: %s (%u bytes)", domain ? domain : "", msg ? msg : "", size);
	free(msg);

	printed = size > 512 ? 512 : size;
	for (i = 0; i < printed; i += 16) {
		char row[3 * 16 + 1];
		unsigned int j;

		for (j = 0; j < 16 && i + j < printed; j++)
			snprintf(row + j * 3, sizeof(row) - j * 3, "%02x ",
				 (unsigned char)data[i + j]);
		pak_log("  %04x: %s", i, row);
	}
}

/* ============================== CameraFile ============================== */

struct _CameraFile {
	char *data;
	unsigned long int size;
	char mime[64];
	char name[256];
	time_t mtime;
};

int gp_file_new(CameraFile **file) {
	*file = calloc(sizeof(struct _CameraFile), 1);
	return *file ? GP_OK : GP_ERROR_NO_MEMORY;
}

int gp_file_free(CameraFile *file) {
	if (!file)
		return GP_OK;
	free(file->data);
	free(file);
	return GP_OK;
}

int gp_file_unref(CameraFile *file) {
	return gp_file_free(file);
}

int gp_file_append(CameraFile *file, const char *data, unsigned long int size) {
	char *ndata = realloc(file->data, file->size + size);

	if (size && !ndata)
		return GP_ERROR_NO_MEMORY;
	file->data = ndata;
	memcpy(file->data + file->size, data, size);
	file->size += size;
	return GP_OK;
}

int gp_file_slurp(CameraFile *file, char *data, size_t size, size_t *readlen) {
	size_t len = size < file->size ? size : file->size;

	memcpy(data, file->data, len);
	if (readlen)
		*readlen = len;
	return GP_OK;
}

int gp_file_get_data_and_size(CameraFile *file, const char **data, unsigned long int *size) {
	*data = file->data;
	*size = file->size;
	return GP_OK;
}

int gp_file_set_data_and_size(CameraFile *file, char *data, unsigned long int size) {
	free(file->data);
	file->data = data;
	file->size = size;
	return GP_OK;
}

int gp_file_get_mime_type(CameraFile *file, const char **mime_type) {
	*mime_type = file->mime;
	return GP_OK;
}

int gp_file_set_mime_type(CameraFile *file, const char *mime_type) {
	snprintf(file->mime, sizeof(file->mime), "%s", mime_type ? mime_type : "");
	return GP_OK;
}

int gp_file_get_mtime(CameraFile *file, time_t *mtime) {
	*mtime = file->mtime;
	return GP_OK;
}

int gp_file_set_mtime(CameraFile *file, time_t mtime) {
	file->mtime = mtime;
	return GP_OK;
}

int gp_file_set_name(CameraFile *file, const char *name) {
	snprintf(file->name, sizeof(file->name), "%s", name ? name : "");
	return GP_OK;
}

/* ============================== CameraList ============================== */

struct _CameraList {
	int n;
	int cap;
	char **name;
	char **value;
};

int gp_list_new(CameraList **list) {
	*list = calloc(sizeof(struct _CameraList), 1);
	return *list ? GP_OK : GP_ERROR_NO_MEMORY;
}

int gp_list_free(CameraList *list) {
	if (!list)
		return GP_OK;
	gp_list_reset(list);
	free(list->name);
	free(list->value);
	free(list);
	return GP_OK;
}

int gp_list_reset(CameraList *list) {
	int i;

	for (i = 0; i < list->n; i++) {
		free(list->name[i]);
		free(list->value[i]);
	}
	list->n = 0;
	return GP_OK;
}

int gp_list_count(CameraList *list) {
	return list->n;
}

static int list_grow(CameraList *list) {
	if (list->n < list->cap)
		return GP_OK;
	int cap = list->cap ? list->cap * 2 : 16;
	char **name = realloc(list->name, cap * sizeof(char *));
	char **value = realloc(list->value, cap * sizeof(char *));

	if (!name || !value) {
		free(name);
		free(value);
		return GP_ERROR_NO_MEMORY;
	}
	list->name = name;
	list->value = value;
	list->cap = cap;
	return GP_OK;
}

int gp_list_append(CameraList *list, const char *name, const char *value) {
	int rc = list_grow(list);

	if (rc != GP_OK)
		return rc;
	list->name[list->n] = strdup(name ? name : "");
	list->value[list->n] = value ? strdup(value) : NULL;
	if (!list->name[list->n])
		return GP_ERROR_NO_MEMORY;
	list->n++;
	return GP_OK;
}

int gp_list_find_by_name(CameraList *list, int *index, const char *name) {
	int i;

	for (i = 0; i < list->n; i++) {
		if (!strcmp(list->name[i], name)) {
			*index = i;
			return GP_OK;
		}
	}
	return GP_ERROR;
}

int gp_list_get_name(CameraList *list, int index, const char **name) {
	if (index < 0 || index >= list->n)
		return GP_ERROR;
	*name = list->name[index];
	return GP_OK;
}

/* ============================== GPContext ============================== */

/* Keep in sync with module.c */
struct _GPContext {
	struct PakModule *mod;
	int job;
};

void gp_context_error(GPContext *context, const char *format, ...) {
	char *msg;
	va_list ap;

	(void)context;
	va_start(ap, format);
	msg = gpi_vsnprintf(format, ap);
	va_end(ap);
	pak_log("ERROR: %s", msg ? msg : "");
	free(msg);
}

void gp_context_idle(GPContext *context) {
	(void)context;
}

GPContextFeedback gp_context_cancel(GPContext *context) {
	if (context && context->mod &&
	    pak_rt_is_job_cancelled(context->mod, context->job))
		return GP_CONTEXT_FEEDBACK_CANCEL;
	return GP_CONTEXT_FEEDBACK_OK;
}

unsigned int gp_context_progress_start(GPContext *context, float target,
				       const char *format, ...) {
	char *msg;
	va_list ap;

	(void)context;
	va_start(ap, format);
	msg = gpi_vsnprintf(format, ap);
	va_end(ap);
	pak_log("progress: %s (target %f)", msg ? msg : "", target);
	free(msg);
	return 0;
}

void gp_context_progress_update(GPContext *context, unsigned int id, float current) {
	(void)id;
	if (context && context->mod)
		pak_rt_set_progress_bar(context->mod, context->job, (int)current);
}

void gp_context_progress_stop(GPContext *context, unsigned int id) {
	(void)context;
	(void)id;
}

/* ============================== GPPort ============================== */

struct _GPPortInfo {
	GPPortType type;
	char path[128];
};

struct _GPPortPrivateLibrary {
	struct _GPPortInfo info;
	libusb_device *d;
	libusb_device_handle *dh;
};

GPPort *gp_alloc_port(struct PakModule *mod) {
	GPPort *port = calloc(1, sizeof(GPPort));

	(void)mod;
	if (!port)
		return NULL;
	port->pl = calloc(1, sizeof(GPPortPrivateLibrary));
	if (!port->pl) {
		free(port);
		return NULL;
	}
	port->type = GP_PORT_PTPIP;
	port->pl->info.type = GP_PORT_PTPIP;
	return port;
}

void gp_port_set_path(GPPort *port, const char *path) {
	if (!port || !port->pl)
		return;
	snprintf(port->pl->info.path, sizeof(port->pl->info.path), "%s", path);
}

int gp_port_get_info(GPPort *port, GPPortInfo *info) {
	C_PARAMS(port && port->pl && info);
	*info = &port->pl->info;
	return GP_OK;
}

int gp_port_info_get_path(GPPortInfo info, char **path) {
	C_PARAMS(info && path);
	*path = strdup(info->path);
	return *path ? GP_OK : GP_ERROR_NO_MEMORY;
}

int gp_port_get_settings(GPPort *port, GPPortSettings *settings) {
	C_PARAMS(port && settings);
	*settings = port->settings;
	return GP_OK;
}

int gp_port_get_timeout(GPPort *port, int *timeout) {
	C_PARAMS(port && timeout);
	*timeout = port->timeout;
	return GP_OK;
}

int gp_port_set_timeout(GPPort *port, int timeout) {
	C_PARAMS(port);
	port->timeout = timeout;
	return GP_OK;
}

/*
 * USB transport, adapted from libgphoto2's libusb1 io-driver
 * (libgphoto2_port/libusb1/libusb1.c). PTPIP does its own socket IO in
 * ptpip.c and never reaches these; the USB PTP path in usb.c goes
 * exclusively through this gp_port surface.
 *
 * The libusb context is initialized once here (never exited) — vcam's
 * wrapper crashes on libusb_init(NULL), so always pass a context pointer.
 */

#define C_LIBUSB(_r, _default_error) \
	do { int _rret = (_r); if (_rret < LIBUSB_SUCCESS) return translate_libusb_error(_rret, _default_error); } while (0)

static int translate_libusb_error(int error, int default_return)
{
	switch (error) {
	case LIBUSB_SUCCESS:			return GP_OK;
	case LIBUSB_ERROR_INVALID_PARAM:	return GP_ERROR_BAD_PARAMETERS;
	case LIBUSB_ERROR_NO_DEVICE:		return GP_ERROR_IO_USB_FIND;
	case LIBUSB_ERROR_TIMEOUT:		return GP_ERROR_TIMEOUT;
	case LIBUSB_ERROR_NO_MEM:		return GP_ERROR_NO_MEMORY;
	case LIBUSB_ERROR_NOT_SUPPORTED:	return GP_ERROR_NOT_SUPPORTED;
	case LIBUSB_ERROR_IO:
	case LIBUSB_ERROR_BUSY:
	case LIBUSB_ERROR_NOT_FOUND:
	case LIBUSB_ERROR_ACCESS:
	case LIBUSB_ERROR_OVERFLOW:
	case LIBUSB_ERROR_PIPE:
	case LIBUSB_ERROR_INTERRUPTED:
	default:
		return default_return;
	}
}

static int gp_libusb1_find_ep(libusb_device *dev, int config, int interface,
			      int altsetting, int direction, int type)
{
	const struct libusb_interface_descriptor *intf;
	struct libusb_config_descriptor *confdesc;
	int i;

	if (libusb_get_config_descriptor(dev, config, &confdesc) < 0)
		return -1;

	intf = &confdesc->interface[interface].altsetting[altsetting];
	for (i = 0; i < intf->bNumEndpoints; i++) {
		if (((intf->endpoint[i].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == direction) &&
		    ((intf->endpoint[i].bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == type)) {
			unsigned char ret = intf->endpoint[i].bEndpointAddress;
			libusb_free_config_descriptor(confdesc);
			return ret;
		}
	}
	libusb_free_config_descriptor(confdesc);
	return -1;
}

static int gp_libusb1_find_first_altsetting(libusb_device *dev, int *config,
					    int *interface, int *altsetting)
{
	int i, i1, i2;
	struct libusb_device_descriptor desc;

	if (libusb_get_device_descriptor(dev, &desc) < 0)
		return -1;

	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct libusb_config_descriptor *confdesc;

		if (libusb_get_config_descriptor(dev, i, &confdesc) < 0)
			return -1;

		for (i1 = 0; i1 < confdesc->bNumInterfaces; i1++)
			for (i2 = 0; i2 < confdesc->interface[i1].num_altsetting; i2++)
				if (confdesc->interface[i1].altsetting[i2].bNumEndpoints) {
					*config = i;
					*interface = i1;
					*altsetting = i2;
					libusb_free_config_descriptor(confdesc);
					return 0;
				}
		libusb_free_config_descriptor(confdesc);
	}
	return -1;
}

/* Fills settings.usb from the device's descriptors, then opens and claims
 * the interface. Equivalent of upstream gp_libusb1_find_path_lib + open +
 * gp_libusb1_update for an already-granted libusb_device.
 *
 * The host owns libusb_init/exit and enumeration (real libusb, vcam's
 * wrapper or the wasm runtime layer); every call here is device/handle
 * scoped and therefore context-free. */
int gp_port_usb_setup(GPPort *port, libusb_device *dev)
{
	struct libusb_config_descriptor *confdesc;
	int config = -1, interface = -1, altsetting = -1;

	C_PARAMS(port && port->pl && dev);
	port->pl->d = dev;

	if (gp_libusb1_find_first_altsetting(dev, &config, &interface, &altsetting) < 0)
		return GP_ERROR_IO_USB_FIND;
	C_LIBUSB(libusb_get_config_descriptor(dev, config, &confdesc), GP_ERROR_IO);

	port->settings.usb.config = confdesc->bConfigurationValue;
	port->settings.usb.interface = confdesc->interface[interface].altsetting[altsetting].bInterfaceNumber;
	port->settings.usb.altsetting = confdesc->interface[interface].altsetting[altsetting].bAlternateSetting;
	port->settings.usb.inep  = gp_libusb1_find_ep(dev, config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_BULK);
	port->settings.usb.outep = gp_libusb1_find_ep(dev, config, interface, altsetting, LIBUSB_ENDPOINT_OUT, LIBUSB_TRANSFER_TYPE_BULK);
	port->settings.usb.intep = gp_libusb1_find_ep(dev, config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_INTERRUPT);
	port->settings.usb.maxpacketsize = libusb_get_max_packet_size(dev, port->settings.usb.inep);
	GP_LOG_D("USB defaults: config %d, interface %d, altsetting %d, inep %02x, outep %02x, intep %02x, maxpacket %d",
		 port->settings.usb.config, port->settings.usb.interface,
		 port->settings.usb.altsetting, port->settings.usb.inep,
		 port->settings.usb.outep, port->settings.usb.intep,
		 port->settings.usb.maxpacketsize);
	libusb_free_config_descriptor(confdesc);

	if (port->settings.usb.inep == -1 || port->settings.usb.outep == -1)
		return GP_ERROR_IO_USB_FIND;

	C_LIBUSB(libusb_open(dev, &port->pl->dh), GP_ERROR_IO);
	C_PARAMS(port->pl->dh);
	libusb_set_auto_detach_kernel_driver(port->pl->dh, 1);
	C_LIBUSB(libusb_claim_interface(port->pl->dh, port->settings.usb.interface),
		 GP_ERROR_IO_USB_CLAIM);
	if (port->settings.usb.altsetting != 0)
		C_LIBUSB(libusb_set_interface_alt_setting(port->pl->dh,
							  port->settings.usb.interface,
							  port->settings.usb.altsetting),
			 GP_ERROR_IO_USB_CLAIM);
	return GP_OK;
}

void gp_port_usb_release(GPPort *port) {
	if (!port || !port->pl)
		return;
	if (port->pl->dh) {
		libusb_release_interface(port->pl->dh, port->settings.usb.interface);
		libusb_close(port->pl->dh);
		port->pl->dh = NULL;
	}
	port->pl->d = NULL;
}

int gp_port_write(GPPort *port, const char *data, int size) {
	int curwritten = 0;

	C_PARAMS(port && port->pl->dh);
	C_LIBUSB(libusb_bulk_transfer(port->pl->dh, port->settings.usb.outep,
				      (unsigned char *)data, size, &curwritten,
				      port->timeout), GP_ERROR_IO_WRITE);
	if (curwritten < 0)
		return GP_ERROR_IO_WRITE;
	return curwritten;
}

int gp_port_read(GPPort *port, char *data, int size) {
	int curread = 0;

	C_PARAMS(port && port->pl->dh);
	C_LIBUSB(libusb_bulk_transfer(port->pl->dh, port->settings.usb.inep,
				      (unsigned char *)data, size, &curread,
				      port->timeout), GP_ERROR_IO_READ);
	if (curread < 0)
		return GP_ERROR_IO_READ;
	return curread;
}

int gp_port_check_int(GPPort *port, char *data, int size) {
	int curread = 0;
	int rc;

	C_PARAMS(port && port->pl->dh);
	/* ptp2 polls events with timeout 0 expecting no blocking at all. */
	if (!port->timeout)
		return GP_ERROR_TIMEOUT;
	rc = libusb_interrupt_transfer(port->pl->dh, port->settings.usb.intep,
				       (unsigned char *)data, size, &curread,
				       port->timeout);
	if (rc < LIBUSB_SUCCESS)
		return translate_libusb_error(rc, GP_ERROR_IO_READ);
	if (curread < 0)
		return GP_ERROR_IO_READ;
	return curread ? curread : GP_ERROR_TIMEOUT;
}

int gp_port_usb_clear_halt(GPPort *port, int ep) {
	unsigned char usb_ep;

	C_PARAMS(port && port->pl->dh);
	switch (ep) {
	case GP_PORT_USB_ENDPOINT_IN:	usb_ep = port->settings.usb.inep; break;
	case GP_PORT_USB_ENDPOINT_OUT:	usb_ep = port->settings.usb.outep; break;
	case GP_PORT_USB_ENDPOINT_INT:	usb_ep = port->settings.usb.intep; break;
	default: return GP_ERROR_BAD_PARAMETERS;
	}
	C_LIBUSB(libusb_clear_halt(port->pl->dh, usb_ep), GP_ERROR_IO_USB_CLEAR_HALT);
	return GP_OK;
}

static int gp_libusb1_msg(GPPort *port, int request, int value, int index,
			  char *bytes, int size, int flags, int default_error)
{
	int handled;

	C_PARAMS(port && port->pl->dh);
	C_LIBUSB(handled = libusb_control_transfer(port->pl->dh, flags, request,
						   value, index,
						   (unsigned char *)bytes, size,
						   port->timeout), default_error);
	if (handled < 0)
		return default_error;
	return handled;
}

int gp_port_usb_msg_class_read(GPPort *port, int request, int value, int index,
			       char *bytes, int size) {
	return gp_libusb1_msg(port, request, value, index, bytes, size,
			      LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
			      LIBUSB_ENDPOINT_IN, GP_ERROR_IO_READ);
}

int gp_port_usb_msg_class_write(GPPort *port, int request, int value, int index,
				char *bytes, int size) {
	return gp_libusb1_msg(port, request, value, index, bytes, size,
			      LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
			      GP_ERROR_IO_WRITE);
}

const char *gp_port_result_as_string(int result) {
	return gp_result_as_string(result);
}

/* ============================== CameraFilesystem ============================== */

struct _CameraFilesystem {
	CameraFilesystemFuncs *funcs;
	void *data;
};

int gp_filesystem_new(CameraFilesystem **fs) {
	*fs = calloc(sizeof(struct _CameraFilesystem), 1);
	return *fs ? GP_OK : GP_ERROR_NO_MEMORY;
}

int gp_filesystem_free(CameraFilesystem *fs) {
	free(fs);
	return GP_OK;
}

CameraFilesystemFuncs *gp_fs_funcs(CameraFilesystem *fs) {
	return fs ? fs->funcs : NULL;
}

int gp_filesystem_set_funcs(CameraFilesystem *fs, CameraFilesystemFuncs *funcs, void *data) {
	C_PARAMS(fs);
	fs->funcs = funcs;
	fs->data = data;
	return GP_OK;
}

/* The camlib uses these as cache no-ops against the core filesystem;
 * we dispatch its funcs directly from module.c instead of caching. */

int gp_filesystem_append(CameraFilesystem *fs, const char *folder, const char *filename,
			 GPContext *context) {
	(void)fs;
	(void)folder;
	(void)filename;
	(void)context;
	return GP_OK;
}

int gp_filesystem_set_file_noop(CameraFilesystem *fs, const char *folder, const char *filename,
				CameraFileType type, CameraFile *file, GPContext *context) {
	(void)fs;
	(void)folder;
	(void)filename;
	(void)type;
	(void)file;
	(void)context;
	return GP_OK;
}

int gp_filesystem_set_info_noop(CameraFilesystem *fs, const char *folder, const char *filename,
				CameraFileInfo info, GPContext *context) {
	(void)fs;
	(void)folder;
	(void)filename;
	(void)info;
	(void)context;
	return GP_OK;
}

int gp_filesystem_set_info_dirty(CameraFilesystem *fs, const char *folder,
				 const char *filename, GPContext *context) {
	(void)fs;
	(void)folder;
	(void)filename;
	(void)context;
	return GP_OK;
}

int gp_filesystem_reset(CameraFilesystem *fs) {
	(void)fs;
	return GP_OK;
}

/* ============================== settings / results ============================== */

#define GP_SETTINGS_MAX 16
static struct {
	char key[64];
	char value[128];
} gp_settings[GP_SETTINGS_MAX];

int gp_setting_set(char *id, char *key, char *value) {
	int i;

	(void)id;
	if (!key || !value)
		return GP_ERROR_BAD_PARAMETERS;
	for (i = 0; i < GP_SETTINGS_MAX; i++) {
		if (!strcmp(gp_settings[i].key, key)) {
			snprintf(gp_settings[i].value, sizeof(gp_settings[i].value), "%s", value);
			return GP_OK;
		}
	}
	for (i = 0; i < GP_SETTINGS_MAX; i++) {
		if (!gp_settings[i].key[0]) {
			snprintf(gp_settings[i].key, sizeof(gp_settings[i].key), "%s", key);
			snprintf(gp_settings[i].value, sizeof(gp_settings[i].value), "%s", value);
			return GP_OK;
		}
	}
	return GP_ERROR;
}

int gp_setting_get(char *id, char *key, char *value) {
	int i;

	(void)id;
	if (!key || !value)
		return GP_ERROR_BAD_PARAMETERS;
	for (i = 0; i < GP_SETTINGS_MAX; i++) {
		if (!strcmp(gp_settings[i].key, key)) {
			strcpy(value, gp_settings[i].value);
			return GP_OK;
		}
	}
	return GP_ERROR;
}

const char *gp_result_as_string(int result) {
	switch (result) {
	case GP_OK: return "OK";
	case GP_ERROR: return "Generic error";
	case GP_ERROR_BAD_PARAMETERS: return "Bad parameters";
	case GP_ERROR_NO_MEMORY: return "Out of memory";
	case GP_ERROR_LIBRARY: return "Library error";
	case GP_ERROR_UNKNOWN_PORT: return "Unknown port";
	case GP_ERROR_NOT_SUPPORTED: return "Not supported";
	case GP_ERROR_IO: return "IO error";
	case GP_ERROR_TIMEOUT: return "Timeout";
	case GP_ERROR_IO_SUPPORTED_SERIAL: return "Serial IO not supported";
	case GP_ERROR_IO_SUPPORTED_USB: return "USB IO not supported";
	case GP_ERROR_IO_INIT: return "IO init failed";
	case GP_ERROR_IO_READ: return "IO read error";
	case GP_ERROR_IO_WRITE: return "IO write error";
	case GP_ERROR_IO_UPDATE: return "IO update error";
	case GP_ERROR_IO_USB_CLEAR_HALT: return "USB clear halt failed";
	case GP_ERROR_IO_USB_FIND: return "USB device not found";
	case GP_ERROR_IO_USB_CLAIM: return "USB interface claim failed";
	case GP_ERROR_IO_LOCK: return "Device locked";
	case GP_ERROR_CORRUPTED_DATA: return "Corrupted data";
	case GP_ERROR_FILE_EXISTS: return "File exists";
	case GP_ERROR_MODEL_NOT_FOUND: return "Model not found";
	case GP_ERROR_DIRECTORY_NOT_FOUND: return "Directory not found";
	case GP_ERROR_FILE_NOT_FOUND: return "File not found";
	case GP_ERROR_DIRECTORY_EXISTS: return "Directory exists";
	case GP_ERROR_CAMERA_BUSY: return "Camera busy";
	case GP_ERROR_PATH_NOT_ABSOLUTE: return "Path not absolute";
	case GP_ERROR_CANCEL: return "Cancelled";
	case GP_ERROR_CAMERA_ERROR: return "Camera error";
	case GP_ERROR_OS_FAILURE: return "OS failure";
	default: return "Unknown error";
	}
}

/* ============================== camera / abilities ============================== */

void gp_free_camera(Camera *camera);

/* Private core struct (opaque to camlibs). The module fills abilities from
 * the granted transport before camera_init() reads them back; no globals. */
struct _CameraPrivateCore {
	struct PakModule *mod;
	CameraAbilities abilities;
};

Camera *gp_alloc_camera(struct PakModule *mod) {
	Camera *camera = calloc(1, sizeof(Camera));

	if (!camera)
		return NULL;
	camera->functions = calloc(1, sizeof(CameraFunctions));
	camera->pc = calloc(1, sizeof(CameraPrivateCore));
	camera->port = gp_alloc_port(mod);
	if (gp_filesystem_new(&camera->fs) != GP_OK)
		camera->fs = NULL;
	if (!camera->functions || !camera->pc || !camera->port || !camera->fs) {
		gp_free_camera(camera);
		return NULL;
	}
	camera->pc->mod = mod;
	camera->port->type = GP_PORT_PTPIP;
	return camera;
}

void gp_free_camera(Camera *camera) {
	if (!camera)
		return;
	free(camera->functions);
	free(camera->pc);
	if (camera->port) {
		/* PTPIP ports have no dh; USB ports release + close here. */
		gp_port_usb_release(camera->port);
		free(camera->port->pl);
		free(camera->port);
	}
	gp_filesystem_free(camera->fs);
	free(camera);
}

int gp_camera_set_abilities(Camera *camera, CameraAbilities abilities) {
	C_PARAMS(camera && camera->pc);
	camera->pc->abilities = abilities;
	return GP_OK;
}

int gp_camera_get_abilities(Camera *camera, CameraAbilities *abilities) {
	C_PARAMS(camera && camera->pc && abilities);
	*abilities = camera->pc->abilities;
	return GP_OK;
}

int gp_abilities_list_append(CameraAbilitiesList *list, CameraAbilities abilities) {
	(void)list;
	(void)abilities;
	return GP_OK;
}

/* ============================== widgets (stubs) ============================== */

/* The config/widget surface is not wired up yet; these only need to link
 * because config.c references them. Nothing calls them at runtime. */

int gp_widget_new(CameraWidgetType type, const char *label, CameraWidget **widget) {
	(void)type;
	(void)label;
	(void)widget;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_free(CameraWidget *widget) {
	(void)widget;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_append(CameraWidget *widget, CameraWidget *child) {
	(void)widget;
	(void)child;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_count_children(CameraWidget *widget) {
	(void)widget;
	return 0;
}

int gp_widget_get_child(CameraWidget *widget, int child_number, CameraWidget **child) {
	(void)widget;
	(void)child_number;
	(void)child;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_child_by_label(CameraWidget *widget, const char *label, CameraWidget **child) {
	(void)widget;
	(void)label;
	(void)child;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_child_by_name(CameraWidget *widget, const char *name, CameraWidget **child) {
	(void)widget;
	(void)name;
	(void)child;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_name(CameraWidget *widget, const char **name) {
	(void)widget;
	(void)name;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_set_name(CameraWidget *widget, const char *name) {
	(void)widget;
	(void)name;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_value(CameraWidget *widget, void *value) {
	(void)widget;
	(void)value;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_set_value(CameraWidget *widget, const void *value) {
	(void)widget;
	(void)value;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_get_type(CameraWidget *widget, CameraWidgetType *type) {
	(void)widget;
	(void)type;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_set_range(CameraWidget *range, float min, float max, float increment) {
	(void)range;
	(void)min;
	(void)max;
	(void)increment;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_add_choice(CameraWidget *widget, const char *choice) {
	(void)widget;
	(void)choice;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_changed(CameraWidget *widget) {
	(void)widget;
	return 0;
}

int gp_widget_set_changed(CameraWidget *widget, int changed) {
	(void)widget;
	(void)changed;
	return GP_ERROR_NOT_SUPPORTED;
}

int gp_widget_set_readonly(CameraWidget *widget, int readonly) {
	(void)widget;
	(void)readonly;
	return GP_ERROR_NOT_SUPPORTED;
}
