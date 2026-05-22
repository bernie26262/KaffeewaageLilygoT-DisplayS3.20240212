#pragma once

#include <stdint.h>

// T4-S3 coffee scale hardware planning.
//
// The current LVGL prototype still uses COFFEE_WEIGHT_SIMULATOR and does not
// initialize the HX711 yet. These constants reserve the intended wiring for
// the later real load-cell build.

namespace coffee_t4s3_pins {

static constexpr uint8_t HX711_DOUT_PIN = 42;  // T4-S3 header pin IO42 -> HX711 DT/DOUT
static constexpr uint8_t HX711_SCK_PIN  = 41;  // T4-S3 header pin IO41 -> HX711 SCK

}  // namespace coffee_t4s3_pins
