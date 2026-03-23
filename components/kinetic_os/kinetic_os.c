#include "kinetic_os.h"

// Global UI Elements
static lv_obj_t * root_tv;
static lv_obj_t * page1;
static lv_obj_t * page2;
static lv_obj_t * page3;
static lv_obj_t * page4;
static lv_obj_t * btn_nav1;
static lv_obj_t * btn_nav2;
static lv_obj_t * btn_nav3;
static lv_obj_t * btn_nav4;

static lv_obj_t * p_top_title;
static kinetic_os_switch_cb_t user_pump_cb = NULL;
static kinetic_os_switch_cb_t user_light_cb = NULL;

// Page 4 Dynamic Elements
static lv_obj_t * p4_pump_icon;
static lv_obj_t * p4_pump_dot;
static lv_obj_t * p4_pump_stat;
static lv_obj_t * p4_pump_sw;

static lv_obj_t * p4_light_icon;
static lv_obj_t * p4_light_dot;
static lv_obj_t * p4_light_stat;
static lv_obj_t * p4_light_sw;
static lv_obj_t * p4_light_slider;
static lv_obj_t * p4_light_val;
static kinetic_os_slider_cb_t user_light_intensity_cb = NULL;

static lv_obj_t * p1_thermal_warn;

// Fonts
#define FONT_BASE &lv_font_montserrat_14

// Icons mapped to LVGL built-in symbols
#define ICON_SENSORS LV_SYMBOL_WIFI
#define ICON_BATTERY LV_SYMBOL_BATTERY_3
#define ICON_DASHBOARD LV_SYMBOL_LIST
#define ICON_OPACITY LV_SYMBOL_TINT
#define ICON_SCIENCE LV_SYMBOL_LOOP // Circular arrows representing continuous mixing

static void build_top_bar(void);
static void build_bottom_bar(void);
static void build_page1(void);
static void build_page2(void);
static void build_page3(void);
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
    page3 = lv_tileview_add_tile(root_tv, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    page4 = lv_tileview_add_tile(root_tv, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);

    build_page1();
    build_page2();
    build_page3();
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
    lv_label_set_text(p_top_title, ICON_SENSORS " KINETIC_OS");
    lv_obj_set_style_text_color(p_top_title, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(p_top_title, FONT_BASE, 0);
    lv_obj_align(p_top_title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * bat = lv_label_create(top);
    lv_label_set_text(bat, ICON_BATTERY);
    lv_obj_set_style_text_color(bat, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_align(bat, LV_ALIGN_RIGHT_MID, 0, 0);
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
    lv_obj_set_size(btn_nav1, 70, 38);
    lv_obj_set_style_radius(btn_nav1, 8, 0);
    lv_obj_add_event_cb(btn_nav1, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)0);
    lv_obj_t * l1 = lv_label_create(btn_nav1);
    lv_label_set_text(l1, ICON_DASHBOARD);
    lv_obj_center(l1);

    btn_nav2 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav2, 70, 38);
    lv_obj_set_style_radius(btn_nav2, 8, 0);
    lv_obj_add_event_cb(btn_nav2, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)1);
    lv_obj_t * l2 = lv_label_create(btn_nav2);
    lv_label_set_text(l2, ICON_OPACITY);
    lv_obj_center(l2);

    btn_nav3 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav3, 70, 38);
    lv_obj_set_style_radius(btn_nav3, 8, 0);
    lv_obj_add_event_cb(btn_nav3, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)2);
    lv_obj_t * l3 = lv_label_create(btn_nav3);
    lv_label_set_text(l3, ICON_SCIENCE);
    lv_obj_center(l3);

    btn_nav4 = lv_btn_create(flex);
    lv_obj_set_size(btn_nav4, 70, 38);
    lv_obj_set_style_radius(btn_nav4, 8, 0);
    lv_obj_add_event_cb(btn_nav4, bottom_nav_event_cb, LV_EVENT_CLICKED, (void*)3);
    lv_obj_t * l4 = lv_label_create(btn_nav4);
    lv_label_set_text(l4, LV_SYMBOL_SETTINGS); // "Setup"
    lv_obj_center(l4);
}

static void update_bottom_nav_styles(uint8_t active_idx) {
    lv_obj_t * btns[4] = {btn_nav1, btn_nav2, btn_nav3, btn_nav4};
    for(int i = 0; i < 4; i++) {
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
    else if(active == page3) update_bottom_nav_styles(2);
    else if(active == page4) update_bottom_nav_styles(3);
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

void kinetic_os_set_wifi_name(const char * ssid) {
    if(!p_top_title) return;
    if(ssid == NULL || strlen(ssid) == 0) {
        lv_label_set_text(p_top_title, ICON_SENSORS " KINETIC_OS");
    } else {
        lv_label_set_text_fmt(p_top_title, ICON_SENSORS " %s", ssid);
    }
}

void kinetic_os_set_pump_switch_cb(kinetic_os_switch_cb_t cb) { user_pump_cb = cb; }
void kinetic_os_set_light_switch_cb(kinetic_os_switch_cb_t cb) { user_light_cb = cb; }

void kinetic_os_set_light_intensity_cb(kinetic_os_slider_cb_t cb) { user_light_intensity_cb = cb; }

void kinetic_os_set_light_intensity(uint8_t pct) {
    if(!p4_light_slider) return;
    lv_slider_set_value(p4_light_slider, pct, LV_ANIM_OFF);
    lv_label_set_text_fmt(p4_light_val, "%d%%", pct);
}

static void light_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    uint8_t v = (uint8_t)lv_slider_get_value(slider);
    lv_label_set_text_fmt(label, "%d%%", v);
    if(user_light_intensity_cb) user_light_intensity_cb(v);
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

static void arc_thermal_event_cb(lv_event_t * e) {
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    int16_t v = lv_arc_get_value(arc);
    lv_label_set_text_fmt(label, "%d.0°", v);
    
    if(p1_thermal_warn) {
        if(v >= 32) {
            lv_obj_clear_flag(p1_thermal_warn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_arc_color(arc, lv_color_hex(0xff0000), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(arc, lv_color_hex(0xff0000), LV_PART_KNOB);
            lv_obj_set_style_text_color(label, lv_color_hex(0xff0000), 0);
        } else {
            lv_obj_add_flag(p1_thermal_warn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_arc_color(arc, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(arc, KINETIC_COLOR_PRIMARY, LV_PART_KNOB);
            lv_obj_set_style_text_color(label, KINETIC_COLOR_TEXT, 0);
        }
    }
}

static void arc_moisture_event_cb(lv_event_t * e) {
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    int16_t v = lv_arc_get_value(arc);
    lv_label_set_text_fmt(label, "%d%%", v);
}

static void build_page1(void) {
    lv_obj_t * flex = lv_obj_create(page1);
    lv_obj_set_size(flex, 320, 170);
    lv_obj_center(flex);
    lv_obj_set_style_bg_opa(flex, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flex, 0, 0);
    lv_obj_set_style_pad_all(flex, 5, 0);
    lv_obj_set_flex_flow(flex, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(flex, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(flex, LV_OBJ_FLAG_SCROLLABLE); // Enforce tile swipe pass-through

    // Thermal Card
    lv_obj_t * c1 = lv_obj_create(flex);
    lv_obj_set_size(c1, 150, 160);
    lv_obj_set_style_bg_color(c1, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(c1, 0, 0);
    lv_obj_set_style_radius(c1, 10, 0);
    lv_obj_clear_flag(c1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * hflex1 = lv_obj_create(c1);
    lv_obj_set_size(hflex1, 140, 24);
    lv_obj_align(hflex1, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(hflex1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hflex1, 0, 0);
    lv_obj_set_style_pad_all(hflex1, 0, 0);
    lv_obj_set_flex_flow(hflex1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hflex1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hflex1, 4, 0);
    lv_obj_clear_flag(hflex1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title1_txt = lv_label_create(hflex1);
    lv_label_set_text(title1_txt, "THERMAL");
    lv_obj_set_style_text_color(title1_txt, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title1_txt, FONT_BASE, 0);

    extern const lv_img_dsc_t thermometer_img;
    lv_obj_t * therm_icon = lv_img_create(hflex1);
    lv_img_set_src(therm_icon, &thermometer_img);
    lv_obj_set_style_img_recolor_opa(therm_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(therm_icon, KINETIC_COLOR_PRIMARY, 0);

    lv_obj_t * v1 = lv_label_create(c1);
    lv_label_set_text(v1, "24.0°");
    lv_obj_set_style_text_font(v1, FONT_BASE, 0);
    lv_obj_set_style_text_color(v1, KINETIC_COLOR_TEXT, 0);
    lv_obj_align(v1, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t * arc1 = lv_arc_create(c1);
    lv_obj_set_size(arc1, 100, 100);
    lv_obj_align(arc1, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_arc_set_bg_angles(arc1, 135, 45); // Standard dial arc
    lv_arc_set_range(arc1, 0, 50); // Valid Earth temps
    lv_arc_set_value(arc1, 24); 
    lv_obj_set_style_arc_color(arc1, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc1, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc1, 6, LV_PART_MAIN);
    // Draw an interactive knob handle for easy grabbing
    lv_obj_set_style_bg_opa(arc1, LV_OPA_COVER, LV_PART_KNOB); 
    lv_obj_set_style_bg_color(arc1, KINETIC_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc1, 4, LV_PART_KNOB); 

    p1_thermal_warn = lv_label_create(c1);
    lv_label_set_text(p1_thermal_warn, LV_SYMBOL_WARNING " EXCEED!!");
    lv_obj_set_style_text_color(p1_thermal_warn, lv_color_hex(0xff0000), 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(p1_thermal_warn, &lv_font_montserrat_12, 0);
#endif
    lv_obj_align(p1_thermal_warn, LV_ALIGN_BOTTOM_MID, 0, 3);
    lv_obj_add_flag(p1_thermal_warn, LV_OBJ_FLAG_HIDDEN);
    
    // Mount the real-time event listener to update v1 dynamically
    lv_obj_add_event_cb(arc1, arc_thermal_event_cb, LV_EVENT_VALUE_CHANGED, v1);

    // Moisture Card
    lv_obj_t * c2 = lv_obj_create(flex);
    lv_obj_set_size(c2, 150, 160);
    lv_obj_set_style_bg_color(c2, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(c2, 0, 0);
    lv_obj_set_style_radius(c2, 10, 0);
    lv_obj_clear_flag(c2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title2 = lv_label_create(c2);
    lv_label_set_text(title2, "MOISTURE " ICON_OPACITY);
    lv_obj_set_style_text_color(title2, KINETIC_COLOR_TERTIARY, 0);
    lv_obj_set_style_text_font(title2, FONT_BASE, 0);
    lv_obj_align(title2, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * v2 = lv_label_create(c2);
    lv_label_set_text(v2, "58%");
    lv_obj_set_style_text_font(v2, FONT_BASE, 0);
    lv_obj_set_style_text_color(v2, KINETIC_COLOR_TEXT, 0);
    lv_obj_align(v2, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t * arc2 = lv_arc_create(c2);
    lv_obj_set_size(arc2, 100, 100);
    lv_obj_align(arc2, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_arc_set_bg_angles(arc2, 135, 45); 
    lv_arc_set_range(arc2, 0, 100); 
    lv_arc_set_value(arc2, 58); 
    lv_obj_set_style_arc_color(arc2, KINETIC_COLOR_TERTIARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc2, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc2, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc2, LV_OPA_COVER, LV_PART_KNOB); 
    lv_obj_set_style_bg_color(arc2, KINETIC_COLOR_TERTIARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc2, 4, LV_PART_KNOB); 
    
    // Mount the real-time event listener to update v2 dynamically
    lv_obj_add_event_cb(arc2, arc_moisture_event_cb, LV_EVENT_VALUE_CHANGED, v2);
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

    lv_obj_t * t1 = lv_label_create(tds);
    lv_label_set_text(t1, "TDS " ICON_OPACITY);
    lv_obj_set_style_text_color(t1, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_align(t1, LV_ALIGN_TOP_LEFT, -5, -5);

    lv_obj_t * u1 = lv_label_create(tds);
    lv_label_set_text(u1, "PPM");
    lv_obj_set_style_text_color(u1, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(u1, FONT_BASE, 0);
    lv_obj_align(u1, LV_ALIGN_TOP_RIGHT, 0, -5);

    lv_obj_t * v1 = lv_label_create(tds);
    lv_label_set_text(v1, "342");
    lv_obj_set_style_text_color(v1, KINETIC_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(v1, FONT_BASE, 0);
    lv_obj_align(v1, LV_ALIGN_CENTER, 0, 15);

    lv_obj_t * a1 = lv_arc_create(tds);
    lv_obj_set_size(a1, 64, 64);
    lv_obj_align(a1, LV_ALIGN_BOTTOM_MID, 0, 30);
    lv_arc_set_bg_angles(a1, 150, 30);
    lv_arc_set_range(a1, 0, 1000); // Expanded range for TDS
    lv_arc_set_value(a1, 342);
    lv_obj_set_style_arc_color(a1, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(a1, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(a1, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(a1, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(a1, KINETIC_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a1, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(a1, arc_tds_event_cb, LV_EVENT_VALUE_CHANGED, v1);

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

    lv_obj_t * v2 = lv_label_create(ec);
    lv_label_set_text(v2, "0.8");
    lv_obj_set_style_text_color(v2, KINETIC_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(v2, FONT_BASE, 0);
    lv_obj_align(v2, LV_ALIGN_CENTER, 0, 15);

    lv_obj_t * a2 = lv_arc_create(ec);
    lv_obj_set_size(a2, 64, 64);
    lv_obj_align(a2, LV_ALIGN_BOTTOM_MID, 0, 30);
    lv_arc_set_bg_angles(a2, 150, 30);
    lv_arc_set_range(a2, 0, 50); // 0.0 to 5.0 scaled
    lv_arc_set_value(a2, 8); // 0.8 default
    lv_obj_set_style_arc_color(a2, KINETIC_COLOR_TERTIARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(a2, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(a2, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(a2, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(a2, KINETIC_COLOR_TERTIARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a2, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(a2, arc_ec_event_cb, LV_EVENT_VALUE_CHANGED, v2);

    // Right Column (Level)
    lv_obj_t * right = lv_obj_create(grid);
    lv_obj_set_size(right, 110, 160);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(right, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(right, 1, 0);
    lv_obj_set_style_border_color(right, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_radius(right, 10, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * t3 = lv_label_create(right);
    lv_label_set_text(t3, "LEVEL");
    lv_obj_set_style_text_color(t3, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_align(t3, LV_ALIGN_TOP_LEFT, -2, -2);

    lv_obj_t * bar = lv_bar_create(right);
    lv_obj_set_size(bar, 30, 90);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 5);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 72, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, KINETIC_COLOR_OUTLINE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(bar, KINETIC_COLOR_PRIMARY_DIM, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_VER, LV_PART_INDICATOR);

    lv_obj_t * v3 = lv_label_create(right);
    lv_label_set_text(v3, "72%");
    lv_obj_set_style_text_color(v3, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(v3, FONT_BASE, 0);
    lv_obj_align(v3, LV_ALIGN_BOTTOM_MID, 0, 5);
}

static void build_page3(void) {
    lv_obj_t * flex = lv_obj_create(page3);
    lv_obj_set_size(flex, 320, 170);
    lv_obj_center(flex);
    lv_obj_set_style_bg_opa(flex, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flex, 0, 0);
    lv_obj_set_style_pad_all(flex, 5, 0);
    lv_obj_set_flex_flow(flex, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(flex, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(flex, LV_OBJ_FLAG_SCROLLABLE);

    // Status Header
    lv_obj_t * top_box = lv_obj_create(flex);
    lv_obj_set_size(top_box, 300, 30);
    lv_obj_set_style_bg_opa(top_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_box, 0, 0);
    lv_obj_set_style_pad_all(top_box, 0, 0);
    lv_obj_clear_flag(top_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * status = lv_label_create(top_box);
    lv_label_set_text(status, "System Online");
    lv_obj_set_style_text_color(status, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * rtm = lv_label_create(top_box);
    lv_label_set_text(rtm, "Ready to Mix");
    lv_obj_set_style_text_color(rtm, KINETIC_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(rtm, FONT_BASE, 0);
    lv_obj_align(rtm, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Big Button
    lv_obj_t * btn = lv_btn_create(flex);
    lv_obj_set_size(btn, 80, 80);
    lv_obj_set_style_radius(btn, 40, 0);
    lv_obj_set_style_bg_color(btn, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x2be800), 0);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);

    extern const lv_img_dsc_t flask_img;
    
    lv_obj_t * flex_btn = lv_obj_create(btn);
    lv_obj_set_size(flex_btn, 80, 80);
    lv_obj_center(flex_btn);
    lv_obj_set_style_bg_opa(flex_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flex_btn, 0, 0);
    lv_obj_set_style_pad_all(flex_btn, 0, 0);
    lv_obj_set_flex_flow(flex_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(flex_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(flex_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(flex_btn, LV_OBJ_FLAG_CLICKABLE); // Allow touches to pass through to the button!

    lv_obj_t * img = lv_img_create(flex_btn);
    lv_img_set_src(img, &flask_img);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img, KINETIC_COLOR_ON_PRIMARY, 0);

    lv_obj_t * btn_l = lv_label_create(flex_btn);
    lv_label_set_text(btn_l, "START");
    lv_obj_set_style_text_font(btn_l, FONT_BASE, 0);
    lv_obj_set_style_text_color(btn_l, KINETIC_COLOR_ON_PRIMARY, 0);

    // Bottom cards
    lv_obj_t * hflex = lv_obj_create(flex);
    lv_obj_set_size(hflex, 310, 45);
    lv_obj_set_style_bg_opa(hflex, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hflex, 0, 0);
    lv_obj_set_style_pad_all(hflex, 0, 0);
    lv_obj_set_flex_flow(hflex, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hflex, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * card1 = lv_obj_create(hflex);
    lv_obj_set_size(card1, 150, 45);
    lv_obj_set_style_bg_color(card1, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(card1, 0, 0);
    lv_obj_set_style_radius(card1, 8, 0);
    lv_obj_clear_flag(card1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * c1t = lv_label_create(card1);
    lv_label_set_text(c1t, "CONCENTRATE");
    lv_obj_set_style_text_color(c1t, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_align(c1t, LV_ALIGN_TOP_LEFT, -5, -5);

    lv_obj_t * c1v = lv_label_create(card1);
    lv_label_set_text(c1v, "72%");
    lv_obj_set_style_text_color(c1v, KINETIC_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(c1v, FONT_BASE, 0);
    lv_obj_align(c1v, LV_ALIGN_BOTTOM_LEFT, -5, 5);

    lv_obj_t * card2 = lv_obj_create(hflex);
    lv_obj_set_size(card2, 150, 45);
    lv_obj_set_style_bg_color(card2, KINETIC_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(card2, 0, 0);
    lv_obj_set_style_radius(card2, 8, 0);
    lv_obj_clear_flag(card2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * c2t = lv_label_create(card2);
    lv_label_set_text(c2t, "PH LEVEL");
    lv_obj_set_style_text_color(c2t, KINETIC_COLOR_TEXT_DIM, 0);
    lv_obj_align(c2t, LV_ALIGN_TOP_LEFT, -5, -5);

    lv_obj_t * c2v = lv_label_create(card2);
    lv_label_set_text(c2v, "6.4 PH");
    lv_obj_set_style_text_color(c2v, KINETIC_COLOR_TERTIARY, 0);
    lv_obj_set_style_text_font(c2v, FONT_BASE, 0);
    lv_obj_align(c2v, LV_ALIGN_BOTTOM_LEFT, -5, 5);
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

    // Row 1: Pump Status
    lv_obj_t * row1 = lv_obj_create(flex);
    lv_obj_set_size(row1, 300, 50);
    lv_obj_set_style_bg_color(row1, lv_color_hex(0x1a1c1a), 0);
    lv_obj_set_style_border_color(row1, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(row1, 1, 0);
    lv_obj_set_style_border_opa(row1, LV_OPA_20, 0);
    lv_obj_set_style_radius(row1, 12, 0);
    lv_obj_set_style_pad_all(row1, 5, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);

    // Left side of Row 1
    lv_obj_t * l1 = lv_obj_create(row1);
    lv_obj_set_size(l1, 200, 50);
    lv_obj_set_style_bg_opa(l1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(l1, 0, 0);
    lv_obj_set_style_pad_all(l1, 0, 0);
    lv_obj_set_flex_flow(l1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(l1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(l1, LV_OBJ_FLAG_SCROLLABLE);

    // Circle Icon
    lv_obj_t * circ1 = lv_obj_create(l1);
    lv_obj_set_size(circ1, 40, 40);
    lv_obj_set_style_radius(circ1, 20, 0);
    lv_obj_set_style_bg_color(circ1, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(circ1, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(circ1, 1, 0);
    lv_obj_set_style_border_opa(circ1, LV_OPA_40, 0);
    lv_obj_clear_flag(circ1, LV_OBJ_FLAG_SCROLLABLE);

    p4_pump_icon = lv_label_create(circ1);
    lv_label_set_text(p4_pump_icon, LV_SYMBOL_TINT); // water drop
    lv_obj_set_style_text_color(p4_pump_icon, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_center(p4_pump_icon);

    // VBox for text
    lv_obj_t * vbox1 = lv_obj_create(l1);
    lv_obj_set_size(vbox1, 140, 40);
    lv_obj_set_style_bg_opa(vbox1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vbox1, 0, 0);
    lv_obj_set_style_pad_all(vbox1, 0, 0);
    lv_obj_set_style_pad_left(vbox1, 10, 0);
    lv_obj_set_flex_flow(vbox1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(vbox1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(vbox1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * t1 = lv_label_create(vbox1);
    lv_label_set_text(t1, "PUMP_STATUS");
    lv_obj_set_style_text_color(t1, lv_color_hex(0xababa8), 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_12, 0);
#endif

    // HBox for active dot
    lv_obj_t * hbox1 = lv_obj_create(vbox1);
    lv_obj_set_size(hbox1, 100, 20);
    lv_obj_set_style_bg_opa(hbox1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hbox1, 0, 0);
    lv_obj_set_style_pad_all(hbox1, 0, 0);
    lv_obj_set_flex_flow(hbox1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hbox1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hbox1, LV_OBJ_FLAG_SCROLLABLE);

    p4_pump_dot = lv_obj_create(hbox1);
    lv_obj_set_size(p4_pump_dot, 8, 8);
    lv_obj_set_style_radius(p4_pump_dot, 4, 0);
    lv_obj_set_style_bg_color(p4_pump_dot, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(p4_pump_dot, 0, 0);
    lv_obj_clear_flag(p4_pump_dot, LV_OBJ_FLAG_SCROLLABLE);

    p4_pump_stat = lv_label_create(hbox1);
    lv_label_set_text(p4_pump_stat, " ACTIVE");
    lv_obj_set_style_text_color(p4_pump_stat, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(p4_pump_stat, FONT_BASE, 0);

    // Switch
    p4_pump_sw = lv_switch_create(row1);
    lv_obj_set_style_bg_color(p4_pump_sw, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(p4_pump_sw, pump_sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    kinetic_os_set_pump_state(true); // Default map init to TRUE (Active)

    // Row 2: Grow Light
    lv_obj_t * row2 = lv_obj_create(flex);
    lv_obj_set_size(row2, 300, 50);
    lv_obj_set_style_bg_color(row2, lv_color_hex(0x1a1c1a), 0);
    lv_obj_set_style_border_color(row2, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_width(row2, 1, 0);
    lv_obj_set_style_border_opa(row2, LV_OPA_20, 0);
    lv_obj_set_style_radius(row2, 12, 0);
    lv_obj_set_style_pad_all(row2, 5, 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

    // Left side of Row 2
    lv_obj_t * l2 = lv_obj_create(row2);
    lv_obj_set_size(l2, 200, 50);
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
    lv_obj_set_style_border_color(circ2, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_width(circ2, 1, 0);
    lv_obj_set_style_border_opa(circ2, LV_OPA_40, 0);
    lv_obj_clear_flag(circ2, LV_OBJ_FLAG_SCROLLABLE);

    extern const lv_img_dsc_t lightbulb_img;
    p4_light_icon = lv_img_create(circ2);
    lv_img_set_src(p4_light_icon, &lightbulb_img);
    lv_obj_set_style_img_recolor_opa(p4_light_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(p4_light_icon, lv_color_hex(0xababa8), 0);
    lv_obj_center(p4_light_icon);

    // VBox for text
    lv_obj_t * vbox2 = lv_obj_create(l2);
    lv_obj_set_size(vbox2, 140, 40);
    lv_obj_set_style_bg_opa(vbox2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vbox2, 0, 0);
    lv_obj_set_style_pad_all(vbox2, 0, 0);
    lv_obj_set_style_pad_left(vbox2, 10, 0);
    lv_obj_set_flex_flow(vbox2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(vbox2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(vbox2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * t2 = lv_label_create(vbox2);
    lv_label_set_text(t2, "GROW_LIGHT");
    lv_obj_set_style_text_color(t2, lv_color_hex(0xababa8), 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(t2, &lv_font_montserrat_12, 0);
#endif

    // HBox for active dot
    lv_obj_t * hbox2 = lv_obj_create(vbox2);
    lv_obj_set_size(hbox2, 100, 20);
    lv_obj_set_style_bg_opa(hbox2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hbox2, 0, 0);
    lv_obj_set_style_pad_all(hbox2, 0, 0);
    lv_obj_set_flex_flow(hbox2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hbox2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hbox2, LV_OBJ_FLAG_SCROLLABLE);

    p4_light_dot = lv_obj_create(hbox2);
    lv_obj_set_size(p4_light_dot, 8, 8);
    lv_obj_set_style_radius(p4_light_dot, 4, 0);
    lv_obj_set_style_bg_color(p4_light_dot, lv_color_hex(0xababa8), 0);
    lv_obj_set_style_border_width(p4_light_dot, 0, 0);
    lv_obj_clear_flag(p4_light_dot, LV_OBJ_FLAG_SCROLLABLE);

    p4_light_stat = lv_label_create(hbox2);
    lv_label_set_text(p4_light_stat, " STANDBY");
    lv_obj_set_style_text_color(p4_light_stat, lv_color_hex(0xababa8), 0);
    lv_obj_set_style_text_font(p4_light_stat, FONT_BASE, 0);

    // Switch (Off)
    p4_light_sw = lv_switch_create(row2);
    lv_obj_set_style_bg_color(p4_light_sw, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(p4_light_sw, light_sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    kinetic_os_set_light_state(false); // Default map init to FALSE (Standby)

    // Row 3: Light Intensity
    lv_obj_t * row3 = lv_obj_create(flex);
    lv_obj_set_size(row3, 300, 50);
    lv_obj_set_style_bg_color(row3, lv_color_hex(0x1a1c1a), 0);
    lv_obj_set_style_border_color(row3, KINETIC_COLOR_OUTLINE, 0);
    lv_obj_set_style_border_width(row3, 1, 0);
    lv_obj_set_style_border_opa(row3, LV_OPA_20, 0);
    lv_obj_set_style_radius(row3, 12, 0);
    lv_obj_set_style_pad_all(row3, 8, 0);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row3, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * htxt = lv_obj_create(row3);
    lv_obj_set_size(htxt, 284, 16);
    lv_obj_set_style_bg_opa(htxt, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(htxt, 0, 0);
    lv_obj_set_style_pad_all(htxt, 0, 0);
    lv_obj_set_flex_flow(htxt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(htxt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(htxt, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * t3 = lv_label_create(htxt);
    lv_label_set_text(t3, "LIGHT_INTENSITY");
    lv_obj_set_style_text_color(t3, lv_color_hex(0xababa8), 0);
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(t3, &lv_font_montserrat_12, 0);
#endif

    p4_light_val = lv_label_create(htxt);
    lv_label_set_text(p4_light_val, "80%");
    lv_obj_set_style_text_color(p4_light_val, KINETIC_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(p4_light_val, FONT_BASE, 0);

    p4_light_slider = lv_slider_create(row3);
    lv_obj_set_size(p4_light_slider, 270, 8);
    lv_slider_set_range(p4_light_slider, 0, 100);
    lv_slider_set_value(p4_light_slider, 80, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(p4_light_slider, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(p4_light_slider, KINETIC_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(p4_light_slider, KINETIC_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(p4_light_slider, 4, LV_PART_KNOB);

    lv_obj_add_event_cb(p4_light_slider, light_slider_event_cb, LV_EVENT_VALUE_CHANGED, p4_light_val);
}
