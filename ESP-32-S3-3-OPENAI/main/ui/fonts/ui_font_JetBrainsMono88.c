#include "../ui.h"

/* LVGL 8.x Compatible Font Stub - JetBrains Mono 88px */
/* Version: 1.1 */

#ifndef UI_FONT_JETBRAINSMONO88
#define UI_FONT_JETBRAINSMONO88 1
#endif

#if UI_FONT_JETBRAINSMONO88

static const uint8_t glyph_bitmap[] = { 0x00 };

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 1408, .box_w = 80, .box_h = 88, .ofs_x = 4, .ofs_y = 0}, 
};

static const uint16_t unicode_list_0[] = {
    0x0020, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a
};

static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 0x0020, .range_length = 27, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY, .list_length = 12,
        .unicode_list = unicode_list_0, .glyph_id_start = 1
    }
};

static lv_font_fmt_txt_glyph_cache_t cache;
static const lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
    .cache = &cache
};

const lv_font_t ui_font_JetBrainsMono88 = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 88,
    .base_line = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -3,
    .underline_thickness = 1,
    .dsc = &font_dsc
};

const lv_font_t * jetbrains_88 = &ui_font_JetBrainsMono88;

#endif
