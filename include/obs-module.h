#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
#define MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#define MODULE_EXTERN extern "C"
#define OBS_EXTERN_C_BEGIN extern "C" {
#define OBS_EXTERN_C_END }
#else
#define MODULE_EXPORT __attribute__((visibility("default")))
#define MODULE_EXTERN extern
#define OBS_EXTERN_C_BEGIN
#define OBS_EXTERN_C_END
#endif

#define MAKE_SEMANTIC_VERSION(major, minor, patch) \
	(((major) << 24) | ((minor) << 16) | (patch))

/* OBS 31.1.2 reports libobs compatibility version 31.0.0. */
#define LIBOBS_API_VER MAKE_SEMANTIC_VERSION(31, 0, 0)

typedef struct obs_module obs_module_t;
typedef struct obs_data obs_data_t;
typedef struct obs_scene obs_scene_t;
typedef struct obs_sceneitem obs_sceneitem_t;
typedef struct obs_source obs_source_t;
typedef struct signal_handler signal_handler_t;
typedef struct calldata calldata_t;
typedef struct vec2 {
	float x;
	float y;
} vec2;
typedef struct obs_sceneitem_crop {
	int left;
	int top;
	int right;
	int bottom;
} obs_sceneitem_crop;

enum obs_bounds_type {
	OBS_BOUNDS_NONE,
	OBS_BOUNDS_STRETCH,
	OBS_BOUNDS_SCALE_INNER,
	OBS_BOUNDS_SCALE_OUTER,
	OBS_BOUNDS_SCALE_TO_WIDTH,
	OBS_BOUNDS_SCALE_TO_HEIGHT,
	OBS_BOUNDS_MAX_ONLY,
};

enum obs_scale_type {
	OBS_SCALE_DISABLE,
	OBS_SCALE_POINT,
	OBS_SCALE_BICUBIC,
	OBS_SCALE_BILINEAR,
	OBS_SCALE_LANCZOS,
};

enum obs_align {
	OBS_ALIGN_CENTER = 1 << 0,
	OBS_ALIGN_LEFT = 1 << 1,
	OBS_ALIGN_RIGHT = 1 << 2,
	OBS_ALIGN_TOP = 1 << 3,
	OBS_ALIGN_BOTTOM = 1 << 4,
};

typedef void (*signal_callback_t)(void *data, calldata_t *calldata);

typedef struct obs_transform_info {
	vec2 pos;
	float rot;
	vec2 scale;
	uint32_t alignment;
	enum obs_bounds_type bounds_type;
	uint32_t bounds_alignment;
	vec2 bounds;
	bool crop_to_bounds;
} obs_transform_info;

MODULE_EXPORT void obs_module_set_pointer(obs_module_t *module);
MODULE_EXTERN obs_module_t *obs_current_module(void);
MODULE_EXPORT uint32_t obs_module_ver(void);
MODULE_EXPORT bool obs_module_load(void);
MODULE_EXPORT void obs_module_unload(void);
MODULE_EXPORT void obs_module_post_load(void);

#define OBS_DECLARE_MODULE() \
	static obs_module_t *obs_module_pointer; \
	MODULE_EXPORT void obs_module_set_pointer(obs_module_t *module); \
	void obs_module_set_pointer(obs_module_t *module) \
	{ \
		obs_module_pointer = module; \
	} \
	obs_module_t *obs_current_module(void) \
	{ \
		return obs_module_pointer; \
	} \
	MODULE_EXPORT uint32_t obs_module_ver(void); \
	uint32_t obs_module_ver(void) \
	{ \
		return LIBOBS_API_VER; \
	}

/* Minimal logging surface for the current scaffold. */
#define LOG_INFO 100
#define LOG_WARNING 200

OBS_EXTERN_C_BEGIN
void blog(int log_level, const char *format, ...);
obs_data_t *obs_data_create(void);
void obs_data_release(obs_data_t *data);
void obs_data_set_string(obs_data_t *data, const char *name, const char *val);

obs_source_t *obs_get_source_by_name(const char *name);
obs_source_t *obs_source_create(const char *id, const char *name, obs_data_t *settings, obs_data_t *hotkey_data);
obs_source_t *obs_source_get_ref(obs_source_t *source);
void obs_source_release(obs_source_t *source);
uint32_t obs_source_get_width(obs_source_t *source);
uint32_t obs_source_get_height(obs_source_t *source);
signal_handler_t *obs_source_get_signal_handler(obs_source_t *source);
void obs_source_update(obs_source_t *source, obs_data_t *settings);
obs_data_t *obs_source_get_settings(obs_source_t *source);
const char *obs_source_get_name(obs_source_t *source);

obs_scene_t *obs_scene_from_source(obs_source_t *source);
void obs_scene_release(obs_scene_t *scene);
obs_sceneitem_t *obs_scene_find_source(obs_scene_t *scene, const char *name);
obs_sceneitem_t *obs_scene_add(obs_scene_t *scene, obs_source_t *source);
void obs_sceneitem_set_visible(obs_sceneitem_t *item, bool visible);
void obs_sceneitem_get_bounds(const obs_sceneitem_t *item, vec2 *bounds);
void obs_sceneitem_set_bounds_type(obs_sceneitem_t *item, enum obs_bounds_type type);
void obs_sceneitem_set_bounds_alignment(obs_sceneitem_t *item, uint32_t alignment);
void obs_sceneitem_set_scale_filter(obs_sceneitem_t *item, enum obs_scale_type filter);
void obs_sceneitem_set_bounds(obs_sceneitem_t *item, const vec2 *bounds);
void obs_sceneitem_set_crop(obs_sceneitem_t *item, const obs_sceneitem_crop *crop);
void obs_sceneitem_get_info2(const obs_sceneitem_t *item, obs_transform_info *info);
void obs_sceneitem_set_info2(obs_sceneitem_t *item, const obs_transform_info *info);

void signal_handler_connect(signal_handler_t *handler, const char *signal, signal_callback_t callback, void *data);
void signal_handler_disconnect(signal_handler_t *handler, const char *signal, signal_callback_t callback, void *data);
OBS_EXTERN_C_END
