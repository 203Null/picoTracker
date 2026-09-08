/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 nILS Podewski
 * Copyright (c) 2026 NullPerator contributors
 * Adapted from copingTracker; see Externals/copingSynth/UPSTREAM.txt.
 */
#pragma once
#include "I_Instrument.h"
#include "Application/Model/Song.h"
#include "Externals/copingSynth/DrumInstrument/DrumEngine.h"
#include "Externals/etl/include/etl/vector.h"
#include <array>

class DrumInstrument final : public I_Instrument {
public:
  DrumInstrument();
  bool Init() override { return true; }
  bool IsInitialized() override { return true; }
  bool IsEmpty() override { return false; }
  InstrumentType GetType() override { return IT_DRUM; }
  bool Start(int channel, unsigned char note, bool retrigger = true) override;
  void Stop(int channel) override;
  void OnStart() override;
  bool Render(int channel, fixed *buffer, int size, bool updateTick) override;
  void ProcessCommand(int channel, FourCC command, ushort value) override;
  int GetTable() override { return VAR_OFF; }
  bool GetTableAutomation() override { return false; }
  void GetTableState(TableSaveState &) override {}
  void SetTableState(TableSaveState &) override {}
  etl::ivector<Variable *> *Variables() override { return &variables_; }
private:
  etl::vector<Variable *, 13> variables_;
  std::array<Variable, 13> parameters_;
  std::array<drum_voice_t, SONG_CHANNEL_COUNT> voices_{};
};
