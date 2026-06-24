#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>

#define PIN_CS 5
#define PIN_DC 4
#define PIN_MOSI 23
#define PIN_SCLK 18

static SPIClass spi(VSPI);

static void cmd(uint8_t c) {
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(c); digitalWrite(PIN_CS, HIGH);
}
static void dat(uint8_t d) {
  digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); spi.transfer(d); digitalWrite(PIN_CS, HIGH);
}

void lcd_init() {
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT); digitalWrite(PIN_DC, HIGH);
  spi.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS); spi.setFrequency(40000000);
  delay(50);
  cmd(0x01); delay(5);
  cmd(0x11); delay(120);
  cmd(0xB1); dat(0x05); dat(0x3A); dat(0x3A);
  cmd(0xB2); dat(0x05); dat(0x3A); dat(0x3A);
  cmd(0xB3); dat(0x05); dat(0x3A); dat(0x3A); dat(0x05); dat(0x3A); dat(0x3A);
  cmd(0xB4); dat(0x03);
  cmd(0xC0); dat(0x62); dat(0x02); dat(0x04);
  cmd(0xC1); dat(0x00);
  cmd(0xC2); dat(0x0C); dat(0x00);
  cmd(0xC3); dat(0x8D); dat(0x6A);
  cmd(0xC4); dat(0x8D); dat(0xEE);
  cmd(0xC5); dat(0x0E);
  cmd(0x36); dat(0x00);
  cmd(0x3A); dat(0x05);
  cmd(0x29); delay(10);
}

void fill(uint16_t c) {
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(0x2A); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); spi.transfer(0); spi.transfer(0); spi.transfer(0); spi.transfer(127); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(0x2B); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); spi.transfer(0); spi.transfer(0); spi.transfer(0); spi.transfer(159); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(0x2C); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH);
  for(int i=0;i<20480;i++){digitalWrite(PIN_CS,LOW);spi.transfer(c>>8);spi.transfer(c&0xFF);digitalWrite(PIN_CS,HIGH);}
}

static lv_color_t buf[128*20];
static lv_disp_draw_buf_t db;
static lv_disp_drv_t drv;

void flush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* p) {
  int w=a->x2-a->x1+1, h=a->y2-a->y1+1;
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(0x2A); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); spi.transfer(a->x1>>8);spi.transfer(a->x1&0xFF);spi.transfer(a->x2>>8);spi.transfer(a->x2&0xFF); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(0x2B); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); spi.transfer(a->y1>>8);spi.transfer(a->y1&0xFF);spi.transfer(a->y2>>8);spi.transfer(a->y2&0xFF); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(0x2C); digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH);
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){digitalWrite(PIN_CS,LOW);spi.transfer(p[y*w+x].full>>8);spi.transfer(p[y*w+x].full&0xFF);digitalWrite(PIN_CS,HIGH);}
  lv_disp_flush_ready(d);
}

static int sel = 0;
static bool in_app = false;
static lv_obj_t* app_label = NULL;

void show_menu();

void enter_app(int i) {
  in_app = true;
  lv_obj_clean(lv_scr_act());
  const char* texts[] = {"Snake Game", "Bluetooth", "NES Emulator"};
  app_label = lv_label_create(lv_scr_act());
  lv_label_set_text(app_label, texts[i]);
  lv_obj_center(app_label);
}

void show_menu() {
  in_app = false;
  lv_obj_clean(lv_scr_act());

  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "SELECT APP");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

  const char* names[] = {"1. Snake", "2. BtPad", "3. NES"};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 110, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 30 + i * 34);
    lv_obj_set_style_border_width(btn, i == sel ? 2 : 0, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(btn, i == sel ? lv_color_hex(0x0044AA) : lv_color_hex(0x333333), 0);
    lv_obj_set_user_data(btn, (void*)(intptr_t)i);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, names[i]);
    lv_obj_center(lbl);
    if (i == 2) lv_obj_add_state(btn, LV_STATE_DISABLED);
  }
}

void update_menu_sel() {
  lv_obj_clean(lv_scr_act());
  show_menu();
}

void setup() {
  Serial.begin(115200); delay(1000);
  pinMode(2, INPUT_PULLUP); pinMode(13, INPUT_PULLUP);
  pinMode(34, INPUT); pinMode(12, INPUT_PULLUP);
  lcd_init(); fill(0x0000);

  lv_init();
  lv_disp_draw_buf_init(&db, buf, NULL, 128*20);
  lv_disp_drv_init(&drv);
  drv.hor_res = 128; drv.ver_res = 160;
  drv.sw_rotate = 1; drv.rotated = LV_DISP_ROT_270;
  drv.flush_cb = flush; drv.draw_buf = &db;
  lv_disp_drv_register(&drv);

  sel = 0;
  show_menu();
}

void loop() {
  static unsigned long t = 0;
  unsigned long n = millis();
  if (n - t > 200) {
    bool up = digitalRead(2) == LOW;
    bool dn = digitalRead(13) == LOW;
    bool a = digitalRead(34) == LOW;
    bool b = digitalRead(12) == LOW;

    if (up || dn || a || b) {
      Serial.printf("GPIO: u=%d d=%d a=%d b=%d\n", up, dn, a, b);
      t = n;
    }

    if (!in_app) {
      if (up && sel > 0) { t = n; sel--; update_menu_sel(); lv_refr_now(NULL); }
      if (dn && sel < 2) { t = n; sel++; update_menu_sel(); lv_refr_now(NULL); }
      if (a && sel == 0) { t = n; enter_app(0); lv_refr_now(NULL); }
      if (a && sel == 1) { t = n; enter_app(1); lv_refr_now(NULL); }
      if (a && sel == 2) { t = n; enter_app(2); lv_refr_now(NULL); }
    } else {
      if (b) { t = n; update_menu_sel(); lv_refr_now(NULL); }
    }
  }
  lv_timer_handler();
  delay(5);
}