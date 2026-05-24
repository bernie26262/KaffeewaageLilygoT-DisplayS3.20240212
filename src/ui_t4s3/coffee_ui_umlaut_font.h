#pragma once

#include <lvgl.h>

// LVGL's built-in Montserrat font in this project does not contain German
// umlauts. This returns the normal default UI font with a tiny fallback font
// for ä, ö, ü, Ä, Ö, Ü and ß.
const lv_font_t *coffee_ui_font();
