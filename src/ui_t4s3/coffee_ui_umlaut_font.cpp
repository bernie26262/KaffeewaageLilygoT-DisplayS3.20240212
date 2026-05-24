#include "coffee_ui_umlaut_font.h"

#include <lvgl.h>

namespace {

struct CoffeeGlyph {
    uint32_t unicode;
    uint16_t adv_w;
    uint8_t box_w;
    uint8_t box_h;
    int8_t ofs_x;
    int8_t ofs_y;
    const uint8_t *bitmap;
};
// LVGL's 1-bpp custom font callback expects a continuous bit stream for each
// glyph. The bits must not be padded to full bytes per row. Row-padded bitmaps
// render as visibly shifted/blank characters, especially in words like "später".
static const uint8_t glyph_ae[] = {0x24, 0x00, 0x01, 0xE0, 0x60, 0xCF, 0xB3, 0x46, 0x9D, 0xE8};
static const uint8_t glyph_oe[] = {0x34, 0x00, 0x00, 0x3C, 0x66, 0x43, 0x43, 0x43, 0x43, 0x66, 0x3C};
static const uint8_t glyph_ue[] = {0x68, 0x00, 0x04, 0x38, 0x70, 0xE1, 0xC3, 0xC7, 0x9D, 0xF8};
static const uint8_t glyph_Ae[] = {0x14, 0x00, 0x00, 0x01, 0x01, 0xC0, 0xA0, 0xD8, 0x6C, 0x22, 0x3F, 0x98, 0xD8, 0x2C, 0x18};
static const uint8_t glyph_Oe[] = {0x0A, 0x00, 0x00, 0x00, 0x0F, 0x83, 0x18, 0xC1, 0x98, 0x32, 0x02, 0x40, 0x4C, 0x19, 0x83, 0x18, 0xC1, 0xF0};
static const uint8_t glyph_Ue[] = {0x24, 0x00, 0x00, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC3, 0x66, 0x3C};
static const uint8_t glyph_ss[] = {0x78, 0xC4, 0xC4, 0xCC, 0xD8, 0xD8, 0xCC, 0xC6, 0xC2, 0xC2, 0xDC};

static const CoffeeGlyph kGlyphs[] = {
    // For callback-based custom fonts LVGL expects adv_w in pixels here.
    // The generated LVGL Montserrat font files use 12.4 fixed-point values
    // internally, but copying those values into this custom callback makes the
    // fallback glyphs advance by huge amounts and creates visible gaps around
    // umlauts (for example "Gefäße").
    {0x00E4, 8, 7, 11, 0, 0, glyph_ae},   // ä
    {0x00F6, 8, 8, 11, 0, 0, glyph_oe},   // ö
    {0x00FC, 9, 7, 11, 0, 0, glyph_ue},   // ü
    {0x00C4, 9, 9, 13, 0, 0, glyph_Ae},   // Ä
    {0x00D6, 11, 11, 13, 0, 0, glyph_Oe}, // Ö
    {0x00DC, 10, 8, 13, 0, 0, glyph_Ue},  // Ü
    {0x00DF, 9, 8, 11, 0, 0, glyph_ss},   // ß
};

const CoffeeGlyph *find_glyph(uint32_t unicode)
{
    for (const CoffeeGlyph &glyph : kGlyphs) {
        if (glyph.unicode == unicode) {
            return &glyph;
        }
    }
    return nullptr;
}

const uint8_t *get_bitmap(const lv_font_t *, uint32_t unicode_letter)
{
    const CoffeeGlyph *glyph = find_glyph(unicode_letter);
    return glyph ? glyph->bitmap : nullptr;
}

bool get_glyph_dsc(const lv_font_t *, lv_font_glyph_dsc_t *dsc_out, uint32_t unicode_letter, uint32_t)
{
    const CoffeeGlyph *glyph = find_glyph(unicode_letter);
    if (!glyph || !dsc_out) {
        return false;
    }

    dsc_out->adv_w = glyph->adv_w;
    dsc_out->box_w = glyph->box_w;
    dsc_out->box_h = glyph->box_h;
    dsc_out->ofs_x = glyph->ofs_x;
    dsc_out->ofs_y = glyph->ofs_y;
    dsc_out->bpp = 1;
    return true;
}

lv_font_t coffeeUmlautFont;
lv_font_t coffeeUiFont;
bool coffeeUiFontReady = false;

void init_umlaut_font()
{
    coffeeUmlautFont.get_glyph_bitmap = get_bitmap;
    coffeeUmlautFont.get_glyph_dsc = get_glyph_dsc;
    coffeeUmlautFont.line_height = 22;
    coffeeUmlautFont.base_line = 6;
#if LVGL_VERSION_MAJOR >= 8
    coffeeUmlautFont.subpx = LV_FONT_SUBPX_NONE;
#endif
    coffeeUmlautFont.underline_position = -2;
    coffeeUmlautFont.underline_thickness = 1;
    coffeeUmlautFont.dsc = nullptr;
    coffeeUmlautFont.fallback = (LV_FONT_DEFAULT)->fallback;
#if LV_USE_USER_DATA
    coffeeUmlautFont.user_data = nullptr;
#endif

    coffeeUiFont = *(LV_FONT_DEFAULT);
    coffeeUiFont.fallback = &coffeeUmlautFont;
}

}  // namespace

const lv_font_t *coffee_ui_font()
{
    if (!coffeeUiFontReady) {
        init_umlaut_font();
        coffeeUiFontReady = true;
    }
    return &coffeeUiFont;
}
