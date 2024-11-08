/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
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
 */

/**
 * HAL for stm32duino.com based on Libmaple and compatible (STM32F1)
 */

#ifdef __STM32F1__

#include "../../inc/MarlinConfig.h"
#include "HAL.h"

#include <STM32ADC.h>

// ------------------------
// Types
// ------------------------

#define __I
#define __IO volatile
 typedef struct {
   __I  uint32_t CPUID;                   /*!< Offset: 0x000 (R/ )  CPUID Base Register                                   */
   __IO uint32_t ICSR;                    /*!< Offset: 0x004 (R/W)  Interrupt Control and State Register                  */
   __IO uint32_t VTOR;                    /*!< Offset: 0x008 (R/W)  Vector Table Offset Register                          */
   __IO uint32_t AIRCR;                   /*!< Offset: 0x00C (R/W)  Application Interrupt and Reset Control Register      */
   __IO uint32_t SCR;                     /*!< Offset: 0x010 (R/W)  System Control Register                               */
   __IO uint32_t CCR;                     /*!< Offset: 0x014 (R/W)  Configuration Control Register                        */
   __IO uint8_t  SHP[12];                 /*!< Offset: 0x018 (R/W)  System Handlers Priority Registers (4-7, 8-11, 12-15) */
   __IO uint32_t SHCSR;                   /*!< Offset: 0x024 (R/W)  System Handler Control and State Register             */
   __IO uint32_t CFSR;                    /*!< Offset: 0x028 (R/W)  Configurable Fault Status Register                    */
   __IO uint32_t HFSR;                    /*!< Offset: 0x02C (R/W)  HardFault Status Register                             */
   __IO uint32_t DFSR;                    /*!< Offset: 0x030 (R/W)  Debug Fault Status Register                           */
   __IO uint32_t MMFAR;                   /*!< Offset: 0x034 (R/W)  MemManage Fault Address Register                      */
   __IO uint32_t BFAR;                    /*!< Offset: 0x038 (R/W)  BusFault Address Register                             */
   __IO uint32_t AFSR;                    /*!< Offset: 0x03C (R/W)  Auxiliary Fault Status Register                       */
   __I  uint32_t PFR[2];                  /*!< Offset: 0x040 (R/ )  Processor Feature Register                            */
   __I  uint32_t DFR;                     /*!< Offset: 0x048 (R/ )  Debug Feature Register                                */
   __I  uint32_t ADR;                     /*!< Offset: 0x04C (R/ )  Auxiliary Feature Register                            */
   __I  uint32_t MMFR[4];                 /*!< Offset: 0x050 (R/ )  Memory Model Feature Register                         */
   __I  uint32_t ISAR[5];                 /*!< Offset: 0x060 (R/ )  Instruction Set Attributes Register                   */
        uint32_t RESERVED0[5];
   __IO uint32_t CPACR;                   /*!< Offset: 0x088 (R/W)  Coprocessor Access Control Register                   */
 } SCB_Type;

// ------------------------
// Local defines
// ------------------------

#define SCS_BASE            (0xE000E000UL)                            /*!< System Control Space Base Address  */
#define SCB_BASE            (SCS_BASE +  0x0D00UL)                    /*!< System Control Block Base Address  */

#define SCB                 ((SCB_Type       *)     SCB_BASE      )   /*!< SCB configuration struct           */

/* SCB Application Interrupt and Reset Control Register Definitions */
#define SCB_AIRCR_VECTKEY_Pos              16                                             /*!< SCB AIRCR: VECTKEY Position */
#define SCB_AIRCR_VECTKEY_Msk              (0xFFFFUL << SCB_AIRCR_VECTKEY_Pos)            /*!< SCB AIRCR: VECTKEY Mask */

#define SCB_AIRCR_PRIGROUP_Pos              8                                             /*!< SCB AIRCR: PRIGROUP Position */
#define SCB_AIRCR_PRIGROUP_Msk             (7UL << SCB_AIRCR_PRIGROUP_Pos)                /*!< SCB AIRCR: PRIGROUP Mask */

// ------------------------
// Serial ports
// ------------------------

#if defined(SERIAL_USB) && !HAS_SD_HOST_DRIVE

  USBSerial SerialUSB;
  DefaultSerial1 MSerial0(true, SerialUSB);

  #if ENABLED(EMERGENCY_PARSER)
    #include "../libmaple/usb/stm32f1/usb_reg_map.h"
    #include "libmaple/usb_cdcacm.h"
    // The original callback is not called (no way to retrieve address).
    // That callback detects a special STM32 reset sequence: this functionality is not essential
    // as M997 achieves the same.
    void my_rx_callback(unsigned int, void*) {
      // max length of 16 is enough to contain all emergency commands
      uint8 buf[16];

      //rx is usbSerialPart.endpoints[2]
      uint16 len = usb_get_ep_rx_count(USB_CDCACM_RX_ENDP);
      uint32 total = usb_cdcacm_data_available();

      if (len == 0 || total == 0 || !WITHIN(total, len, COUNT(buf)))
        return;

      // cannot get character by character due to bug in composite_cdcacm_peek_ex
      len = usb_cdcacm_peek(buf, total);

      for (uint32 i = 0; i < len; i++)
        emergency_parser.update(MSerial0.emergency_state, buf[i + total - len]);
    }
  #endif
#endif

// ------------------------
// Watchdog Timer
// ------------------------

#if ENABLED(USE_WATCHDOG)

  #include <libmaple/iwdg.h>

  void watchdogSetup() {
    // do whatever. don't remove this function.
  }

  /**
   *  The watchdog clock is 40Khz. So for a 4s or 8s interval use a /256 preescaler and 625 or 1250 reload value (counts down to 0).
   */
  #define STM32F1_WD_RELOAD TERN(WATCHDOG_DURATION_8S, 1250, 625) // 4 or 8 second timeout

  /**
   * @brief  Initialize the independent hardware watchdog.
   *
   * @return No return
   *
   * @details The watchdog clock is 40Khz. So for a 4s or 8s interval use a /256 preescaler and 625 or 1250 reload value (counts down to 0).
   */
  void MarlinHAL::watchdog_init() {
    #if DISABLED(DISABLE_WATCHDOG_INIT)
      iwdg_init(IWDG_PRE_256, STM32F1_WD_RELOAD);
    #endif
  }

  // Reset watchdog. MUST be called every 4 or 8 seconds after the
  // first watchdog_init or the STM32F1 will reset.
  void MarlinHAL::watchdog_refresh() {
    #if DISABLED(PINS_DEBUGGING) && PIN_EXISTS(LED)
      TOGGLE(LED_PIN);  // heartbeat indicator
    #endif
    iwdg_feed();
  }

#endif // USE_WATCHDOG

// ------------------------
// ADC
// ------------------------

// Watch out for recursion here! Our pin_t is signed, so pass through to Arduino -> analogRead(uint8_t)

uint16_t analogRead(const pin_t pin) {
  const bool is_analog = _GET_MODE(pin) == GPIO_INPUT_ANALOG;
  return is_analog ? analogRead(uint8_t(pin)) : 0;
}

// Wrapper to maple unprotected analogWrite
void analogWrite(const pin_t pin, int pwm_val8) {
  if (PWM_PIN(pin)) analogWrite(uint8_t(pin), pwm_val8);
}

uint16_t MarlinHAL::adc_result;

// ------------------------
// Private functions
// ------------------------

static void NVIC_SetPriorityGrouping(uint32_t PriorityGroup) {
  uint32_t reg_value;
  uint32_t PriorityGroupTmp = (PriorityGroup & (uint32_t)0x07);               // only values 0..7 are used

  reg_value  =  SCB->AIRCR;                                                   // read old register configuration
  reg_value &= ~(SCB_AIRCR_VECTKEY_Msk | SCB_AIRCR_PRIGROUP_Msk);             // clear bits to change
  reg_value  =  (reg_value                                 |
                ((uint32_t)0x5FA << SCB_AIRCR_VECTKEY_Pos) |
                (PriorityGroupTmp << 8));                                     // Insert write key & priority group
  SCB->AIRCR =  reg_value;
}

// ------------------------
// Public functions
// ------------------------

void flashFirmware(const int16_t) { hal.reboot(); }

//
// Leave PA11/PA12 intact if USBSerial is not used
//
#if SERIAL_USB
  namespace wirish { namespace priv {
    #if SERIAL_PORT > 0
      #if SERIAL_PORT2
        #if SERIAL_PORT2 > 0
          void board_setup_usb() {}
        #endif
      #else
        void board_setup_usb() {}
      #endif
    #endif
  } }
#endif

TERN_(POSTMORTEM_DEBUGGING, extern void install_min_serial());

uint16_t VariantCoreClock = uint16_t(F_CPU/1000000); // in MHz

#ifdef OVERCLOCK

#define RCC_HSE_ON      ((uint32_t)0x00010000)
#define RCC_SYSCLKSource_PLLCLK ((uint32_t)0x00000002)
#define RCC_SYSCLK_Div1 ((uint32_t)0x00000000)
#define RCC_HCLK_Div1   ((uint32_t)0x00000000)
#define RCC_HCLK_Div2   ((uint32_t)0x00000400)
#define RCC_PLLMul_9    ((uint32_t)0x001C0000) // 72 MHz
#define RCC_PLLMul_10   ((uint32_t)0x00200000) // 80 MHz
#define RCC_PLLMul_12   ((uint32_t)0x00280000) // 96 MHz
#define RCC_PLLMul_13   ((uint32_t)0x002C0000) // 104MHz
#define RCC_PLLMul_14   ((uint32_t)0x00300000) // 112MHz
#define RCC_PLLMul_16   ((uint32_t)0x00380000) // 128MHz
#define RCC_PLLSource_HSE_Div1  ((uint32_t)0x00010000)
#define RCC_FLAG_PLLRDY ((uint8_t)0x39)
#define FLASH_Latency_2 ((uint32_t)0x00000002)
#define FLASH_PrefetchBuffer_Enable  ((uint32_t)0x00000010)

extern "C" {
  void RCC_DeInit(void);
  void RCC_HSEConfig(uint32_t RCC_HSE);
  void RCC_SYSCLKConfig(uint32_t RCC_SYSCLKSource);
  uint8_t RCC_GetSYSCLKSource(void);
  void RCC_HCLKConfig(uint32_t RCC_SYSCLK);
  void RCC_PCLK1Config(uint32_t RCC_HCLK);
  void RCC_PCLK2Config(uint32_t RCC_HCLK);
  typedef enum {RESET = 0, SET = !RESET} FlagStatus;
  FlagStatus RCC_GetFlagStatus(uint8_t RCC_FLAG);
  typedef enum {DISABLE = 0, ENABLE = !DISABLE} FunctionalState;
  void RCC_PLLCmd(FunctionalState NewState);
  void RCC_PLLConfig(uint32_t RCC_PLLSource, uint32_t RCC_PLLMul);
  typedef enum {ERROR = 0, SUCCESS = !ERROR} ErrorStatus;
  ErrorStatus RCC_WaitForHSEStartUp(void);
  void FLASH_PrefetchBufferCmd(uint32_t FLASH_PrefetchBuffer);
  void FLASH_SetLatency(uint32_t FLASH_Latency);
}

/* Overclock the STM32F103, up to 128 MHz */
static uint16_t overclock_stm32f103() {
  // Reset the RCC config. to the default state,
  // doesn't modify PCLK, LSI, LSE and RTC clocks
  RCC_DeInit();

  // Enable the External High Speed oscillator
  RCC_HSEConfig(RCC_HSE_ON);
  ErrorStatus HSEStartUpStatus = RCC_WaitForHSEStartUp();
  if (HSEStartUpStatus == SUCCESS) {
    // Enable Prefetch Buffer
    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
    // Flash 2 wait state
    FLASH_SetLatency(FLASH_Latency_2);

    // HCLK = SYSCLK
    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    // PCLK1 = HCLK/2
    RCC_PCLK1Config(RCC_HCLK_Div2);
    // PCLK2 = HCLK
    RCC_PCLK2Config(RCC_HCLK_Div1);

    uint32_t mul_pll = RCC_PLLMul_9;
    switch(OC_TARGET_MHZ) {
      case 80:  mul_pll = RCC_PLLMul_10; break;
      case 96:  mul_pll = RCC_PLLMul_12; break;
      case 104: mul_pll = RCC_PLLMul_13; break;
      case 112: mul_pll = RCC_PLLMul_14; break;
      case 128: mul_pll = RCC_PLLMul_16; break;
    }
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, mul_pll);

    // Enable main PLL
    RCC_PLLCmd(ENABLE);
    // Wait till PLL is ready
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {;}
    // Select PLL as system clock source
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    // Wait till PLL is used as system clock source
    while(RCC_GetSYSCLKSource() != 0x08) {;}

    return OC_TARGET_MHZ;
  }
  return OC_BASE_MHZ;
}
#endif // OVERCLOCK

// ------------------------
// MarlinHAL class
// ------------------------

void MarlinHAL::init() {
  #ifdef OVERCLOCK
    VariantCoreClock = overclock_stm32f103();
  #endif
  NVIC_SetPriorityGrouping(0x3);
  #if PIN_EXISTS(LED)
    OUT_WRITE(LED_PIN, LOW);
  #endif
  #if HAS_SD_HOST_DRIVE
    MSC_SD_init();
  #elif ALL(SERIAL_USB, EMERGENCY_PARSER)
    usb_cdcacm_set_hooks(USB_CDCACM_HOOK_RX, my_rx_callback);
  #endif
  #if PIN_EXISTS(USB_CONNECT)
    OUT_WRITE(USB_CONNECT_PIN, !USB_CONNECT_INVERTING);  // USB clear connection
    delay(1000);                                         // Give OS time to notice
    WRITE(USB_CONNECT_PIN, USB_CONNECT_INVERTING);
  #endif
  TERN_(POSTMORTEM_DEBUGGING, install_min_serial());    // Install the minimal serial handler
}

// HAL idle task
void MarlinHAL::idletask() {
  #if HAS_SHARED_MEDIA
    // If Marlin is using the SD card we need to lock it to prevent access from
    // a PC via USB.
    // Other HALs use IS_SD_PRINTING() and IS_SD_FILE_OPEN() to check for access but
    // this will not reliably detect delete operations. To be safe we will lock
    // the disk if Marlin has it mounted. Unfortunately there is currently no way
    // to unmount the disk from the LCD menu.
    // if (IS_SD_PRINTING() || IS_SD_FILE_OPEN())
    /* copy from lpc1768 framework, should be fixed later for process HAS_SD_HOST_DRIVE*/
    // process USB mass storage device class loop
    MarlinMSC.loop();
  #endif
}

void MarlinHAL::reboot() { nvic_sys_reset(); }

// ------------------------
// Free Memory Accessor
// ------------------------

extern "C" {
  extern unsigned int _ebss; // end of bss section
}

/**
 * TODO: Change this to correct it for libmaple
 */

// return free memory between end of heap (or end bss) and whatever is current

/*
#include <wirish/syscalls.c>
//extern caddr_t _sbrk(int incr);
#ifndef CONFIG_HEAP_END
extern char _lm_heap_end;
#define CONFIG_HEAP_END ((caddr_t)&_lm_heap_end)
#endif

extern "C" {
  static int freeMemory() {
    char top = 't';
    return &top - reinterpret_cast<char*>(sbrk(0));
  }
  int freeMemory() {
    int free_memory;
    int heap_end = (int)_sbrk(0);
    free_memory = ((int)&free_memory) - ((int)heap_end);
    return free_memory;
  }
}
*/

// ------------------------
// ADC
// ------------------------

enum ADCIndex : uint8_t {
  OPTITEM(HAS_TEMP_ADC_0, TEMP_0)
  OPTITEM(HAS_TEMP_ADC_1, TEMP_1)
  OPTITEM(HAS_TEMP_ADC_2, TEMP_2)
  OPTITEM(HAS_TEMP_ADC_3, TEMP_3)
  OPTITEM(HAS_TEMP_ADC_4, TEMP_4)
  OPTITEM(HAS_TEMP_ADC_5, TEMP_5)
  OPTITEM(HAS_TEMP_ADC_6, TEMP_6)
  OPTITEM(HAS_TEMP_ADC_7, TEMP_7)
  OPTITEM(HAS_HEATED_BED, TEMP_BED)
  OPTITEM(HAS_TEMP_CHAMBER, TEMP_CHAMBER)
  OPTITEM(HAS_TEMP_ADC_PROBE, TEMP_PROBE)
  OPTITEM(HAS_TEMP_COOLER, TEMP_COOLER)
  OPTITEM(HAS_TEMP_BOARD, TEMP_BOARD)
  OPTITEM(FILAMENT_WIDTH_SENSOR, FILWIDTH)
  OPTITEM(HAS_ADC_BUTTONS, ADC_KEY)
  OPTITEM(HAS_JOY_ADC_X, JOY_X)
  OPTITEM(HAS_JOY_ADC_Y, JOY_Y)
  OPTITEM(HAS_JOY_ADC_Z, JOY_Z)
  OPTITEM(POWER_MONITOR_CURRENT, POWERMON_CURRENT)
  OPTITEM(POWER_MONITOR_VOLTAGE, POWERMON_VOLTS)
  ADC_COUNT
};

static uint16_t adc_results[ADC_COUNT];

// Init the AD in continuous capture mode
void MarlinHAL::adc_init() {
  static const uint8_t adc_pins[] = {
    OPTITEM(HAS_TEMP_ADC_0, TEMP_0_PIN)
    OPTITEM(HAS_TEMP_ADC_1, TEMP_1_PIN)
    OPTITEM(HAS_TEMP_ADC_2, TEMP_2_PIN)
    OPTITEM(HAS_TEMP_ADC_3, TEMP_3_PIN)
    OPTITEM(HAS_TEMP_ADC_4, TEMP_4_PIN)
    OPTITEM(HAS_TEMP_ADC_5, TEMP_5_PIN)
    OPTITEM(HAS_TEMP_ADC_6, TEMP_6_PIN)
    OPTITEM(HAS_TEMP_ADC_7, TEMP_7_PIN)
    OPTITEM(HAS_HEATED_BED, TEMP_BED_PIN)
    OPTITEM(HAS_TEMP_CHAMBER, TEMP_CHAMBER_PIN)
    OPTITEM(HAS_TEMP_ADC_PROBE, TEMP_PROBE_PIN)
    OPTITEM(HAS_TEMP_COOLER, TEMP_COOLER_PIN)
    OPTITEM(HAS_TEMP_BOARD, TEMP_BOARD_PIN)
    OPTITEM(FILAMENT_WIDTH_SENSOR, FILWIDTH_PIN)
    OPTITEM(HAS_ADC_BUTTONS, ADC_KEYPAD_PIN)
    OPTITEM(HAS_JOY_ADC_X, JOY_X_PIN)
    OPTITEM(HAS_JOY_ADC_Y, JOY_Y_PIN)
    OPTITEM(HAS_JOY_ADC_Z, JOY_Z_PIN)
    OPTITEM(POWER_MONITOR_CURRENT, POWER_MONITOR_CURRENT_PIN)
    OPTITEM(POWER_MONITOR_VOLTAGE, POWER_MONITOR_VOLTAGE_PIN)
  };
  static STM32ADC adc(ADC1);
  // configure the ADC
  adc.calibrate();
  adc.setSampleRate((F_CPU > 72000000) ? ADC_SMPR_71_5 : ADC_SMPR_41_5); // 71.5 or 41.5 ADC cycles
  adc.setPins((uint8_t *)adc_pins, ADC_COUNT);
  adc.setDMA(adc_results, uint16_t(ADC_COUNT), uint32_t(DMA_MINC_MODE | DMA_CIRC_MODE), nullptr);
  adc.setScanMode();
  adc.setContinuous();
  adc.startConversion();
}

void MarlinHAL::adc_start(const pin_t pin) {
  #define __TCASE(N,I) case N: pin_index = I; break;
  #define _TCASE(C,N,I) TERN_(C, __TCASE(N, I))
  ADCIndex pin_index;
  switch (pin) {
    default: return;
    _TCASE(HAS_TEMP_ADC_0,        TEMP_0_PIN,                TEMP_0)
    _TCASE(HAS_TEMP_ADC_1,        TEMP_1_PIN,                TEMP_1)
    _TCASE(HAS_TEMP_ADC_2,        TEMP_2_PIN,                TEMP_2)
    _TCASE(HAS_TEMP_ADC_3,        TEMP_3_PIN,                TEMP_3)
    _TCASE(HAS_TEMP_ADC_4,        TEMP_4_PIN,                TEMP_4)
    _TCASE(HAS_TEMP_ADC_5,        TEMP_5_PIN,                TEMP_5)
    _TCASE(HAS_TEMP_ADC_6,        TEMP_6_PIN,                TEMP_6)
    _TCASE(HAS_TEMP_ADC_7,        TEMP_7_PIN,                TEMP_7)
    _TCASE(HAS_HEATED_BED,        TEMP_BED_PIN,              TEMP_BED)
    _TCASE(HAS_TEMP_CHAMBER,      TEMP_CHAMBER_PIN,          TEMP_CHAMBER)
    _TCASE(HAS_TEMP_ADC_PROBE,    TEMP_PROBE_PIN,            TEMP_PROBE)
    _TCASE(HAS_TEMP_COOLER,       TEMP_COOLER_PIN,           TEMP_COOLER)
    _TCASE(HAS_TEMP_BOARD,        TEMP_BOARD_PIN,            TEMP_BOARD)
    _TCASE(HAS_JOY_ADC_X,         JOY_X_PIN,                 JOY_X)
    _TCASE(HAS_JOY_ADC_Y,         JOY_Y_PIN,                 JOY_Y)
    _TCASE(HAS_JOY_ADC_Z,         JOY_Z_PIN,                 JOY_Z)
    _TCASE(FILAMENT_WIDTH_SENSOR, FILWIDTH_PIN,              FILWIDTH)
    _TCASE(HAS_ADC_BUTTONS,       ADC_KEYPAD_PIN,            ADC_KEY)
    _TCASE(POWER_MONITOR_CURRENT, POWER_MONITOR_CURRENT_PIN, POWERMON_CURRENT)
    _TCASE(POWER_MONITOR_VOLTAGE, POWER_MONITOR_VOLTAGE_PIN, POWERMON_VOLTS)
  }
  adc_result = (adc_results[(int)pin_index] & 0xFFF) >> (12 - HAL_ADC_RESOLUTION); // shift out unused bits
}

#endif // __STM32F1__
