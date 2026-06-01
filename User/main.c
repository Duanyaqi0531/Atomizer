/**
 ******************************************************************************
 * @file    main.c
 * @brief   Atomiser — LVGL + ESP8266 AP HTTP 雾化器控制
 *          + AT24C02 EEPROM 断电记忆
 *
 *  Local Mode: 触摸屏控制输出百分比
 *  WiFi Mode:  手机网页远程控制
 *  每 1 秒自动保存当前值到 EEPROM，上电自动恢复
 *
 *  野火 F103-指南者, ESP8266: PB8/9/10/11, AT24C02: PB6/SCL PB7/SDA
 ******************************************************************************
 */

#include "stm32f10x.h"
#include "./lcd/bsp_ili9341_lcd.h"
#include "./lcd/bsp_xpt2046_lcd.h"
#include "./wifi/bsp_esp8266.h"
#include "./eeprom/bsp_at24c02.h"
#include <string.h>
#include <stdio.h>
#include "../../lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

static lv_obj_t * g_slider     = NULL;
static lv_obj_t * g_pct_label   = NULL;
static lv_obj_t * g_mode_btn    = NULL;
static lv_obj_t * g_mode_label  = NULL;
static lv_obj_t * g_info_label  = NULL;
static lv_obj_t * g_btn_label   = NULL;

static volatile int g_mode     = 0;
static int g_last_saved_val    = -1;    /* 记录上次保存的值，避免重复写 EEPROM */

#define EEPROM_VAL_ADDR   0x00          /* 滑条值存储在 EEPROM 地址 0 */

/* ======== 滑条回调 ======== */
static void slider_cb(lv_event_t * e)
{
    int v = (int)lv_slider_get_value(g_slider);
    lv_label_set_text_fmt(g_pct_label, "%d%%", v);
}

/* ======== 模式切换 ======== */
static void mode_btn_cb(lv_event_t * e)
{
    if (g_mode == 0) {
        g_mode = 1;
        lv_obj_set_style_bg_color(g_mode_btn, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_DEFAULT);
        lv_label_set_text(g_mode_label, "WiFi Control");
        lv_label_set_text(g_btn_label, "Switch Mode");
        lv_obj_add_state(g_slider, LV_STATE_DISABLED);
    } else {
        g_mode = 0;
        lv_obj_set_style_bg_color(g_mode_btn, lv_palette_main(LV_PALETTE_GREEN), LV_STATE_DEFAULT);
        lv_label_set_text(g_mode_label, "Local Control");
        lv_label_set_text(g_btn_label, "Switch Mode");
        lv_obj_clear_state(g_slider, LV_STATE_DISABLED);
    }
}

/* ======== 网页 HTML ======== */
static void build_html(char *buf, uint16_t sz, int cur)
{
    snprintf(buf, sz,
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
        "<title>Atomiser</title>"
        "<style>"
        "*{margin:0;padding:0;box-sizing:border-box;}"
        "body{background:#111;color:#eee;font-family:Arial;text-align:center;padding:20px;}"
        "h1{color:#FFB300;font-size:22px;margin-bottom:20px;}"
        ".big{font-size:80px;font-weight:bold;margin:10px 0;color:#fff;}"
        "input[type=range]{width:90vw;max-width:340px;height:10px;"
          "border-radius:5px;background:#333;outline:none;-webkit-appearance:none;}"
        "input[type=range]::-webkit-slider-thumb{"
          "-webkit-appearance:none;width:40px;height:40px;"
          "border-radius:50%%;background:#4CAF50;border:3px solid #fff;}"
        ".btn{background:#4CAF50;color:#fff;border:none;border-radius:8px;"
              "padding:14px 40px;font-size:20px;margin-top:24px;cursor:pointer;}"
        ".btn:active{background:#388E3C;}"
        ".hint{color:#555;margin-top:20px;font-size:12px;}"
        "</style></head><body>"
        "<h1>Atomiser Control</h1>"
        "<div class='big' id='v'>%d%%</div>"
        "<input type='range' id='s' min='0' max='100' value='%d'"
        " oninput='document.getElementById(\"v\").innerText=this.value+\"%%\"'>"
        "<br>"
        "<button class='btn' onclick='send()'>Send</button>"
        "<div class='hint'>Slide then tap Send</div>"
        "<script>"
        "function send(){"
          "var v=document.getElementById('s').value;"
          "fetch('/set?val='+v);"
        "}"
        "</script></body></html>",
        cur, cur);
}

/* ======== LCD 界面 ======== */
static void create_ui(void)
{
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x181818), LV_STATE_DEFAULT);

    /* 标题 */
    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "Atomiser");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFB300), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    /* 状态标签 */
    g_mode_label = lv_label_create(scr);
    lv_label_set_text(g_mode_label, "Local Control");
    lv_obj_set_style_text_color(g_mode_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_mode_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_align(g_mode_label, LV_ALIGN_TOP_MID, 0, 32);

    /* 切换按钮 */
    g_mode_btn = lv_btn_create(scr);
    lv_obj_set_size(g_mode_btn, 170, 40);
    lv_obj_align(g_mode_btn, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_set_style_bg_color(g_mode_btn, lv_palette_main(LV_PALETTE_GREEN), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_mode_btn, 8, LV_STATE_DEFAULT);

    g_btn_label = lv_label_create(g_mode_btn);
    lv_label_set_text(g_btn_label, "Switch Mode");
    lv_obj_set_style_text_color(g_btn_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_center(g_btn_label);

    lv_obj_add_event_cb(g_mode_btn, mode_btn_cb, LV_EVENT_CLICKED, NULL);

    /* 百分比 */
    g_pct_label = lv_label_create(scr);
    lv_label_set_text(g_pct_label, "50%");
    lv_obj_set_style_text_color(g_pct_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_pct_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_align(g_pct_label, LV_ALIGN_TOP_MID, 0, 120);

    /* 滑轨 */
    static lv_style_t sb; lv_style_init(&sb);
    lv_style_set_bg_color(&sb, lv_color_hex(0x333333));
    lv_style_set_radius(&sb, 8); lv_style_set_border_width(&sb, 0);

    /* 填充 */
    static lv_style_t si; lv_style_init(&si);
    lv_style_set_bg_color(&si, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_radius(&si, 8);

    /* 旋钮 */
    static lv_style_t sk; lv_style_init(&sk);
    lv_style_set_bg_color(&sk, lv_color_white());
    lv_style_set_radius(&sk, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&sk, 4);
    lv_style_set_border_width(&sk, 3);
    lv_style_set_border_color(&sk, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_shadow_width(&sk, 10);
    lv_style_set_shadow_color(&sk, lv_color_hex(0x222222));

    /* 滑条 */
    g_slider = lv_slider_create(scr);
    lv_obj_set_width(g_slider, 200);
    lv_obj_align(g_slider, LV_ALIGN_TOP_MID, 0, 175);
    lv_slider_set_range(g_slider, 0, 100);
    lv_slider_set_value(g_slider, 50, LV_ANIM_OFF);
    lv_obj_add_style(g_slider, &sb, LV_PART_MAIN);
    lv_obj_add_style(g_slider, &si, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(g_slider, &sk, LV_PART_KNOB);
    lv_obj_add_event_cb(g_slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 底部信息 */
    g_info_label = lv_label_create(scr);
    lv_label_set_text(g_info_label, "192.168.4.1");
    lv_obj_set_style_text_color(g_info_label, lv_color_hex(0x888888), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_info_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_align(g_info_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

/* ==================================================================== */

int main(void)
{
    ILI9341_Init();
    XPT2046_Init();
    ILI9341_GramScan(6);

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    /* ---- EEPROM 初始化并恢复上次保存的值 ---- */
    AT24C02_Init();
    {
        uint8_t saved = AT24C02_ReadByte(EEPROM_VAL_ADDR);
        if (saved <= 100) {
            /* 有效值：0~100，设置滑条初始值 */
            /* 在 create_ui 之后再设，这里先记录 */
        } else {
            saved = 50;  /* EEPROM 首次上电是 0xFF，默认 50% */
        }
        g_last_saved_val = saved;
    }

    create_ui();

    /* 恢复上电值 */
    {
        lv_slider_set_value(g_slider, g_last_saved_val, LV_ANIM_OFF);
        lv_label_set_text_fmt(g_pct_label, "%d%%", g_last_saved_val);
    }

    /* WiFi 初始化 */
    ESP8266_Init();
    { volatile uint32_t d; for (d = 0; d < 800000; d++); }

    if (!ESP8266_StartAP("Atomiser", "12345678")) {
        lv_label_set_text(g_info_label, "AP FAIL");
        while (1) { lv_task_handler(); lv_tick_inc(1); }
    }
    if (!ESP8266_StartTCPServer(80)) {
        lv_label_set_text(g_info_label, "SRV FAIL");
        while (1) { lv_task_handler(); lv_tick_inc(1); }
    }
    lv_label_set_text(g_info_label, "Atomiser WiFi OK");

    char html[2048];
    char path[64];

    uint32_t tick        = 0;
    uint32_t last_save   = 0;

    while (1) {
        lv_tick_inc(1);
        lv_task_handler();
        tick++;

        int cur = (int)lv_slider_get_value(g_slider);

        /* ---- 每 1 秒保存一次到 EEPROM (约 1000 个 LVGL tick) ---- */
        if (tick - last_save >= 1000) {
            last_save = tick;
            if (cur != g_last_saved_val) {
                AT24C02_WriteByte(EEPROM_VAL_ADDR, (uint8_t)cur);
                g_last_saved_val = cur;
            }
        }

        /* ---- 处理 HTTP 请求 ---- */
        uint16_t pl = sizeof(path) - 1;
        uint8_t  lid = 0;

        if (ESP8266_ParseHTTP(path, &pl, &lid)) {

            if (strncmp(path, "/set?val=", 9) == 0) {
                int v = atoi(path + 9);
                if (v < 0) v = 0; else if (v > 100) v = 100;
                lv_slider_set_value(g_slider, v, LV_ANIM_ON);
                lv_label_set_text_fmt(g_pct_label, "%d%%", v);
            }

            build_html(html, sizeof(html),
                       (int)lv_slider_get_value(g_slider));
            ESP8266_SendHTTP(lid, html);

            { volatile uint32_t dd; for (dd = 0; dd < 200000; dd++); }
            ESP8266_CloseLink(lid);
        }
    }
}
