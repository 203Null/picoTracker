/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the copingTracker firmware
 */

#pragma once

#include "Application/Instruments/EnvelopeGenerators.h"
#include "Application/Utils/fixed.h"
#include "StackWavetables.generated.h"
#include <cstdint>

#include "System/Console/Trace.h"

#include "../ChiptuneInstrument/ChiptuneTables.h"
#include "StackEnums.h"

// Cent multipliers in Q16 fixed point (2^(cents/1200) * 65536)
#define CENT_MULT_0 65536   // 2^(0/1200)
#define CENT_MULT_25 66495  // 2^(25/1200)
#define CENT_MULT_50 67523  // 2^(50/1200)
#define CENT_MULT_M25 64648 // 2^(-25/1200)
#define CENT_MULT_M50 63686 // 2^(-50/1200)

/******************************************************************************
 * voice                                                                      *
 ******************************************************************************/

typedef struct stack_parameters_t {
  uint8_t spread;
  uint8_t attack;
  uint8_t decay;
  uint8_t sustain;
  uint8_t release;
  uint8_t volume;
  uint8_t brightness;
  uint8_t glide;
  uint8_t wave;
  int8_t transpose;
} stack_parameters_t;

typedef struct stack_pitch_envelope_t {
  int32_t value;
  int32_t rate;

  void trigger() {
    value = 0xffff;
  }

  int32_t tick() {
    value -= (static_cast<uint32_t>(value) * rate) >> 16;
    return value;
  }

  void set_rate(uint8_t inRate) {
    rate = (static_cast<uint16_t>(inRate) << 8) | inRate;
  }
} stack_pitch_envelope_t;

// (!) alignment has to be manually kept in this struct to allow using pack()
//     to keep the size as small as possible
#pragma pack(push, 1)
typedef struct stack_voice_t {
  stack_parameters_t parameters; // parameters passed from instrument (10 bytes)

  uint8_t drive;    // unused currently
  uint8_t bitcrush; // bitcrush setting (only settable via command)

  uint32_t phase[5];         // oscillator phases
  int32_t frequency[5];      // precomp'd oscillator frequencies
  int32_t base_frequency[5]; // precomp'd oscillator frequencies
  uint8_t lut_index[5];      // index of mip table to use

  uint8_t volume;
  uint8_t level;
  uint8_t note;           // current base note
  stack_wave_type_e wave; // selected waveform

  adsr_envelope_t envelope; // volume envelope, size is 9 bytes
  uint8_t tock;             // sample counter for 1000Hz updates
  uint16_t tick;            // sample counter for 100Hz updates

  uint32_t time; // sample counter
  uint32_t timeToLive;

  stack_pitch_envelope_t pitch[5]; // pitch envelopes (8 bytes)

  stack_flags flags;
  uint8_t notes[5];

  // implementation ------------------------------------------------------------

  inline uint32_t compute_cent_multiplier(int16_t cents) {
    if (cents == 0)
      return CENT_MULT_0;
    if (cents > 0) {
      if (cents <= 25) {
        return CENT_MULT_0 + ((CENT_MULT_25 - CENT_MULT_0) * cents) / 25;
      } else {
        return CENT_MULT_25 + ((CENT_MULT_50 - CENT_MULT_25) * (cents - 25)) / 25;
      }
    } else {
      cents = -cents;
      if (cents <= 25) {
        return CENT_MULT_0 - ((CENT_MULT_0 - CENT_MULT_M25) * cents) / 25;
      } else {
        return CENT_MULT_M25 - ((CENT_MULT_M25 - CENT_MULT_M50) * (cents - 25)) / 25;
      }
    }
  }

  inline void stop() {
    for (int o = 0; o < stackNumOscillators; o++) {
      frequency[o] = 0;
      phase[o] = 0;
    }
  }

  inline void tick_100Hz() {
    // processing at ~100Hz

    // volume
    envelope.tick(); // TODO POD: handle output value to know when to kill the note on NOTE_OFF -> release ringing out

    // recompute combined gain when envelope, pan or volume changes
    level = (parameters.volume * volume * envelope.value) >> 24;

    // pitch
    for (int o = 0; o < stackNumOscillators; o++) {
      int32_t value = pitch[o].tick();
      frequency[o] = (uint32_t)((uint64_t)base_frequency[o] * value) >> 16;
    }
  }

  inline void tick_1000Hz() {
    if (timeToLive == 0) {
      if (flags.retrigger) {
        flags.retrigger = 0; // clear retrigger flag
        // retrigger without resetting clocks
        note_on(note, volume, false, parameters, true);
      } else {
        // note off, kill everything
        wave = stackWaveNone;
        envelope.state = adsrIdle;
        volume = 0;
      }
    } else {
      // length
      timeToLive--;
    }
  }

  inline void sample(fixed *left, fixed *right) {
    // precompute the gain, it doesn't need to be updated every sample

    // cold loop @ 100 Hz ------------------------------------------------------
    if (tick == 0) {
      tick = stackTicks100Hz;
      tick_100Hz();
    }

    // warm loop @ ~1000 Hz ----------------------------------------------------
    if (tock == 0) {
      tock = stackTicks1000Hz;
      tick_1000Hz(); // update at 1kHz for smoother pan and volume slides
    }

    // hot loop @ ~44100 Hz ----------------------------------------------------
    tick--;
    tock--;
    time++;

    for (int o = 0; o < stackNumOscillators; o++) {
      // advance phase
      phase[o] += frequency[o];
    }

    int32_t sample = 0;

    // generate sample based on waveform
    if (wave != stackWaveNone) {
      for (int o = 0; o < stackNumOscillators; o++) {
         // render wavetable
         sample += (StackWavetables::stack_wavetables[wave][0][phase[o] >> 21]);
      }
    }

    // sample is in the 17bits * 5 == +-0x27ffb range (18 bits), sample target volume is 31 bits
    // applying combined gain (volume * envelope) does not need any scaling back as it gets the level up to 26 bits
    sample *= level;

    // breng it up to 29 bits
    sample <<= 3;

    // apply bitcrush
    if (bitcrush) {
      sample &= (0xffff'ffff << bitcrush);
      // drive is ignored at the moment
      // sample *= drive;
    }

    // apply panning
    *left = sample;
    *right = sample;
  }

  inline void set_oscillator_note(int osc, int note) {
    const int8_t cent_offsets[5] = {0, -1, 1, -2, 2};

    // clip note
    while (note < fLUT_MinNote) {
      note += 12;
    }
    while (note > fLUT_MaxNote) {
      note -= 12;
    }

    // apply spread (0-255) to cents (max ±25/±50 cents)
    int16_t cents = (cent_offsets[osc] * (int16_t)parameters.spread * 25) / 255;
    uint32_t multiplier = compute_cent_multiplier(cents);
    notes[osc] = note;
    base_frequency[osc] = (uint32_t)((uint64_t)frequencyLUT[note] * multiplier) >> 16;
    frequency[osc] = base_frequency[osc];

    set_oscillator_lut_index(osc, note);
  }

  inline void set_oscillator_lut_index(int osc, uint8_t note) {
    // note must be within fLUT_MinNote..fLUT_MaxNote
    constexpr uint8_t noteRange = fLUT_MaxNote - fLUT_MinNote;
    const uint8_t notePos = note - fLUT_MinNote;
    const uint8_t mapped = (notePos * 6) / noteRange;
    const uint8_t brightness = parameters.brightness;

    // map the note to its LUT using brightness as a scale
    if (brightness <= 7) {
      lut_index[osc] = (mapped * brightness) / 7;
    } else {
      lut_index[osc] = mapped + ((6 - mapped) * (brightness - 7)) / 5;
    }
  }

  inline void note_on(unsigned char note, uint8_t inVolume, bool retrigger, const stack_parameters_t inParameters,
                      bool keepClocks = false) {
    // bool retrigger is currently unused
    parameters = inParameters;

    // store volume
    volume = inVolume;
    level = 0xff;

    bitcrush = 0; // only accessible via command
    drive = 0;

    this->note = note;

    // oscillator frequency setup
    for (uint8_t o = 0; o < stackNumOscillators; o++) {
      set_oscillator_note(o, note + parameters.transpose);

      // reset oscillator phase
      phase[o] = 0;
    }
    wave = (stack_wave_type_e)parameters.wave;

    timeToLive = -1;

    // don't reset timers on internal retrigger via command (IRT, ...)
    // they might be mid-execution and will underflow
    if (!keepClocks) {
      time = 0;
      tick = 0;
      tock = 0;
    }

    // reset envelope
    envelope.set_attack(parameters.attack);
    envelope.set_decay(parameters.decay);
    envelope.set_sustain(parameters.sustain);
    envelope.set_release(parameters.release);
    envelope.trigger();

    // reset pitch envelope
    for (int o = 0; o < stackNumOscillators; o++) {
      pitch[o].set_rate(parameters.glide << 4);
      pitch[o].trigger();
    }
  }

  /****************************************************************************
   * command processing                                                       *
   ****************************************************************************/

  void set_instrument_parameter(uint8_t param, uint8_t value) {
    Trace::Error("Set parameter %d to %d", param, value);
    switch (param) {
      case 0: // wave
        wave = (stack_wave_type_e)((value <= (int)stackWaveLastItem) ? value : (int)stackWaveLastItem);
        break;
      case 1: // transpose
        parameters.transpose = (int8_t)value;
        break;
      case 2: // volume
        volume = value;
        break;
      case 3: // attack
        envelope.set_attack(value);
        break;
      case 4: // decay
        envelope.set_decay(value);
        break;
      case 5: // sustain
        envelope.set_sustain(value);
        break;
      case 6: // release
        envelope.set_release(value);
        break;
      case 7: // spread
        parameters.spread = value;
        for (int o = 0; o < stackNumOscillators; o++) {
          set_oscillator_note(o, notes[o]);
        }
        break;
      case 8: // brightness
        parameters.brightness = value;
        for (int o = 0; o < stackNumOscillators; o++) {
          set_oscillator_lut_index(o, notes[o]);
        }
        break;
      case 9: // glide
        parameters.glide = value;
        for (int o = 0; o < stackNumOscillators; o++) {
          pitch[o].set_rate(value << 4);
        }
        break;
      default:
        // invalid parameter index, ignore for now
        break;
    }
  }

  void set_step_volume(uint8_t inVolume) {
    volume = inVolume;
  }

  void set_chord(int8_t a, int8_t b, int8_t c, int8_t d) {
    int baseNote = note + parameters.transpose;

    set_oscillator_note(1, baseNote + a);
    set_oscillator_note(2, baseNote + b);
    set_oscillator_note(3, baseNote + c);
    set_oscillator_note(4, baseNote + d);
  }
} stack_voice_t;
#pragma pack(pop)

// 128 bytes per voice max to keep the entire thing under 1kB for the 8 voices,
// also struct needs to be aligned to 4 bytes to prevent unaligned access
static_assert(sizeof(stack_voice_t) <= 152, "Check sizeof(stack_voice_t) in error message");
static_assert((sizeof(stack_voice_t) % 4) == 0, "stack_voice_t size must be multiple of 4");

