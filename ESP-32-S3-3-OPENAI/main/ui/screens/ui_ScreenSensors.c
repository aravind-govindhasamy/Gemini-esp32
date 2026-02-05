#include "../ui.h"

void ui_ScreenSensors_screen_init(void);
lv_obj_t *ui_ScreenSensors;
lv_obj_t *ui_LabelPresenceTitle;
lv_obj_t *ui_LabelPresenceValue;
lv_obj_t *ui_LabelTempTitle;
lv_obj_t *ui_LabelHumTitle;
lv_obj_t *ui_LabelSensorsTitle;
lv_obj_t *ui_LabelTempValue;
lv_obj_t *ui_LabelHumValue;

void ui_event_ScreenSensors(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_GESTURE &&
      lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
    lv_scr_load_anim(ui_ScreenStatus, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                     false);
  }
  if (event_code == LV_EVENT_GESTURE &&
      lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
    lv_scr_load_anim(ui_ScreenListen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0,
                     false);
  }
}

void ui_event_ButtonSensorBack(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    lv_disp_load_scr(ui_ScreenListen);
  }
}

void ui_ScreenSensors_screen_init(void) {
  ui_ScreenSensors = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenSensors, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_src(ui_ScreenSensors, &ui_img_setup_bg_png,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelSensorsTitle = lv_label_create(ui_ScreenSensors);
  lv_obj_set_width(ui_LabelSensorsTitle, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LabelSensorsTitle, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_LabelSensorsTitle, LV_ALIGN_TOP_MID);
  lv_obj_set_y(ui_LabelSensorsTitle, 30);
  lv_label_set_text(ui_LabelSensorsTitle, "SENSOR DATA");
  lv_obj_set_style_text_color(ui_LabelSensorsTitle, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelSensorsTitle, &ui_font_PingFangEN20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Temperature
  ui_LabelTempTitle = lv_label_create(ui_ScreenSensors);
  lv_obj_set_align(ui_LabelTempTitle, LV_ALIGN_LEFT_MID);
  lv_obj_set_x(ui_LabelTempTitle, 40);
  lv_obj_set_y(ui_LabelTempTitle, -30);
  lv_label_set_text(ui_LabelTempTitle, "Temp:");
  lv_obj_set_style_text_color(ui_LabelTempTitle, lv_color_hex(0xCCCCCC),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelTempTitle, &ui_font_PingFangEN16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelTempValue = lv_label_create(ui_ScreenSensors);
  lv_obj_set_align(ui_LabelTempValue, LV_ALIGN_RIGHT_MID);
  lv_obj_set_x(ui_LabelTempValue, -40);
  lv_obj_set_y(ui_LabelTempValue, -30);
  lv_label_set_text(ui_LabelTempValue, "--.- °C");
  lv_obj_set_style_text_color(ui_LabelTempValue, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelTempValue, &ui_font_PingFangEN20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Humidity
  ui_LabelHumTitle = lv_label_create(ui_ScreenSensors);
  lv_obj_set_align(ui_LabelHumTitle, LV_ALIGN_LEFT_MID);
  lv_obj_set_x(ui_LabelHumTitle, 40);
  lv_obj_set_y(ui_LabelHumTitle, 20);
  lv_label_set_text(ui_LabelHumTitle, "Humidity:");
  lv_obj_set_style_text_color(ui_LabelHumTitle, lv_color_hex(0xCCCCCC),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelHumTitle, &ui_font_PingFangEN16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelHumValue = lv_label_create(ui_ScreenSensors);
  lv_obj_set_align(ui_LabelHumValue, LV_ALIGN_RIGHT_MID);
  lv_obj_set_x(ui_LabelHumValue, -40);
  lv_obj_set_y(ui_LabelHumValue, 20);
  lv_label_set_text(ui_LabelHumValue, "--.- %");
  lv_obj_set_style_text_color(ui_LabelHumValue, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelHumValue, &ui_font_PingFangEN20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Presence
  ui_LabelPresenceTitle = lv_label_create(ui_ScreenSensors);
  lv_obj_set_align(ui_LabelPresenceTitle, LV_ALIGN_LEFT_MID);
  lv_obj_set_x(ui_LabelPresenceTitle, 40);
  lv_obj_set_y(ui_LabelPresenceTitle, 70);
  lv_label_set_text(ui_LabelPresenceTitle, "Presence:");
  lv_obj_set_style_text_color(ui_LabelPresenceTitle, lv_color_hex(0xCCCCCC),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelPresenceTitle, &ui_font_PingFangEN16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelPresenceValue = lv_label_create(ui_ScreenSensors);
  lv_obj_set_align(ui_LabelPresenceValue, LV_ALIGN_RIGHT_MID);
  lv_obj_set_x(ui_LabelPresenceValue, -40);
  lv_obj_set_y(ui_LabelPresenceValue, 70);
  lv_label_set_text(ui_LabelPresenceValue, "NONE");
  lv_obj_set_style_text_color(ui_LabelPresenceValue, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelPresenceValue, &ui_font_PingFangEN20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_add_event_cb(ui_ScreenSensors, ui_event_ScreenSensors, LV_EVENT_ALL,
                      NULL);

  // BACK BUTTON
  ui_ButtonSensorBack = lv_btn_create(ui_ScreenSensors);
  lv_obj_set_width(ui_ButtonSensorBack, 80);
  lv_obj_set_height(ui_ButtonSensorBack, 40);
  lv_obj_set_align(ui_ButtonSensorBack, LV_ALIGN_BOTTOM_MID);
  lv_obj_set_y(ui_ButtonSensorBack, -20);
  lv_obj_add_event_cb(ui_ButtonSensorBack, ui_event_ButtonSensorBack,
                      LV_EVENT_ALL, NULL);
  lv_obj_set_style_bg_color(ui_ButtonSensorBack, lv_color_hex(0x607D8B),
                            LV_PART_MAIN);
  lv_obj_set_style_radius(ui_ButtonSensorBack, 20, LV_PART_MAIN);

  ui_LabelSensorBack = lv_label_create(ui_ButtonSensorBack);
  lv_obj_set_align(ui_LabelSensorBack, LV_ALIGN_CENTER);
  lv_label_set_text(ui_LabelSensorBack, "BACK");
  lv_obj_set_style_text_font(ui_LabelSensorBack, &ui_font_PingFangEN14, 0);
}
