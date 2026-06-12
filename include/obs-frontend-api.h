#pragma once

#include <obs-module.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum obs_frontend_event {
	OBS_FRONTEND_EVENT_RECORDING_STARTING = 4,
	OBS_FRONTEND_EVENT_RECORDING_STARTED = 5,
	OBS_FRONTEND_EVENT_RECORDING_STOPPING = 6,
	OBS_FRONTEND_EVENT_RECORDING_STOPPED = 7,
	OBS_FRONTEND_EVENT_RECORDING_PAUSED = 8,
	OBS_FRONTEND_EVENT_RECORDING_UNPAUSED = 9,
	OBS_FRONTEND_EVENT_SCENE_CHANGED = 10,
	OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED = 13,
	OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP = 19,
	OBS_FRONTEND_EVENT_FINISHED_LOADING = 27,
};

typedef void (*obs_frontend_event_cb)(enum obs_frontend_event event, void *private_data);

bool obs_frontend_add_dock_by_id(const char *id, const char *title, void *widget);
void obs_frontend_remove_dock(const char *id);
void obs_frontend_add_event_callback(obs_frontend_event_cb callback, void *private_data);
void obs_frontend_remove_event_callback(obs_frontend_event_cb callback, void *private_data);
void obs_frontend_recording_start(void);
void obs_frontend_recording_stop(void);
bool obs_frontend_recording_active(void);
obs_source_t *obs_frontend_get_current_scene(void);

#ifdef __cplusplus
}
#endif
