#include "../ui.h"

void ui_ScreenPlay_screen_init(void) {
  ui_ScreenPlay = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenPlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_src(ui_ScreenPlay, &ui_img_setup_bg_png,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelPlayTitle = lv_label_create(ui_ScreenPlay);
  lv_obj_set_width(ui_LabelPlayTitle, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LabelPlayTitle, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_LabelPlayTitle, LV_ALIGN_TOP_MID);
  lv_obj_set_y(ui_LabelPlayTitle, 30);
  lv_label_set_text(ui_LabelPlayTitle, "FUN & GAMES");
  lv_obj_set_style_text_color(ui_LabelPlayTitle, lv_color_hex(0xFF9800),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelPlayTitle, &ui_font_PingFangEN20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // DANCE BUTTON
  ui_ButtonDance = lv_btn_create(ui_ScreenPlay);
  lv_obj_set_width(ui_ButtonDance, 120);
  lv_obj_set_height(ui_ButtonDance, 50);
  lv_obj_set_align(ui_ButtonDance, LV_ALIGN_CENTER);
  lv_obj_set_y(ui_ButtonDance, -40);
  lv_obj_add_event_cb(ui_ButtonDance, ui_event_ButtonDance, LV_EVENT_ALL, NULL);
  lv_obj_set_style_bg_color(ui_ButtonDance, lv_color_hex(0xE91E63),
                            LV_PART_MAIN); // Pink
  lv_obj_set_style_radius(ui_ButtonDance, 25, LV_PART_MAIN);

  ui_LabelDance = lv_label_create(ui_ButtonDance);
  lv_obj_set_align(ui_LabelDance, LV_ALIGN_CENTER);
  lv_label_set_text(ui_LabelDance, "ROBOT DANCE");
  lv_obj_set_style_text_font(ui_LabelDance, &ui_font_PingFangEN14, 0);

  // MOOD BUTTON
  ui_ButtonMood = lv_btn_create(ui_ScreenPlay);
  lv_obj_set_width(ui_ButtonMood, 120);
  lv_obj_set_height(ui_ButtonMood, 50);
  lv_obj_set_align(ui_ButtonMood, LV_ALIGN_CENTER);
  lv_obj_set_y(ui_ButtonMood, 30);
  lv_obj_add_event_cb(ui_ButtonMood, ui_event_ButtonMood, LV_EVENT_ALL, NULL);
  lv_obj_set_style_bg_color(ui_ButtonMood, lv_color_hex(0x9C27B0),
                            LV_PART_MAIN); // Purple
  lv_obj_set_style_radius(ui_ButtonMood, 25, LV_PART_MAIN);

  ui_LabelMood = lv_label_create(ui_ButtonMood);
  lv_obj_set_align(ui_LabelMood, LV_ALIGN_CENTER);
  lv_label_set_text(ui_LabelMood, "CHANGE MOOD");
  lv_obj_set_style_text_font(ui_LabelMood, &ui_font_PingFangEN14, 0);

  // BACK BUTTON
  ui_ButtonPlayBack = lv_btn_create(ui_ScreenPlay);
  lv_obj_set_width(ui_ButtonPlayBack, 80);
  lv_obj_set_height(ui_ButtonPlayBack, 40);
  lv_obj_set_align(ui_ButtonPlayBack, LV_ALIGN_BOTTOM_MID);
  lv_obj_set_y(ui_ButtonPlayBack, -20);
  lv_obj_add_event_cb(ui_ButtonPlayBack, ui_event_ButtonPlayBack, LV_EVENT_ALL,
                      NULL);
  lv_obj_set_style_bg_color(ui_ButtonPlayBack, lv_color_hex(0x607D8B),
                            LV_PART_MAIN);
  lv_obj_set_style_radius(ui_ButtonPlayBack, 20, LV_PART_MAIN);

  ui_LabelPlayBack = lv_label_create(ui_ButtonPlayBack);
  lv_obj_set_align(ui_LabelPlayBack, LV_ALIGN_CENTER);
  lv_label_set_text(ui_LabelPlayBack, "BACK");
  lv_obj_set_style_text_font(ui_LabelPlayBack, &ui_font_PingFangEN14, 0);
}
