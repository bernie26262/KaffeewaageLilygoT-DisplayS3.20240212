#pragma once

// Minimal pin API mock for the B4c.2a host-only relay driver test.
#include <stdint.h>

static constexpr uint8_t HIGH = 1;
static constexpr uint8_t LOW = 0;
static constexpr uint8_t OUTPUT = 1;

void digitalWrite(uint8_t pin, uint8_t level);
void pinMode(uint8_t pin, uint8_t mode);
