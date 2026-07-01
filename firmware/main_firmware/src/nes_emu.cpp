#include <Arduino.h>
#include <SPI.h>
#include <SPIFFS.h>
#include "nes_emu.h"
#include "rom_data.h"
#include "esp_task_wdt.h"

extern SPIClass spi;
extern "C" {
extern volatile int nofrendo_ticks;
#include "nofrendo.h"
#include "nes/nes.h"
#include "nes/nesinput.h"
#include "osd.h"
#include "bitmap.h"
#include "vid_drv.h"
#include "noftypes.h"
#include "event.h"
}

#define PIN_CS 5
#define PIN_DC 4

static bool running = false;

static void lcd_cmd(uint8_t c) {
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(c); digitalWrite(PIN_CS, HIGH);
}
static void lcd_dat_start() { digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); }
static void lcd_dat_end() { digitalWrite(PIN_CS, HIGH); }

#define COL_OFF 0
#define ROW_OFF 0

static void lcd_set_win(int x1, int y1, int x2, int y2) {
  lcd_cmd(0x2A); lcd_dat_start(); spi.transfer((y1+ROW_OFF)>>8); spi.transfer((y1+ROW_OFF)&0xFF); spi.transfer((y2+ROW_OFF)>>8); spi.transfer((y2+ROW_OFF)&0xFF); lcd_dat_end();
  lcd_cmd(0x2B); lcd_dat_start(); spi.transfer((x1+COL_OFF)>>8); spi.transfer((x1+COL_OFF)&0xFF); spi.transfer((x2+COL_OFF)>>8); spi.transfer((x2+COL_OFF)&0xFF); lcd_dat_end();
  lcd_cmd(0x2C); lcd_dat_start();
}

static uint16_t pal[64];
static uint16_t* fb = NULL;
#define FB_W 256
#define FB_H 240

int my_vid_init(int w, int h) {
  fb = (uint16_t*)malloc(FB_W * FB_H * 2);
  return fb ? 0 : -1;
}
void my_vid_shutdown() { free(fb); fb = NULL; }
int my_set_mode(int w, int h) { return 0; }

void my_set_palette(rgb_t* p) {
  for (int i = 0; i < 64; i++)
    pal[i] = ((p[i].r>>3)<<11) | ((p[i].g>>2)<<5) | (p[i].b>>3);
}

void my_vid_clear(uint8 color) {
  if (!fb) return;
  uint16_t c = pal[color & 0x3F];
  for (int i = 0; i < FB_W * FB_H; i++) fb[i] = c;
}

bitmap_t* my_lock_write() {
  static bitmap_t bm;
  bm.width = FB_W; bm.height = FB_H; bm.pitch = FB_W * 2; bm.data = (uint8*)fb;
  return &bm;
}

void my_free_write(int n, rect_t* r) {}

void my_custom_blit(bitmap_t* bm, int n, rect_t* r) {
  if (!bm || !bm->data) return;
  uint16_t* s = (uint16_t*)bm->data;
  lcd_set_win(4, 20, 123, 139);
  for (int y = 0; y < 120; y++) {
    int si = y * 2 * FB_W + 8;
    for (int x = 0; x < 120; x++) {
      uint16_t c = s[si + x * 2];
      spi.transfer(c>>8); spi.transfer(c&0xFF);
    }
  }
  lcd_dat_end();
  esp_task_wdt_reset();
}

static viddriver_t vd = {
  "st7735", my_vid_init, my_vid_shutdown, my_set_mode,
  my_set_palette, my_vid_clear, my_lock_write,
  my_free_write, my_custom_blit, false
};

static nesinput_t my_input = {INP_JOYPAD0, 0};

void* mem_alloc(int sz, bool fast) {
  return malloc(sz);
}

void osd_getvideoinfo(vidinfo_t* i) { i->default_width = FB_W; i->default_height = FB_H; i->driver = &vd; }
void osd_getsoundinfo(sndinfo_t* i) { i->sample_rate = 44100; i->bps = 16; }
int osd_init() { return 0; }
void osd_shutdown() {}
int osd_main(int c, char** v) { return 0; }
int osd_installtimer(int f, void* fn, int sz, void* cnt, int csz) { return 0; }

void osd_getinput() {
  static unsigned long lt = 0;
  unsigned long n = millis();
  if (n - lt >= 16) { nofrendo_ticks++; lt = n; }

  nes_t* ctx = nes_getcontextptr();
  if (!ctx) return;
  if (digitalRead(12) == LOW) { ctx->poweroff = true; return; }

  uint8 d = 0;
  if (digitalRead(2) == LOW) d |= INP_PAD_UP;
  if (digitalRead(13) == LOW) d |= INP_PAD_DOWN;
  if (digitalRead(27) == LOW) d |= INP_PAD_LEFT;
  if (digitalRead(35) == LOW) d |= INP_PAD_RIGHT;
  if (digitalRead(34) == LOW) d |= INP_PAD_A;
  my_input.data = d;

  esp_task_wdt_reset();
}

void osd_getmouse(int* x, int* y, int* b) { *x = *y = *b = 0; }
void osd_fullname(char* fn, const char* sn) { strcpy(fn, sn); }
char* osd_newextension(char* s, char* e) { return s; }
int osd_makesnapname(char* fn, int l) { return -1; }
void osd_setsound(void (*f)(void*, int)) {}

void nes_emu_init() {
  running = true;
  Serial.println("[NES] init start");
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT);

  Serial.print("[NES] SPIFFS begin... ");
  if (!SPIFFS.begin(true)) { Serial.println("FAIL"); return; }
  Serial.println("OK");

  Serial.print("[NES] open /rom.nes... ");
  File f = SPIFFS.open("/rom.nes", "w");
  if (!f) { Serial.println("FAIL"); return; }
  Serial.println("OK");

  Serial.print("[NES] write ROM... ");
  f.write((uint8_t*)rom_data, rom_size);
  f.close();
  Serial.println("OK");

  Serial.print("[NES] vid_init... ");
  if (my_vid_init(FB_W, FB_H)) { Serial.println("FAIL"); return; }
  Serial.println("OK");

  Serial.print("[NES] input_register... ");
  input_register(&my_input);
  Serial.println("OK");

  Serial.print("[NES] nes_create... ");
  nes_t* nes = nes_create();
  if (!nes) { Serial.println("FAIL"); return; }
  Serial.println("OK");

  Serial.print("[NES] nes_insertcart... ");
  if (nes_insertcart("/rom.nes", nes)) { Serial.println("FAIL"); nes_destroy(&nes); return; }
  Serial.println("OK");

  Serial.println("[NES] starting emulation...");
  nes_emulate();
}

void nes_emu_deinit() {
  running = false;
  nes_t* ctx = nes_getcontextptr();
  if (ctx) { nes_poweroff(); nes_destroy(&ctx); }
  my_vid_shutdown();
  SPIFFS.remove("/rom.nes");
}