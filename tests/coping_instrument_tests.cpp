#include "doctest/doctest.h"
#include "Application/Instruments/DrumInstrument.h"
#include "Application/Instruments/StackInstrument.h"
#include "Application/Instruments/InstrumentBankRestorePolicy.h"
#include "Application/UI2/Ui2InstrumentParameters.h"
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

TEST_CASE_TEMPLATE("Coping UI descriptors bind every real parameter exactly once",
                   Synth, DrumInstrument, StackInstrument) {
  Synth synth;
  const auto type = synth.GetType();
  REQUIRE(ui2::Ui2InstrumentFieldCount(type) == synth.Variables()->size());
  for (std::uint8_t i = 0; i < ui2::Ui2InstrumentFieldCount(type); ++i) {
    const auto descriptor = ui2::Ui2InstrumentFieldParameter(type, i);
    REQUIRE(descriptor.Valid());
    auto *value = synth.FindVariable(descriptor.primary);
    REQUIRE(value != nullptr);
    CHECK(value->GetInt() >= (descriptor.offValue ? VAR_OFF : descriptor.minimum));
    CHECK(value->GetInt() <= descriptor.maximum);
    for (std::uint8_t j = 0; j < i; ++j)
      CHECK(descriptor.primary != ui2::Ui2InstrumentFieldParameter(type, j).primary);
  }
}

TEST_CASE("Coping UI edits drum nibbles and wraps all Stack waves") {
  using namespace ui2;
  const auto drum = Ui2InstrumentFieldParameter(IT_DRUM, 0);
  const auto spec = Ui2InstrumentSubfields(drum);
  REQUIRE(spec.count == 4);
  CHECK(Ui2AdjustInstrumentSubfieldParameter(drum, 0x4562, spec.mode, 2,
      Ui2InstrumentValueDirection::Up) == 0x4572);
  const auto wave = Ui2InstrumentFieldParameter(IT_STACK, 0);
  CHECK(Ui2AdjustInstrumentParameter(wave, 6, Ui2InstrumentValueDirection::Right) == 0);
  CHECK(Ui2AdjustInstrumentParameter(wave, 0, Ui2InstrumentValueDirection::Left) == 6);
  const auto transpose = Ui2InstrumentFieldParameter(IT_STACK, 3);
  CHECK(Ui2AdjustInstrumentParameter(transpose, 0, Ui2InstrumentValueDirection::Up) == 12);
  CHECK(Ui2AdjustInstrumentParameter(transpose, 24, Ui2InstrumentValueDirection::Up) == 24);
}
