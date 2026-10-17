/*
 * eprom.h
 * Universal EEPROM driver for AT24Cxx family
 * Supports: AT24C01, AT24C02, AT24C04, AT24C08, AT24C16, AT24C32, AT24C64, AT24C128, AT24C256, AT24C512
 */

#ifndef __EPROM_H
#define __EPROM_H

#include "main.h"
#include <stdbool.h>
#include <string.h>

// =========================================================
// ======================= НАСТРОЙКИ =======================
// =========================================================

/* Выбор шины I2C */
#define EEPROM_I2C               hi2c1
extern I2C_HandleTypeDef EEPROM_I2C;

/* Выбор шины CRC */
#define EEPROM_CRC               hcrc
extern CRC_HandleTypeDef EEPROM_CRC;

/* Адрес EEPROM на шине I2C */
#define EEPROM_I2C_ADDR          (0x50 << 1)

/* ================== ВЫБОР ТИПА ПАМЯТИ ================== */
//#define EEPROM_AT24C01
//#define EEPROM_AT24C02
#define EEPROM_AT24C04
//#define EEPROM_AT24C08
//#define EEPROM_AT24C16
//#define EEPROM_AT24C32
//#define EEPROM_AT24C64
//#define EEPROM_AT24C128
//#define EEPROM_AT24C256
//#define EEPROM_AT24C512

/* ============ УПРАВЛЕНИЕ ЗАЩИТОЙ ЗАПИСИ (WP) =========== */
//#define EEPROM_USE_WP_PIN

#ifdef EEPROM_USE_WP_PIN
    /* Нога для блокировки записи (WP) */
    #define EEPROM_WP_PORT           GPIOB
    #define EEPROM_WP_PIN            GPIO_PIN_5

    /* Макросы для управления WP пином */
    #define EEPROM_WP_ENABLE()       do { EEPROM_WP_PORT->BSRR = EEPROM_WP_PIN; } while(0)
    #define EEPROM_WP_DISABLE()      do { EEPROM_WP_PORT->BSRR = (uint32_t)EEPROM_WP_PIN << 16u; } while(0)
#else
    /* Если WP не используется - пустые макросы */
    #define EEPROM_WP_ENABLE()
    #define EEPROM_WP_DISABLE()
#endif

// ==========================================================
// ==================== ПАРАМЕТРЫ ПАМЯТИ ====================
// ==========================================================

#if defined(EEPROM_AT24C01)
    #define EEPROM_PAGE_SIZE        8
    #define EEPROM_SIZE             128
    #define EEPROM_ADDR_SIZE        1
#elif defined(EEPROM_AT24C02)
    #define EEPROM_PAGE_SIZE        8
    #define EEPROM_SIZE             256
    #define EEPROM_ADDR_SIZE        1
#elif defined(EEPROM_AT24C04)
    #define EEPROM_PAGE_SIZE        16
    #define EEPROM_SIZE             512
    #define EEPROM_ADDR_SIZE        1
#elif defined(EEPROM_AT24C08)
    #define EEPROM_PAGE_SIZE        16
    #define EEPROM_SIZE             1024
    #define EEPROM_ADDR_SIZE        1
#elif defined(EEPROM_AT24C16)
    #define EEPROM_PAGE_SIZE        16
    #define EEPROM_SIZE             2048
    #define EEPROM_ADDR_SIZE        1
#elif defined(EEPROM_AT24C32)
    #define EEPROM_PAGE_SIZE        32
    #define EEPROM_SIZE             4096
    #define EEPROM_ADDR_SIZE        2
#elif defined(EEPROM_AT24C64)
    #define EEPROM_PAGE_SIZE        32
    #define EEPROM_SIZE             8192
    #define EEPROM_ADDR_SIZE        2
#elif defined(EEPROM_AT24C128)
    #define EEPROM_PAGE_SIZE        64
    #define EEPROM_SIZE             16384
    #define EEPROM_ADDR_SIZE        2
#elif defined(EEPROM_AT24C256)
    #define EEPROM_PAGE_SIZE        64
    #define EEPROM_SIZE             32768
    #define EEPROM_ADDR_SIZE        2
#elif defined(EEPROM_AT24C512)
    #define EEPROM_PAGE_SIZE        128
    #define EEPROM_SIZE             65536
    #define EEPROM_ADDR_SIZE        2
#else
    // По умолчанию: AT24C32
    #define EEPROM_PAGE_SIZE        32
    #define EEPROM_SIZE             4096
    #define EEPROM_ADDR_SIZE        2
#endif

// =========================================================
// ====================== КОДЫ ОШИБОК ======================
// =========================================================

#define EEPROM_OK                  0
#define EEPROM_ERROR               1
#define EEPROM_ERROR_TIMEOUT       2
#define EEPROM_ERROR_ADDRESS       3
#define EEPROM_ERROR_SIZE          4
#define EEPROM_ERROR_CRC           5

// ==========================================================
// ==================== ОСНОВНЫЕ ФУНКЦИИ ====================
// ==========================================================

uint8_t EEPROM_Init(void);
uint8_t EEPROM_WriteByte(uint16_t address, uint8_t data);
uint8_t EEPROM_ReadByte(uint16_t address, uint8_t *data);
uint8_t EEPROM_WriteBuffer(uint16_t address, uint8_t *data, uint16_t size);
uint8_t EEPROM_ReadBuffer(uint16_t address, uint8_t *buffer, uint16_t size);
uint8_t EEPROM_Clear(void);
uint8_t EEPROM_EraseChip(void);

/* Функции с CRC */
uint8_t EEPROM_WriteData(uint16_t address, uint8_t *data, uint8_t len);
uint8_t EEPROM_ReadData(uint16_t address, uint8_t *data, uint8_t len);

#endif /* __EPROM_H */
