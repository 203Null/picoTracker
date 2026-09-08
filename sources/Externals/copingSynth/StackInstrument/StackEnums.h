/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the copingTracker firmware
 */

#pragma once

enum stack_wave_type_e : uint8_t {
  stackWavePulse12_5 = 0,
  stackWavePulse25,
  stackWavePulse50,
  stackWaveSaw,
  stackWaveTriangle,
  stackWaveOrgan,
  stackWaveVox,
  stackWaveLastItem = stackWaveVox,
  stackWaveNone
};

enum stack_constants_e {
  stackEnvAttackThreshold = 65530,
  stackEnvDecayThreshold = 10,
  stackNumWaveforms = stackWaveLastItem + 1,
  stackQ16_16_1 = 0x0001'0000,
  stackTicks100Hz = 441,
  stackTicks1000Hz = 44,
  stackBrightnessMax = 12,
  stackNumOscillators = 5,
};

inline constexpr const char *stackWaveNames[] = {"PULSE 12.5", "PULSE 25", "PULSE 50", "SAW", "TRIANGLE", "ORGAN", "VOX"};

enum stack_instrument_defaults_e {
  stackDefaultSpread = 0,
  stackDefaultWave = stackWaveSaw,
  stackDefaultTranspose = 0,
  stackDefaultAttack = 0,
  stackDefaultDecay = 0,
  stackDefaultSustain = 255,
  stackDefaultRelease = 0,
  stackDefaultVolume = 0x80,
  stackDefaultBrightness = stackBrightnessMax,
  stackDefaultGlide = 0
};

typedef union stack_flags {
  struct {
    uint8_t retrigger : 1;
    uint8_t unused : 7;
  };
  uint8_t byte;
} stack_flags;
