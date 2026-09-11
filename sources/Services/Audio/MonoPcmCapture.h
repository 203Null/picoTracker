/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

struct MonoPcmCaptureStats {
  std::size_t frames = 0;
  std::uint16_t peak = 0;
  std::uint32_t clipped = 0;
  std::uint32_t invalid = 0;
};

// Convert one complete input block without allocating or imposing a callback
// size. Output capacity is not input length: a successful short render must
// not insert zero samples between otherwise contiguous input blocks.
inline MonoPcmCaptureStats
CopyMonoPcmCapture(std::span<const float> input,
                   std::span<std::int16_t> output) noexcept {
  MonoPcmCaptureStats result;
  result.frames = std::min(input.size(), output.size());
  for (std::size_t index = 0; index < result.frames; ++index) {
    float value = input[index];
    if (!std::isfinite(value)) {
      value = 0.0F;
      ++result.invalid;
    }
    if (value <= -1.0F || value >= 1.0F)
      ++result.clipped;
    value = std::clamp(value, -1.0F, 1.0F);
    const auto sample = static_cast<std::int16_t>(
        std::lrint(value * (value < 0.0F ? 32768.0F : 32767.0F)));
    output[index] = sample;
    const auto magnitude = static_cast<std::uint16_t>(
        sample == INT16_MIN ? INT16_MAX : std::abs(sample));
    result.peak = std::max(result.peak, magnitude);
  }
  return result;
}
