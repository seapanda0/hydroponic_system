#include "kinetic_os.h"

// Global UI Elements
static lv_obj_t * root_tv;
static lv_obj_t * page1;
static lv_obj_t * page2;
static lv_obj_t * page4;
static lv_obj_t * page5; // Dosing routines
static lv_obj_t * page6; // EC history chart
static lv_obj_t * page7; // Dosing settings
static lv_obj_t * btn_nav1;
static lv_obj_t * btn_nav2;
static lv_obj_t * btn_nav3;
static lv_obj_t * btn_nav4;
static lv_obj_t * btn_nav5;
static lv_obj_t * btn_nav6;
#define NAV_BTN_COUNT 6

// Page 5 Dynamic Elements (routine buttons)
static lv_obj_t * p5_routine_btns[KINETIC_ROUTINE_COUNT];
static lv_obj_t * p5_routine_labels[KINETIC_ROUTINE_COUNT];
static bool p5_routine_active[KINETIC_ROUTINE_COUNT];
static lv_obj_t * p5_target_ec_label;
static int p5_target_ec_tenths = -1; // cached to skip redundant restyling
static kinetic_os_routine_cb_t user_routine_cb = NULL;
static kinetic_os_ec_adjust_cb_t user_ec_adjust_cb = NULL;

// Page 7 Dynamic Elements (settings sliders)
static lv_obj_t * p7_shot_slider;
static lv_obj_t * p7_shot_value_label;
static lv_obj_t * p7_mix_slider;
static lv_obj_t * p7_mix_value_label;
static kinetic_os_setting_cb_t user_shot_dose_setting_cb = NULL;
static kinetic_os_setting_cb_t user_mix_interval_setting_cb = NULL;

// Page 6 Dynamic Elements (EC chart)
static lv_obj_t * p6_chart;
static lv_chart_series_t * p6_ec_series;
static lv_obj_t * p6_ec_value_label;
static uint32_t p6_ec_samples[KINETIC_EC_CHART_POINT_COUNT];
static uint16_t p6_ec_sample_count = 0;
static uint16_t p6_ec_sample_head = 0;
static uint32_t s_last_ec = 0;
static bool s_ec_seen = false;

static lv_obj_t * p_top_title;
static kinetic_os_switch_cb_t user_pump_cb = NULL;
static kinetic_os_switch_cb_t user_light_cb = NULL;

// Page 4 Dynamic Elements
static lv_obj_t * p4_pump_icon;
static lv_obj_t * p4_pump_dot;
static lv_obj_t * p4_pump_stat;
static lv_obj_t * p4_pump_sw;

static lv_obj_t * p4_fert_a_box;
static lv_obj_t * p4_fert_a_stat;
static lv_obj_t * p4_fert_b_box;
static lv_obj_t * p4_fert_b_stat;
static bool p4_fert_a_on = false;
static bool p4_fert_b_on = false;

static lv_obj_t * p4_light_icon;
static lv_obj_t * p4_light_dot;
static lv_obj_t * p4_light_stat;
static lv_obj_t * p4_light_sw;
static kinetic_os_switch_cb_t user_fert_a_cb = NULL;
static kinetic_os_switch_cb_t user_fert_b_cb = NULL;

// Page 1 Dynamic Elements
static lv_obj_t * p1_temp_val;
static lv_obj_t * p1_humidity_val;
static lv_obj_t * p1_water_level_val;
static lv_obj_t * p1_tds_val;
static lv_obj_t * p1_ec_val;
static lv_obj_t * p1_distance_val;

// Page 2 calibration UI elements
static lv_obj_t * p2_cal_raw_ec_label;
static lv_obj_t * p2_cal_temp_label;
static lv_obj_t * p2_cal_status_label;
static lv_obj_t * p2_cal_timer_label;
static lv_obj_t * p2_cal_avg_label;
static lv_obj_t * p2_cal_k_label;
static lv_obj_t * p2_cal_ignore_label;
static kinetic_os_cal_start_cb_t       s_cal_start_cb       = NULL;
static kinetic_os_cal_clear_cb_t       s_cal_clear_cb       = NULL;
static kinetic_os_cal_ignore_temp_cb_t s_cal_ignore_temp_cb = NULL;

// Fonts
#define FONT_BASE &lv_font_montserrat_14

// Icons mapped to LVGL built-in symbols
#define ICON_SENSORS LV_SYMBOL_WIFI
#define ICON_DASHBOARD LV_SYMBOL_LIST
#define ICON_OPACITY LV_SYMBOL_TINT

// Dose-complete popup (single instance; a new call replaces any open one)
static lv_obj_t * s_dose_popup_backdrop = NULL;

static void build_top_bar(void);
static void build_bottom_bar(void);
static void build_page1(void);
static void build_page2(void);
static void build_page4(void);
static void build_page5(void);
static void build_page6(void);
static void build_page7(void);
static void bottom_nav_event_cb(lv_event_t * e);
static void tileview_scroll_event_cb(lv_event_t * e);
static void update_bottom_nav_styles(uint8_t active_idx);
static void ec_chart_timer_cb(lv_timer_t * timer);

void kinetic_os_ui_init(void) {
    lv_obj_set_style_bg_color(lv_scr_act(), KINETIC_COLOR_BG, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    root_tv = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(root_tv, 320, 170);
    lv_obj_align(root_tv, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(root_tv, KINETIC_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(root_tv, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(root_tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(root_tv, tileview_scroll_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    page1 = lv_tileview_add_tile(root_tv, 0, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    page2 = lv_tileview_add_tile(root_tv, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    page4 = lv_tileview_add_tile(root_tv, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    page5 = lv_tileview_add_tile(root_tv, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    page6 = lv_tileview_add_tile(root_tv, 4, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    page7 = lv_tileview_add_tile(root_tv, 5, 0, LV_DIR_LEFT | LV_DIR_RIGHT);

    build_page1();
    build_page2();
    build_page4();
    build_page5();
    build_page6();
    build_page7();

    build_top_bar();
    build_bottom_bar();

    update_bottom_nav_styles(0);

    lv_timer_create(ec_chart_timer_cb, KINETIC_EC_CHART_SAMPLE_PERIOD_MS, NULL);
}

static void build_top_bar(void) {
    lv_obj_t * top = lv_obj_create(lv_scr_act());
    lv_obj_set_size(top, 320, 30);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top, KINETIC_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_80, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_radius(top, 0, 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * line = lv_line_create(top);
    static lv_point_t points[] = { {0, 29}, {320, 29} };
    lv_line_set_points(line, points, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(0x1e201d), 0);

    p_top_title = lv_label_create(top);
    lv_label_set_text(p_top_title, ICON_SENSORS " smarthome_5G");
    lv_obj_set_style_text_color(p_top_title, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(p_top_title, FONT_BASE, 0);
    lv_obj_align(p_top_title, LV_ALIGN_LEFT_MID, 0, 0);
}

static void build_bottom_bar(void) {
    lv_obj_t * bot = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bot, 320, 40);
    lv_obj_align(bot, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bot, KINETIC_COLOR_BG, 0);
    lv_obj_set_style_border_color(bot, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_width(bot, 1, 0);
    lv_obj_set_style_border_side(bot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_radius(bot, 8, 0);
    lv_obj_set_style_pad_all(bot, 0, 0); // Prevent internal clipping
    lv_obj_clear_flag(bot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * flex = lv_obj_create(bot);
    lv_obj_set_size(flex, 320, 40);
    lv_obj_center(flex);
    lv_obj_set_style_bg_opa(flex, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flex, 0, 0);
    lv_obj_set_style_pad_all(flex, 0, 0);
    lv_obj_set_flex_flow(flex, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(flex, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(flex, LV_OBJ_FLAG_SCROLLABLE); // Prevent accidental nav bar scrolling

    btn_nav1 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav1, 48, 38);
    lv_obj_set_style_radius(btn_nav1, 8, 0);
    lv_obj_add_event_cb(btn_nav1, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)0);
    lv_obj_t * l1 = lv_label_create(btn_nav1);
    lv_label_set_text(l1, ICON_DASHBOARD);
    lv_obj_center(l1);

    btn_nav2 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav2, 48, 38);
    lv_obj_set_style_radius(btn_nav2, 8, 0);
    lv_obj_add_event_cb(btn_nav2, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)1);
    lv_obj_t * l2 = lv_label_create(btn_nav2);
    lv_label_set_text(l2, ICON_OPACITY);
    lv_obj_center(l2);

    btn_nav3 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav3, 48, 38);
    lv_obj_set_style_radius(btn_nav3, 8, 0);
    lv_obj_add_event_cb(btn_nav3, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)2);
    lv_obj_t * l3 = lv_label_create(btn_nav3);
    lv_label_set_text(l3, LV_SYMBOL_SETTINGS); // "Setup"
    lv_obj_center(l3);

    btn_nav4 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav4, 48, 38);
    lv_obj_set_style_radius(btn_nav4, 8, 0);
    lv_obj_add_event_cb(btn_nav4, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)3);
    lv_obj_t * l4 = lv_label_create(btn_nav4);
    lv_label_set_text(l4, LV_SYMBOL_PLAY); // "Routines"
    lv_obj_center(l4);

    btn_nav5 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav5, 48, 38);
    lv_obj_set_style_radius(btn_nav5, 8, 0);
    lv_obj_add_event_cb(btn_nav5, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)4);
    lv_obj_t * l5 = lv_label_create(btn_nav5);
    lv_label_set_text(l5, LV_SYMBOL_IMAGE); // "EC Chart"
    lv_obj_center(l5);

    btn_nav6 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav6, 48, 38);
    lv_obj_set_style_radius(btn_nav6, 8, 0);
    lv_obj_add_event_cb(btn_nav6, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)5);
    lv_obj_t * l6 = lv_label_create(btn_nav6);
    lv_label_set_text(l6, LV_SYMBOL_EDIT); // "Dosing Settings"
    lv_obj_center(l6);
}

static void update_bottom_nav_styles(uint8_t active_idx) {
    lv_obj_t * btns[NAV_BTN_COUNT] = {btn_nav1, btn_nav2, btn_nav3, btn_nav4, btn_nav5, btn_nav6};
    for(int i = 0; i < NAV_BTN_COUNT; i++) {
        if(i == active_idx) {
            lv_obj_set_style_bg_color(btns[i], KINETIC_COLOR_PRIMARY, 0);
            lv_obj_set_style_bg_opa(btns[i], LV_OPA_20, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(btns[i], 0), KINETIC_COLOR_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_opa(btns[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(btns[i], 0), KINETIC_COLOR_TEXT_DIM, 0);
        }
    }
}

static void bottom_nav_event_cb(lv_event_t * e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_set_tile_id(root_tv, idx, 0, LV_ANIM_ON);
    update_bottom_nav_styles(idx);
}

static void tileview_scroll_event_cb(lv_event_t * e) {
    lv_obj_t * active = lv_tileview_get_tile_act(root_tv);
    if(active == page1) update_bottom_nav_styles(0);
    else if(active == page2) update_bottom_nav_styles(1);
    else if(active == page4) update_bottom_nav_styles(2);
    else if(active == page5) update_bottom_nav_styles(3);
    else if(active == page6) update_bottom_nav_styles(4);
    else if(active == page7) update_bottom_nav_styles(5);
}


void kinetic_os_set_pump_state(bool on) {
    if(!p4_pump_sw) return;
    if(on) {
        lv_obj_add_state(p4_pump_sw, LV_STATE_CHECKED);
        lv_label_set_text(p4_pump_stat, " ACTIVE");
        lv_obj_set_style_text_color(p4_pump_stat, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_color(p4_pump_dot, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_text_color(p4_pump_icon, KINETIC_COLOR_PRIMARY, 0);
    } else {
        lv_obj_clear_state(p4_pump_sw, LV_STATE_CHECKED);
        lv_label_set_text(p4_pump_stat, " STANDBY");
        lv_obj_set_style_text_color(p4_pump_stat, KINETIC_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_bg_color(p4_pump_dot, KINETIC_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_color(p4_pump_icon, KINETIC_COLOR_TEXT_DIM, 0);
    }
}

void kinetic_os_set_light_state(bool on) {
    if(!p4_light_sw) return;
    if(on) {
        lv_obj_add_state(p4_light_sw, LV_STATE_CHECKED);
        lv_label_set_text(p4_light_stat, " ACTIVE");
        lv_obj_set_style_text_color(p4_light_stat, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_color(p4_light_dot, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_img_recolor(p4_light_icon, KINETIC_COLOR_PRIMARY, 0);
    } else {
        lv_obj_clear_state(p4_light_sw, LV_STATE_CHECKED);
        lv_label_set_text(p4_light_stat, " STANDBY");
        lv_obj_set_style_text_color(p4_light_stat, KINETIC_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_bg_color(p4_light_dot, KINETIC_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_img_recolor(p4_light_icon, KINETIC_COLOR_TEXT_DIM, 0);
    }
}

void kinetic_os_set_fertilizer_a_state(bool on) {
    if(!p4_fert_a_box) return;
    p4_fert_a_on = on;
    lv_label_set_text(p4_fert_a_stat, "PUMP A");
    if(on) {
        lv_obj_set_style_text_color(p4_fert_a_stat, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_color(p4_fert_a_box, lv_color_hex(0x103010), 0);
        lv_obj_set_style_border_color(p4_fert_a_box, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_opa(p4_fert_a_box, LV_OPA_70, 0);
    } else {
        lv_obj_set_style_text_color(p4_fert_a_stat, KINETIC_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_bg_color(p4_fert_a_box, lv_color_hex(0x111311), 0);
        lv_obj_set_style_border_color(p4_fert_a_box, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_opa(p4_fert_a_box, LV_OPA_40, 0);
    }
}

void kinetic_os_set_fertilizer_b_state(bool on) {
    if(!p4_fert_b_box) return;
    p4_fert_b_on = on;
    lv_label_set_text(p4_fert_b_stat, "PUMP B");
    if(on) {
        lv_obj_set_style_text_color(p4_fert_b_stat, lv_color_hex(0xff5a5a), 0);
        lv_obj_set_style_bg_color(p4_fert_b_box, lv_color_hex(0x301010), 0);
        lv_obj_set_style_border_color(p4_fert_b_box, lv_color_hex(0xff5a5a), 0);
        lv_obj_set_style_border_opa(p4_fert_b_box, LV_OPA_80, 0);
    } else {
        lv_obj_set_style_text_color(p4_fert_b_stat, KINETIC_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_bg_color(p4_fert_b_box, lv_color_hex(0x111311), 0);
        lv_obj_set_style_border_color(p4_fert_b_box, lv_color_hex(0xff5a5a), 0);
        lv_obj_set_style_border_opa(p4_fert_b_box, LV_OPA_40, 0);
    }
}

void kinetic_os_set_wifi_name(const char * ssid) {
    if(!p_top_title) return;
    if(ssid == NULL || strlen(ssid) == 0) {
        lv_label_set_text(p_top_title, ICON_SENSORS " smarthome_5G");
    } else {
        lv_label_set_text_fmt(p_top_title, ICON_SENSORS " %s", ssid);
    }
}

void kinetic_os_set_temperature(float celsius) {
    if(!p1_temp_val) return;
    int16_t temp_tenths = (int16_t)(celsius * 10.0f + (celsius >= 0.0f ? 0.5f : -0.5f));
    uint16_t abs_tenths = (uint16_t)(temp_tenths < 0 ? -temp_tenths : temp_tenths);
    lv_label_set_text_fmt(p1_temp_val, "%s%u.%u C", temp_tenths < 0 ? "-" : "", abs_tenths / 10U, abs_tenths % 10U);
}

void kinetic_os_set_humidity(float pct) {
    if(!p1_humidity_val) return;
    if(pct < 0.0f) pct = 0.0f;
    if(pct > 100.0f) pct = 100.0f;
    uint16_t humidity_hundredths = (uint16_t)(pct * 100.0f + 0.5f);
    lv_label_set_text_fmt(p1_humidity_val, "%u.%02u%%", humidity_hundredths / 100U, humidity_hundredths % 100U);
}

void kinetic_os_set_water_level(uint8_t pct) {
    if(!p1_water_level_val) return;
    if(pct > 100U) pct = 100U;
    lv_label_set_text_fmt(p1_water_level_val, "%u%%", (unsigned)pct);
}

void kinetic_os_set_distance(uint16_t cm) {
    if(!p1_distance_val) return;
    lv_label_set_text_fmt(p1_distance_val, "%u mm", (unsigned)cm);
}

void kinetic_os_set_tds(uint16_t ppm) {
    if(p1_tds_val) {
        lv_label_set_text_fmt(p1_tds_val, "%u PPM", (unsigned)ppm);
    }
}

void kinetic_os_set_ec(uint32_t us_per_cm) {
    s_last_ec = us_per_cm;
    s_ec_seen = true;
    if(p1_ec_val) {
        lv_label_set_text_fmt(p1_ec_val, "%u uS/cm", (unsigned)us_per_cm);
    }
}

void kinetic_os_set_temperature_not_detected(void) {
    if(p1_temp_val) lv_label_set_text(p1_temp_val, "Not Detected");
}

void kinetic_os_set_humidity_not_detected(void) {
    if(p1_humidity_val) lv_label_set_text(p1_humidity_val, "Not Detected");
}

void kinetic_os_set_water_level_not_detected(void) {
    if(p1_water_level_val) lv_label_set_text(p1_water_level_val, "Not Detected");
}

void kinetic_os_set_distance_not_detected(void) {
    if(p1_distance_val) lv_label_set_text(p1_distance_val, "Not Detected");
}

void kinetic_os_set_tds_not_detected(void) {
    if(p1_tds_val) lv_label_set_text(p1_tds_val, "Not Detected");
}

void kinetic_os_set_ec_not_detected(void) {
    if(p1_ec_val) lv_label_set_text(p1_ec_val, "Not Detected");
}

static void dose_popup_ok_event_cb(lv_event_t * e) {
    lv_obj_t * backdrop = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_del(backdrop);
    if (s_dose_popup_backdrop == backdrop) {
        s_dose_popup_backdrop = NULL;
    }
}

void kinetic_os_show_dose_complete_popup(const char *routine_label, uint32_t ec_us_cm) {
    // A new completion replaces whatever popup is currently on screen
    if (s_dose_popup_backdrop) {
        lv_obj_del(s_dose_popup_backdrop);
        s_dose_popup_backdrop = NULL;
    }

    lv_obj_t * backdrop = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(backdrop);
    lv_obj_set_size(backdrop, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_70, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE); // eat touches to whatever's behind it

    lv_obj_t * card = lv_obj_create(backdrop);
    lv_obj_set_size(card, 240, 180);
    lv_obj_center(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_color(card, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 12, 0);

    // Checkmark badge
    lv_obj_t * badge = lv_obj_create(card);
    lv_obj_set_size(badge, 48, 48);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t * badge_icon = lv_label_create(badge);
    lv_label_set_text(badge_icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(badge_icon, KINETIC_COLOR_ON_PRIMARY, 0);
    lv_obj_center(badge_icon);

    lv_obj_t * title = lv_label_create(card);
    lv_label_set_text(title, "Target Reached");
    lv_obj_set_style_text_color(title, KINETIC_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 66);

    lv_obj_t * value_lbl = lv_label_create(card);
    lv_label_set_text_fmt(value_lbl, "%s\n%u uS/cm", routine_label, (unsigned)ec_us_cm);
    lv_obj_set_style_text_align(value_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(value_lbl, KINETIC_COLOR_SECONDARY, 0);
    lv_obj_align(value_lbl, LV_ALIGN_TOP_MID, 0, 90);

    lv_obj_t * ok_btn = lv_btn_create(card);
    lv_obj_set_size(ok_btn, 100, 40);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(ok_btn, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_t * ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "OK");
    lv_obj_set_style_text_color(ok_lbl, KINETIC_COLOR_ON_PRIMARY, 0);
    lv_obj_center(ok_lbl);
    lv_obj_add_event_cb(ok_btn, dose_popup_ok_event_cb, LV_EVENT_CLICKED, backdrop);

    s_dose_popup_backdrop = backdrop;
}

void kinetic_os_set_pump_switch_cb(kinetic_os_switch_cb_t cb) { user_pump_cb = cb; }

void kinetic_os_set_light_switch_cb(kinetic_os_switch_cb_t cb) { user_light_cb = cb; }

void kinetic_os_set_fertilizer_a_switch_cb(kinetic_os_switch_cb_t cb) { user_fert_a_cb = cb; }

void kinetic_os_set_fertilizer_b_switch_cb(kinetic_os_switch_cb_t cb) { user_fert_b_cb = cb; }

void kinetic_os_set_light_intensity(uint8_t pct) {
    (void)pct;
}

static void pump_sw_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    kinetic_os_set_pump_state(is_on);
    if(user_pump_cb) user_pump_cb(is_on); // Broadcast to ESP logic
}

static void light_sw_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    kinetic_os_set_light_state(is_on);
    if(user_light_cb) user_light_cb(is_on); // Broadcast to ESP logic
}

static void fert_a_sw_event_cb(lv_event_t * e) {
    (void)e;
    bool is_on = !p4_fert_a_on;
    kinetic_os_set_fertilizer_a_state(is_on);
    if(user_fert_a_cb) user_fert_a_cb(is_on);
}

static void fert_b_sw_event_cb(lv_event_t * e) {
    (void)e;
    bool is_on = !p4_fert_b_on;
    kinetic_os_set_fertilizer_b_state(is_on);
    if(user_fert_b_cb) user_fert_b_cb(is_on);
}


static void build_page1(void) {
    // 6-row list: sensor name left, live value right.
    // 6*24 + 5*4 + 2 = 166px — fits the 170px content area.
    const lv_coord_t ROW_W = 312;
    const lv_coord_t ROW_H = 24;
    const lv_coord_t PAD_X = 4;
    const lv_coord_t GAP   = 4;
    const lv_coord_t TOP   = 2;

    const char *names[6] = {
        "TEMPERATURE", "HUMIDITY", "TDS", "EC", "WATER LEVEL", "DISTANCE"
    };
    const char *defaults[6] = {
        "-- C", "--%", "-- PPM", "-- uS/cm", "--%", "-- mm"
    };
    lv_obj_t **val_ptrs[6] = {
        &p1_temp_val, &p1_humidity_val, &p1_tds_val,
        &p1_ec_val, &p1_water_level_val, &p1_distance_val
    };

    for (int i = 0; i < 6; i++) {
        lv_color_t val_color;
        if      (i == 1) val_color = KINETIC_COLOR_TERTIARY;
        else if (i == 3) val_color = KINETIC_COLOR_TERTIARY;
        else if (i == 4) val_color = KINETIC_COLOR_SECONDARY;
        else             val_color = KINETIC_COLOR_PRIMARY;

        lv_coord_t y = TOP + i * (ROW_H + GAP);

        lv_obj_t *row = lv_obj_create(page1);
        lv_obj_set_size(row, ROW_W, ROW_H);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, PAD_X, y);
        lv_obj_set_style_bg_color(row, KINETIC_COLOR_SURFACE, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, KINETIC_COLOR_OUTLINE, 0);
        lv_obj_set_style_border_opa(row, LV_OPA_30, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, names[i]);
        lv_obj_set_style_text_color(name_lbl, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, 0);
#endif
        lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 8, 0);

        *val_ptrs[i] = lv_label_create(row);
        lv_label_set_text(*val_ptrs[i], defaults[i]);
        lv_obj_set_style_text_color(*val_ptrs[i], val_color, 0);
        lv_obj_set_style_text_font(*val_ptrs[i], FONT_BASE, 0);
        lv_obj_align(*val_ptrs[i], LV_ALIGN_RIGHT_MID, -8, 0);
    }
}

static void cal_start_event_cb(lv_event_t * e) {
    (void)e;
    if(s_cal_start_cb) s_cal_start_cb();
}

static void cal_clear_event_cb(lv_event_t * e) {
    (void)e;
    if(s_cal_clear_cb) s_cal_clear_cb();
}

static void cal_ignore_temp_event_cb(lv_event_t * e) {
    (void)e;
    if(s_cal_ignore_temp_cb) s_cal_ignore_temp_cb();
}

static lv_obj_t * cal_make_row(lv_coord_t y, lv_coord_t h) {
    lv_obj_t * row = lv_obj_create(page2);
    lv_obj_set_size(row, 312, h);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 4, y);
    lv_obj_set_style_bg_color(row, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(row, LV_OPA_30, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

static void build_page2(void) {
    lv_obj_clear_flag(page2, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t * title = lv_label_create(page2);
    lv_label_set_text(title, "EC CALIBRATION");
    lv_obj_set_style_text_color(title, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, FONT_BASE, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    // Row 1: live EC + temperature (y=20)
    lv_obj_t * row1 = cal_make_row(20, 24);
    lv_obj_t * ec_hdr = lv_label_create(row1);
    lv_label_set_text(ec_hdr, "EC:");
    lv_obj_set_style_text_color(ec_hdr, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(ec_hdr, FONT_BASE, 0);
    lv_obj_align(ec_hdr, LV_ALIGN_LEFT_MID, 8, 0);

    p2_cal_raw_ec_label = lv_label_create(row1);
    lv_label_set_text(p2_cal_raw_ec_label, "-- uS/cm");
    lv_obj_set_style_text_color(p2_cal_raw_ec_label, KINETIC_COLOR_TERTIARY, 0);
    lv_obj_set_style_text_font(p2_cal_raw_ec_label, FONT_BASE, 0);
    lv_obj_align(p2_cal_raw_ec_label, LV_ALIGN_LEFT_MID, 30, 0);

    p2_cal_temp_label = lv_label_create(row1);
    lv_label_set_text(p2_cal_temp_label, "T: --.-C");
    lv_obj_set_style_text_color(p2_cal_temp_label, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(p2_cal_temp_label, FONT_BASE, 0);
    lv_obj_align(p2_cal_temp_label, LV_ALIGN_RIGHT_MID, -8, 0);

    // Row 2: status + countdown (y=48)
    lv_obj_t * row2 = cal_make_row(48, 24);
    p2_cal_status_label = lv_label_create(row2);
    lv_label_set_text(p2_cal_status_label, "READY");
    lv_obj_set_style_text_color(p2_cal_status_label, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(p2_cal_status_label, FONT_BASE, 0);
    lv_obj_align(p2_cal_status_label, LV_ALIGN_LEFT_MID, 8, 0);

    p2_cal_timer_label = lv_label_create(row2);
    lv_label_set_text(p2_cal_timer_label, "");
    lv_obj_set_style_text_color(p2_cal_timer_label, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(p2_cal_timer_label, FONT_BASE, 0);
    lv_obj_align(p2_cal_timer_label, LV_ALIGN_RIGHT_MID, -8, 0);

    // Row 3: running average + stored k (y=76)
    lv_obj_t * row3 = cal_make_row(76, 24);
    p2_cal_avg_label = lv_label_create(row3);
    lv_label_set_text(p2_cal_avg_label, "AVG: --");
    lv_obj_set_style_text_color(p2_cal_avg_label, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(p2_cal_avg_label, FONT_BASE, 0);
    lv_obj_align(p2_cal_avg_label, LV_ALIGN_LEFT_MID, 8, 0);

    p2_cal_k_label = lv_label_create(row3);
    lv_label_set_text(p2_cal_k_label, "k=1.000");
    lv_obj_set_style_text_color(p2_cal_k_label, KINETIC_COLOR_TERTIARY, 0);
    lv_obj_set_style_text_font(p2_cal_k_label, FONT_BASE, 0);
    lv_obj_align(p2_cal_k_label, LV_ALIGN_RIGHT_MID, -8, 0);

    // Row 4: START button + IGNORE TEMP toggle (y=104)
    lv_obj_t * start_btn = lv_btn_create(page2);
    lv_obj_set_size(start_btn, 148, 32);
    lv_obj_align(start_btn, LV_ALIGN_TOP_LEFT, 4, 104);
    lv_obj_set_style_bg_color(start_btn, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(start_btn, 1, 0);
    lv_obj_set_style_border_color(start_btn, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_opa(start_btn, LV_OPA_60, 0);
    lv_obj_set_style_radius(start_btn, 8, 0);
    lv_obj_add_event_cb(start_btn, cal_start_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * start_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_lbl, LV_SYMBOL_PLAY " START CAL");
    lv_obj_set_style_text_color(start_lbl, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(start_lbl, FONT_BASE, 0);
    lv_obj_center(start_lbl);

    lv_obj_t * ignore_btn = lv_btn_create(page2);
    lv_obj_set_size(ignore_btn, 160, 32);
    lv_obj_align(ignore_btn, LV_ALIGN_TOP_RIGHT, -4, 104);
    lv_obj_set_style_bg_color(ignore_btn, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(ignore_btn, 1, 0);
    lv_obj_set_style_border_color(ignore_btn, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(ignore_btn, LV_OPA_50, 0);
    lv_obj_set_style_radius(ignore_btn, 8, 0);
    lv_obj_add_event_cb(ignore_btn, cal_ignore_temp_event_cb, LV_EVENT_CLICKED, NULL);

    p2_cal_ignore_label = lv_label_create(ignore_btn);
    lv_label_set_text(p2_cal_ignore_label, "IGNORE T: OFF");
    lv_obj_set_style_text_color(p2_cal_ignore_label, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(p2_cal_ignore_label, FONT_BASE, 0);
    lv_obj_center(p2_cal_ignore_label);

    // Row 5: CLEAR calibration (y=140)
    lv_obj_t * clear_btn = lv_btn_create(page2);
    lv_obj_set_size(clear_btn, 312, 28);
    lv_obj_align(clear_btn, LV_ALIGN_TOP_LEFT, 4, 140);
    lv_obj_set_style_bg_color(clear_btn, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(clear_btn, 1, 0);
    lv_obj_set_style_border_color(clear_btn, lv_color_hex(0xff5a5a), 0);
    lv_obj_set_style_border_opa(clear_btn, LV_OPA_40, 0);
    lv_obj_set_style_radius(clear_btn, 8, 0);
    lv_obj_add_event_cb(clear_btn, cal_clear_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * clear_lbl = lv_label_create(clear_btn);
    lv_label_set_text(clear_lbl, LV_SYMBOL_TRASH " CLEAR CALIBRATION");
    lv_obj_set_style_text_color(clear_lbl, lv_color_hex(0xff5a5a), 0);
    lv_obj_set_style_text_font(clear_lbl, FONT_BASE, 0);
    lv_obj_center(clear_lbl);
}

static void build_page4(void) {
    lv_obj_t * flex = lv_obj_create(page4);
    lv_obj_set_size(flex, 320, 170);
    lv_obj_center(flex);
    lv_obj_set_style_bg_opa(flex, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flex, 0, 0);
    lv_obj_set_style_pad_all(flex, 5, 0);
    lv_obj_set_flex_flow(flex, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(flex, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(flex, 3, 0);
    lv_obj_clear_flag(flex, LV_OBJ_FLAG_SCROLLABLE);

    // Row 1: Fertilizer A and B in one row
    lv_obj_t * row1 = lv_obj_create(flex);
    lv_obj_set_size(row1, 300, 56);
    lv_obj_set_style_bg_color(row1, lv_color_hex(0x1a1c1a), 0);
    lv_obj_set_style_border_color(row1, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_width(row1, 1, 0);
    lv_obj_set_style_border_opa(row1, LV_OPA_20, 0);
    lv_obj_set_style_radius(row1, 12, 0);
    lv_obj_set_style_pad_all(row1, 6, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);

    p4_fert_a_box = lv_obj_create(row1);
    lv_obj_set_size(p4_fert_a_box, 142, 44);
    lv_obj_set_style_bg_color(p4_fert_a_box, lv_color_hex(0x111311), 0);
    lv_obj_set_style_border_color(p4_fert_a_box, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(p4_fert_a_box, 1, 0);
    lv_obj_set_style_border_opa(p4_fert_a_box, LV_OPA_40, 0);
    lv_obj_set_style_radius(p4_fert_a_box, 10, 0);
    lv_obj_set_style_pad_all(p4_fert_a_box, 4, 0);
    lv_obj_set_flex_flow(p4_fert_a_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p4_fert_a_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(p4_fert_a_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(p4_fert_a_box, fert_a_sw_event_cb, LV_EVENT_CLICKED, NULL);

    p4_fert_a_stat = lv_label_create(p4_fert_a_box);
    lv_label_set_text(p4_fert_a_stat, "PUMP A");
    lv_obj_set_style_text_color(p4_fert_a_stat, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_16
    lv_obj_set_style_text_font(p4_fert_a_stat, &lv_font_montserrat_16, 0);
#else
    lv_obj_set_style_text_font(p4_fert_a_stat, FONT_BASE, 0);
#endif
    lv_obj_center(p4_fert_a_stat);

    p4_fert_b_box = lv_obj_create(row1);
    lv_obj_set_size(p4_fert_b_box, 142, 44);
    lv_obj_set_style_bg_color(p4_fert_b_box, lv_color_hex(0x111311), 0);
    lv_obj_set_style_border_color(p4_fert_b_box, lv_color_hex(0xff5a5a), 0);
    lv_obj_set_style_border_width(p4_fert_b_box, 1, 0);
    lv_obj_set_style_border_opa(p4_fert_b_box, LV_OPA_40, 0);
    lv_obj_set_style_radius(p4_fert_b_box, 10, 0);
    lv_obj_set_style_pad_all(p4_fert_b_box, 4, 0);
    lv_obj_set_flex_flow(p4_fert_b_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p4_fert_b_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(p4_fert_b_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(p4_fert_b_box, fert_b_sw_event_cb, LV_EVENT_CLICKED, NULL);

    p4_fert_b_stat = lv_label_create(p4_fert_b_box);
    lv_label_set_text(p4_fert_b_stat, "PUMP B");
    lv_obj_set_style_text_color(p4_fert_b_stat, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_16
    lv_obj_set_style_text_font(p4_fert_b_stat, &lv_font_montserrat_16, 0);
#else
    lv_obj_set_style_text_font(p4_fert_b_stat, FONT_BASE, 0);
#endif
    lv_obj_center(p4_fert_b_stat);

    kinetic_os_set_fertilizer_a_state(false);
    kinetic_os_set_fertilizer_b_state(false);

    // Row 2: Circulation Pump (same style as before)
    lv_obj_t * row2 = lv_obj_create(flex);
    lv_obj_set_size(row2, 300, 50);
    lv_obj_set_style_bg_color(row2, lv_color_hex(0x1a1c1a), 0);
    lv_obj_set_style_border_color(row2, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(row2, 1, 0);
    lv_obj_set_style_border_opa(row2, LV_OPA_20, 0);
    lv_obj_set_style_radius(row2, 12, 0);
    lv_obj_set_style_pad_all(row2, 5, 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

    // Left side of Row 2
    lv_obj_t * l2 = lv_obj_create(row2);
    lv_obj_set_size(l2, 210, 50);
    lv_obj_set_style_bg_opa(l2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(l2, 0, 0);
    lv_obj_set_style_pad_all(l2, 0, 0);
    lv_obj_set_flex_flow(l2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(l2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(l2, LV_OBJ_FLAG_SCROLLABLE);

    // Circle Icon
    lv_obj_t * circ2 = lv_obj_create(l2);
    lv_obj_set_size(circ2, 40, 40);
    lv_obj_set_style_radius(circ2, 20, 0);
    lv_obj_set_style_bg_color(circ2, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(circ2, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(circ2, 1, 0);
    lv_obj_set_style_border_opa(circ2, LV_OPA_40, 0);
    lv_obj_clear_flag(circ2, LV_OBJ_FLAG_SCROLLABLE);

    p4_pump_icon = lv_label_create(circ2);
    lv_label_set_text(p4_pump_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(p4_pump_icon, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_center(p4_pump_icon);

    // VBox for text
    lv_obj_t * vbox2 = lv_obj_create(l2);
    lv_obj_set_size(vbox2, 150, 40);
    lv_obj_set_style_bg_opa(vbox2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vbox2, 0, 0);
    lv_obj_set_style_pad_all(vbox2, 0, 0);
    lv_obj_set_style_pad_left(vbox2, 0, 0);
    lv_obj_set_flex_flow(vbox2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(vbox2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(vbox2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(vbox2, LV_ALIGN_LEFT_MID, 48, 0);

    lv_obj_t * t2 = lv_label_create(vbox2);
    lv_label_set_text(t2, "CIRCULATION");
    lv_obj_set_style_text_color(t2, lv_color_hex(0xababa8), 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(t2, &lv_font_montserrat_12, 0);
#endif

    // HBox for active dot
    lv_obj_t * hbox2 = lv_obj_create(vbox2);
    lv_obj_set_size(hbox2, 110, 20);
    lv_obj_set_style_bg_opa(hbox2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hbox2, 0, 0);
    lv_obj_set_style_pad_all(hbox2, 0, 0);
    lv_obj_set_flex_flow(hbox2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hbox2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hbox2, LV_OBJ_FLAG_SCROLLABLE);

    p4_pump_dot = lv_obj_create(hbox2);
    lv_obj_set_size(p4_pump_dot, 8, 8);
    lv_obj_set_style_radius(p4_pump_dot, 4, 0);
    lv_obj_set_style_bg_color(p4_pump_dot, lv_color_hex(0xababa8), 0);
    lv_obj_set_style_border_width(p4_pump_dot, 0, 0);
    lv_obj_clear_flag(p4_pump_dot, LV_OBJ_FLAG_SCROLLABLE);

    p4_pump_stat = lv_label_create(hbox2);
    lv_label_set_text(p4_pump_stat, " STANDBY");
    lv_obj_set_style_text_color(p4_pump_stat, lv_color_hex(0xababa8), 0);
    lv_obj_set_style_text_font(p4_pump_stat, FONT_BASE, 0);

    // Switch
    p4_pump_sw = lv_switch_create(row2);
    lv_obj_set_style_bg_color(p4_pump_sw, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(p4_pump_sw, pump_sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    kinetic_os_set_pump_state(false);

    // Row 3: Grow Light (same style)
    lv_obj_t * row3 = lv_obj_create(flex);
    lv_obj_set_size(row3, 300, 50);
    lv_obj_set_style_bg_color(row3, lv_color_hex(0x1a1c1a), 0);
    lv_obj_set_style_border_color(row3, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_width(row3, 1, 0);
    lv_obj_set_style_border_opa(row3, LV_OPA_20, 0);
    lv_obj_set_style_radius(row3, 12, 0);
    lv_obj_set_style_pad_all(row3, 5, 0);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row3, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * l3 = lv_obj_create(row3);
    lv_obj_set_size(l3, 210, 50);
    lv_obj_set_style_bg_opa(l3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(l3, 0, 0);
    lv_obj_set_style_pad_all(l3, 0, 0);
    lv_obj_set_flex_flow(l3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(l3, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(l3, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * circ3 = lv_obj_create(l3);
    lv_obj_set_size(circ3, 40, 40);
    lv_obj_set_style_radius(circ3, 20, 0);
    lv_obj_set_style_bg_color(circ3, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(circ3, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_width(circ3, 1, 0);
    lv_obj_set_style_border_opa(circ3, LV_OPA_40, 0);
    lv_obj_clear_flag(circ3, LV_OBJ_FLAG_SCROLLABLE);

    extern const lv_img_dsc_t lightbulb_img;
    p4_light_icon = lv_img_create(circ3);
    lv_img_set_src(p4_light_icon, &lightbulb_img);
    lv_obj_set_style_img_recolor_opa(p4_light_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(p4_light_icon, lv_color_hex(0xababa8), 0);
    lv_obj_center(p4_light_icon);

    lv_obj_t * vbox3 = lv_obj_create(l3);
    lv_obj_set_size(vbox3, 150, 40);
    lv_obj_set_style_bg_opa(vbox3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vbox3, 0, 0);
    lv_obj_set_style_pad_all(vbox3, 0, 0);
    lv_obj_set_style_pad_left(vbox3, 0, 0);
    lv_obj_set_flex_flow(vbox3, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(vbox3, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(vbox3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(vbox3, LV_ALIGN_LEFT_MID, 48, 0);

    lv_obj_t * t3 = lv_label_create(vbox3);
    lv_label_set_text(t3, "GROW_LIGHT");
    lv_obj_set_style_text_color(t3, lv_color_hex(0xababa8), 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(t3, &lv_font_montserrat_12, 0);
#endif

    lv_obj_t * hbox3 = lv_obj_create(vbox3);
    lv_obj_set_size(hbox3, 110, 20);
    lv_obj_set_style_bg_opa(hbox3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hbox3, 0, 0);
    lv_obj_set_style_pad_all(hbox3, 0, 0);
    lv_obj_set_flex_flow(hbox3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hbox3, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hbox3, LV_OBJ_FLAG_SCROLLABLE);

    p4_light_dot = lv_obj_create(hbox3);
    lv_obj_set_size(p4_light_dot, 8, 8);
    lv_obj_set_style_radius(p4_light_dot, 4, 0);
    lv_obj_set_style_bg_color(p4_light_dot, lv_color_hex(0xababa8), 0);
    lv_obj_set_style_border_width(p4_light_dot, 0, 0);
    lv_obj_clear_flag(p4_light_dot, LV_OBJ_FLAG_SCROLLABLE);

    p4_light_stat = lv_label_create(hbox3);
    lv_label_set_text(p4_light_stat, " STANDBY");
    lv_obj_set_style_text_color(p4_light_stat, lv_color_hex(0xababa8), 0);
    lv_obj_set_style_text_font(p4_light_stat, FONT_BASE, 0);

    p4_light_sw = lv_switch_create(row3);
    lv_obj_set_style_bg_color(p4_light_sw, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(p4_light_sw, light_sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    kinetic_os_set_light_state(false);
}

// --- Page 5: Dosing Routines ---

void kinetic_os_set_routine_cb(kinetic_os_routine_cb_t cb) { user_routine_cb = cb; }

void kinetic_os_set_routine_state(kinetic_routine_t routine, bool active) {
    if(routine >= KINETIC_ROUTINE_COUNT) return;
    if(!p5_routine_btns[routine]) return;
    if(p5_routine_active[routine] == active) return; // called every UI tick, skip redundant restyling
    p5_routine_active[routine] = active;

    lv_obj_t * btn = p5_routine_btns[routine];
    lv_obj_t * label = p5_routine_labels[routine];
    if(active) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x183018), 0);
        lv_obj_set_style_border_color(btn, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_opa(btn, LV_OPA_80, 0);
        lv_obj_set_style_text_color(label, KINETIC_COLOR_PRIMARY, 0);
    } else {
        lv_obj_set_style_bg_color(btn, KINETIC_COLOR_SURFACE, 0);
        lv_obj_set_style_border_color(btn, KINETIC_COLOR_OUTLINE, 0);
        lv_obj_set_style_border_opa(btn, LV_OPA_50, 0);
        lv_obj_set_style_text_color(label, KINETIC_COLOR_TEXT_DIM, 0);
    }
}

static void routine_btn_event_cb(lv_event_t * e) {
    kinetic_routine_t routine = (kinetic_routine_t)(uintptr_t)lv_event_get_user_data(e);
    if(user_routine_cb) user_routine_cb(routine);
}

static lv_obj_t * create_routine_btn(kinetic_routine_t routine, const char * text,
                                     lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t * btn = lv_btn_create(page5);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_50, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, routine_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)routine);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(label, FONT_BASE, 0);
    lv_obj_center(label);

    p5_routine_btns[routine] = btn;
    p5_routine_labels[routine] = label;
    p5_routine_active[routine] = false;
    return btn;
}

void kinetic_os_set_ec_adjust_cb(kinetic_os_ec_adjust_cb_t cb) { user_ec_adjust_cb = cb; }

void kinetic_os_set_target_ec_display(float ec_ms_cm) {
    if(!p5_target_ec_label) return;
    int tenths = (int)(ec_ms_cm * 10.0f + 0.5f);
    if(tenths == p5_target_ec_tenths) return; // called every UI tick, skip redundant redraws
    p5_target_ec_tenths = tenths;
    lv_label_set_text_fmt(p5_target_ec_label, "%d.%d mS/cm", tenths / 10, tenths % 10);
}

static void ec_adjust_btn_event_cb(lv_event_t * e) {
    bool increase = (bool)(uintptr_t)lv_event_get_user_data(e);
    if(user_ec_adjust_cb) user_ec_adjust_cb(increase);
}

static lv_obj_t * create_ec_adjust_btn(const char * text, bool increase,
                                       lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t * btn = lv_btn_create(page5);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_50, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, ec_adjust_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)increase);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(label, FONT_BASE, 0);
    lv_obj_center(label);
    return btn;
}

static void build_page5(void) {
    lv_obj_clear_flag(page5, LV_OBJ_FLAG_SCROLLABLE);

    create_routine_btn(KINETIC_ROUTINE_PRIME_A, LV_SYMBOL_TINT " PRIME A", 5, 5, 150, 48);
    create_routine_btn(KINETIC_ROUTINE_PRIME_B, LV_SYMBOL_TINT " PRIME B", 165, 5, 150, 48);
    create_routine_btn(KINETIC_ROUTINE_SHOT_A, LV_SYMBOL_CHARGE " SHOT A", 5, 58, 150, 48);
    create_routine_btn(KINETIC_ROUTINE_SHOT_B, LV_SYMBOL_CHARGE " SHOT B", 165, 58, 150, 48);

    // Bottom row: [-] [TARGET DOSE A+B + EC readout] [+]
    create_ec_adjust_btn(LV_SYMBOL_MINUS, false, 5, 111, 48, 48);
    lv_obj_t * target_btn = create_routine_btn(KINETIC_ROUTINE_TARGET_AB, LV_SYMBOL_REFRESH " TARGET DOSE A+B", 58, 111, 204, 48);
    create_ec_adjust_btn(LV_SYMBOL_PLUS, true, 267, 111, 48, 48);

    // Make room for the EC readout under the main label
    lv_obj_align(p5_routine_labels[KINETIC_ROUTINE_TARGET_AB], LV_ALIGN_CENTER, 0, -8);

    p5_target_ec_label = lv_label_create(target_btn);
    lv_label_set_text(p5_target_ec_label, "-.- mS/cm");
    lv_obj_set_style_text_color(p5_target_ec_label, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(p5_target_ec_label, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(p5_target_ec_label, LV_ALIGN_CENTER, 0, 10);
}

// --- Page 6: EC History Chart ---

static void ec_chart_timer_cb(lv_timer_t * timer) {
    (void)timer;
    if(!p6_chart || !p6_ec_series) return;
    if(!s_ec_seen) return; // no sensor reading yet, keep the chart empty

    // Keep our own copy of the visible window to autoscale the Y axis
    p6_ec_samples[p6_ec_sample_head] = s_last_ec;
    p6_ec_sample_head = (p6_ec_sample_head + 1) % KINETIC_EC_CHART_POINT_COUNT;
    if(p6_ec_sample_count < KINETIC_EC_CHART_POINT_COUNT) p6_ec_sample_count++;

    uint32_t max_ec = 0;
    for(uint16_t i = 0; i < p6_ec_sample_count; i++) {
        if(p6_ec_samples[i] > max_ec) max_ec = p6_ec_samples[i];
    }
    uint32_t range_max = max_ec + max_ec / 10U; // 10% headroom
    if(range_max < 100U) range_max = 100U;
    lv_chart_set_range(p6_chart, LV_CHART_AXIS_PRIMARY_Y, 0, (lv_coord_t)range_max);

    lv_chart_set_next_value(p6_chart, p6_ec_series, (lv_coord_t)s_last_ec);

    if(p6_ec_value_label) {
        lv_label_set_text_fmt(p6_ec_value_label, "%u uS/cm", (unsigned)s_last_ec);
    }
}

static void build_page6(void) {
    lv_obj_clear_flag(page6, LV_OBJ_FLAG_SCROLLABLE);

    p6_chart = lv_chart_create(page6);
    lv_obj_set_size(p6_chart, 314, 164);
    lv_obj_align(p6_chart, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(p6_chart, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_color(p6_chart, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_width(p6_chart, 1, 0);
    lv_obj_set_style_border_opa(p6_chart, LV_OPA_30, 0);
    lv_obj_set_style_radius(p6_chart, 8, 0);
    lv_obj_set_style_pad_all(p6_chart, 4, 0);
    lv_obj_set_style_line_color(p6_chart, lv_color_hex(0x1e201d), LV_PART_MAIN); // division lines
    lv_obj_set_style_size(p6_chart, 0, LV_PART_INDICATOR); // hide point dots

    lv_chart_set_type(p6_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(p6_chart, KINETIC_EC_CHART_POINT_COUNT);
    lv_chart_set_update_mode(p6_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(p6_chart, 4, 6);
    lv_chart_set_range(p6_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);

    p6_ec_series = lv_chart_add_series(p6_chart, KINETIC_COLOR_TERTIARY, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(p6_chart, p6_ec_series, LV_CHART_POINT_NONE);

    lv_obj_t * title = lv_label_create(p6_chart);
    lv_label_set_text(title, "EC - 3 MIN");
    lv_obj_set_style_text_color(title, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 2);

    p6_ec_value_label = lv_label_create(p6_chart);
    lv_label_set_text(p6_ec_value_label, "-- uS/cm");
    lv_obj_set_style_text_color(p6_ec_value_label, KINETIC_COLOR_TERTIARY, 0);
    lv_obj_set_style_text_font(p6_ec_value_label, FONT_BASE, 0);
    lv_obj_align(p6_ec_value_label, LV_ALIGN_TOP_RIGHT, -4, 2);
}

// --- Page 7: Dosing Settings ---

void kinetic_os_set_shot_dose_setting_cb(kinetic_os_setting_cb_t cb) { user_shot_dose_setting_cb = cb; }

void kinetic_os_set_mix_interval_setting_cb(kinetic_os_setting_cb_t cb) { user_mix_interval_setting_cb = cb; }

// Renders milliseconds as seconds with one decimal, e.g. 500 -> "0.5s"
static void setting_label_set_seconds(lv_obj_t * label, uint32_t ms) {
    lv_label_set_text_fmt(label, "%u.%us", (unsigned)(ms / 1000U), (unsigned)((ms % 1000U) / 100U));
}

static void shot_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    uint32_t ms = (uint32_t)lv_slider_get_value(slider) * KINETIC_SHOT_DOSE_STEP_MS;
    if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        setting_label_set_seconds(p7_shot_value_label, ms);
    } else if(lv_event_get_code(e) == LV_EVENT_RELEASED) {
        // Only commit on release so a drag doesn't spam NVS writes
        if(user_shot_dose_setting_cb) user_shot_dose_setting_cb(ms);
    }
}

static void mix_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    uint32_t ms = (uint32_t)lv_slider_get_value(slider) * KINETIC_MIX_INTERVAL_STEP_MS;
    if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        setting_label_set_seconds(p7_mix_value_label, ms);
    } else if(lv_event_get_code(e) == LV_EVENT_RELEASED) {
        if(user_mix_interval_setting_cb) user_mix_interval_setting_cb(ms);
    }
}

void kinetic_os_set_shot_dose_setting(uint32_t ms) {
    if(!p7_shot_slider || !p7_shot_value_label) return;
    if(ms < KINETIC_SHOT_DOSE_MIN_MS) ms = KINETIC_SHOT_DOSE_MIN_MS;
    if(ms > KINETIC_SHOT_DOSE_MAX_MS) ms = KINETIC_SHOT_DOSE_MAX_MS;
    lv_slider_set_value(p7_shot_slider, ms / KINETIC_SHOT_DOSE_STEP_MS, LV_ANIM_OFF);
    setting_label_set_seconds(p7_shot_value_label, ms);
}

void kinetic_os_set_mix_interval_setting(uint32_t ms) {
    if(!p7_mix_slider || !p7_mix_value_label) return;
    if(ms < KINETIC_MIX_INTERVAL_MIN_MS) ms = KINETIC_MIX_INTERVAL_MIN_MS;
    if(ms > KINETIC_MIX_INTERVAL_MAX_MS) ms = KINETIC_MIX_INTERVAL_MAX_MS;
    lv_slider_set_value(p7_mix_slider, ms / KINETIC_MIX_INTERVAL_STEP_MS, LV_ANIM_OFF);
    setting_label_set_seconds(p7_mix_value_label, ms);
}

// Builds one settings row and returns the slider; *value_label_out gets the value label
static lv_obj_t * create_setting_row(const char * title, lv_coord_t y,
                                     int32_t range_min, int32_t range_max,
                                     lv_event_cb_t event_cb, lv_obj_t ** value_label_out) {
    lv_obj_t * card = lv_obj_create(page7);
    lv_obj_set_size(card, 310, 76);
    lv_obj_set_pos(card, 5, y);
    lv_obj_set_style_bg_color(card, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * value_label = lv_label_create(card);
    lv_label_set_text(value_label, "-.-s");
    lv_obj_set_style_text_color(value_label, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(value_label, FONT_BASE, 0);
    lv_obj_align(value_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    *value_label_out = value_label;

    lv_obj_t * slider = lv_slider_create(card);
    lv_obj_set_size(slider, 270, 8);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_slider_set_range(slider, range_min, range_max);
    lv_obj_set_style_bg_color(slider, KINETIC_COLOR_SURFACE_VARIANT, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, KINETIC_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, event_cb, LV_EVENT_RELEASED, NULL);
    return slider;
}

static void build_page7(void) {
    lv_obj_clear_flag(page7, LV_OBJ_FLAG_SCROLLABLE);

    // Slider values are in step units so LVGL snaps to the increments natively
    p7_shot_slider = create_setting_row("SHOT DOSE", 5,
                                        KINETIC_SHOT_DOSE_MIN_MS / KINETIC_SHOT_DOSE_STEP_MS,
                                        KINETIC_SHOT_DOSE_MAX_MS / KINETIC_SHOT_DOSE_STEP_MS,
                                        shot_slider_event_cb, &p7_shot_value_label);
    p7_mix_slider = create_setting_row("MIX INTERVAL", 89,
                                       KINETIC_MIX_INTERVAL_MIN_MS / KINETIC_MIX_INTERVAL_STEP_MS,
                                       KINETIC_MIX_INTERVAL_MAX_MS / KINETIC_MIX_INTERVAL_STEP_MS,
                                       mix_slider_event_cb, &p7_mix_value_label);

    kinetic_os_set_shot_dose_setting(KINETIC_SHOT_DOSE_MIN_MS);
    kinetic_os_set_mix_interval_setting(KINETIC_MIX_INTERVAL_MIN_MS);
}

// --- Page 2: EC Calibration API ---

void kinetic_os_update_cal(const kinetic_cal_update_t *d) {
    if(!d) return;

    if(p2_cal_raw_ec_label) {
        lv_label_set_text_fmt(p2_cal_raw_ec_label, "%u uS/cm", (unsigned)d->raw_ec);
    }
    if(p2_cal_temp_label) {
        int16_t tt = (int16_t)(d->temperature * 10.0f + (d->temperature >= 0.0f ? 0.5f : -0.5f));
        int16_t abs_tt = tt < 0 ? -tt : tt;
        lv_label_set_text_fmt(p2_cal_temp_label, "T:%s%d.%dC",
                              tt < 0 ? "-" : "", abs_tt / 10, abs_tt % 10);
    }
    if(p2_cal_status_label && d->status_text) {
        lv_label_set_text(p2_cal_status_label, d->status_text);
    }
    if(p2_cal_timer_label) {
        if(d->seconds_remaining > 0) {
            lv_label_set_text_fmt(p2_cal_timer_label, "%us", (unsigned)d->seconds_remaining);
        } else {
            lv_label_set_text(p2_cal_timer_label, "");
        }
    }
    if(p2_cal_avg_label) {
        if(d->n_samples > 0) {
            lv_label_set_text_fmt(p2_cal_avg_label, "AVG:%u n=%u",
                                  (unsigned)d->running_avg, (unsigned)d->n_samples);
        } else {
            lv_label_set_text(p2_cal_avg_label, "AVG: --");
        }
    }
    if(p2_cal_k_label) {
        uint32_t k_thou = (uint32_t)(d->stored_k * 1000.0f + 0.5f);
        lv_label_set_text_fmt(p2_cal_k_label, "k=%lu.%03lu", (unsigned long)(k_thou / 1000U), (unsigned long)(k_thou % 1000U));
    }
    if(p2_cal_ignore_label) {
        if(d->ignore_temp_active) {
            lv_label_set_text(p2_cal_ignore_label, "IGNORE T: ON");
            lv_obj_set_style_text_color(p2_cal_ignore_label, KINETIC_COLOR_PRIMARY, 0);
        } else {
            lv_label_set_text(p2_cal_ignore_label, "IGNORE T: OFF");
            lv_obj_set_style_text_color(p2_cal_ignore_label, KINETIC_COLOR_TEXT_DIM, 0);
        }
    }
}

void kinetic_os_set_cal_start_cb(kinetic_os_cal_start_cb_t cb)       { s_cal_start_cb = cb; }
void kinetic_os_set_cal_clear_cb(kinetic_os_cal_clear_cb_t cb)       { s_cal_clear_cb = cb; }
void kinetic_os_set_cal_ignore_temp_cb(kinetic_os_cal_ignore_temp_cb_t cb) { s_cal_ignore_temp_cb = cb; }
