#include <Arduino.h>
#include <BLEDevice.h>
#include "main.h"
#include "lcd/lcd.h"
#include "input/input.h"
#include "menu/menu.h"
#include "apps/bt_gamepad.h"
#include "apps/sd_manager.h"
#include "apps/web_manager.h"

void setup() {
  Serial.begin(115200); delay(1000);
  Serial.printf("Heap: %d free / %d total\n", ESP.getFreeHeap(), ESP.getHeapSize());
  Serial.printf("PSRAM: %d free / %d total\n", ESP.getFreePsram(), ESP.getPsramSize());
  Serial.printf("PSRAM ok: %s\n", ESP.getPsramSize() > 0 ? "YES" : "NO");
  pinMode(2, INPUT_PULLUP); pinMode(13, INPUT_PULLUP);
  pinMode(27, INPUT_PULLUP); pinMode(35, INPUT);
  pinMode(34, INPUT); pinMode(12, INPUT_PULLUP);
  lcd_init(); fill_screen(BLACK);
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
      if (dn && sel < MENU_ITEMS - 1) { sel++; update_menu_sel(); }
      if (a) { enter_app(sel); }
    }
  } else {
    if (app_id == 1 && n - t > 50) {
      t = n; bt_gamepad_loop();
    }
    if (app_id == 1 && !bt_gamepad_is_connected() && digitalRead(12) == LOW) {
      BLEDevice::deinit(false);
      show_menu();
    }
    if (app_id == 3) {
      sd_manager_loop();
    }
    if (app_id == 4) {
      web_manager_loop();
    }
  }

  delay(5);
}
