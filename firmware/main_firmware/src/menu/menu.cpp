#include <Arduino.h>
#include "../main.h"
#include "../lcd/lcd.h"
#include "../input/input.h"
#include "menu.h"
#include "../apps/bt_gamepad.h"
#include "../apps/nes_emu.h"
#include "../apps/sd_manager.h"
#include "../apps/web_manager.h"

int sel = 0;
int scrollOffset = 0;
bool in_app = false;
int app_id = -1;

static const char* itemLabels[MENU_ITEMS] = {
  "BT Gamepad", "NES Emulator", "SD File Manager", "Web Manager"
};

static void drawItem(int idx, int row) {
  int y = 28 + row * 20;
  bool active = (idx == sel);
  fill_rect(0, y, 160, 16, BLACK);
  char buf[24];
  snprintf(buf, sizeof(buf), "%s%d. %s", active ? "*" : " ", idx + 1, itemLabels[idx]);
  draw_str(8, y, buf, active ? WHITE : GRAY, BLACK);
}

void show_menu() {
  in_app = false; app_id = -1;
  scrollOffset = 0;
  sel = 0;
  fill_screen(BLACK);
  draw_str_center(4, "Select App", WHITE, BLACK);
  for (int i = 0; i < VISIBLE_ITEMS && i < MENU_ITEMS; i++)
    drawItem(i, i);
  draw_str_center(98, "UP/DN:Sel A:Open", DKGRAY, BLACK);
  draw_str_center(114, "B:Back to menu", DKGRAY, BLACK);
}

void update_menu_sel() {
  if (sel < scrollOffset) scrollOffset = sel;
  if (sel >= scrollOffset + VISIBLE_ITEMS) scrollOffset = sel - VISIBLE_ITEMS + 1;
  for (int i = 0; i < VISIBLE_ITEMS && i < MENU_ITEMS; i++) {
    int idx = scrollOffset + i;
    if (idx < MENU_ITEMS) drawItem(idx, i);
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
    case 3: in_app = true; app_id = 4; web_manager_init(); break;
  }
}