/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the copingTracker firmware
 */

#pragma once

#define Drum_Name_0 "BassDrum"
#define Drum_Name_1 "Rim shot"
#define Drum_Name_2 "Snare 1 "
#define Drum_Name_3 "Clap    "
#define Drum_Name_4 "Snare 2 "
#define Drum_Name_5 "Low tom "
#define Drum_Name_6 "Hihat cl"
#define Drum_Name_7 "Mid tom "
#define Drum_Name_8 "Hihat pd"
#define Drum_Name_9 "High tom"
#define Drum_Name_A "Hihat op"
#define Drum_Name_B "Crash   "

static const char drumNames[12][9] = {Drum_Name_0, Drum_Name_1, Drum_Name_2, Drum_Name_3, Drum_Name_4, Drum_Name_5,
                                      Drum_Name_6, Drum_Name_7, Drum_Name_8, Drum_Name_A, Drum_Name_B};

static const char drumShortNames[12][4] = {
    "BDr", // c
    "Rim",
    "Snr", // d
    "Clp",
    "Sn2", // e
    "TmL",
    "HHc", // f#
    "TmM",
    "HHp", // g#
    "TmH",
    "HHo", // a#
    "Csh",
};

enum drum_constants_e {
  drumEnvAttackThreshold = 65530,
  drumEnvDecayThreshold = 10,
  drumNumWaveforms = 8,
  drumQ16_16_1 = 0x0001'0000,
  drumTicks100Hz = 441,
  drumTicks1000Hz = 44,
};

enum drum_wave_type_e : uint8_t {
  drumWavePulse12_5 = 0,
  drumWavePulse25,
  drumWavePulse50,
  drumWaveTriangle,
  drumWaveNoiseGameBoy7,
  drumWaveNoiseNES,
  drumWaveNoiseSN76489,
  drumWaveNoiseWhite,
  drumWaveLastItem = drumWaveNoiseWhite,
  drumWaveNone
};

enum drum_env_state_e : uint8_t { drumEnvIdle, drumEnvDecay };

enum drum_instrument_defaults_e {
  defaultInstrument0 = 0x4562,
  defaultInstrument1 = 0x0845,
  defaultInstrument2 = 0x0464,
  defaultInstrument3 = 0x4F95,
  defaultInstrument4 = 0x2B64,
  defaultInstrument5 = 0x1452,
  defaultInstrument6 = 0x0F37,
  defaultInstrument7 = 0x1652,
  defaultInstrument8 = 0x0944,
  defaultInstrument9 = 0x1852,
  defaultInstrument10 = 0x0B6F,
  defaultInstrument11 = 0x1F84,
  defaultCharacter = 0x00
};

typedef union drum_flags {
  struct {
    uint8_t retrigger : 1;
    uint8_t unused : 7;
  };
  uint8_t byte;
} drum_flags;
