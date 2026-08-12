// monday/voice/audio_io.cpp
//
// PortAudio capture abstraction layer implementation with lock-free SPSC ring buffer.

#include "monday/voice/audio_io.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <cstring>
#include <cstdlib>
#include <atomic>

#ifdef MONDAY_HAVE_PORTAUDIO
#include <portaudio.h>
#endif

struct monday_audio_stream {
    monday_audio_config_t     config;
    monday_spsc_ring_t        ring;
    monday_bandpass_filter_t  filter;
    std::atomic<bool>         active;
    std::atomic<bool>         device_lost;
#ifdef MONDAY_HAVE_PORTAUDIO
    PaStream*                 pa_stream;
#endif
};

#ifdef MONDAY_HAVE_PORTAUDIO
static int pa_audio_callback(const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData) {
    (void)outputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    monday_audio_stream_t *stream = (monday_audio_stream_t *)userData;
    if (!stream || !stream->active.load(std::memory_order_relaxed)) {
        return paContinue;
    }

    const int16_t *in = (const int16_t *)inputBuffer;
    if (in) {
        for (unsigned long i = 0; i < framesPerBuffer; i++) {
            int16_t sample = in[i];
            if (stream->config.enable_filter) {
                sample = monday_bandpass_filter_process_sample(&stream->filter, sample);
            }
            monday_spsc_ring_push(&stream->ring, &sample);
        }
    }

    return paContinue;
}
#endif

monday_error_t monday_audio_init(monday_audio_stream_t **out_stream,
                                 const monday_audio_config_t *config) {
    if (!out_stream || !config) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null argument to audio init");
    }

    monday_audio_stream_t *stream = (monday_audio_stream_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_audio_stream_t));
    if (!stream) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to allocate audio stream");
    }

    std::memset(stream, 0, sizeof(monday_audio_stream_t));
    stream->config = *config;
    if (stream->config.sample_rate == 0) stream->config.sample_rate = 16000;
    if (stream->config.frame_size == 0) stream->config.frame_size = 512;
    if (stream->config.ring_capacity == 0) stream->config.ring_capacity = 8192;

    monday_error_t err = monday_spsc_ring_init(&stream->ring, stream->config.ring_capacity, sizeof(int16_t));
    if (err.code != MONDAY_OK) {
        return err;
    }

    if (stream->config.enable_filter) {
        monday_bandpass_filter_init(&stream->filter, stream->config.sample_rate);
    }

    stream->active.store(false);
    stream->device_lost.store(false);

#ifdef MONDAY_HAVE_PORTAUDIO
    PaError pa_err = Pa_Initialize();
    if (pa_err != paNoError) {
        monday_log_write(MONDAY_LOG_SYSTEM, "Pa_Initialize failed");
        return MONDAY_ERROR(MONDAY_ERR_AUDIO_DEVICE_LOST, "portaudio init failed");
    }

    PaStreamParameters inputParameters;
    std::memset(&inputParameters, 0, sizeof(inputParameters));
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        monday_log_write(MONDAY_LOG_SYSTEM, "No default audio input device found");
        stream->device_lost.store(true);
    } else {
        inputParameters.channelCount = 1;
        inputParameters.sampleFormat = paInt16;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = NULL;

        pa_err = Pa_OpenStream(&stream->pa_stream,
                               &inputParameters,
                               NULL,
                               stream->config.sample_rate,
                               stream->config.frame_size,
                               paClipOff,
                               pa_audio_callback,
                               stream);
        if (pa_err != paNoError) {
            stream->device_lost.store(true);
        }
    }
#endif

    *out_stream = stream;
    return MONDAY_ERROR(MONDAY_OK, "audio init complete");
}

monday_error_t monday_audio_start(monday_audio_stream_t *stream) {
    if (!stream) return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null stream");
    stream->active.store(true);

#ifdef MONDAY_HAVE_PORTAUDIO
    if (stream->pa_stream) {
        Pa_StartStream(stream->pa_stream);
    }
#endif
    return MONDAY_ERROR(MONDAY_OK, "audio start success");
}

monday_error_t monday_audio_stop(monday_audio_stream_t *stream) {
    if (!stream) return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null stream");
    stream->active.store(false);

#ifdef MONDAY_HAVE_PORTAUDIO
    if (stream->pa_stream) {
        Pa_StopStream(stream->pa_stream);
    }
#endif
    return MONDAY_ERROR(MONDAY_OK, "audio stop success");
}

monday_error_t monday_audio_read(monday_audio_stream_t *stream,
                                 int16_t *out_samples,
                                 size_t sample_count) {
    if (!stream || !out_samples || sample_count == 0) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid audio read parameters");
    }

    if (stream->device_lost.load(std::memory_order_relaxed)) {
        return MONDAY_ERROR(MONDAY_ERR_AUDIO_DEVICE_LOST, "audio device unavailable");
    }

    size_t read_count = 0;
    while (read_count < sample_count) {
        int16_t s;
        monday_error_t err = monday_spsc_ring_pop(&stream->ring, &s);
        if (err.code != MONDAY_OK) {
            break;
        }
        out_samples[read_count++] = s;
    }

    if (read_count == 0) {
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "ring empty");
    }

    return MONDAY_ERROR(MONDAY_OK, "audio read success");
}

void monday_audio_shutdown(monday_audio_stream_t *stream) {
    if (!stream) return;
    monday_audio_stop(stream);

#ifdef MONDAY_HAVE_PORTAUDIO
    if (stream->pa_stream) {
        Pa_CloseStream(stream->pa_stream);
        stream->pa_stream = NULL;
    }
    Pa_Terminate();
#endif

    monday_spsc_ring_free(&stream->ring);
}
