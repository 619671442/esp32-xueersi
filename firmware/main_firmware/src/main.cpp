#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include "nes_emu.h"

#define PIN_CS 5
#define PIN_DC 4
#define PIN_MOSI 23
#define PIN_SCLK 18

SPIClass spi(VSPI);

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
static int app_id = -1;

static BLEHIDDevice* hid = NULL;
static BLECharacteristic* input = NULL;
static volatile bool deviceConnected = false;

static const int btn_pins[6] = {2, 13, 27, 35, 34, 12};
static const char* btn_names[6] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B"};
static bool btn_prev[6] = {false};
static int btn_mode[6] = {0};
static lv_obj_t* bt_status_label = NULL;
static lv_obj_t* bt_btn_label = NULL;
static lv_obj_t* bt_mode_label = NULL;

static const uint8_t btn_keycodes[6] = {0x52, 0x51, 0x50, 0x4F, 0x04, 0x05};

static const uint8_t hidReportDescriptor[] = {
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
  0x85, 0x01,
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
  0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x08,
  0x81, 0x02,
  0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
  0x95, 0x06, 0x75, 0x08,
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x05, 0x07, 0x19, 0x00, 0x2A, 0xFF, 0x00,
  0x81, 0x00,
  0xC0
};

class BTServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer*) { deviceConnected = true; }
  void onDisconnect(BLEServer* s) { deviceConnected = false; s->getAdvertising()->start(); }
};

void show_menu();
void update_menu_sel();

void bt_gamepad_init() {
  in_app = true;
  app_id = 1;
  lv_obj_clean(lv_scr_act());

  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "BT Gamepad");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

  bt_status_label = lv_label_create(lv_scr_act());
  lv_label_set_text(bt_status_label, "Status: Waiting...");
  lv_obj_align(bt_status_label, LV_ALIGN_TOP_MID, 0, 38);

  bt_mode_label = lv_label_create(lv_scr_act());
  lv_label_set_text(bt_mode_label, "A:A  B:B");
  lv_obj_align(bt_mode_label, LV_ALIGN_TOP_MID, 0, 22);

  bt_btn_label = lv_label_create(lv_scr_act());
  lv_label_set_text(bt_btn_label, "");
  lv_obj_align(bt_btn_label, LV_ALIGN_BOTTOM_MID, 0, -10);

  for (int i = 0; i < 6; i++) { btn_prev[i] = false; btn_mode[i] = 0; }
  deviceConnected = false;

  BLEDevice::init("Xueersi Gamepad");
  BLEServer* srv = BLEDevice::createServer();
  srv->setCallbacks(new BTServerCB());
  hid = new BLEHIDDevice(srv);
  input = hid->inputReport(1);
  hid->manufacturer()->setValue("Espressif");
  hid->pnp(0x02, 0x1234, 0x5678, 0x0110);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
  hid->startServices();

  BLEAdvertising* adv = srv->getAdvertising();
  adv->setAppearance(0x03C1);
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->start();
}

void bt_gamepad_loop() {
  static bool lastConnected = false;
  if (deviceConnected != lastConnected) {
    lastConnected = deviceConnected;
    lv_label_set_text(bt_status_label, deviceConnected ? "Status: Running" : "Status: Waiting...");
    if (!deviceConnected) lv_label_set_text(bt_btn_label, "");
  }

  static int combo_cnt = 0;
  static int swapStep = 0;
  static uint8_t prevReport[8] = {0};

  if (swapStep == 1) {
    uint8_t empty[8] = {0};
    if (deviceConnected) {
      input->setValue(empty, 8);
      input->notify();
    }
    memcpy(prevReport, empty, 8);
    swapStep = 2;
    return;
  }

  bool left = digitalRead(27) == LOW;
  bool right = digitalRead(35) == LOW;
  if (left && right) {
    combo_cnt++;
    if (combo_cnt == 20) {
      btn_mode[4] = !btn_mode[4];
      btn_mode[5] = !btn_mode[5];
      lv_label_set_text(bt_mode_label, btn_mode[4] ? "A:Space B:Enter" : "A:A  B:B");
    }
  } else {
    combo_cnt = 0;
  }

  uint8_t report[8] = {0};
  char buf[64] = "";
  int idx = 2;

  for (int i = 0; i < 6; i++) {
    bool curr = (digitalRead(btn_pins[i]) == LOW);
    if (!curr) continue;

    uint8_t kc = btn_keycodes[i];
    const char* name = btn_names[i];

    if (i == 4) { kc = btn_mode[4] ? 0x2C : btn_keycodes[i]; name = btn_mode[4] ? "Space" : btn_names[i]; }
    if (i == 5) { kc = btn_mode[5] ? 0x28 : btn_keycodes[i]; name = btn_mode[5] ? "Enter" : btn_names[i]; }

    if (idx < 8) report[idx++] = kc;
    if (buf[0]) strcat(buf, " + ");
    strcat(buf, name);
  }

  if (memcmp(report, prevReport, 8) != 0) {
    if (deviceConnected) {
      bool swap = false;
      for (int i = 0; i < 8; i++) {
        if (report[i] != prevReport[i] && report[i] != 0 && prevReport[i] != 0) {
          swap = true; break;
        }
      }
      if (swap) {
        swapStep = 1;
        lv_label_set_text(bt_btn_label, buf);
        return;
      }
      input->setValue(report, 8);
      input->notify();
    }
    memcpy(prevReport, report, 8);
  }

  if (swapStep == 2) {
    swapStep = 0;
    if (deviceConnected) {
      input->setValue(report, 8);
      input->notify();
    }
    memcpy(prevReport, report, 8);
  }

  lv_label_set_text(bt_btn_label, buf[0] ? buf : "");
}

void enter_app(int i) {
  switch (i) {
    case 0: bt_gamepad_init(); break;
    case 1:
      in_app = true;
      app_id = 2;
      lv_obj_clean(lv_scr_act());
      fill(0x0000);
      nes_emu_init();
      nes_emu_deinit();
      update_menu_sel();
      lv_refr_now(NULL);
      break;
  }
}

void show_menu() {
  in_app = false;
  app_id = -1;
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "Select App");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

  const char* names[] = {"1. BT Gamepad", "2. NES Emulator"};
  for (int i = 0; i < 2; i++) {
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 120, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 30 + i * 34);
    lv_obj_set_style_border_width(btn, i == sel ? 2 : 0, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(btn, i == sel ? lv_color_hex(0x0044AA) : lv_color_hex(0x333333), 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, names[i]);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
  }
}

void update_menu_sel() {
  lv_obj_clean(lv_scr_act());
  show_menu();
}

void setup() {
  Serial.begin(115200); delay(1000);
  Serial.printf("Heap: %d free / %d total\n", ESP.getFreeHeap(), ESP.getHeapSize());
  Serial.printf("PSRAM: %d free / %d total\n", ESP.getFreePsram(), ESP.getPsramSize());
  Serial.printf("PSRAM ok: %s\n", ESP.getPsramSize() > 0 ? "YES" : "NO");
  pinMode(2, INPUT_PULLUP); pinMode(13, INPUT_PULLUP);
  pinMode(27, INPUT_PULLUP); pinMode(35, INPUT);
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
  static unsigned long t_menu = 0;
  static unsigned long t_bt = 0;
  unsigned long n = millis();

  if (!in_app) {
    if (n - t_menu > 200) {
      bool up = digitalRead(2) == LOW;
      bool dn = digitalRead(13) == LOW;
      bool a = digitalRead(34) == LOW;

      if (up || dn || a) t_menu = n;
      if (up && sel > 0) { sel--; update_menu_sel(); lv_refr_now(NULL); }
      if (dn && sel < 1) { sel++; update_menu_sel(); lv_refr_now(NULL); }
      if (a) { enter_app(sel); lv_refr_now(NULL); }
    }
  } else {
    if (app_id == 1 && n - t_bt > 50) {
      t_bt = n;
      bt_gamepad_loop();
      lv_refr_now(NULL);
    }
    if ((app_id == 1 && !deviceConnected && digitalRead(12) == LOW)) {
      if (app_id == 1) BLEDevice::deinit(false);
      update_menu_sel();
      lv_refr_now(NULL);
    }
  }

  lv_timer_handler();
  delay(5);
}