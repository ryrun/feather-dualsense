#pragma once

// Minimal board definition for Adafruit Feather RP2040 USB Host (feather_host).
// This definition is intentionally lean and only provides values needed by this firmware.

#define PICO_BOARD "feather_host"
#define PICO_PLATFORM "rp2040"

#ifndef PICO_XOSC_STARTUP_DELAY_MULTIPLIER
#define PICO_XOSC_STARTUP_DELAY_MULTIPLIER 64
#endif

#define PICO_DEFAULT_LED_PIN 13
#define PICO_DEFAULT_WS2812_PIN 16
#define PICO_DEFAULT_WS2812_POWER_PIN 17
#define PICO_DEFAULT_WS2812_IS_RGBW 0

#define PICO_DEFAULT_UART 0
#define PICO_DEFAULT_UART_TX_PIN 0
#define PICO_DEFAULT_UART_RX_PIN 1

#define PICO_DEFAULT_I2C 1
#define PICO_DEFAULT_I2C_SDA_PIN 2
#define PICO_DEFAULT_I2C_SCL_PIN 3

#define PICO_DEFAULT_SPI 0
#define PICO_DEFAULT_SPI_SCK_PIN 18
#define PICO_DEFAULT_SPI_TX_PIN 19
#define PICO_DEFAULT_SPI_RX_PIN 20
#define PICO_DEFAULT_SPI_CSN_PIN 21

#define PICO_FLASH_SIZE_BYTES (8 * 1024 * 1024)
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#define PICO_RP2040_USB_DEVICE_ENUMERATION_FIX 1
