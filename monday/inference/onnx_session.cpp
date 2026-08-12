// monday/inference/onnx_session.cpp
//
// Shared ONNX Runtime environment manager implementation.

#include "monday/inference/onnx_session.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <cstring>
#include <string>

#ifdef MONDAY_HAVE_ONNX
#include <onnxruntime_c_api.h>
#endif

struct monday_onnx_env {
    bool inited;
#ifdef MONDAY_HAVE_ONNX
    const OrtApi* api;
    OrtEnv*       ort_env;
#endif
};

struct monday_onnx_session {
    monday_onnx_env_t *env;
    std::string        model_path;
#ifdef MONDAY_HAVE_ONNX
    OrtSession*        ort_session;
#endif
};

monday_error_t monday_onnx_env_init(monday_onnx_env_t **out_env) {
    if (!out_env) return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null out_env");

    monday_onnx_env_t *env = (monday_onnx_env_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_onnx_env_t));
    if (!env) return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "alloc onnx env failed");

    std::memset(env, 0, sizeof(monday_onnx_env_t));
    env->inited = true;

#ifdef MONDAY_HAVE_ONNX
    env->api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (env->api) {
        env->api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "MondayONNX", &env->ort_env);
    }
#endif

    *out_env = env;
    return MONDAY_ERROR(MONDAY_OK, "onnx env initialized");
}

monday_error_t monday_onnx_session_create(monday_onnx_env_t *env,
                                          const char *model_path,
                                          monday_onnx_session_t **out_session) {
    if (!env || !model_path || !out_session) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid session create params");
    }

    monday_onnx_session_t *sess = (monday_onnx_session_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_onnx_session_t));
    if (!sess) return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "alloc session failed");

    sess->env = env;
    sess->model_path = model_path;

    *out_session = sess;
    return MONDAY_ERROR(MONDAY_OK, "onnx session created");
}

void monday_onnx_session_destroy(monday_onnx_session_t *session) {
    if (!session) return;
#ifdef MONDAY_HAVE_ONNX
    if (session->env && session->env->api && session->ort_session) {
        session->env->api->ReleaseSession(session->ort_session);
        session->ort_session = nullptr;
    }
#endif
}

void monday_onnx_env_shutdown(monday_onnx_env_t *env) {
    if (!env) return;
#ifdef MONDAY_HAVE_ONNX
    if (env->api && env->ort_env) {
        env->api->ReleaseEnv(env->ort_env);
        env->ort_env = nullptr;
    }
#endif
    env->inited = false;
}
