#include "../ui.h"

void ui_ScreenNetwork_screen_init(void) {
  ui_ScreenNetwork = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenNetwork, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_src(ui_ScreenNetwork, &ui_img_setup_bg_png,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelNetTitle = lv_label_create(ui_ScreenNetwork);
  lv_obj_set_width(ui_LabelNetTitle, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LabelNetTitle, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_LabelNetTitle, LV_ALIGN_TOP_MID);
  lv_obj_set_y(ui_LabelNetTitle, 30);
  lv_label_set_text(ui_LabelNetTitle, "NETWORK DETAILS");
  lv_obj_set_style_text_color(ui_LabelNetTitle, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelNetTitle, &ui_font_PingFangEN20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Device IP
  lv_obj_t *ui_LabelIPTitle = lv_label_create(ui_ScreenNetwork);
  lv_obj_set_align(ui_LabelIPTitle, LV_ALIGN_LEFT_MID);
  lv_obj_set_x(ui_LabelIPTitle, 20);
  lv_obj_set_y(ui_LabelIPTitle, -30);
  lv_label_set_text(ui_LabelIPTitle, "Device IP:");
  lv_obj_set_style_text_color(ui_LabelIPTitle, lv_color_hex(0xCCCCCC),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelIPTitle, &ui_font_PingFangEN14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelNetIP = lv_label_create(ui_ScreenNetwork);
  lv_obj_set_align(ui_LabelNetIP, LV_ALIGN_RIGHT_MID);
  lv_obj_set_x(ui_LabelNetIP, -20);
  lv_obj_set_y(ui_LabelNetIP, -30);
  lv_label_set_text(ui_LabelNetIP, "0.0.0.0");
  lv_obj_set_style_text_color(ui_LabelNetIP, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelNetIP, &ui_font_PingFangEN16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Target Hub
  lv_obj_t *ui_LabelHubTitle = lv_label_create(ui_ScreenNetwork);
  lv_obj_set_align(ui_LabelHubTitle, LV_ALIGN_LEFT_MID);
  lv_obj_set_x(ui_LabelHubTitle, 20);
  lv_obj_set_y(ui_LabelHubTitle, 10);
  lv_label_set_text(ui_LabelHubTitle, "Target Hub:");
  lv_obj_set_style_text_color(ui_LabelHubTitle, lv_color_hex(0xCCCCCC),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelHubTitle, &ui_font_PingFangEN14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelNetHub = lv_label_create(ui_ScreenNetwork);
  lv_obj_set_align(ui_LabelNetHub, LV_ALIGN_RIGHT_MID);
  lv_obj_set_x(ui_LabelNetHub, -20);
  lv_obj_set_y(ui_LabelNetHub, 10);
  lv_label_set_text(ui_LabelNetHub, "0.0.0.0:8000");
  lv_obj_set_style_text_color(ui_LabelNetHub, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelNetHub, &ui_font_PingFangEN16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Gateway
  lv_obj_t *ui_LabelGWTitle = lv_label_create(ui_ScreenNetwork);
  lv_obj_set_align(ui_LabelGWTitle, LV_ALIGN_LEFT_MID);
  lv_obj_set_x(ui_LabelGWTitle, 20);
  lv_obj_set_y(ui_LabelGWTitle, 50);
  lv_label_set_text(ui_LabelGWTitle, "Gateway:");
  lv_obj_set_style_text_color(ui_LabelGWTitle, lv_color_hex(0xCCCCCC),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelGWTitle, &ui_font_PingFangEN14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LabelNetGW = lv_label_create(ui_ScreenNetwork);
  lv_obj_set_align(ui_LabelNetGW, LV_ALIGN_RIGHT_MID);
  lv_obj_set_x(ui_LabelNetGW, -20);
  lv_obj_set_y(ui_LabelNetGW, 50);
  lv_label_set_text(ui_LabelNetGW, "0.0.0.0");
  lv_obj_set_style_text_color(ui_LabelNetGW, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LabelNetGW, &ui_font_PingFangEN16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // BACK BUTTON
  ui_ButtonNetBack = lv_btn_create(ui_ScreenNetwork);
  lv_obj_set_width(ui_ButtonNetBack, 80);
  lv_obj_set_height(ui_ButtonNetBack, 40);
  lv_obj_set_align(ui_ButtonNetBack, LV_ALIGN_BOTTOM_MID);
  lv_obj_set_y(ui_ButtonNetBack, -20);
  lv_obj_add_event_cb(ui_ButtonNetBack, ui_event_ButtonNetBack, LV_EVENT_ALL,
                      NULL);
  lv_obj_set_style_bg_color(ui_ButtonNetBack, lv_color_hex(0x607D8B),
                            LV_PART_MAIN);

  ui_LabelNetBack = lv_label_create(ui_ButtonNetBack);
  lv_obj_set_align(ui_LabelNetBack, LV_ALIGN_CENTER);
  lv_label_set_text(ui_LabelNetBack, "BACK");
  lv_obj_set_style_text_font(ui_LabelNetBack, &ui_font_PingFangEN14, 0);
}
