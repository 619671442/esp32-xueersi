#include <Arduino.h>
#include <SPI.h>
#include <SPIFFS.h>
#include "nes_emu.h"
#include "esp_timer.h"

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

#define FB_W 256
#define FB_H 240

static uint16_t pal[64];
static uint16_t* fb = NULL;
static esp_timer_handle_t tick_timer = NULL;

static void lcd_cmd(uint8_t c) {
  digitalWrite(PIN_DC, LOW); digitalWrite(PIN_CS, LOW); spi.transfer(c); digitalWrite(PIN_CS, HIGH);
}
static void lcd_dat_start() { digitalWrite(PIN_DC, HIGH); digitalWrite(PIN_CS, LOW); }
static void lcd_dat_end() { digitalWrite(PIN_CS, HIGH); }

static void lcd_set_win(int x1, int y1, int x2, int y2) {
  lcd_cmd(0x2A); lcd_dat_start(); spi.transfer((y1)>>8); spi.transfer((y1)&0xFF); spi.transfer((y2)>>8); spi.transfer((y2)&0xFF); lcd_dat_end();
  lcd_cmd(0x2B); lcd_dat_start(); spi.transfer((x1)>>8); spi.transfer((x1)&0xFF); spi.transfer((x2)>>8); spi.transfer((x2)&0xFF); lcd_dat_end();
  lcd_cmd(0x2C); lcd_dat_start();
}

static void tick_timer_cb(void* arg) {
  nofrendo_ticks++;
}

// ---- Video driver ----

int my_vid_init(int w, int h) {
  fb = (uint16_t*)malloc(FB_W * FB_H * 2);
  return fb ? 0 : -1;
}

void my_vid_shutdown() {
  free(fb); fb = NULL;
}

int my_set_mode(int w, int h) { return 0; }

void my_set_palette(rgb_t* p) {
  for (int i = 0; i < 64; i++)
    pal[i] = ((p[i].r >> 3) << 11) | ((p[i].g >> 2) << 5) | (p[i].b >> 3);
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
      spi.transfer(c >> 8); spi.transfer(c & 0xFF);
    }
  }
  lcd_dat_end();
}

static viddriver_t vd = {
  "st7735", my_vid_init, my_vid_shutdown, my_set_mode,
  my_set_palette, my_vid_clear, my_lock_write,
  my_free_write, my_custom_blit, false
};

static nesinput_t my_input = {INP_JOYPAD0, 0};

// ---- OSD callbacks ----

void* mem_alloc(int sz, bool fast) {
  return malloc(sz);
}

void osd_getvideoinfo(vidinfo_t* i) {
  i->default_width = FB_W; i->default_height = FB_H; i->driver = &vd;
}

void osd_getsoundinfo(sndinfo_t* i) {
  i->sample_rate = 44100; i->bps = 16;
}

int osd_init() { return 0; }
void osd_shutdown() {}
int osd_main(int c, char** v) { return 0; }

int osd_installtimer(int hertz, void* fn, int sz, void* cnt, int csz) {
  esp_timer_create_args_t args = {};
  args.callback = &tick_timer_cb;
  args.name = "nofrendo_tmr";
  if (esp_timer_create(&args, &tick_timer) != ESP_OK) return -1;
  if (esp_timer_start_periodic(tick_timer, 1000000 / hertz) != ESP_OK) return -1;
  return 0;
}

void osd_getinput() {
  nes_t* ctx = nes_getcontextptr();
  if (!ctx) return;

  if (digitalRead(12) == LOW) {
    ctx->poweroff = true;
    return;
  }

  uint8 d = 0;
  if (digitalRead(2) == LOW)  d |= INP_PAD_UP;
  if (digitalRead(13) == LOW) d |= INP_PAD_DOWN;
  if (digitalRead(27) == LOW) d |= INP_PAD_LEFT;
  if (digitalRead(35) == LOW) d |= INP_PAD_RIGHT;
  if (digitalRead(34) == LOW) d |= INP_PAD_A;
  my_input.data = d;
}

void osd_getmouse(int* x, int* y, int* b) { *x = *y = *b = 0; }
void osd_fullname(char* fn, const char* sn) { strcpy(fn, sn); }
char* osd_newextension(char* s, char* e) { return s; }
int osd_makesnapname(char* fn, int l) { return -1; }
void osd_setsound(void (*f)(void*, int)) {}

// ---- Public API ----

void nes_emu_init() {
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT);

  if (!SPIFFS.begin(true, "/")) { Serial.println("[NES] SPIFFS FAIL"); return; }

  File f = SPIFFS.open("/nesrom.nes", "r");
  if (!f) {
    Serial.println("[NES] no /nesrom.nes, will use library intro ROM");
  } else {
    int rom_fsize = f.size();
    f.close();
    Serial.printf("[NES] ROM size: %d\n", rom_fsize);
  }

  if (my_vid_init(FB_W, FB_H)) { Serial.println("[NES] vid_init FAIL"); return; }

  input_register(&my_input);

  osd_installtimer(60, NULL, 0, NULL, 0);

  nes_t* nes = nes_create();
  if (!nes) { Serial.println("[NES] nes_create FAIL"); return; }

  if (nes_insertcart("/nesrom.nes", nes)) {
    Serial.println("[NES] nes_insertcart FAIL");
    nes_destroy(&nes);
    return;
  }

  Serial.println("[NES] starting emulation");
  nes_emulate();
}

void nes_emu_deinit() {
  if (tick_timer) {
    esp_timer_stop(tick_timer);
    esp_timer_delete(tick_timer);
    tick_timer = NULL;
  }
  nes_t* ctx = nes_getcontextptr();
  if (ctx) { nes_poweroff(); nes_destroy(&ctx); }
  my_vid_shutdown();
  SPIFFS.end();
}