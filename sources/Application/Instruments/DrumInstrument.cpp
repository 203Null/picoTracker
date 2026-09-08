/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 nILS Podewski
 * Copyright (c) 2026 NullPerator contributors
 */
#include "DrumInstrument.h"
#include <algorithm>

DrumInstrument::DrumInstrument()
    : I_Instrument(&variables_), parameters_{Variable(FourCC::DrumVoice0, defaultInstrument0),
      Variable(FourCC::DrumVoice1, defaultInstrument1),
      Variable(FourCC::DrumVoice2, defaultInstrument2),
      Variable(FourCC::DrumVoice3, defaultInstrument3),
      Variable(FourCC::DrumVoice4, defaultInstrument4),
      Variable(FourCC::DrumVoice5, defaultInstrument5),
      Variable(FourCC::DrumVoice6, defaultInstrument6),
      Variable(FourCC::DrumVoice7, defaultInstrument7),
      Variable(FourCC::DrumVoice8, defaultInstrument8),
      Variable(FourCC::DrumVoice9, defaultInstrument9),
      Variable(FourCC::DrumVoice10, defaultInstrument10),
      Variable(FourCC::DrumVoice11, defaultInstrument11),
      Variable(FourCC::DrumCharacter, 0)} {
  for (auto &parameter : parameters_) variables_.push_back(&parameter);
}

void DrumInstrument::OnStart() {
  for (auto &voice : voices_) voice.stop();
}

void DrumInstrument::Stop(int channel) {
  if (channel >= 0 && channel < SONG_CHANNEL_COUNT) voices_[channel].stop();
}

bool DrumInstrument::Start(int channel, unsigned char note, bool retrigger) {
  if (channel < 0 || channel >= SONG_CHANNEL_COUNT || note > HIGHEST_NOTE) return false;
  const unsigned packed = parameters_[note % 12].GetInt();
  drum_parameters_t params{};
  params.wave = packed & 0xF;
  params.decay = (packed >> 4) & 0xF;
  params.note = (packed >> 8) & 0xF;
  params.pitch = (packed >> 12) & 0xF;
  params.character = std::clamp(parameters_[12].GetInt(), 0, 255);
  voices_[channel].note_on(note, 255, retrigger, params);
  return true;
}

bool DrumInstrument::Render(int channel, fixed *buffer, int size, bool) {
  if (!buffer || size <= 0 || channel < 0 || channel >= SONG_CHANNEL_COUNT) return false;
  auto &voice = voices_[channel];
  if (voice.wave == drumWaveNone) return false;
  for (int i = 0; i < size; ++i) voice.sample(buffer + i * 2, buffer + i * 2 + 1);
  return true;
}

void DrumInstrument::ProcessCommand(int channel, FourCC command, ushort value) {
  if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return;
  auto &voice = voices_[channel];
  switch (command) {
  case FourCC::InstrumentCommandKill:
    voice.stop();
    break;
  case FourCC::InstrumentCommandGateOff:
    voice.stop();
    break;
  case FourCC::InstrumentCommandVolume:
    voice.volume = value & 0xFF;
    break;
  case FourCC::InstrumentCommandCrush:
    voice.bitcrush = value & 0x0F;
    break;
  default:
    break;
  }
}
