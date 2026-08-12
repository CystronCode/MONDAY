// monday/engram/summariser.h
//
// Background episodic and semantic summarisation pipeline.

#pragma once

#include "monday/storage/db.h"
#include "monday/inference/IInferenceEngine.h"
#include "monday/core/errors.h"

namespace monday {

// Starts the consolidation process. Iterates over unsummarized episodic entries,
// templates the consolidate prompt, and executes inference on the primary engine.
// Can be aborted asynchronously by calling monday_summariser_cancel().
monday_error_t monday_summariser_consolidate(IInferenceEngine *engine, monday_db_t *db);

// Asynchronously requests cancellation of active summarization.
void monday_summariser_cancel(void);

} // namespace monday
