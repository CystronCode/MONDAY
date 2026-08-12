// monday/rag/tier_selector.h
//
// Automatically manages MRL vs DiskANN index tier transitions (Architecture §2.6).

#pragma once

#include "monday/rag/IVectorIndex.h"
#include "monday/storage/db.h"
#include <memory>
#include <string>
#include <mutex>

namespace monday {

enum class IndexTier {
    MRL,
    DISKANN
};

class TierSelector {
public:
    TierSelector(const std::string &diskann_path, monday_db_t *db, size_t threshold = 500000);
    ~TierSelector();

    // Returns a pointer to the currently active index. Rebuilds if the tier changed.
    Result<IVectorIndex*> get_active_index();

    // Re-evaluates database size and changes index tiers if boundary crossed.
    Result<void> evaluate_and_update();

    // Retrieve active index tier.
    IndexTier get_current_tier() const { return current_tier_; }

private:
    Result<void> rebuild_index_from_db(IVectorIndex *index);

    std::string                   diskann_path_;
    monday_db_t                  *db_;
    size_t                        threshold_;
    IndexTier                     current_tier_;
    std::unique_ptr<IVectorIndex> active_index_;
    std::mutex                    mutex_;
};

} // namespace monday
