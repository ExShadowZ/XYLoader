#ifndef QSPI_QSPI_H_
#define QSPI_QSPI_H_

#include "main.h"

// =======================================================
// ==================== БАЗОВЫЙ АДРЕС ====================
// =======================================================

#define QSPI_BASE_ADDR              0x90000000UL

// =====================================================
// ==================== РАЗДЕЛЫ A/B ====================
// =====================================================

#define QSPI_PARTITION_SIZE         (QSPI_FLASH_SIZE / 2)
#define QSPI_PARTITION_A_OFFSET     0
#define QSPI_PARTITION_B_OFFSET     QSPI_PARTITION_SIZE

#define QSPI_PARTITION_A_ADDR       (QSPI_BASE_ADDR + QSPI_PARTITION_A_OFFSET)
#define QSPI_PARTITION_B_ADDR       (QSPI_BASE_ADDR + QSPI_PARTITION_B_OFFSET)

// ======================================================
// ==================== КОМАНДЫ QSPI ====================
// ======================================================

#define QSPI_CMD_WRITE_ENABLE       0x06
#define QSPI_CMD_WRITE_DISABLE      0x04
#define QSPI_CMD_READ_STATUS_REG    0x05
#define QSPI_CMD_READ_JEDEC_ID      0x9F
#define QSPI_CMD_FAST_READ          0x0B
#define QSPI_CMD_PAGE_PROGRAM       0x02
#define QSPI_CMD_SECTOR_ERASE       0x20
#define QSPI_CMD_BLOCK_ERASE_32     0x52
#define QSPI_CMD_BLOCK_ERASE_64     0xD8
#define QSPI_CMD_CHIP_ERASE         0xC7

// =========================================================
// ==================== ВЫБОР ТИПА ЧИПА ====================
// =========================================================

// Раскомментируй нужный чип:

// ---- Winbond ----
//#define QSPI_CHIP_W25Q16      // 2MB
//#define QSPI_CHIP_W25Q32      // 4MB
#define QSPI_CHIP_W25Q64      // 8MB
//#define QSPI_CHIP_W25Q128     // 16MB
//#define QSPI_CHIP_W25Q256     // 32MB (4-байтовый адрес)
//#define QSPI_CHIP_W25Q512     // 64MB (4-байтовый адрес)

// ---- Macronix ----
//#define QSPI_CHIP_MX25L64     // 8MB
//#define QSPI_CHIP_MX25L128    // 16MB
//#define QSPI_CHIP_MX25L256    // 32MB (4-байтовый адрес)

// ---- Micron ----
//#define QSPI_CHIP_MT25QL64    // 8MB
//#define QSPI_CHIP_MT25QL128   // 16MB
//#define QSPI_CHIP_MT25QL256   // 32MB (4-байтовый адрес)

// ---- GigaDevice ----
//#define QSPI_CHIP_GD25Q64     // 8MB
//#define QSPI_CHIP_GD25Q128    // 16MB

// ====================================================
// ==================== ПАРАМЕТРЫ ЧИПА ====================
// ====================================================

#if defined(QSPI_CHIP_W25Q16)
    #define QSPI_FLASH_SIZE     (2 * 1024 * 1024)
    #define QSPI_SECTOR_SIZE    4096
    #define QSPI_PAGE_SIZE      256
    #define QSPI_TOTAL_SECTORS  (QSPI_FLASH_SIZE / QSPI_SECTOR_SIZE)
    #define QSPI_4BYTE_ADDR     0
    #define QSPI_MANUFACTURER   0xEF
    #define QSPI_MEMORY_TYPE    0x40
    #define QSPI_CAPACITY       0x12

#elif defined(QSPI_CHIP_W25Q32)
    #define QSPI_FLASH_SIZE     (4 * 1024 * 1024)
    #define QSPI_SECTOR_SIZE    4096
    #define QSPI_PAGE_SIZE      256
    #define QSPI_TOTAL_SECTORS  (QSPI_FLASH_SIZE / QSPI_SECTOR_SIZE)
    #define QSPI_4BYTE_ADDR     0
    #define QSPI_MANUFACTURER   0xEF
    #define QSPI_MEMORY_TYPE    0x40
    #define QSPI_CAPACITY       0x13

#elif defined(QSPI_CHIP_W25Q64)
    #define QSPI_FLASH_SIZE     (8 * 1024 * 1024)
    #define QSPI_SECTOR_SIZE    4096
    #define QSPI_PAGE_SIZE      256
    #define QSPI_TOTAL_SECTORS  (QSPI_FLASH_SIZE / QSPI_SECTOR_SIZE)
    #define QSPI_4BYTE_ADDR     0
    #define QSPI_MANUFACTURER   0xEF
    #define QSPI_MEMORY_TYPE    0x40
    #define QSPI_CAPACITY       0x14

#elif defined(QSPI_CHIP_W25Q128)
    #define QSPI_FLASH_SIZE     (16 * 1024 * 1024)
    #define QSPI_SECTOR_SIZE    4096
    #define QSPI_PAGE_SIZE      256
    #define QSPI_TOTAL_SECTORS  (QSPI_FLASH_SIZE / QSPI_SECTOR_SIZE)
    #define QSPI_4BYTE_ADDR     0
    #define QSPI_MANUFACTURER   0xEF
    #define QSPI_MEMORY_TYPE    0x40
    #define QSPI_CAPACITY       0x15

#elif defined(QSPI_CHIP_W25Q256)
    #define QSPI_FLASH_SIZE     (32 * 1024 * 1024)
    #define QSPI_SECTOR_SIZE    4096
    #define QSPI_PAGE_SIZE      256
    #define QSPI_TOTAL_SECTORS  (QSPI_FLASH_SIZE / QSPI_SECTOR_SIZE)
    #define QSPI_4BYTE_ADDR     1
    #define QSPI_MANUFACTURER   0xEF
    #define QSPI_MEMORY_TYPE    0x40
    #define QSPI_CAPACITY       0x16

#else
    // По умолчанию: W25Q64 (8MB)
    #define QSPI_FLASH_SIZE     (8 * 1024 * 1024)
    #define QSPI_SECTOR_SIZE    4096
    #define QSPI_PAGE_SIZE      256
    #define QSPI_TOTAL_SECTORS  (QSPI_FLASH_SIZE / QSPI_SECTOR_SIZE)
    #define QSPI_4BYTE_ADDR     0
    #define QSPI_MANUFACTURER   0xEF
    #define QSPI_MEMORY_TYPE    0x40
    #define QSPI_CAPACITY       0x14
#endif

// ==============================================================
// ==================== СТРУКТУРА ПАРАМЕТРОВ ====================
// ==============================================================

typedef struct {
    uint32_t size;
    uint32_t page_size;
    uint32_t sector_size;
    uint32_t total_sectors;
    uint8_t  manufacturer_id;
    uint8_t  memory_type;
    uint8_t  capacity;
    uint8_t  supports_4byte_addr;
} QSPI_FlashParams_t;

// ====================================================
// ====================== ФУНКЦИИ =====================
// ====================================================

uint8_t QSPI_Init(void);
void QSPI_GetParams(QSPI_FlashParams_t *params);
uint8_t QSPI_EraseSector(uint32_t address);
uint8_t QSPI_Write(uint32_t address, uint8_t *buffer, uint32_t size);
uint8_t QSPI_Read(uint32_t address, uint8_t *buffer, uint32_t size);
void QSPI_EnableMemoryMapped(void);

#endif /* QSPI_QSPI_H_ */
