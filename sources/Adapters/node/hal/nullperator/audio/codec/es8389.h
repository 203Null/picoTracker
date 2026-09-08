#pragma once

#include "Adapters/node/hal/nullperator/audio/audio.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include <cstdint>

namespace NullperatorHAL::Codec::ES8389 {
esp_err_t Init(i2s_chan_handle_t txChan, i2s_chan_handle_t rxChan);
esp_err_t SetVolume(uint8_t volume);
esp_err_t SyncState(Audio::OutputMode_t outputMode,
                    Audio::InputMode_t inputMode);
bool IsEnabled();
} // namespace NullperatorHAL::Codec::ES8389
