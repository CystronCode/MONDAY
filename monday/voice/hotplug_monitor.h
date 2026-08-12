// monday/voice/hotplug_monitor.h
//
// 2-second polling loop for audio device hotplug detection and auto-reconnection.

#pragma once

#include <stdbool.h>
#include "monday/core/errors.h"
#include "monday/voice/audio_io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_hotplug_monitor monday_hotplug_monitor_t;

/* Initialize hotplug monitor for an audio stream. */
monday_error_t monday_hotplug_monitor_init(monday_hotplug_monitor_t **out_monitor,
                                           monday_audio_stream_t *stream);

/* Start the 2-second background polling loop. */
monday_error_t monday_hotplug_monitor_start(monday_hotplug_monitor_t *monitor);

/* Stop the background polling loop. */
monday_error_t monday_hotplug_monitor_stop(monday_hotplug_monitor_t *monitor);

/* Check if the audio device is currently connected and active. */
bool monday_hotplug_monitor_is_connected(monday_hotplug_monitor_t *monitor);

/* Shutdown hotplug monitor. */
void monday_hotplug_monitor_shutdown(monday_hotplug_monitor_t *monitor);

#ifdef __cplusplus
}
#endif
