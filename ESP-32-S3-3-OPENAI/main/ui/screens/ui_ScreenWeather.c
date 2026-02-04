#include "../ui.h"
#include "../../app/app_sensor.h"

lv_obj_t *ui_ScreenWeather;
lv_obj_t *ui_WeatherClock;
lv_obj_t *ui_LabelTempVal;
lv_obj_t *ui_LabelHumVal;

void ui_event_ScreenWeather(lv_event_t * e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
        lv_scr_load_anim(ui_ScreenStatus, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    }
    if (event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        lv_scr_load_anim(ui_ScreenListen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
    }
}

void ui_ScreenWeather_screen_init(void) {
    ui_ScreenWeather = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenWeather, lv_color_hex(0x000000), LV_PART_MAIN);
    
    ui_WeatherClock = lv_label_create(ui_ScreenWeather);
    lv_obj_set_align(ui_WeatherClock, LV_ALIGN_TOP_MID);
    lv_obj_set_y(ui_WeatherClock, 20);
    lv_label_set_text(ui_WeatherClock, "00:00");
    lv_obj_set_style_text_font(ui_WeatherClock, &ui_font_JetBrainsMono88, 0); 
    lv_obj_set_style_text_color(ui_WeatherClock, lv_color_hex(0xFFFFFF), 0);

    ui_LabelTempVal = lv_label_create(ui_ScreenWeather);
    lv_obj_set_align(ui_LabelTempVal, LV_ALIGN_CENTER);
    lv_obj_set_y(ui_LabelTempVal, 20);
    lv_label_set_text(ui_LabelTempVal, "TEMP: --. -°C");
    lv_obj_set_style_text_font(ui_LabelTempVal, &ui_font_JetBrainsMono40, 0);
    lv_obj_set_style_text_color(ui_LabelTempVal, lv_color_hex(0x00FF00), 0);

    ui_LabelHumVal = lv_label_create(ui_ScreenWeather);
    lv_obj_set_align(ui_LabelHumVal, LV_ALIGN_CENTER);
    lv_obj_set_y(ui_LabelHumVal, 60);
    lv_label_set_text(ui_LabelHumVal, "HUM:  --. -%");
    lv_obj_set_style_text_font(ui_LabelHumVal, &ui_font_JetBrainsMono40, 0);
    lv_obj_set_style_text_color(ui_LabelHumVal, lv_color_hex(0x00FFFF), 0);

    lv_obj_add_event_cb(ui_ScreenWeather, ui_event_ScreenWeather, LV_EVENT_ALL, NULL);
}
