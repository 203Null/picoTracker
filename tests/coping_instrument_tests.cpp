#include "doctest/doctest.h"
#include "Application/Instruments/DrumInstrument.h"
#include "Application/Instruments/StackInstrument.h"
#include "Application/Instruments/InstrumentBankRestorePolicy.h"
#include <algorithm>
#include <array>

TEST_CASE_TEMPLATE("Coping instruments isolate voices and preserve block continuity",
                   Synth, DrumInstrument, StackInstrument) {
  Synth whole, split;
  std::array<fixed, 2048> a{}, b{};
  CHECK_FALSE(whole.Render(0, a.data(), 1024, false));
  CHECK_FALSE(whole.Start(-1, 60));
  CHECK_FALSE(whole.Start(SONG_CHANNEL_COUNT, 60));
  CHECK_FALSE(whole.Start(0, 255));
  REQUIRE(whole.Start(0, 60));
  REQUIRE(split.Start(0, 60));
  REQUIRE(whole.Start(1, 64));
  whole.Stop(1);
  REQUIRE(whole.Render(0, a.data(), 1024, false));
  for (int i = 0; i < 16; ++i)
    REQUIRE(split.Render(0, b.data() + i * 128, 64, false));
  CHECK(a == b);
  CHECK(std::any_of(a.begin(), a.end(), [](fixed x) { return x != 0; }));
  whole.ProcessCommand(0, FourCC::InstrumentCommandKill, 0);
  CHECK_FALSE(whole.Render(0, a.data(), 1024, false));
  CHECK(split.Render(0, b.data(), 1024, false));
  split.OnStart();
  CHECK_FALSE(split.Render(0, b.data(), 1024, false));
}

TEST_CASE("Stack GateOff releases the voice") {
  StackInstrument synth;
  REQUIRE(synth.Start(0, 60));
  std::array<fixed, 512> buffer{};
  REQUIRE(synth.Render(0, buffer.data(), 256, false));
  synth.ProcessCommand(0, FourCC::InstrumentCommandGateOff, 0);
  for (int i = 0; i < 200; ++i)
    synth.Render(0, buffer.data(), 256, false);
  CHECK_FALSE(synth.Render(0, buffer.data(), 256, false));
}

TEST_CASE("Drum and Stack have independent fixed restore capacities") {
  InstrumentBankRestorePolicy policy;
  for (int i = 0; i < MAX_DRUMINSTRUMENT_COUNT; ++i)
    CHECK(policy.Reserve(i, IT_DRUM));
  CHECK_FALSE(policy.Reserve(MAX_DRUMINSTRUMENT_COUNT, IT_DRUM));
  for (int i = 0; i < MAX_STACKINSTRUMENT_COUNT; ++i)
    CHECK(policy.Reserve(MAX_DRUMINSTRUMENT_COUNT + i, IT_STACK));
  CHECK_FALSE(policy.Reserve(MAX_DRUMINSTRUMENT_COUNT + MAX_STACKINSTRUMENT_COUNT, IT_STACK));
}
