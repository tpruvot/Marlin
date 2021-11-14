/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2021 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Based on framework-arduinoststm32/cores/arduino/wiring_analog.c
 */

#include "../platforms.h"

#ifdef HAL_STM32

#include "../../inc/MarlinConfig.h"

#include <PinConfigured.h>

#if (defined(HAL_TIM_MODULE_ENABLED) && !defined(HAL_TIM_MODULE_ONLY))
  // This is the list of the IOs configured in framework
  // It is required by pwm_start, for a second start after a pwm_stop()
  extern "C" uint32_t g_anOutputPinConfigured[MAX_NB_PORT];
#endif

uint16_t pwmFrequencies[TIMER_NUM] = { 0 };

// Optionally used to preconfigure a timer pwm frequency
// use timer_num 1 for TIM1
void pwmSetFrequency(const uint8_t timer_num, uint16_t freq) {
  if (timer_num <= TIMER_NUM && timer_num >= 1)
    pwmFrequencies[timer_num - 1] = freq;
}

// Can reduce memory footprint, if pwmFrequencies array not used
uint16_t pwmGetDefaultFrequency(const pin_t pin) {
  switch(pin) {
  case FAN_PIN:
  #if PIN_EXISTS(FAN1)
    case FAN1_PIN:
  #endif
  #if PIN_EXISTS(FAN2)
    case FAN2_PIN:
  #endif
    #if ENABLED(FAST_PWM_FAN) && defined(FAST_PWM_FAN_FREQUENCY)
      return FAST_PWM_FAN_FREQUENCY;
    #endif
  default:
    break;
  }
  return PWM_FREQUENCY;
}

// Right now, PWM output only works on the pins with hardware support.
// These are defined in the appropriate variant.cpp file.
// For the rest of the pins, we default to digital output.
void pwmWrite(const pin_t pin, const uint8_t val) {
  PinName p = digitalPinToPinName(pin);
  if (p == NC) return;

  #if defined(HAL_TIM_MODULE_ENABLED) && !defined(HAL_TIM_MODULE_ONLY)
    if (pin_in_pinmap(p, PinMap_PWM)) {
      if (!is_pin_configured(p, g_anOutputPinConfigured))
        set_pin_configured(p, g_anOutputPinConfigured);
      uint16_t pwmFreq = pwmGetDefaultFrequency(pin);
      TIM_TypeDef *Instance = (TIM_TypeDef *)pinmap_peripheral(p, PinMap_PWM);
      timer_index_t num = get_timer_index(Instance); // TIM1 = 0
      if (num < TIMER_NUM && num != UNKNOWN_TIMER && pwmFrequencies[num])
        pwmFreq = pwmFrequencies[num];
      pwm_start(p, int(pwmFreq), val, RESOLUTION_8B_COMPARE_FORMAT);
    }
    else
  #endif /* HAL_TIM_MODULE_ENABLED && !HAL_TIM_MODULE_ONLY */
  {
    // DIGITAL PIN ONLY, defaults to digital write
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val < 128 ? LOW : HIGH);
  }
}

#endif // HAL_STM32
