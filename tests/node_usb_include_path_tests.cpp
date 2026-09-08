#include "doctest/doctest.h"
#include <string_view>

#define TUP_MCU_ESPRESSIF 1
#include "Adapters/node/usb/tusb_config.h"
#include "Externals/tinyusb/src/common/tusb_compiler.h"

TEST_CASE("TinyUSB FreeRTOS include path survives source formatting") {
  CHECK(std::string_view(TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH, FreeRTOS.h)) ==
        "freertos/FreeRTOS.h");
  CHECK(std::string_view(TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH, semphr.h)) ==
        "freertos/semphr.h");
}
