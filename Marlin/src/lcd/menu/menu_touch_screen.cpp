/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../inc/MarlinConfigPre.h"

#if ALL(HAS_MARLINUI_MENU, TOUCH_SCREEN_CALIBRATION)

#include "menu_item.h"
#include "../marlinui.h"

void touch_screen_calibration() {
  ui.touch_calibration_screen();
}

void menu_touch_screen() {
  START_MENU();
  BACK_ITEM(MSG_MAIN_MENU);
  MENU_ITEM(submenu, MSG_CALIBRATION, touch_screen_calibration);
#ifdef TS_V11
  STATIC_ITEM_F(F("ILI9341 v1.1"), SS_LEFT);
#elif defined(TS_V12)
  STATIC_ITEM_F(F("ILI9341 v1.2"), SS_LEFT);
#elif defined(TS_V19)
  STATIC_ITEM_F(F("ILI9341 2019"), SS_LEFT);
#endif
  {
    char line[20];
    snprintf(line, 20, "X: %d %d", int(touch_calibration.calibration.x), int(touch_calibration.calibration.offset_x));
    STATIC_ITEM_F(F(line), SS_LEFT);
    snprintf(line, 20, "Y: %d %d", int(touch_calibration.calibration.y), int(touch_calibration.calibration.offset_y));
    STATIC_ITEM_F(F(line), SS_LEFT);
  }
  END_MENU();
}

#endif // HAS_LCD_MENU && TOUCH_SCREEN_CALIBRATION
