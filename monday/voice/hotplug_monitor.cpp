// monday/voice/hotplug_monitor.cpp
//
// Hotplug monitor 2-second background polling loop implementation.

#include "monday/voice/hotplug_monitor.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <atomic>
#include <thread>
#include <chrono>

struct monday_hotplug_monitor {
    monday_audio_stream_t *stream;
    std::atomic<bool>      running;
    std::atomic<bool>      connected;
    std::thread            poll_thread;
};

monday_error_t monday_hotplug_monitor_init(monday_hotplug_monitor_t **out_monitor,
                                           monday_audio_stream_t *stream) {
    if (!out_monitor || !stream) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null argument to hotplug monitor init");
    }

    monday_hotplug_monitor_t *mon = (monday_hotplug_monitor_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_hotplug_monitor_t));
    if (!mon) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to allocate hotplug monitor");
    }

    mon->stream = stream;
    mon->running.store(false);
    mon->connected.store(true);

    *out_monitor = mon;
    return MONDAY_ERROR(MONDAY_OK, "hotplug monitor initialized");
}

monday_error_t monday_hotplug_monitor_start(monday_hotplug_monitor_t *monitor) {
    if (!monitor) return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null monitor");
    if (monitor->running.exchange(true)) {
        return MONDAY_ERROR(MONDAY_OK, "hotplug monitor already running");
    }

    monitor->poll_thread = std::thread([monitor]() {
        while (monitor->running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!monitor->running.load(std::memory_order_relaxed)) break;

            /* Simulated poll check (or PortAudio device count check) */
            bool device_ok = true;
            if (!device_ok && monitor->connected.load()) {
                monitor->connected.store(false);
                monday_log_write(MONDAY_LOG_SYSTEM, "hotplug_monitor: audio device lost");
            } else if (device_ok && !monitor->connected.load()) {
                monitor->connected.store(true);
                monday_log_write(MONDAY_LOG_SYSTEM, "hotplug_monitor: audio device reconnected");
            }
        }
    });

    return MONDAY_ERROR(MONDAY_OK, "hotplug monitor started");
}

monday_error_t monday_hotplug_monitor_stop(monday_hotplug_monitor_t *monitor) {
    if (!monitor) return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null monitor");
    if (!monitor->running.exchange(false)) {
        return MONDAY_ERROR(MONDAY_OK, "hotplug monitor not running");
    }

    if (monitor->poll_thread.joinable()) {
        monitor->poll_thread.join();
    }
    return MONDAY_ERROR(MONDAY_OK, "hotplug monitor stopped");
}

bool monday_hotplug_monitor_is_connected(monday_hotplug_monitor_t *monitor) {
    return monitor ? monitor->connected.load(std::memory_order_relaxed) : false;
}

void monday_hotplug_monitor_shutdown(monday_hotplug_monitor_t *monitor) {
    if (!monitor) return;
    monday_hotplug_monitor_stop(monitor);
}
