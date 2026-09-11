/* SPDX-License-Identifier: BSD-3-Clause */
#include "Services/Audio/MonoPcmCapture.h"
#include "doctest/doctest.h"

#include <array>
#include <limits>
#include <vector>

TEST_CASE("Mono capture preserves large callbacks across arbitrary block boundaries") {
  std::vector<float> input(16384);
  for (std::size_t index = 0; index < input.size(); ++index)
    input[index] = 0.8F * std::sin(static_cast<float>(index) * 0.13F);
  std::vector<std::int16_t> whole(input.size()), partitioned(input.size());
  const auto wholeStats = CopyMonoPcmCapture(input, whole);

  std::size_t offset = 0;
  std::uint16_t peak = 0;
  for (const std::size_t block : {128U, 4096U, 8192U, 3968U}) {
    const auto stats = CopyMonoPcmCapture(
        std::span(input).subspan(offset, block),
        std::span(partitioned).subspan(offset, block));
    peak = std::max(peak, stats.peak);
    CHECK(stats.frames == block);
    CHECK(stats.clipped == 0U);
    offset += block;
  }
  REQUIRE(offset == input.size());
  CHECK(partitioned == whole);
  CHECK(peak == wholeStats.peak);
  CHECK(whole[8192] != 0);
}

TEST_CASE("Mono capture writes only valid input frames within destination bounds") {
  std::array<std::int16_t, 10> recording;
  recording.fill(1234);
  const std::array<float, 2> first{1.0F, -1.0F};
  CHECK(CopyMonoPcmCapture(first, std::span(recording).subspan(1, 4)).frames == 2);
  CHECK(CopyMonoPcmCapture({}, std::span(recording).subspan(5, 2)).frames == 0);
  const std::array<float, 4> next{0.5F, -0.5F, 1.0F, 1.0F};
  CHECK(CopyMonoPcmCapture(next, std::span(recording).subspan(7, 2)).frames == 2);
  CHECK(recording == std::array<std::int16_t, 10>{
      1234, 32767, -32768, 1234, 1234, 1234, 1234, 16384, -16384, 1234});
}

TEST_CASE("Mono capture concatenates successful short renders without periodic zero clicks") {
  std::array<std::int16_t, 16384> output{};
  std::array<float, 1015> input{};
  input.fill(0.5F);
  std::size_t written = 0;
  for (unsigned block = 0; block < 10; ++block) {
    const std::size_t available = block % 3 == 0 ? 1014 : 1015;
    const auto stats = CopyMonoPcmCapture(std::span(input).first(available),
                                         std::span(output).subspan(written, 1015));
    written += stats.frames;
  }
  CHECK(written == 10146);
  for (std::size_t i = 0; i < written; ++i)
    CHECK(output[i] == 16384);
  CHECK(output[written] == 0);
}

TEST_CASE("Mono capture contains invalid floats and reports clipped samples") {
  const std::array<float, 8> input{
      -2.0F, -1.0F, 0.0F, 1.0F, 2.0F,
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::infinity(),
      -std::numeric_limits<float>::infinity()};
  std::array<std::int16_t, 8> output{};
  const auto stats = CopyMonoPcmCapture(input, output);
  CHECK(output == std::array<std::int16_t, 8>{
      -32768, -32768, 0, 32767, 32767, 0, 0, 0});
  CHECK(stats.peak == 32767);
  CHECK(stats.clipped == 4);
  CHECK(stats.invalid == 3);
}
