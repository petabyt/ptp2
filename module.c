/*
 * ptp2 libpak module glue.
 *
 * Exposes the unmodified gphoto2 ptp2 camlib (ptp2_upstream/) as a libpak
 * module: connects to Canon cameras over PTP/IP (the host joins the camera
 * AP via the manifest wifiFilter, the camera answers at a fixed address)
 * and serves the photo gallery (storage/file listing, metadata, thumbnails,
 * downloads) by dispatching the camlib's CameraFilesystemFuncs directly.
 *
 * The libgphoto2 core side lives in gphoto2.c.
 * Keep struct _GPContext in sync with gphoto2.c.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <libusb.h>

#include <runtime.h>
#include <runtime_ext.h>
#include <wifi.h>

#include "ptp.h"
#include "ptp-private.h"

/* The host joins the camera AP (manifest wifiFilter); the camera then
 * answers at its fixed PTP/IP address, port 15740 (PTPIP standard). */
#define CANON_PTPIP "192.168.1.2:15740"

/* Resolve at most this many files per folder during the gallery walk. */
#define MAX_FOLDER_FILES 512

void gp_port_set_path(GPPort *port, const char *path);
void gp_set_module(struct PakModule *mod);
CameraFilesystemFuncs *gp_fs_funcs(CameraFilesystem *fs);
int gp_port_usb_setup(GPPort *port, libusb_device *dev);
void gp_port_usb_release(GPPort *port);
Camera *gp_alloc_camera(struct PakModule *mod);
void gp_free_camera(Camera *camera);

static void free_folders(struct PakModule *mod);

/* Keep in sync with gphoto2.c */
struct _GPContext {
	struct PakModule *mod;
	int job;
};

/* The runtime caches file metadata (pak_rt_add_file_metadata /
 * pak_rt_get_metadata) and upstream caches PTPObjectInfo (ptp_object_want).
 * The module only keeps the irreducible map: (storage_name, path) -> PTP
 * handle array, so index_in_view resolves to an object handle. */
#define MAX_FOLDERS 64

struct FolderCache {
	char storage_name[32];
	char path[256];		/* "" = storage root */
	uint32_t storage_id;
	uint32_t handle;	/* association handle, 0 for root */
	uint32_t *handles;	/* file handles in enumeration order */
	unsigned int n_handles;
};

struct ModulePriv {
	GPContext *context;
	Camera *camera;
	struct FolderCache *folders;
	int n_folders;
};

struct FileEntry {
	char *name;
	CameraFileInfo info;
};

static void drop_camera(struct PakModule *mod) {
	struct ModulePriv *priv = mod->priv;
	Camera *camera = priv->camera;

	if (!camera)
		return;
	priv->camera = NULL;
	/* camera_exit closes the PTP session + PTPIP sockets and frees
	 * camera->pl (incl. params). */
	if (camera->functions && camera->functions->exit)
		camera->functions->exit(camera, priv->context);
	gp_free_camera(camera);
}

static Camera *alloc_camera(struct PakModule *mod) {
	struct ModulePriv *priv = mod->priv;
	Camera *camera = gp_alloc_camera(mod);

	if (!camera)
		return NULL;
	priv->camera = camera;
	return camera;
}

static int init(struct PakModule *mod) {
	gp_set_module(mod);
	mod->priv = calloc(sizeof(struct ModulePriv), 1);
	mod->priv->context = calloc(sizeof(struct _GPContext), 1);
	mod->priv->context->mod = mod;

	pak_debug_log(mod, "ptp2-gphoto module init");
	pak_rt_set_screen_supported(mod, PAK_SCREEN_DASHBOARD, 1);
	pak_rt_set_screen_supported(mod, PAK_SCREEN_FILE_GALLERY, 1);
	pak_rt_set_screen_supported(mod, PAK_SCREEN_FILE_VIEWER, 1);
	pak_rt_set_tick_interval(mod, 1000 * 1000);
	return 0;
}

static int on_free(struct PakModule *mod) {
	struct ModulePriv *priv = mod->priv;

	if (!priv)
		return 0;
	drop_camera(mod);
	free_folders(mod);
	free(priv->folders);
	free(priv->context);
	free(priv);
	mod->priv = NULL;
	return 0;
}

/* Shared tail for both transports: runs camera_init (PTP handshake +
 * GetDeviceInfo), then reports the session props and signature. */
static int finish_connect(struct PakModule *mod, Camera *camera, int job) {
	struct ModulePriv *priv = mod->priv;
	PTPParams *params;
	int rc;

	(void)job;
	rc = camera_init(camera, priv->context);
	if (rc != GP_OK) {
		pak_debug_log(mod, "ptp2: camera_init failed: %s", gp_result_as_string(rc));
		return rc;
	}

	params = &camera->pl->params;
	if (params->deviceinfo.Model) {
		pak_debug_log(mod, "ptp2: connected to %s", params->deviceinfo.Model);
		pak_rt_set_session_property(mod, PAK_PROP_NAME, params->deviceinfo.Model);
	}
	if (params->deviceinfo.DeviceVersion)
		pak_rt_set_session_property(mod, PAK_PROP_FW_VER, params->deviceinfo.DeviceVersion);

	const char *id = params->deviceinfo.SerialNumber ? params->deviceinfo.SerialNumber :
			 (params->deviceinfo.Model ? params->deviceinfo.Model : "ptp2");
	struct PakSavedConnection sig = {};
	sig.unique_id = id;
	sig.name = params->deviceinfo.Model ? params->deviceinfo.Model : id;
	if (pak_rt_save_session_signature(mod, &sig) != 0)
		pak_debug_log(mod, "ptp2: saving session signature failed");

	return 0;
}

static int on_try_connect_wifi(struct PakModule *mod, struct PakWiFiAdapter *handle,
			       struct PakSavedConnection *saved, int job) {
	struct ModulePriv *priv = mod->priv;
	Camera *camera;
	int rc;

	(void)handle;
	(void)saved;
	priv->context->job = job;
	drop_camera(mod);

	camera = alloc_camera(mod);
	if (!camera) {
		pak_debug_log(mod, "ptp2: out of memory");
		return PAK_ERR_NO_CONNECTION;
	}

	gp_port_set_path(camera->port, CANON_PTPIP);
	pak_debug_log(mod, "ptp2: connecting to %s", CANON_PTPIP);
	rc = finish_connect(mod, camera, job);
	if (rc != GP_OK) {
		drop_camera(mod);
		return PAK_ERR_NO_CONNECTION;
	}
	return 0;
}

static int on_try_connect_usb(struct PakModule *mod, libusb_device *dev,
			      struct PakSavedConnection *saved, int job) {
	struct ModulePriv *priv = mod->priv;
	struct libusb_device_descriptor desc;
	CameraAbilities abilities;
	Camera *camera;
	int rc;

	(void)saved;
	priv->context->job = job;
	drop_camera(mod);

	camera = alloc_camera(mod);
	if (!camera) {
		pak_debug_log(mod, "ptp2: out of memory");
		return PAK_ERR_NO_CONNECTION;
	}
	camera->port->type = GP_PORT_USB;

	if (libusb_get_device_descriptor(dev, &desc) != 0) {
		pak_debug_log(mod, "ptp2: failed to read USB device descriptor");
		drop_camera(mod);
		return PAK_ERR_NO_CONNECTION;
	}
	pak_debug_log(mod, "ptp2: connecting to USB device %04x:%04x", desc.idVendor, desc.idProduct);

	rc = gp_port_usb_setup(camera->port, dev);
	if (rc != GP_OK) {
		pak_debug_log(mod, "ptp2: USB setup failed: %s", gp_result_as_string(rc));
		drop_camera(mod);
		return PAK_ERR_NO_CONNECTION;
	}

	/* vid/pid drive camera_init's models[] lookup for device_flags;
	 * the model string stays empty (only used for the Fuji check). */
	memset(&abilities, 0, sizeof(abilities));
	abilities.usb_vendor = desc.idVendor;
	abilities.usb_product = desc.idProduct;
	gp_camera_set_abilities(camera, abilities);

	rc = finish_connect(mod, camera, job);
	if (rc != GP_OK) {
		drop_camera(mod);
		return PAK_ERR_NO_CONNECTION;
	}
	return 0;
}

static int on_disconnect(struct PakModule *mod) {
	drop_camera(mod);
	return 0;
}

/* ============================== gallery ============================== */

static struct FolderCache *find_folder(struct PakModule *mod, const char *storage_name,
				       const char *path) {
	struct ModulePriv *priv = mod->priv;
	int i;

	if (!priv->folders)
		return NULL;
	if (!path)
		path = "";
	for (i = 0; i < priv->n_folders; i++) {
		if (!strcmp(priv->folders[i].storage_name, storage_name ? storage_name : "") &&
		    !strcmp(priv->folders[i].path, path))
			return &priv->folders[i];
	}
	return NULL;
}

/* Resolves a PakFileHandle to its PTP object handle, then to the cached
 * PTPObject (one GetObjectInfo on first touch, cached by ptp_object_want). */
static PTPObject *resolve_handle(struct PakModule *mod, struct PakFileHandle *file) {
	PTPParams *params = &mod->priv->camera->pl->params;
	struct FolderCache *fc;
	PTPObject *ob = NULL;
	uint32_t handle;

	fc = find_folder(mod, file->storage_name, file->path);
	if (!fc || file->index_in_view < 0 || (unsigned int)file->index_in_view >= fc->n_handles)
		return NULL;
	handle = fc->handles[file->index_in_view];
	if (ptp_object_want(params, handle, PTPOBJECT_OBJECTINFO_LOADED, &ob) != PTP_RC_OK)
		return NULL;
	return ob;
}

static const char *mime_for_format(uint16_t ofc) {
	switch (ofc) {
	case PTP_OFC_EXIF_JPEG:	return GP_MIME_JPEG;
	case PTP_OFC_PNG:	return GP_MIME_PNG;
	case PTP_OFC_DNG:	return GP_MIME_DNG;
	case PTP_OFC_AVI:	return GP_MIME_AVI;
	case PTP_OFC_MPEG:	return GP_MIME_MPEG;
	case PTP_OFC_WAV:	return GP_MIME_WAV;
	case PTP_OFC_MP3:	return GP_MIME_MP3;
	case PTP_OFC_TIFF:	return GP_MIME_TIFF;
	case PTP_OFC_CANON_CRW:	return GP_MIME_CRW;
	case PTP_OFC_CANON_CRW3:return GP_MIME_CR2;
	case PTP_OFC_CANON_CR3:	return GP_MIME_CR3;
	case PTP_OFC_CANON_MOV:	return GP_MIME_QUICKTIME;
	default:		return GP_MIME_UNKNOWN;
	}
}

/* Recursively scans one storage level via ptp_list_folder (upstream's
 * dispatcher: Canon EOS bulk GetObjectInfoEx fast path on EOS, cached
 * directory listings afterwards; generic devices do GetObjectHandles +
 * per-child GetObjectInfo here). Fills the handle array for later index
 * resolution and declares folders to the runtime. */
static int scan_folder(struct PakModule *mod, struct FolderCache *fc, int job) {
	struct ModulePriv *priv = mod->priv;
	PTPParams *params = &mod->priv->camera->pl->params;
	PTPObjectHandles children = {0};
	PTPObject *ob = NULL;
	unsigned int i, n_files = 0;
	int rc;

	if (pak_rt_is_job_cancelled(mod, job))
		return GP_ERROR_CANCEL;
	rc = ptp_list_folder(params, fc->storage_id, fc->handle, &children);
	if (rc != PTP_RC_OK)
		return GP_ERROR_IO;

	fc->handles = calloc(children.len ? children.len : 1, sizeof(uint32_t));
	if (!fc->handles) {
		free(children.val);
		return GP_ERROR_NO_MEMORY;
	}
	for (i = 0; i < children.len; i++) {
		if (ptp_object_want(params, children.val[i],
				    PTPOBJECT_OBJECTINFO_LOADED | PTPOBJECT_PARENTOBJECT_LOADED,
				    &ob) != PTP_RC_OK || !ob)
			continue;
		if (ob->oi.ObjectFormat == PTP_OFC_Association)
			continue; /* folders recursed below */
		fc->handles[n_files++] = children.val[i];
	}
	fc->n_handles = n_files;

	if (fc->path[0])
		pak_rt_add_folder_info(mod, fc->storage_name, fc->path, fc->n_handles, PAK_DEFAULT);

	/* Recurse into subfolders; their names are already in the cache. */
	for (i = 0; i < children.len; i++) {
		struct FolderCache *child;

		if (priv->n_folders >= MAX_FOLDERS)
			break;
		if (ptp_object_want(params, children.val[i],
				    PTPOBJECT_OBJECTINFO_LOADED | PTPOBJECT_PARENTOBJECT_LOADED,
				    &ob) != PTP_RC_OK || !ob || !ob->oi.Filename)
			continue;
		if (ob->oi.ObjectFormat != PTP_OFC_Association)
			continue;
		child = &priv->folders[priv->n_folders++];
		memset(child, 0, sizeof(*child));
		snprintf(child->storage_name, sizeof(child->storage_name), "%s", fc->storage_name);
		snprintf(child->path, sizeof(child->path), "%s%s%s", fc->path,
			 fc->path[0] ? "/" : "", ob->oi.Filename);
		child->storage_id = fc->storage_id;
		child->handle = children.val[i];
		scan_folder(mod, child, job);
	}
	free(children.val);
	return GP_OK;
}

static void free_folders(struct PakModule *mod) {
	struct ModulePriv *priv = mod->priv;
	int i;

	for (i = 0; i < priv->n_folders; i++)
		free(priv->folders[i].handles);
	priv->n_folders = 0;
}

/* First gallery entry: declare storages + folders (counts only, cheap),
 * the runtime drives everything else through on_request_* calls. */
static int scan_gallery(struct PakModule *mod, int job) {
	struct ModulePriv *priv = mod->priv;
	Camera *camera = priv->camera;
	CameraFilesystemFuncs *f;
	PTPParams *params;
	PTPObjectHandles sids = {0};
	unsigned int i, total = 0;
	int rc;

	if (!camera)
		return PAK_ERR_UNDEFINED;
	if (priv->n_folders)
		return 0; /* already scanned; invalidated by events later */

	params = &camera->pl->params;
	rc = ptp_getstorageids(params, &sids);
	if (rc != PTP_RC_OK)
		return GP_ERROR_IO;

	priv->folders = calloc(MAX_FOLDERS, sizeof(struct FolderCache));
	if (!priv->folders) {
		free(sids.val);
		return GP_ERROR_NO_MEMORY;
	}

	for (i = 0; i < sids.len && priv->n_folders < MAX_FOLDERS; i++) {
		struct FolderCache *root = &priv->folders[priv->n_folders++];
		char sname[32];
		CameraStorageInformation *sinfos = NULL;
		int nsinfos = 0, j;
		unsigned int n_files = 0;

		memset(root, 0, sizeof(*root));
		root->storage_id = sids.val[i];
		snprintf(sname, sizeof(sname), "store_%08x", sids.val[i]);
		snprintf(root->storage_name, sizeof(root->storage_name), "%s", sname);
		rc = scan_folder(mod, root, job);
		if (rc != GP_OK)
			pak_debug_log(mod, "ptp2: storage %08x scan failed: %s",
				      sids.val[i], gp_result_as_string(rc));

		for (j = 0; j < priv->n_folders; j++)
			if (priv->folders[j].storage_id == sids.val[i])
				n_files += priv->folders[j].n_handles;

		struct PakStorageInfo si = {};
		si.n_files_total = n_files;
		si.sorted_by = PAK_DEFAULT;
		si.is_live = 0;
		f = gp_fs_funcs(camera->fs);
		if (f && f->storage_info_func &&
		    f->storage_info_func(camera->fs, &sinfos, &nsinfos, camera, priv->context) == GP_OK &&
		    nsinfos > 0) {
			if (sinfos[0].fields & GP_STORAGEINFO_MAXCAPACITY)
				si.size_bytes = sinfos[0].capacitykbytes * 1024;
			if (sinfos[0].fields & GP_STORAGEINFO_FREESPACEKBYTES &&
			    sinfos[0].capacitykbytes >= sinfos[0].freekbytes)
				si.used_bytes = (sinfos[0].capacitykbytes - sinfos[0].freekbytes) * 1024;
		}
		pak_rt_set_storage_info(mod, sname, &si);
		total += n_files;
	}
	free(sids.val);
	pak_debug_log(mod, "ptp2: gallery scan done (%u files, %d folders)", total, priv->n_folders);
	return 0;
}

static int on_switch_screen(struct PakModule *mod, int old_screen, int new_screen, int job) {
	(void)old_screen;
	if (new_screen == PAK_SCREEN_FILE_GALLERY)
		return scan_gallery(mod, job);
	return 0;
}

/* ============================== file requests ============================== */

static int build_metadata(struct PakModule *mod, struct PakFileHandle *file,
			  struct PakFileMetadata *md) {
	PTPObject *ob = resolve_handle(mod, file);

	if (!ob)
		return GP_ERROR;
	memset(md, 0, sizeof(*md));
	md->filename = ob->oi.Filename;
	md->mime_type = mime_for_format(ob->oi.ObjectFormat);
	md->file_size = ob->oi.ObjectSize;
	md->image_width = ob->oi.ImagePixWidth;
	md->image_height = ob->oi.ImagePixHeight;
	return GP_OK;
}

static int on_request_file_metadata(struct PakModule *mod, int job, struct PakFileHandle *file) {
	struct PakFileMetadata md;

	(void)job;
	if (build_metadata(mod, file, &md) != GP_OK)
		return pak_rt_add_file_metadata(mod, file, NULL);
	return pak_rt_add_file_metadata(mod, file, &md);
}

/* Resolves the handle to a gp folder path + filename and runs the camlib's
 * get_file_func (self-warms cold folders internally via ptp_list_folder). */
static int get_file_via_fs(struct PakModule *mod, struct PakFileHandle *file,
			   CameraFileType type, CameraFile *cf) {
	struct ModulePriv *priv = mod->priv;
	CameraFilesystemFuncs *f;
	struct FolderCache *fc;
	PTPObject *ob;
	char gpfolder[512];

	f = gp_fs_funcs(priv->camera->fs);
	if (!f || !f->get_file_func)
		return GP_ERROR_NOT_SUPPORTED;
	fc = find_folder(mod, file->storage_name, file->path);
	if (!fc)
		return GP_ERROR;
	ob = resolve_handle(mod, file);
	if (!ob || !ob->oi.Filename)
		return GP_ERROR;
	snprintf(gpfolder, sizeof(gpfolder), "/%s%s%s", fc->storage_name,
		 fc->path[0] ? "/" : "", fc->path);
	return f->get_file_func(priv->camera->fs, gpfolder, ob->oi.Filename, type,
				cf, priv->camera, priv->context);
}

static int on_request_file_thumbnail(struct PakModule *mod, int job, struct PakFileHandle *file) {
	struct ModulePriv *priv = mod->priv;
	CameraFile *cf = NULL;
	const char *data;
	unsigned long int size;
	int rc;

	priv->context->job = job;
	gp_file_new(&cf);
	rc = get_file_via_fs(mod, file, GP_FILE_TYPE_PREVIEW, cf);
	if (rc != GP_OK) {
		pak_debug_log(mod, "ptp2: get thumbnail failed: %s", gp_result_as_string(rc));
		pak_rt_add_file_thumbnail(mod, file, NULL, 0);
	} else {
		gp_file_get_data_and_size(cf, &data, &size);
		if (data && size)
			pak_rt_add_file_thumbnail(mod, file, (void *)data, size);
		else
			pak_rt_add_file_thumbnail(mod, file, NULL, 0);
	}
	gp_file_free(cf);
	return 0;
}

static int on_request_file_contents(struct PakModule *mod, int job, struct PakFileHandle *file) {
	struct ModulePriv *priv = mod->priv;
	CameraFile *cf = NULL;
	const char *data;
	unsigned long int size;
	int rc;

	priv->context->job = job;
	gp_file_new(&cf);
	rc = get_file_via_fs(mod, file, GP_FILE_TYPE_NORMAL, cf);
	if (rc != GP_OK) {
		pak_debug_log(mod, "ptp2: get file failed: %s", gp_result_as_string(rc));
	} else {
		gp_file_get_data_and_size(cf, &data, &size);
		if (data && size)
			pak_rt_add_file_contents(mod, file, (void *)data, size, 0, size);
	}
	gp_file_free(cf);
	return 0;
}


/* ============================== stubs ============================== */

static int on_idle_tick(struct PakModule *mod, unsigned int us_since_last_tick) {
	(void)mod;
	(void)us_since_last_tick;
	return 0;
}

static int on_custom_command(struct PakModule *mod, int job, int argc, const char * const *argv) {
	(void)mod;
	(void)job;
	(void)argc;
	(void)argv;
	return 0;
}

static int on_prop_changed(struct PakModule *mod, int job, const char *name, struct PakWidget *prop) {
	(void)mod;
	(void)job;
	(void)name;
	(void)prop;
	return 0;
}

static int on_run_test(struct PakModule *mod, int job) {
	(void)mod;
	(void)job;
	/* Keep the headless CLI (no device) green: nothing to test yet. */
	return 0;
}

int get_module(struct PakModule *mod) {
	mod->init = init;
	mod->free = on_free;
	mod->on_try_connect_wifi = on_try_connect_wifi;
	mod->on_try_connect_usb = on_try_connect_usb;
	mod->on_idle_tick = on_idle_tick;
	mod->on_disconnect = on_disconnect;
	mod->on_switch_screen = on_switch_screen;
	mod->on_request_file_contents = on_request_file_contents;
	mod->on_request_file_thumbnail = on_request_file_thumbnail;
	mod->on_request_file_metadata = on_request_file_metadata;
	mod->on_custom_command = on_custom_command;
	mod->on_widget_updated = on_prop_changed;
	mod->on_run_test = on_run_test;
	return 0;
}
