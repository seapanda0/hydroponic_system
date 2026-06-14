#include "kinetic_os.h"

// Global UI Elements
static lv_obj_t * root_tv;
static lv_obj_t * page1;
static lv_obj_t * page2;
static lv_obj_t * page4;
static lv_obj_t * btn_nav1;
static lv_obj_t * btn_nav2;
static lv_obj_t * btn_nav3;

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

// Page 2 dynamic references for TDS/EC
static lv_obj_t * p2_tds_label;
static lv_obj_t * p2_ec_label;
static lv_obj_t * p2_tds_arc;
static lv_obj_t * p2_ec_arc;

// Page 2 Dynamic Elements
static lv_obj_t * p2_auto_concentration_btn;
static lv_obj_t * p2_auto_concentration_state;
static bool p2_auto_concentration_on = false;

// Fonts
#define FONT_BASE &lv_font_montserrat_14

// Icons mapped to LVGL built-in symbols
#define ICON_SENSORS LV_SYMBOL_WIFI
#define ICON_DASHBOARD LV_SYMBOL_LIST
#define ICON_OPACITY LV_SYMBOL_TINT

static void build_top_bar(void);
static void build_bottom_bar(void);
static void build_page1(void);
static void build_page2(void);
static void build_page4(void);
static void bottom_nav_event_cb(lv_event_t * e);
static void tileview_scroll_event_cb(lv_event_t * e);
static void update_bottom_nav_styles(uint8_t active_idx);

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

    build_page1();
    build_page2();
    build_page4();

    build_top_bar();
    build_bottom_bar();

    update_bottom_nav_styles(0);
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
    lv_obj_set_size(btn_nav1, 104, 38);
    lv_obj_set_style_radius(btn_nav1, 8, 0);
    lv_obj_add_event_cb(btn_nav1, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)0);
    lv_obj_t * l1 = lv_label_create(btn_nav1);
    lv_label_set_text(l1, ICON_DASHBOARD);
    lv_obj_center(l1);

    btn_nav2 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav2, 104, 38);
    lv_obj_set_style_radius(btn_nav2, 8, 0);
    lv_obj_add_event_cb(btn_nav2, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)1);
    lv_obj_t * l2 = lv_label_create(btn_nav2);
    lv_label_set_text(l2, ICON_OPACITY);
    lv_obj_center(l2);

    btn_nav3 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav3, 104, 38);
    lv_obj_set_style_radius(btn_nav3, 8, 0);
    lv_obj_add_event_cb(btn_nav3, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)2);
    lv_obj_t * l3 = lv_label_create(btn_nav3);
    lv_label_set_text(l3, LV_SYMBOL_SETTINGS); // "Setup"
    lv_obj_center(l3);
}

static void update_bottom_nav_styles(uint8_t active_idx) {
    lv_obj_t * btns[3] = {btn_nav1, btn_nav2, btn_nav3};
    for(int i = 0; i < 3; i++) {
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
}

static void arc_tds_event_cb(lv_event_t * e) {
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    int16_t v = lv_arc_get_value(arc);
    lv_label_set_text_fmt(label, "%d", v);
}

static void arc_ec_event_cb(lv_event_t * e) {
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    int16_t v = lv_arc_get_value(arc);
    lv_label_set_text_fmt(label, "%d.%d", v / 10, v % 10); // Display 8 as 0.8
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

void kinetic_os_set_tds(uint16_t ppm) {
    if(p1_tds_val) {
        lv_label_set_text_fmt(p1_tds_val, "%u", (unsigned)ppm);
    }
    if(p2_tds_label) {
        lv_label_set_text_fmt(p2_tds_label, "%u", (unsigned)ppm);
    }
    if(p2_tds_arc) {
        uint16_t v = ppm > 1000 ? 1000 : ppm;
        lv_arc_set_value(p2_tds_arc, v);
    }
}

void kinetic_os_set_ec(uint32_t us_per_cm) {
    if(p1_ec_val) {
        lv_label_set_text_fmt(p1_ec_val, "%u", (unsigned)us_per_cm);
    }
    if(p2_ec_label) {
        lv_label_set_text_fmt(p2_ec_label, "%u", (unsigned)us_per_cm);
    }
    if(p2_ec_arc) {
        uint32_t v = us_per_cm;
        if(v > 5000U) v = 5000U;
        lv_arc_set_value(p2_ec_arc, v);
    }
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

static void set_auto_concentration_state(bool on) {
    p2_auto_concentration_on = on;
    if(!p2_auto_concentration_btn || !p2_auto_concentration_state) return;

    if(on) {
        lv_obj_set_style_bg_color(p2_auto_concentration_btn, lv_color_hex(0x183018), 0);
        lv_obj_set_style_border_color(p2_auto_concentration_btn, KINETIC_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_opa(p2_auto_concentration_btn, LV_OPA_80, 0);
        lv_label_set_text(p2_auto_concentration_state, "ON");
        lv_obj_set_style_text_color(p2_auto_concentration_state, KINETIC_COLOR_PRIMARY, 0);
    } else {
        lv_obj_set_style_bg_color(p2_auto_concentration_btn, KINETIC_COLOR_SURFACE, 0);
        lv_obj_set_style_border_color(p2_auto_concentration_btn, KINETIC_COLOR_OUTLINE, 0);
        lv_obj_set_style_border_opa(p2_auto_concentration_btn, LV_OPA_50, 0);
        lv_label_set_text(p2_auto_concentration_state, "OFF");
        lv_obj_set_style_text_color(p2_auto_concentration_state, KINETIC_COLOR_TEXT_DIM, 0);
    }
}

static void auto_concentration_event_cb(lv_event_t * e) {
    (void)e;
    set_auto_concentration_state(!p2_auto_concentration_on);
}

static void build_page1(void) {
    lv_obj_t * grid = lv_obj_create(page1);
    lv_obj_set_size(grid, 320, 170);
    lv_obj_center(grid);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 4, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    // 3x2 summary cards so all key water metrics fit in one page.
    const lv_coord_t card_w = 100;
    const lv_coord_t card_h = 76;
    const lv_coord_t col_x[3] = {4, 109, 214};
    const lv_coord_t row_y[2] = {4, 84};

    lv_obj_t * c1 = lv_obj_create(grid);
    lv_obj_set_size(c1, card_w, card_h);
    lv_obj_align(c1, LV_ALIGN_TOP_LEFT, col_x[0], row_y[0]);
    lv_obj_set_style_bg_color(c1, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(c1, 1, 0);
    lv_obj_set_style_border_color(c1, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(c1, LV_OPA_30, 0);
    lv_obj_set_style_radius(c1, 8, 0);
    lv_obj_clear_flag(c1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * c1_t = lv_label_create(c1);
    lv_label_set_text(c1_t, "TEMPERATURE");
    lv_obj_set_style_text_color(c1_t, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(c1_t, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(c1_t, LV_ALIGN_TOP_LEFT, -4, -2);

    p1_temp_val = lv_label_create(c1);
    lv_label_set_text(p1_temp_val, "24.0 C");
    lv_obj_set_style_text_color(p1_temp_val, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(p1_temp_val, FONT_BASE, 0);
    lv_obj_align(p1_temp_val, LV_ALIGN_BOTTOM_LEFT, -4, 4);

    lv_obj_t * c2 = lv_obj_create(grid);
    lv_obj_set_size(c2, card_w, card_h);
    lv_obj_align(c2, LV_ALIGN_TOP_LEFT, col_x[1], row_y[0]);
    lv_obj_set_style_bg_color(c2, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(c2, 1, 0);
    lv_obj_set_style_border_color(c2, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(c2, LV_OPA_30, 0);
    lv_obj_set_style_radius(c2, 8, 0);
    lv_obj_clear_flag(c2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * c2_t = lv_label_create(c2);
    lv_label_set_text(c2_t, "HUMIDITY");
    lv_obj_set_style_text_color(c2_t, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(c2_t, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(c2_t, LV_ALIGN_TOP_LEFT, -4, -2);

    p1_humidity_val = lv_label_create(c2);
    lv_label_set_text(p1_humidity_val, "58.00%");
    lv_obj_set_style_text_color(p1_humidity_val, KINETIC_COLOR_TERTIARY, 0);
    lv_obj_set_style_text_font(p1_humidity_val, FONT_BASE, 0);
    lv_obj_align(p1_humidity_val, LV_ALIGN_BOTTOM_LEFT, -4, 4);

    lv_obj_t * c3 = lv_obj_create(grid);
    lv_obj_set_size(c3, card_w, card_h);
    lv_obj_align(c3, LV_ALIGN_TOP_LEFT, col_x[2], row_y[0]);
    lv_obj_set_style_bg_color(c3, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(c3, 1, 0);
    lv_obj_set_style_border_color(c3, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(c3, LV_OPA_30, 0);
    lv_obj_set_style_radius(c3, 8, 0);
    lv_obj_clear_flag(c3, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * c3_t = lv_label_create(c3);
    lv_label_set_text(c3_t, "TDS");
    lv_obj_set_style_text_color(c3_t, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(c3_t, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(c3_t, LV_ALIGN_TOP_LEFT, -4, -2);

    lv_obj_t * c3_u = lv_label_create(c3);
    lv_label_set_text(c3_u, "PPM");
    lv_obj_set_style_text_color(c3_u, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(c3_u, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(c3_u, LV_ALIGN_TOP_RIGHT, 2, -2);

    p1_tds_val = lv_label_create(c3);
    lv_label_set_text(p1_tds_val, "342");
    lv_obj_set_style_text_color(p1_tds_val, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(p1_tds_val, FONT_BASE, 0);
    lv_obj_align(p1_tds_val, LV_ALIGN_BOTTOM_LEFT, -4, 4);

    lv_obj_t * c4 = lv_obj_create(grid);
    lv_obj_set_size(c4, card_w, card_h);
    lv_obj_align(c4, LV_ALIGN_TOP_LEFT, col_x[0], row_y[1]);
    lv_obj_set_style_bg_color(c4, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(c4, 1, 0);
    lv_obj_set_style_border_color(c4, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(c4, LV_OPA_30, 0);
    lv_obj_set_style_radius(c4, 8, 0);
    lv_obj_clear_flag(c4, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * c4_t = lv_label_create(c4);
    lv_label_set_text(c4_t, "EC");
    lv_obj_set_style_text_color(c4_t, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(c4_t, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(c4_t, LV_ALIGN_TOP_LEFT, -4, -2);

    lv_obj_t * c4_u = lv_label_create(c4);
    lv_label_set_text(c4_u, "uS/cm");
    lv_obj_set_style_text_color(c4_u, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(c4_u, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(c4_u, LV_ALIGN_TOP_RIGHT, 2, -2);

    p1_ec_val = lv_label_create(c4);
    lv_label_set_text(p1_ec_val, "0.8");
    lv_obj_set_style_text_color(p1_ec_val, KINETIC_COLOR_TERTIARY, 0);
    lv_obj_set_style_text_font(p1_ec_val, FONT_BASE, 0);
    lv_obj_align(p1_ec_val, LV_ALIGN_BOTTOM_LEFT, -4, 4);

    lv_obj_t * c5 = lv_obj_create(grid);
    lv_obj_set_size(c5, card_w, card_h);
    lv_obj_align(c5, LV_ALIGN_TOP_LEFT, col_x[1], row_y[1]);
    lv_obj_set_style_bg_color(c5, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(c5, 1, 0);
    lv_obj_set_style_border_color(c5, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(c5, LV_OPA_30, 0);
    lv_obj_set_style_radius(c5, 8, 0);
    lv_obj_clear_flag(c5, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * c5_t = lv_label_create(c5);
    lv_label_set_text(c5_t, "PH");
    lv_obj_set_style_text_color(c5_t, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(c5_t, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(c5_t, LV_ALIGN_TOP_LEFT, -4, -2);

    lv_obj_t * c5_v = lv_label_create(c5);
    lv_label_set_text(c5_v, "6.4");
    lv_obj_set_style_text_color(c5_v, KINETIC_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(c5_v, FONT_BASE, 0);
    lv_obj_align(c5_v, LV_ALIGN_BOTTOM_LEFT, -4, 4);

    lv_obj_t * c6 = lv_obj_create(grid);
    lv_obj_set_size(c6, card_w, card_h);
    lv_obj_align(c6, LV_ALIGN_TOP_LEFT, col_x[2], row_y[1]);
    lv_obj_set_style_bg_color(c6, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(c6, 1, 0);
    lv_obj_set_style_border_color(c6, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_opa(c6, LV_OPA_30, 0);
    lv_obj_set_style_radius(c6, 8, 0);
    lv_obj_clear_flag(c6, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * c6_t = lv_label_create(c6);
    lv_label_set_text(c6_t, "WATER LEVEL");
    lv_obj_set_style_text_color(c6_t, KINETIC_COLOR_TEXT_DIM, 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(c6_t, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(c6_t, LV_ALIGN_TOP_LEFT, -4, -2);

    p1_water_level_val = lv_label_create(c6);
    lv_label_set_text(p1_water_level_val, "72%");
    lv_obj_set_style_text_color(p1_water_level_val, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(p1_water_level_val, FONT_BASE, 0);
    lv_obj_align(p1_water_level_val, LV_ALIGN_BOTTOM_LEFT, -4, 4);
}

static void build_page2(void) {
    lv_obj_t * grid = lv_obj_create(page2);
    lv_obj_set_size(grid, 320, 170);
    lv_obj_center(grid);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 5, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    // Left Column
    lv_obj_t * left = lv_obj_create(grid);
    lv_obj_set_size(left, 190, 160);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // TDS Card
    lv_obj_t * tds = lv_obj_create(left);
    lv_obj_set_size(tds, 190, 75);
    lv_obj_set_style_bg_color(tds, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(tds, 0, 0);
    lv_obj_set_style_border_color(tds, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_side(tds, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(tds, 4, 0); // Fake border-l-2
    lv_obj_set_style_radius(tds, 8, 0);
    lv_obj_clear_flag(tds, LV_OBJ_FLAG_SCROLLABLE);

    p2_tds_label = lv_label_create(tds);
    lv_label_set_text(p2_tds_label, "342");
    lv_obj_set_style_text_color(p2_tds_label, KINETIC_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(p2_tds_label, FONT_BASE, 0);
    lv_obj_align(p2_tds_label, LV_ALIGN_CENTER, 0, 15);

    p2_tds_arc = lv_arc_create(tds);
    lv_obj_set_size(p2_tds_arc, 64, 64);
    lv_obj_align(p2_tds_arc, LV_ALIGN_BOTTOM_MID, 0, 30);
    lv_arc_set_bg_angles(p2_tds_arc, 150, 30);
    lv_arc_set_range(p2_tds_arc, 0, 1000); // Expanded range for TDS
    lv_arc_set_value(p2_tds_arc, 342);
    lv_obj_set_style_arc_color(p2_tds_arc, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(p2_tds_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(p2_tds_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(p2_tds_arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(p2_tds_arc, KINETIC_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(p2_tds_arc, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(p2_tds_arc, arc_tds_event_cb, LV_EVENT_VALUE_CHANGED, p2_tds_label);

    // EC Card
    lv_obj_t * ec = lv_obj_create(left);
    lv_obj_set_size(ec, 190, 75);
    lv_obj_set_style_bg_color(ec, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_color(ec, KINETIC_COLOR_TERTIARY, 0);
    lv_obj_set_style_border_side(ec, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(ec, 4, 0);
    lv_obj_set_style_radius(ec, 8, 0);
    lv_obj_clear_flag(ec, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * hflex2 = lv_obj_create(ec);
    lv_obj_set_size(hflex2, 140, 24);
    lv_obj_align(hflex2, LV_ALIGN_TOP_LEFT, -5, -8);
    lv_obj_set_style_bg_opa(hflex2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hflex2, 0, 0);
    lv_obj_set_style_pad_all(hflex2, 0, 0);
    lv_obj_set_flex_flow(hflex2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hflex2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hflex2, 4, 0);
    lv_obj_clear_flag(hflex2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * ec_l = lv_label_create(hflex2);
    lv_label_set_text(ec_l, "EC");
    lv_obj_set_style_text_color(ec_l, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(ec_l, FONT_BASE, 0);

    extern const lv_img_dsc_t bolt_img;
    lv_obj_t * ec_icon = lv_img_create(hflex2);
    lv_img_set_src(ec_icon, &bolt_img);
    lv_obj_set_style_img_recolor_opa(ec_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(ec_icon, KINETIC_COLOR_TEXT_DIM, 0);

    lv_obj_t * u2 = lv_label_create(ec);
    lv_label_set_text(u2, "mS/cm");
    lv_obj_set_style_text_color(u2, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(u2, FONT_BASE, 0);
    lv_obj_align(u2, LV_ALIGN_TOP_RIGHT, 0, -5);

    p2_ec_label = lv_label_create(ec);
    lv_label_set_text(p2_ec_label, "0.8");
    lv_obj_set_style_text_color(p2_ec_label, KINETIC_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(p2_ec_label, FONT_BASE, 0);
    lv_obj_align(p2_ec_label, LV_ALIGN_CENTER, 0, 15);

    p2_ec_arc = lv_arc_create(ec);
    lv_obj_set_size(p2_ec_arc, 64, 64);
    lv_obj_align(p2_ec_arc, LV_ALIGN_BOTTOM_MID, 0, 30);
    lv_arc_set_bg_angles(p2_ec_arc, 150, 30);
    lv_arc_set_range(p2_ec_arc, 0, 5000);
    lv_arc_set_value(p2_ec_arc, 800);
    lv_obj_set_style_arc_color(p2_ec_arc, KINETIC_COLOR_TERTIARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(p2_ec_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(p2_ec_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(p2_ec_arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(p2_ec_arc, KINETIC_COLOR_TERTIARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(p2_ec_arc, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(p2_ec_arc, arc_ec_event_cb, LV_EVENT_VALUE_CHANGED, p2_ec_label);

    // Right Column (Auto Concentration Button)
    p2_auto_concentration_btn = lv_btn_create(grid);
    lv_obj_set_size(p2_auto_concentration_btn, 110, 160);
    lv_obj_align(p2_auto_concentration_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(p2_auto_concentration_btn, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(p2_auto_concentration_btn, 1, 0);
    lv_obj_set_style_border_color(p2_auto_concentration_btn, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_radius(p2_auto_concentration_btn, 10, 0);
    lv_obj_set_style_pad_all(p2_auto_concentration_btn, 8, 0);
    lv_obj_add_event_cb(p2_auto_concentration_btn, auto_concentration_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * t3 = lv_label_create(p2_auto_concentration_btn);
    lv_label_set_text(t3, "AUTO MODE");
    lv_label_set_long_mode(t3, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(t3, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(t3, 92);
    lv_obj_set_style_text_color(t3, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_align(t3, LV_ALIGN_TOP_MID, 0, 18);

    extern const lv_img_dsc_t flask_img;
    lv_obj_t * flask = lv_img_create(p2_auto_concentration_btn);
    lv_img_set_src(flask, &flask_img);
    lv_obj_set_style_img_recolor_opa(flask, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(flask, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_align(flask, LV_ALIGN_CENTER, 0, 8);

    p2_auto_concentration_state = lv_label_create(p2_auto_concentration_btn);
    lv_label_set_text(p2_auto_concentration_state, "OFF");
    lv_obj_set_style_text_font(p2_auto_concentration_state, FONT_BASE, 0);
    lv_obj_set_style_text_color(p2_auto_concentration_state, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_align(p2_auto_concentration_state, LV_ALIGN_BOTTOM_MID, 0, -14);

    set_auto_concentration_state(false);
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
