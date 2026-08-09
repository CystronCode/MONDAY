// monday/core/result.h
#pragma once
#include "monday/core/errors.h"
#include <tl/expected.hpp>

namespace monday {
    template<typename T>
    using Result = tl::expected<T, monday_error_t>;
} // namespace monday
