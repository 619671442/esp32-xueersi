#pragma once
#include <stdint.h>

#define MENU_ITEMS 4
#define VISIBLE_ITEMS 3

extern bool in_app;
extern int app_id;
extern int sel;
extern int scrollOffset;

void show_menu();
void update_menu_sel();
void enter_app(int i);
