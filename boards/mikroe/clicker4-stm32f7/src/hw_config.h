/*
 * hw_config.h
 */

#ifndef HW_CONFIG_H_
#define HW_CONFIG_H_

/* Boot device selection list */
#define USB0_DEV       0x01
#define SERIAL0_DEV    0x02
#define SERIAL1_DEV    0x04

/*
 * Reserve first 128 KiB (sectors 0-3 and sector 4) for bootloader.
 * Application starts on sector 4 boundary for erase/program safety.
 */
#define APP_LOAD_ADDRESS               0x08020000
#define BOOTLOADER_DELAY               500

#define INTERFACE_USB                  1
#define INTERFACE_USB_CONFIG           "/dev/ttyACM0"
#define BOARD_VBUS                     MK_GPIO_INPUT(GPIO_OTGFS_VBUS)

#define BOOT_DELAY_ADDRESS             0x000001a0
#define BOARD_TYPE                     7454
#define BOARD_FLASH_SECTORS            (7)
#define BOARD_FLASH_SIZE               (1024 * 1024)
#define BOARD_FIRST_FLASH_SECTOR_TO_ERASE (4)
#define APP_RESERVATION_SIZE           (16 * 1024)

#define BOARD_PIN_LED_ACTIVITY         GPIO_LED_STATUS
#define BOARD_LED_ON                   1
#define BOARD_LED_OFF                  0

#define USBMFGSTRING                   "MikroElektronika"
#define USBDEVICESTRING                "PX4 BL Clicker4 STM32F7"
#define USBPRODUCTID                   0x0050

#ifndef STM32_CPUCLK_FREQUENCY
# define STM32_CPUCLK_FREQUENCY STM32_SYSCLK_FREQUENCY
#endif

#if !defined(ARCH_SN_MAX_LENGTH)
# define ARCH_SN_MAX_LENGTH 12
#endif

#if !defined(APP_RESERVATION_SIZE)
# define APP_RESERVATION_SIZE 0
#endif

#if !defined(BOARD_FIRST_FLASH_SECTOR_TO_ERASE)
# define BOARD_FIRST_FLASH_SECTOR_TO_ERASE 1
#endif

#if !defined(USB_DATA_ALIGN)
# define USB_DATA_ALIGN
#endif

#ifndef BOOT_DEVICES_SELECTION
# define BOOT_DEVICES_SELECTION USB0_DEV
#endif

#ifndef BOOT_DEVICES_FILTER_ONUSB
# define BOOT_DEVICES_FILTER_ONUSB USB0_DEV
#endif

#endif /* HW_CONFIG_H_ */
