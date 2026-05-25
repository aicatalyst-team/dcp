// Minimal LVGL "hello world" mirroring LilyGo's official Lvgl.ino config
// exactly. Purpose: confirm the panel can render LVGL widgets stably.
// If this shows a single static "HELLO LVGL" centered, the panel+LVGL stack
// is healthy. If it flickers or stays blank, the board itself has an issue.

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#define TOUCH_MODULES_CST_MUTUAL
#include "TouchLib.h"

#define IIC_SDA           17
#define IIC_SCL           18
#define TOUCH_INT         21
#define LCD_WIDTH         480
#define LCD_HEIGHT        480
#define LCD_VSYNC         40
#define LCD_HSYNC         39
#define LCD_PCLK          41
#define LCD_B0 1
#define LCD_B1 2
#define LCD_B2 3
#define LCD_B3 4
#define LCD_B4 5
#define LCD_G0 6
#define LCD_G1 7
#define LCD_G2 8
#define LCD_G3 9
#define LCD_G4 10
#define LCD_G5 11
#define LCD_R0 12
#define LCD_R1 13
#define LCD_R2 42
#define LCD_R3 46
#define LCD_R4 45
#define LCD_BL            14
#define XL95X5_CS         17
#define XL95X5_SCLK       15
#define XL95X5_MOSI       16
#define XL95X5_TOUCH_RST  4
#define CST3240_ADDRESS   0x5A

volatile bool Touch_Int_Flag = false;

TouchLib touch(Wire, IIC_SDA, IIC_SCL, CST3240_ADDRESS);

Arduino_DataBus *bus = new Arduino_XL9535SWSPI(
    IIC_SDA, IIC_SCL, -1, XL95X5_CS, XL95X5_SCLK, XL95X5_MOSI);
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    -1, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
    LCD_B0, LCD_B1, LCD_B2, LCD_B3, LCD_B4,
    LCD_G0, LCD_G1, LCD_G2, LCD_G3, LCD_G4, LCD_G5,
    LCD_R0, LCD_R1, LCD_R2, LCD_R3, LCD_R4,
    1, 20, 2, 0,
    1, 30, 8, 1,
    10, 6000000L, false, 0, 0);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    LCD_WIDTH, LCD_HEIGHT, rgbpanel, 0, true,
    bus, -1, st7701_type9_init_operations, sizeof(st7701_type9_init_operations));

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    if (Touch_Int_Flag) {
        touch.read();
        TP_Point t = touch.getPoint(0);
        if (touch.getPointNum() == 1 && t.pressure > 0 && t.state != 0) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = t.x;
            data->point.y = t.y;
        }
        Touch_Int_Flag = false;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("[lvgl_test] start");

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    attachInterrupt(TOUCH_INT, [] { Touch_Int_Flag = true; }, FALLING);

    Wire.begin(IIC_SDA, IIC_SCL);

    gfx->begin();
    gfx->fillScreen(BLACK);
    Serial.println("[lvgl_test] gfx ready");

    gfx->XL_digitalWrite(XL95X5_TOUCH_RST, LOW); delay(200);
    gfx->XL_digitalWrite(XL95X5_TOUCH_RST, HIGH); delay(200);
    touch.init();
    Serial.println("[lvgl_test] touch ready");

    lv_init();
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
        sizeof(lv_color_t) * LCD_WIDTH * 40, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
        sizeof(lv_color_t) * LCD_WIDTH * 40, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_WIDTH * 40);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Single big centered label on white background
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "HELLO LVGL\nT-Panel DCP");
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);
    Serial.println("[lvgl_test] label created");
}

void loop() {
    lv_timer_handler();
    delay(5);
}
