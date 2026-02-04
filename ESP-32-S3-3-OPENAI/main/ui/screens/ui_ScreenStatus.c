#include "../ui.h"

lv_obj_t *ui_ScreenStatus;
lv_obj_t *ui_StatusClock;
lv_obj_t *ui_LabelIPVal;
lv_obj_t *ui_LabelLuxVal;

void ui_event_ScreenStatus(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_GESTURE &&
      lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
    lv_scr_load_anim(ui_ScreenWeather, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                     false);
  }
  if (event_code == LV_EVENT_GESTURE &&
      lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
    lv_scr_load_anim(ui_ScreenSensors, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0,
                     false);
  }
}

void ui_event_ImageListenSettings(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(ui_ScreenSetup, LV_SCR_LOAD_ANIM_NONE, 0, 0);
  }
}

void ui_ScreenStatus_screen_init(void) {
  ui_ScreenStatus = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ui_ScreenStatus, lv_color_hex(0x000000),
                            LV_PART_MAIN);

  ui_StatusClock = lv_label_create(ui_ScreenStatus);
  lv_obj_set_align(ui_StatusClock, LV_ALIGN_TOP_MID);
  lv_obj_set_y(ui_StatusClock, 20);
  lv_label_set_text(ui_StatusClock, "00:00");
  lv_obj_set_style_text_font(ui_StatusClock, &ui_font_JetBrainsMono88, 0);
  lv_obj_set_style_text_color(ui_StatusClock, lv_color_hex(0xFFFFFF), 0);

  ui_LabelIPVal = lv_label_create(ui_ScreenStatus);
  lv_obj_set_align(ui_LabelIPVal, LV_ALIGN_CENTER);
  lv_obj_set_y(ui_LabelIPVal, 20);
  lv_label_set_text(ui_LabelIPVal, "IP: 192.168.x.x");
  lv_obj_set_style_text_font(ui_LabelIPVal, &ui_font_JetBrainsMono22, 0);
  lv_obj_set_style_text_color(ui_LabelIPVal, lv_color_hex(0xFFFFFF), 0);

  ui_LabelLuxVal = lv_label_create(ui_ScreenStatus);
  lv_obj_set_align(ui_LabelLuxVal, LV_ALIGN_CENTER);
  lv_obj_set_y(ui_LabelLuxVal, 60);
  lv_label_set_text(ui_LabelLuxVal, "LUX: ----");
  lv_obj_set_style_text_font(ui_LabelLuxVal, &ui_font_JetBrainsMono22, 0);
  lv_obj_set_style_text_color(ui_LabelLuxVal, lv_color_hex(0xFFFF00), 0);

  // Relocated Settings Button from Home
  ui_ImageListenSettings = lv_img_create(ui_ScreenStatus);
  lv_img_set_src(ui_ImageListenSettings, &ui_img_settings_icon_png);
  lv_obj_set_width(ui_ImageListenSettings, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ImageListenSettings, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ImageListenSettings, -10);
  lv_obj_set_y(ui_ImageListenSettings, 10);
  lv_obj_set_align(ui_ImageListenSettings, LV_ALIGN_TOP_RIGHT);
  lv_obj_add_flag(ui_ImageListenSettings,
                  LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_add_event_cb(ui_ImageListenSettings, ui_event_ImageListenSettings,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_add_event_cb(ui_ScreenStatus, ui_event_ScreenStatus, LV_EVENT_ALL,
                      NULL);
}
