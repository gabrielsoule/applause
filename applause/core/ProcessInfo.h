#pragma once

#include <cstdint>

namespace applause {

/**
 * @brief Audio processing configuration struct passed through activate().
 */
struct ProcessInfo {
    double sample_rate;
    uint32_t min_frame_size;
    uint32_t max_frame_size;
};

}  // namespace applause