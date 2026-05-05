/****************************************************************************
 *
 *   Copyright (c) 2019 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file board_config.h
 *
 * MikroElektronika Clicker 4 STM32F7 internal definitions
 */

#pragma once

/****************************************************************************************************
 * Included Files
 ****************************************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

#include <stm32_dma.h>
#include <stm32_gpio.h>

/****************************************************************************************************
 * Definitions
 ****************************************************************************************************/

/* GPIOs ***********************************************************************************/

/*
 * Schematic labels (clicker-4-for-stm32f7-schematic-v100.pdf):
 *   LEDs:    L1=PC8, L2=PA8, L3=PB13, L4=PE2, L5=PB15, L6=PA10
 *   Buttons: T1=PE10, T2=PE11, T3=PE12, T4=PE13, T5=PB12, T6=PD10
 */
#define GPIO_LED_L1              /* PC8  */ (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_50MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTC|GPIO_PIN8)
#define GPIO_LED_L2              /* PA8  */ (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_50MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTA|GPIO_PIN8)
#define GPIO_LED_L3              /* PB13 */ (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_50MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTB|GPIO_PIN13)
#define GPIO_LED_L4              /* PE2  */ (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_50MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTE|GPIO_PIN2)
#define GPIO_LED_L5              /* PB15 */ (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_50MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTB|GPIO_PIN15)
#define GPIO_LED_L6              /* PA10 */ (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_50MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTA|GPIO_PIN10)

#define GPIO_LED_STATUS GPIO_LED_L2

#define GPIO_BTN_T1              /* PE10 */ (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTE|GPIO_PIN10)
#define GPIO_BTN_T2              /* PE11 */ (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTE|GPIO_PIN11)
#define GPIO_BTN_T3              /* PE12 */ (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTE|GPIO_PIN12)
#define GPIO_BTN_T4              /* PE13 */ (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTE|GPIO_PIN13)
#define GPIO_BTN_T5              /* PB12 */ (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTB|GPIO_PIN12)
#define GPIO_BTN_T6              /* PD10 */ (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTD|GPIO_PIN10)

#define BOARD_HAS_CONTROL_STATUS_LEDS      1
#define BOARD_ARMED_STATE_LED  LED_BLUE

/* Single-wire WS281x/WS2815 data output on PC9. */
#define GPIO_SRGBLED_DATA          /* PC9  */ (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_50MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTC|GPIO_PIN9)
#define BOARD_HAS_N_S_RGB_LED      1
#define BOARD_MAX_LEDS             BOARD_HAS_N_S_RGB_LED
#define BOARD_SRGBLED_PORT         STM32_GPIOC_ODR
#define BOARD_SRGBLED_BIT          9

/*
 * USE_S_RGB_LED_DMA is passed in from the *.px4board file.
 * This mirrors the upstream CAN-GPS board pattern: bootloader can stay on
 * software timing, while the application uses timer+DMA for deterministic
 * output timing.
 */
#if defined(USE_S_RGB_LED_DMA)
#  define S_RGB_LED_TIMER            8
#  define S_RGB_LED_CHANNEL          4
/* WS2815F datasheet: 24-bit color order is GRB, MSB first. */
/* Use up-count PWM timing for non-inverted WS281x symbol generation. */
#  define S_RGB_LED_TIM_USE_DOWNCOUNT 0

/* Hard-select the TIM8_CH4 macro that resolves to PC9 (MB4PWM). */
#  define _SLED_PC9_MASK             (GPIO_PORT_MASK | GPIO_PIN_MASK)
#  define _SLED_PC9_VALUE            (GPIO_PORTC | GPIO_PIN9)

#  if defined(GPIO_TIM8_CH4OUT) && (((GPIO_TIM8_CH4OUT) & _SLED_PC9_MASK) == _SLED_PC9_VALUE)
#    define S_RGB_LED_TIM_GPIO       GPIO_TIM8_CH4OUT
#  elif defined(GPIO_TIM8_CH4OUT_1) && (((GPIO_TIM8_CH4OUT_1) & _SLED_PC9_MASK) == _SLED_PC9_VALUE)
#    define S_RGB_LED_TIM_GPIO       GPIO_TIM8_CH4OUT_1
#  elif defined(GPIO_TIM8_CH4OUT_2) && (((GPIO_TIM8_CH4OUT_2) & _SLED_PC9_MASK) == _SLED_PC9_VALUE)
#    define S_RGB_LED_TIM_GPIO       GPIO_TIM8_CH4OUT_2
#  elif defined(GPIO_TIM8_CH4) && (((GPIO_TIM8_CH4) & _SLED_PC9_MASK) == _SLED_PC9_VALUE)
#    define S_RGB_LED_TIM_GPIO       GPIO_TIM8_CH4
#  elif defined(GPIO_TIM8_CH4_1) && (((GPIO_TIM8_CH4_1) & _SLED_PC9_MASK) == _SLED_PC9_VALUE)
#    define S_RGB_LED_TIM_GPIO       GPIO_TIM8_CH4_1
#  elif defined(GPIO_TIM8_CH4_2) && (((GPIO_TIM8_CH4_2) & _SLED_PC9_MASK) == _SLED_PC9_VALUE)
#    define S_RGB_LED_TIM_GPIO       GPIO_TIM8_CH4_2
#  else
#    error No TIM8_CH4 pin macro maps to PC9; cannot route WS281x timer output to MB4PWM
#  endif

#  undef _SLED_PC9_MASK
#  undef _SLED_PC9_VALUE

#  if defined(DMAMAP_TIM8_CH4)
#    define S_RGB_LED_DMA          DMAMAP_TIM8_CH4
#    define S_RGB_LED_DMA_NAME     "DMAMAP_TIM8_CH4"
#  elif defined(DMAMAP_TIM8_CH4_2)
#    define S_RGB_LED_DMA          DMAMAP_TIM8_CH4_2
#    define S_RGB_LED_DMA_NAME     "DMAMAP_TIM8_CH4_2"
#  elif defined(DMAMAP_TIM8_CH4_1)
#    define S_RGB_LED_DMA          DMAMAP_TIM8_CH4_1
#    define S_RGB_LED_DMA_NAME     "DMAMAP_TIM8_CH4_1"
#  elif defined(DMAMAP_DMA12_TIM8CH4_0)
#    define S_RGB_LED_DMA          DMAMAP_DMA12_TIM8CH4_0
#    define S_RGB_LED_DMA_NAME     "DMAMAP_DMA12_TIM8CH4_0"
#  elif defined(DMAMAP_DMA12_TIM8CH4_1)
#    define S_RGB_LED_DMA          DMAMAP_DMA12_TIM8CH4_1
#    define S_RGB_LED_DMA_NAME     "DMAMAP_DMA12_TIM8CH4_1"
#  else
#    error No TIM8 CH4 DMA map macro available in stm32_dma.h for this toolchain
#  endif
#endif

#define  FLASH_BASED_PARAMS

/* Clicker 4 does not expose a dedicated analog battery voltage/current interface. */
#define BOARD_NUMBER_BRICKS 0


/* PWM */
#define DIRECT_PWM_OUTPUT_CHANNELS  5

/* USB OTG FS
 *
 * PA9 OTG_FS_VBUS VBUS sensing
 */
#define GPIO_OTGFS_VBUS         /* PA9 */ (GPIO_INPUT|GPIO_PULLDOWN|GPIO_SPEED_100MHz|GPIO_PORTA|GPIO_PIN9)

/* High-resolution timer */
#define HRT_TIMER               2  /* use timer 2 for the HRT */
#define HRT_TIMER_CHANNEL       1  /* use capture/compare channel 1 */

/* RC Serial port */

#define RC_SERIAL_PORT                     "/dev/ttyS2"


/* This board provides a DMA pool and APIs */
#define BOARD_DMA_ALLOC_POOL_SIZE 5120

/* This board provides the board_on_reset interface */
#define BOARD_HAS_ON_RESET 1

#define BOARD_ADC_USB_CONNECTED	       (px4_arch_gpioread(GPIO_OTGFS_VBUS))
#define BOARD_ADC_USB_VALID            BOARD_ADC_USB_CONNECTED

#define PX4_GPIO_INIT_LIST { \
		GPIO_LED_L1,                     \
		GPIO_LED_L2,                     \
		GPIO_LED_L3,                     \
		GPIO_LED_L4,                     \
		GPIO_LED_L5,                     \
		GPIO_LED_L6,                     \
		GPIO_SRGBLED_DATA,               \
		GPIO_BTN_T1,                     \
		GPIO_BTN_T2,                     \
		GPIO_BTN_T3,                     \
		GPIO_BTN_T4,                     \
		GPIO_BTN_T5,                     \
		GPIO_BTN_T6,                     \
		GPIO_OTGFS_VBUS,                \
	}

#define BOARD_ENABLE_CONSOLE_BUFFER

#define BOARD_NUM_IO_TIMERS 4

__BEGIN_DECLS

/****************************************************************************************************
 * Public Types
 ****************************************************************************************************/

/****************************************************************************************************
 * Public data
 ****************************************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

/****************************************************************************************************
 * Name: stm32_spiinitialize
 *
 * Description:
 *   Called to configure SPI chip select GPIO pins for the board.
 *
 ****************************************************************************************************/

extern void stm32_spiinitialize(void);

extern void stm32_usbinitialize(void);

extern void board_peripheral_reset(int ms);

#include <px4_platform_common/board_common.h>

#endif /* __ASSEMBLY__ */

__END_DECLS
