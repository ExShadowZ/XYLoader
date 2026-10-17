/*
 * update_firmware.h
 *
 *  Created on: Jul 9, 2026
 *      Author: ExShadow
 */

#ifndef QSPI_UPDATE_FIRMWARE_H_
#define QSPI_UPDATE_FIRMWARE_H_

#include <stdint.h>

// ====================================================
// ===================== ПАРАМЕТРЫ ====================
// ====================================================

#define UPDATE_BUFFER_SIZE         4096
#define VERSION_BUFFER_SIZE        16
#define BOOT_ATTEMPTS_MAX          3

#define UPDATE_DIR                 "UPDATE"
#define UPDATE_BIN_PATH            "UPDATE/UPD.BIN"

#define CURRENT_VERSION            "0.0.0"

#define PARTITION_A                0
#define PARTITION_B                1

// ====================================================
// ================== АДРЕСА В EEPROM =================
// ====================================================

// Раздел A
#define EEPROM_MAGIC_A_ADDR         0x00
#define EEPROM_MAJOR_A_ADDR         0x10
#define EEPROM_MINOR_A_ADDR         0x20
#define EEPROM_PATCH_A_ADDR         0x30
#define EEPROM_CRC_A_ADDR           0x40

// Раздел B
#define EEPROM_MAGIC_B_ADDR         0x50
#define EEPROM_MAJOR_B_ADDR         0x60
#define EEPROM_MINOR_B_ADDR         0x70
#define EEPROM_PATCH_B_ADDR         0x80
#define EEPROM_CRC_B_ADDR           0x90

// Служебные
#define EEPROM_UPDATE_COUNT_ADDR    0xA0
#define EEPROM_BOOT_ATTEMPTS_ADDR   0xB0
#define EEPROM_RESTORE_FLAG_ADDR    0xC0

// ====================================================
// ================= СТРУКТУРА ФУТЕРА =================
// ====================================================

typedef struct {
    uint32_t magic;
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t reserved;
    uint32_t crc;
} FirmwareFooter_t;

#define FIRMWARE_MAGIC 0x43EAB508

// ====================================================
// ====================== ФУНКЦИИ =====================
// ====================================================

uint8_t Update_CheckAndStart(void);
void JumpToApplication(void);
void Version_ShowInfo(void);

#endif
