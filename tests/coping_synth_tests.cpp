#include "Externals/copingSynth/DrumInstrument/DrumEngine.h"
#include "Externals/copingSynth/StackInstrument/StackEngine.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

int main() {
  // MIDI A4 phase increment at 44.1 kHz; catch table-origin and truncation bugs.
  assert(std::abs(double(noteFrequency(69)) * 44100.0 / 4294967296.0 - 440) < .01);
  for (int wave = 0; wave < drumNumWaveforms; ++wave) {
    drum_voice_t voice{};
    fixed left = 1, right = 1;
    voice.sample(&left, &right);
    assert(left == 0 && right == 0);
    drum_parameters_t params{};
    params.wave = wave;
    params.decay = 8;
    params.note = 8;
    params.pitch = 4;
    voice.note_on(60, 255, true, params);
    bool audible = false;
    for (int i = 0; i < 44100; ++i) {
      voice.sample(&left, &right);
      audible |= left != 0;
      assert(left == right);
    }
    assert(audible);
    voice.stop();
    for (int i = 0; i < 512; ++i) {
      voice.sample(&left, &right);
      assert(left == 0 && right == 0);
    }
  }
  for (int wave = 0; wave < stackNumWaveforms; ++wave) {
    for (int note : {0, 12, 69, 119}) {
      stack_voice_t voice{};
      stack_parameters_t params{};
      params.wave = wave;
      params.volume = 128;
      params.sustain = 255;
      params.brightness = 7;
      params.transpose = -24;
      params.spread = 255;
      voice.note_on(note, 255, true, params);
      voice.set_chord(-12, 4, 7, 12);
      fixed left = 0, right = 0;
      bool audible = false;
      for (int i = 0; i < 4096; ++i) {
        voice.sample(&left, &right);
        audible |= left != 0;
        assert(left == right);
        assert(std::abs(int64_t(left)) < INT32_MAX);
      }
      assert(audible);
      voice.envelope.release_note();
      for (int i = 0; i < 44100; ++i) voice.sample(&left, &right);
      assert(voice.wave == stackWaveNone);
      assert(left == 0 && right == 0);
    }
  }
  stack_parameters_t params{};
  params.wave = stackWaveSaw;
  params.volume = 128;
  params.sustain = 255;
  stack_voice_t dark{}, bright{};
  params.brightness = 12;
  bright.note_on(69, 255, true, params);
  params.brightness = 0;
  dark.note_on(69, 255, true, params);
  bool differs = false;
  for (int i = 0; i < 4096; ++i) {
    fixed a, b, unused;
    bright.sample(&a, &unused);
    dark.sample(&b, &unused);
    differs |= a != b;
  }
  assert(differs);
  std::cout << "Drum and Stack DSP tests passed; voice bytes: "
            << sizeof(drum_voice_t) << ", " << sizeof(stack_voice_t) << '\n';
}
