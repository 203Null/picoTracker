/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the copingTracker firmware
 */

#pragma once

#include "../ChiptuneInstrument/ChiptuneMath.h"
#include "../ChiptuneInstrument/ChiptuneTables.h"
#include "DrumEnums.h"
#include <algorithm>


typedef struct drum_envelope_t {
  uint16_t value;
  uint16_t decay;
  drum_env_state_e state;

  void set_decay(uint8_t d) {
    // map 8 bit decay value to 16 bit coefficient using LUT and interpolation
    decay = interpolateU16(decayCoeffLUT.data(), d);
  }

  void trigger() {
    state = drumEnvDecay;
    value = 0xffff;
  }

  void tick() {
    if (state == drumEnvIdle)
      return;

    int32_t tmp = value - std::max<uint32_t>(1, (uint32_t(value) * decay) >> 16);

    if (tmp <= drumEnvDecayThreshold) {
      tmp = 0;
      state = drumEnvIdle;
    }

    value = tmp;
  }
} drum_envelope_t;
