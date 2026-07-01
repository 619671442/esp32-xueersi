#include <Arduino.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include "nes_emu.h"
#include "sd_manager.h"
#include "font_8x16.h"

#define PIN_CS 5
#define PIN_DC 4
#define PIN_MOSI 23
#define PIN_SCLK 18

SPIClass spi(VSPI);

#define BLACK   0x0000
#define WHITE   0xFFFF
#define BLUE    0x001F
#define DARKBL  0x0044AA
#define GRAY    0x8410
#define DKGRAY  0x39E7
#define SEL_BG  0x0044AA
#define BTN_BG  0x333333

static void lcd_cmd(uint8_t c) {
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(c); digitalWrite(PIN_CS, HIGH);
}
static void lcd_dat_start() { digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); }
static void lcd_dat_end() { digitalWrite(PIN_CS, HIGH); }
static void lcd_dat(uint8_t d) { lcd_dat_start(); spi.transfer(d); lcd_dat_end(); }

void lcd_init() {
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT); digitalWrite(PIN_DC, HIGH);
  spi.begin(PIN_SCLK, 19, PIN_MOSI, PIN_CS); spi.setFrequency(40000000);
  delay(50);
  lcd_cmd(0x01); delay(5);
  lcd_cmd(0x11); delay(120);
  lcd_cmd(0xB1); lcd_dat(0x05); lcd_dat(0x3A); lcd_dat(0x3A);
  lcd_cmd(0xB2); lcd_dat(0x05); lcd_dat(0x3A); lcd_dat(0x3A);
  lcd_cmd(0xB3); lcd_dat(0x05); lcd_dat(0x3A); lcd_dat(0x3A); lcd_dat(0x05); lcd_dat(0x3A); lcd_dat(0x3A);
  lcd_cmd(0xB4); lcd_dat(0x03);
  lcd_cmd(0xC0); lcd_dat(0x62); lcd_dat(0x02); lcd_dat(0x04);
  lcd_cmd(0xC1); lcd_dat(0x00);
  lcd_cmd(0xC2); lcd_dat(0x0C); lcd_dat(0x00);
  lcd_cmd(0xC3); lcd_dat(0x8D); lcd_dat(0x6A);
  lcd_cmd(0xC4); lcd_dat(0x8D); lcd_dat(0xEE);
  lcd_cmd(0xC5); lcd_dat(0x0E);
  lcd_cmd(0x36); lcd_dat(0x40);
  lcd_cmd(0x3A); lcd_dat(0x05);
  lcd_cmd(0x29); delay(10);
}

#define COL_OFF 0
#define ROW_OFF 0

void lcd_set_win(int x1, int y1, int x2, int y2) {
  lcd_cmd(0x2A); lcd_dat_start(); spi.transfer((y1+ROW_OFF)>>8); spi.transfer((y1+ROW_OFF)&0xFF); spi.transfer((y2+ROW_OFF)>>8); spi.transfer((y2+ROW_OFF)&0xFF); lcd_dat_end();
  lcd_cmd(0x2B); lcd_dat_start(); spi.transfer((x1+COL_OFF)>>8); spi.transfer((x1+COL_OFF)&0xFF); spi.transfer((x2+COL_OFF)>>8); spi.transfer((x2+COL_OFF)&0xFF); lcd_dat_end();
  lcd_cmd(0x2C); lcd_dat_start();
}

void fill_screen(uint16_t c) {
  // Fill all 160 physical columns × 128 physical rows
  lcd_cmd(0x2A); lcd_dat_start(); spi.transfer(0>>8); spi.transfer(0&0xFF); spi.transfer(127>>8); spi.transfer(127&0xFF); lcd_dat_end();
  lcd_cmd(0x2B); lcd_dat_start(); spi.transfer(0>>8); spi.transfer(0&0xFF); spi.transfer(159>>8); spi.transfer(159&0xFF); lcd_dat_end();
  lcd_cmd(0x2C); lcd_dat_start();
  for (int i = 0; i < 20480; i++) { spi.transfer(c>>8); spi.transfer(c&0xFF); }
  lcd_dat_end();
}

void fill_rect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
  if (x + w > 160) w = 160 - x; if (y + h > 128) h = 128 - y;
  if (w <= 0 || h <= 0) return;
  lcd_set_win(x, y, x + w - 1, y + h - 1);
  for (int i = 0; i < w * h; i++) { spi.transfer(c>>8); spi.transfer(c&0xFF); }
  lcd_dat_end();
}

void draw_pixel(int x, int y, uint16_t c) {
  if (x < 0 || x >= 160 || y < 0 || y >= 128) return;
  lcd_set_win(x, y, x, y);
  spi.transfer(c>>8); spi.transfer(c&0xFF);
  lcd_dat_end();
}

void draw_char(int x, int y, unsigned char ch, uint16_t fg, uint16_t bg) {
  if (ch < 0x20 || ch > 0x7E) return;
  int idx = ch - 0x20;
  for (int row = 0; row < 16; row++) {
    uint8_t bits = font8x16[idx][row];
    int yy = y + row;
    if (yy < 0 || yy >= 128) continue;
    lcd_set_win(x, yy, x + 7, yy);
    for (int col = 0; col < 8; col++) {
      uint16_t c = (bits & (1 << (7 - col))) ? fg : bg;
      spi.transfer(c>>8); spi.transfer(c&0xFF);
    }
    lcd_dat_end();
  }
}

void draw_str(int x, int y, const char* s, uint16_t fg, uint16_t bg) {
  while (*s) {
    if (x + 8 > 160) break;
    draw_char(x, y, *s, fg, bg);
    x += 8; s++;
  }
}

void draw_str_center(int y, const char* s, uint16_t fg, uint16_t bg) {
  int len = 0; const char* p = s; while (*p) { len++; p++; }
  int x = (160 - len * 8) / 2;
  if (x < 0) x = 0;
  draw_str(x, y, s, fg, bg);
}

void draw_menu_item(int x, int y, int num, const char* label, bool sel) {
  char buf[32];
  sprintf(buf, "%s%d. %s", sel ? "*" : " ", num, label);
  draw_str(x, y, buf, sel ? WHITE : GRAY, BLACK);
}

void blit_area(int x, int y, const uint16_t* data, int w, int h) {
  lcd_set_win(x, y, x + w - 1, y + h - 1);
  for (int i = 0; i < w * h; i++) { spi.transfer(data[i]>>8); spi.transfer(data[i]&0xFF); }
  lcd_dat_end();
}

// Menu / App state
static int sel = 0;
static bool in_app = false;
static int app_id = -1;

// BT gamepad globals
static BLEHIDDevice* hid = NULL;
static BLECharacteristic* input = NULL;
static volatile bool deviceConnected = false;

static const int btn_pins[6] = {2, 13, 27, 35, 34, 12};
static int btn_mode[6] = {0};
static unsigned long long_press[6] = {0};
static bool last_btn[6] = {0};
static char btn_display[64] = "";
static bool bt_connected = false;

#define BIT_A     0
#define BIT_B     1
#define BIT_UP    2
#define BIT_DOWN  3
#define BIT_LEFT  4
#define BIT_RIGHT 5

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

void bt_gamepad_init() {
  in_app = true; app_id = 1;
  fill_screen(BLACK);
  draw_str_center(4, "BT Gamepad", WHITE, BLACK);
  draw_str_center(22, "A:A  B:B", GRAY, BLACK);
  draw_str_center(38, "Status: Waiting...", GRAY, BLACK);
  btn_display[0] = 0;

  for (int i = 0; i < 6; i++) { last_btn[i] = false; long_press[i] = 0; }
  btn_mode[4] = 0; btn_mode[5] = 0;

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
  if (deviceConnected != bt_connected) {
    bt_connected = deviceConnected;
    fill_rect(0, 38, 160, 16, BLACK);
    draw_str_center(38, deviceConnected ? "Status: Running" : "Status: Waiting...", GRAY, BLACK);
    if (!deviceConnected) { fill_rect(0, 96, 160, 32, BLACK); btn_display[0] = 0; }
  }

  int combo = 0;
  for (int i = 0; i < 6; i++) {
    bool curr = (digitalRead(btn_pins[i]) == LOW);
    if (curr && !last_btn[i]) long_press[i] = millis();
    if (!curr) long_press[i] = 0;
    last_btn[i] = curr;
    if (curr) combo++;
  }

  if (combo >= 2) {
    bool both_sides = (digitalRead(27) == LOW && digitalRead(35) == LOW);
    if (both_sides && long_press[2] > 0 && millis() - long_press[2] >= 1000) {
      btn_mode[4] = !btn_mode[4]; btn_mode[5] = !btn_mode[5];
      long_press[2] = 0; long_press[3] = 0;
      fill_rect(0, 22, 160, 16, BLACK);
      draw_str_center(22, btn_mode[4] ? "A:Space B:Enter" : "A:A  B:B", GRAY, BLACK);
      return;
    }
  }

  uint8_t report[8] = {0};
  char buf[64] = ""; int idx = 2;
  static const uint8_t kc[6] = {0x52, 0x51, 0x50, 0x4F, 0x04, 0x05};
  static const uint8_t kc_alt[2] = {0x2C, 0x28};
  static const char* btn_labels[6] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B"};
  static const char* btn_labels_alt[6] = {"UP", "DOWN", "LEFT", "RIGHT", "Space", "Enter"};

  for (int i = 0; i < 6; i++) {
    if (!last_btn[i]) continue;
    const char* name = (i >= 4 && btn_mode[i]) ? btn_labels_alt[i] : btn_labels[i];
    uint8_t key = (i >= 4 && btn_mode[i]) ? kc_alt[i-4] : kc[i];
    if (idx < 8) report[idx++] = key;
    if (buf[0]) strcat(buf, " + ");
    strcat(buf, name);
  }

  static uint8_t prevReport[8] = {0};
  if (memcmp(report, prevReport, 8) != 0) {
    if (deviceConnected) {
      bool swap = false;
      for (int i = 0; i < 8; i++) {
        if (report[i] != prevReport[i] && report[i] != 0 && prevReport[i] != 0) { swap = true; break; }
      }
      if (swap) {
        uint8_t empty[8] = {0};
        input->setValue(empty, 8); input->notify();
        delay(5);
      }
      input->setValue(report, 8); input->notify();
    }
    memcpy(prevReport, report, 8);
  }

  if (strcmp(buf, btn_display) != 0) {
    strcpy(btn_display, buf);
    fill_rect(0, 96, 160, 32, BLACK);
    if (buf[0]) draw_str_center(100, buf, WHITE, BLACK);
  }
}

void enter_app(int i) {
  switch (i) {
    case 0: bt_gamepad_init(); break;
    case 1:
      in_app = true; app_id = 2;
      fill_screen(BLACK);
      nes_emu_init();
      nes_emu_deinit();
      show_menu();
      break;
    case 2: in_app = true; app_id = 3; sd_manager_init(); break;
  }
}

void show_menu() {
  in_app = false; app_id = -1;
  fill_screen(BLACK);
  draw_str_center(4, "Select App", WHITE, BLACK);
  draw_menu_item(8, 36, 1, "BT Gamepad", sel == 0);
  draw_menu_item(8, 66, 2, "NES Emulator", sel == 1);
  draw_menu_item(8, 96, 3, "SD File Manager", sel == 2);
}

void update_menu_sel() {
  draw_menu_item(8, 36, 1, "BT Gamepad", sel == 0);
  draw_menu_item(8, 66, 2, "NES Emulator", sel == 1);
  draw_menu_item(8, 96, 3, "SD File Manager", sel == 2);
}

void setup() {
  Serial.begin(115200); delay(1000);
  Serial.printf("Heap: %d free / %d total\n", ESP.getFreeHeap(), ESP.getHeapSize());
  Serial.printf("PSRAM: %d free / %d total\n", ESP.getFreePsram(), ESP.getPsramSize());
  Serial.printf("PSRAM ok: %s\n", ESP.getPsramSize() > 0 ? "YES" : "NO");
  pinMode(2, INPUT_PULLUP); pinMode(13, INPUT_PULLUP);
  pinMode(27, INPUT_PULLUP); pinMode(35, INPUT);
  pinMode(34, INPUT); pinMode(12, INPUT_PULLUP);
  lcd_init(); fill_screen(BLACK);
  sel = 0;
  show_menu();
}

void loop() {
  static unsigned long t = 0;
  unsigned long n = millis();

  if (!in_app) {
    if (n - t > 200) {
      bool up = digitalRead(2) == LOW;
      bool dn = digitalRead(13) == LOW;
      bool a = digitalRead(34) == LOW;
      if (up || dn || a) t = n;
      if (up && sel > 0) { sel--; update_menu_sel(); }
      if (dn && sel < 2) { sel++; update_menu_sel(); }
      if (a) { enter_app(sel); }
    }
  } else {
    if (app_id == 1 && n - t > 50) {
      t = n; bt_gamepad_loop();
    }
    if (app_id == 1 && !deviceConnected && digitalRead(12) == LOW) {
      BLEDevice::deinit(false);
      show_menu();
    }
    if (app_id == 3) {
      sd_manager_loop();
    }
  }

  delay(5);
}