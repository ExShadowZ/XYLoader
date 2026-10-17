/*
 * update_firmware.c
 * Bootloader update functions with A/B partitioning
 * Всегда грузимся с раздела A. Раздел B - резервная копия
 */

#include "update_firmware.h"
#include "sd.h"
#include "qspi.h"
#include "../EPROM/eprom.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../LOG/LOG.h"

// #pragma GCC diagnostic ignored "-Wunused-function" //ДЛЯ ОТСЕЧКИ КОМПИЛЯТОРА ПО НЕИСПОЛЬЗУЕМЫМ ФУНКЦИЯМ

// ====================================================
// ==================== ПЕРЕМЕННЫЕ ====================
// ====================================================

static uint8_t read_buffer[UPDATE_BUFFER_SIZE];
static QSPI_FlashParams_t flash_params;
static char progress_buf[32];

extern CRC_HandleTypeDef hcrc;

extern uint32_t ErrorCode;

// ============================================================
// ==================== РАБОТА С РАЗДЕЛАМИ ====================
// ============================================================

/* Получить адрес раздела в QSPI */
static uint32_t GetPartitionAddress(uint8_t partition)
{
    return (partition == PARTITION_A) ? QSPI_PARTITION_A_ADDR : QSPI_PARTITION_B_ADDR;
}

/* Получить имя раздела */
static const char* GetPartitionName(uint8_t partition)
{
    return (partition == PARTITION_A) ? "A" : "B";
}

// =========================================================
// ==================== РАБОТА С EEPROM ====================
// =========================================================

/* Очистить страницу EEPROM (записать 0xFF) */
static uint8_t EEPROM_ClearPage(uint16_t addr)
{
    uint8_t data[EEPROM_PAGE_SIZE];
    memset(data, 0xFF, EEPROM_PAGE_SIZE);
    return EEPROM_WriteBuffer(addr, data, EEPROM_PAGE_SIZE);
}

/* Записать 32-битное значение */
static uint8_t EEPROM_WriteU32(uint16_t addr, uint32_t value)
{
    uint8_t result = EEPROM_ClearPage(addr);
    if (result != EEPROM_OK) return result;
    return EEPROM_WriteBuffer(addr, (uint8_t*)&value, 4);
}

/* Прочитать 32-битное значение */
static uint32_t EEPROM_ReadU32(uint16_t addr)
{
    uint32_t value = 0;
    EEPROM_ReadBuffer(addr, (uint8_t*)&value, 4);
    return value;
}

/* Записать 8-битное значение */
static uint8_t EEPROM_WriteU8(uint16_t addr, uint8_t value)
{
    uint8_t result = EEPROM_ClearPage(addr);
    if (result != EEPROM_OK) return result;
    return EEPROM_WriteBuffer(addr, &value, 1);
}

/* Прочитать 8-битное значение */
static uint8_t EEPROM_ReadU8(uint16_t addr)
{
    uint8_t value = 0;
    EEPROM_ReadBuffer(addr, &value, 1);
    return value;
}

/* Записать информацию о разделе в EEPROM */
static uint8_t EEPROM_WritePartitionInfo(uint8_t partition, const FirmwareFooter_t* footer)
{
    uint8_t result;

    if (partition == PARTITION_A) {
        result = EEPROM_WriteU32(EEPROM_MAGIC_A_ADDR, footer->magic);
        if (result != EEPROM_OK) return result;
        result = EEPROM_WriteU8(EEPROM_MAJOR_A_ADDR, footer->major);
        if (result != EEPROM_OK) return result;
        result = EEPROM_WriteU8(EEPROM_MINOR_A_ADDR, footer->minor);
        if (result != EEPROM_OK) return result;
        result = EEPROM_WriteU8(EEPROM_PATCH_A_ADDR, footer->patch);
        if (result != EEPROM_OK) return result;
        result = EEPROM_WriteU32(EEPROM_CRC_A_ADDR, footer->crc);
    } else {
        result = EEPROM_WriteU32(EEPROM_MAGIC_B_ADDR, footer->magic);
        if (result != EEPROM_OK) return result;
        result = EEPROM_WriteU8(EEPROM_MAJOR_B_ADDR, footer->major);
        if (result != EEPROM_OK) return result;
        result = EEPROM_WriteU8(EEPROM_MINOR_B_ADDR, footer->minor);
        if (result != EEPROM_OK) return result;
        result = EEPROM_WriteU8(EEPROM_PATCH_B_ADDR, footer->patch);
        if (result != EEPROM_OK) return result;
        result = EEPROM_WriteU32(EEPROM_CRC_B_ADDR, footer->crc);
    }

    return result;
}

/* Прочитать информацию о разделе из EEPROM */
static uint8_t EEPROM_ReadPartitionInfo(uint8_t partition, FirmwareFooter_t* footer)
{
    if (!footer) return 0;

    if (partition == PARTITION_A) {
        footer->magic = EEPROM_ReadU32(EEPROM_MAGIC_A_ADDR);
        footer->major = EEPROM_ReadU8(EEPROM_MAJOR_A_ADDR);
        footer->minor = EEPROM_ReadU8(EEPROM_MINOR_A_ADDR);
        footer->patch = EEPROM_ReadU8(EEPROM_PATCH_A_ADDR);
        footer->crc = EEPROM_ReadU32(EEPROM_CRC_A_ADDR);
    } else {
        footer->magic = EEPROM_ReadU32(EEPROM_MAGIC_B_ADDR);
        footer->major = EEPROM_ReadU8(EEPROM_MAJOR_B_ADDR);
        footer->minor = EEPROM_ReadU8(EEPROM_MINOR_B_ADDR);
        footer->patch = EEPROM_ReadU8(EEPROM_PATCH_B_ADDR);
        footer->crc = EEPROM_ReadU32(EEPROM_CRC_B_ADDR);
    }

    return 1;
}

/* Проверить, инициализирована ли EEPROM */
static uint8_t EEPROM_IsInitialized(void)
{
    uint32_t magic_a = EEPROM_ReadU32(EEPROM_MAGIC_A_ADDR);
    uint32_t magic_b = EEPROM_ReadU32(EEPROM_MAGIC_B_ADDR);

    return (magic_a == FIRMWARE_MAGIC && magic_b == FIRMWARE_MAGIC);
}

/* Инициализация EEPROM при первом запуске */
static uint8_t EEPROM_InitFirst(void)
{
    FirmwareFooter_t footer = {0};
    footer.magic = FIRMWARE_MAGIC;

    int major, minor, patch;
    if (sscanf(CURRENT_VERSION, "%d.%d.%d", &major, &minor, &patch) == 3) {
        footer.major = major;
        footer.minor = minor;
        footer.patch = patch;
    } else {
        footer.major = 1;
        footer.minor = 0;
        footer.patch = 0;
    }
    footer.crc = 0;

    if (EEPROM_WritePartitionInfo(PARTITION_A, &footer) != EEPROM_OK) return 0;
    if (EEPROM_WritePartitionInfo(PARTITION_B, &footer) != EEPROM_OK) return 0;
    if (EEPROM_WriteU8(EEPROM_BOOT_ATTEMPTS_ADDR, 0) != EEPROM_OK) return 0;
    if (EEPROM_WriteU32(EEPROM_UPDATE_COUNT_ADDR, 0) != EEPROM_OK) return 0;
    if (EEPROM_WriteU8(EEPROM_RESTORE_FLAG_ADDR, 0) != EEPROM_OK) return 0;

    return 1;
}

/* Прочитать/записать счётчик обновлений */
static uint32_t EEPROM_ReadUpdateCount(void)   { return EEPROM_ReadU32(EEPROM_UPDATE_COUNT_ADDR); }
static uint8_t EEPROM_SaveUpdateCount(uint32_t count) { return EEPROM_WriteU32(EEPROM_UPDATE_COUNT_ADDR, count); }

/* Прочитать/записать счётчик попыток загрузки */
static uint8_t EEPROM_ReadBootAttempts(void)   { return EEPROM_ReadU8(EEPROM_BOOT_ATTEMPTS_ADDR); }
static uint8_t EEPROM_SaveBootAttempts(uint8_t attempts) { return EEPROM_WriteU8(EEPROM_BOOT_ATTEMPTS_ADDR, attempts); }
static uint8_t EEPROM_ClearBootAttempts(void) {
    uint8_t result = EEPROM_ClearPage(EEPROM_BOOT_ATTEMPTS_ADDR);
    if (result != EEPROM_OK) return result;
    return EEPROM_WriteU8(EEPROM_BOOT_ATTEMPTS_ADDR, 0);
}

/* Прочитать/записать флаг восстановления */
static uint8_t EEPROM_ReadRestoreFlag(void)    { return EEPROM_ReadU8(EEPROM_RESTORE_FLAG_ADDR); }
static uint8_t EEPROM_SaveRestoreFlag(uint8_t flag) { return EEPROM_WriteU8(EEPROM_RESTORE_FLAG_ADDR, flag); }
static uint8_t EEPROM_ClearRestoreFlag(void) {
    uint8_t result = EEPROM_ClearPage(EEPROM_RESTORE_FLAG_ADDR);
    if (result != EEPROM_OK) return result;
    return EEPROM_WriteU8(EEPROM_RESTORE_FLAG_ADDR, 0);
}

// ===========================================================
// ==================== РАБОТА С ВЕРСИЯМИ ====================
// ===========================================================

/* Сравнить две версии в формате "x.y.z" */
/* Возвращает: 1 если v1 > v2, 0 если равны, -1 если v1 < v2 */
static int8_t Version_Compare(const char* v1, const char* v2)
{
    if (!v1 || !v2) return 0;

    int m1, mn1, p1, m2, mn2, p2;
    if (sscanf(v1, "%d.%d.%d", &m1, &mn1, &p1) != 3) return 0;
    if (sscanf(v2, "%d.%d.%d", &m2, &mn2, &p2) != 3) return 0;

    if (m1 > m2) return 1;
    if (m1 < m2) return -1;
    if (mn1 > mn2) return 1;
    if (mn1 < mn2) return -1;
    if (p1 > p2) return 1;
    if (p1 < p2) return -1;
    return 0;
}

/* Преобразовать футер в строку версии */
static void FooterToVersion(const FirmwareFooter_t* footer, char* buffer, uint32_t size)
{
    if (!footer || !buffer) return;
    snprintf(buffer, size, "%d.%d.%d", footer->major, footer->minor, footer->patch);
}

/* Получить версию раздела в виде строки */
static uint8_t Version_GetPartitionString(char* buffer, uint32_t size, uint8_t partition)
{
    if (!buffer || size < 2) return 0;

    FirmwareFooter_t footer = {0};
    if (!EEPROM_ReadPartitionInfo(partition, &footer)) {
        strncpy(buffer, "0.0.0", size - 1);
        buffer[size - 1] = '\0';
        return 0;
    }

    FooterToVersion(&footer, buffer, size);
    return 1;
}

/* Показать информацию о разделах на экране */
void Version_ShowInfo(void)
{
    char ver_a[VERSION_BUFFER_SIZE] = {0};
    char ver_b[VERSION_BUFFER_SIZE] = {0};

    Version_GetPartitionString(ver_a, VERSION_BUFFER_SIZE, PARTITION_A);
    Version_GetPartitionString(ver_b, VERSION_BUFFER_SIZE, PARTITION_B);

    // Вместо #ifdef UPDATE_FIRMWARE_DEBUG
    PROGRAMM_LOG_INFO("Part A: %s", ver_a);
    PROGRAMM_LOG_INFO("Part B: %s", ver_b);
    PROGRAMM_LOG_INFO("Boot:   A (always)");
    PROGRAMM_LOG_INFO("Updates: %lu", EEPROM_ReadUpdateCount());
    PROGRAMM_LOG_INFO("Attempts: %d", EEPROM_ReadBootAttempts());
    PROGRAMM_LOG_INFO("Restore: %d", EEPROM_ReadRestoreFlag());
}

// =========================================================
// ==================== ВСПОМОГАТЕЛЬНЫЕ ====================
// =========================================================

/* Мигание светодиодом */
static void LED_Blink(uint32_t on, uint32_t off)
{
    HAL_GPIO_WritePin(PE3_GPIO_Port, PE3_Pin, GPIO_PIN_SET);
    HAL_Delay(on - 1);
    HAL_GPIO_WritePin(PE3_GPIO_Port, PE3_Pin, GPIO_PIN_RESET);
    HAL_Delay(off - 1);
}

// ==========================================================
// ==================== РАБОТА С ФУТЕРОМ ====================
// ==========================================================

/* Прочитать футер из конца файла UPD.BIN */
static uint8_t Update_ReadFooterFromFile(FirmwareFooter_t* footer)
{
    if (!footer) return 0;

    if (!SD_Open_File(UPDATE_BIN_PATH, SD_MODE_READ)) {
        PROGRAMM_LOG_ERROR("Open fail");
        return 0;
    }

    uint32_t file_size = SD_Get_Open_File_Size();

    if (file_size < sizeof(FirmwareFooter_t)) {
        PROGRAMM_LOG_ERROR("Too small");
        SD_Close_File();
        return 0;
    }

    if (file_size > QSPI_PARTITION_SIZE) {
        PROGRAMM_LOG_ERROR("Firmware too big!");
        PROGRAMM_LOG_ERROR("Max: %luKB", QSPI_PARTITION_SIZE / 1024);
        SD_Close_File();
        return 0;
    }

    uint32_t footer_offset = file_size - sizeof(FirmwareFooter_t);

    if (!SD_Seek_File(footer_offset)) {
        PROGRAMM_LOG_ERROR("Seek fail");
        SD_Close_File();
        return 0;
    }

    uint32_t bytes_read = 0;
    if (!SD_Read_From_File((uint8_t*)footer, sizeof(FirmwareFooter_t), &bytes_read)) {
        PROGRAMM_LOG_ERROR("Read fail");
        SD_Close_File();
        return 0;
    }

    SD_Close_File();

    if (bytes_read != sizeof(FirmwareFooter_t)) {
        PROGRAMM_LOG_ERROR("Size mismatch");
        return 0;
    }

    return 1;
}

/* Проверить наличие файлов обновления на SD */
static uint8_t Update_CheckFilesExist(void)
{
    if (!SD_Directory_Exists(UPDATE_DIR)) {
        PROGRAMM_LOG_ERROR("No UPDATE dir");
        return 0;
    }

    if (!SD_File_Exists(UPDATE_BIN_PATH)) {
        PROGRAMM_LOG_ERROR("No UPD.BIN");
        return 0;
    }

    PROGRAMM_LOG_OK("UPD found");
    return 1;
}

// ====================================================
// ==================== РАСЧЕТ CRC ====================
// ====================================================

/* Вычислить CRC32 файла (с исключением футера) */
static uint32_t Update_CalculateFileCRC(uint32_t exclude_footer)
{
    uint32_t bytes_read = 0;
    uint32_t file_size = 0;
    uint32_t crc_value = 0;

    if (!SD_Open_File(UPDATE_BIN_PATH, SD_MODE_READ)) {
        PROGRAMM_LOG_ERROR("CRC open fail");
        return 0;
    }

    file_size = SD_Get_Open_File_Size();

    uint32_t read_size = file_size;
    if (exclude_footer) {
        if (file_size < sizeof(FirmwareFooter_t)) {
            SD_Close_File();
            return 0;
        }
        read_size = file_size - sizeof(FirmwareFooter_t);
    }

    PROGRAMM_LOG_INFO("Size: %luKB", file_size / 1024);

    __HAL_CRC_DR_RESET(&hcrc);

    uint32_t bytes_processed = 0;
    uint32_t last_progress = 0;

    while (bytes_processed < read_size) {
        uint32_t to_read = UPDATE_BUFFER_SIZE;
        if (bytes_processed + to_read > read_size) {
            to_read = read_size - bytes_processed;
        }

        if (!SD_Read_From_File(read_buffer, to_read, &bytes_read)) {
            PROGRAMM_LOG_ERROR("CRC read err");
            SD_Close_File();
            return 0;
        }

        if (bytes_read == 0) break;

        uint32_t words = bytes_read / 4;
        if (words > 0) {
            crc_value = HAL_CRC_Calculate(&hcrc, (uint32_t*)read_buffer, words);
        }

        uint32_t rem = bytes_read % 4;
        if (rem > 0) {
            uint32_t last_word = 0;
            uint8_t* ptr = (uint8_t*)&last_word;
            for (uint32_t i = 0; i < rem; i++) {
                ptr[i] = read_buffer[words * 4 + i];
            }
            crc_value = HAL_CRC_Calculate(&hcrc, &last_word, 1);
        }

        bytes_processed += bytes_read;

        if (read_size > 0) {
            uint32_t progress = (bytes_processed * 100) / read_size;
            if (progress >= last_progress + 10) {
                last_progress = progress;
                snprintf(progress_buf, sizeof(progress_buf), "%lu%%", progress);
                PROGRAMM_LOG_PROGRESS("%s", progress_buf);
            }
        }
    }

    SD_Close_File();

    crc_value = hcrc.Instance->DR;
    PROGRAMM_LOG_INFO("CRC:0x%08lX", crc_value);

    return crc_value;
}

// =======================================================
// ==================== РАБОТА С QSPI ====================
// =======================================================

/* Стереть один раздел QSPI */
static uint8_t Update_ErasePartition(uint32_t partition_offset)
{
    uint32_t sector_size = flash_params.sector_size;
    uint32_t sectors = QSPI_PARTITION_SIZE / sector_size;
    uint8_t success = 1;

    PROGRAMM_LOG_WARNING("Erase partition...");
    snprintf(progress_buf, sizeof(progress_buf), "Sectors:%lu", sectors);
    PROGRAMM_LOG_INFO("%s", progress_buf);

    for (uint32_t sector = 0; sector < sectors; sector++) {
        uint32_t addr = partition_offset + sector * sector_size;

        if (!QSPI_EraseSector(addr)) {
            PROGRAMM_LOG_ERROR("Erase fail at %lu", sector);
            success = 0;
            break;
        }

        if (sector % 100 == 0 || sector == sectors - 1) {
            snprintf(progress_buf, sizeof(progress_buf), "%lu/%lu", sector + 1, sectors);
            PROGRAMM_LOG_PROGRESS("%s", progress_buf);
        }
    }

    if (success) {
        PROGRAMM_LOG_OK("Erase done");
    } else {
        PROGRAMM_LOG_ERROR("Erase fail");
    }

    return success;
}

/* Записать UPD.BIN в указанный раздел QSPI */
static uint8_t Update_WriteBinToPartition(uint32_t partition_offset)
{
    uint32_t total = 0;
    uint32_t bytes_read = 0;

    if (!SD_Open_File(UPDATE_BIN_PATH, SD_MODE_READ)) {
        PROGRAMM_LOG_ERROR("Open fail");
        return 0;
    }

    uint32_t file_size = SD_Get_Open_File_Size();
    PROGRAMM_LOG_INFO("Size:%luKB", file_size / 1024);

    uint32_t last_progress = 0;

    while (1) {
        if (!SD_Read_From_File(read_buffer, UPDATE_BUFFER_SIZE, &bytes_read)) {
            PROGRAMM_LOG_ERROR("Read fail");
            SD_Close_File();
            return 0;
        }

        if (bytes_read == 0) break;

        if (!QSPI_Write(partition_offset + total, read_buffer, bytes_read)) {
            PROGRAMM_LOG_ERROR("Write fail at %luKB", total / 1024);
            SD_Close_File();
            return 0;
        }

        total += bytes_read;

        if (file_size > 0) {
            uint32_t progress = (total * 100) / file_size;
            if (progress >= last_progress + 10) {
                last_progress = progress;
                snprintf(progress_buf, sizeof(progress_buf), "%lu%%", progress);
                PROGRAMM_LOG_PROGRESS("%s", progress_buf);
            }
        }
    }

    SD_Close_File();
    PROGRAMM_LOG_OK("Write done (%luKB)", total / 1024);
    return 1;
}

// ==============================================================
// ==================== КОПИРОВАНИЕ РАЗДЕЛОВ ====================
// ==============================================================

/* Копировать раздел A в B (резервное копирование перед обновлением) */
static uint8_t CopyPartitionAtoB(void)
{
    uint32_t src_offset = QSPI_PARTITION_A_OFFSET;
    uint32_t dst_offset = QSPI_PARTITION_B_OFFSET;
    uint32_t size = QSPI_PARTITION_SIZE;
    uint32_t offset = 0;
    uint32_t chunk;
    uint32_t sectors = size / QSPI_SECTOR_SIZE;

    PROGRAMM_LOG_WARNING("Backup A → B...");

    // Стираем раздел B
    PROGRAMM_LOG_INFO("Erase B...");
    snprintf(progress_buf, sizeof(progress_buf), "Sectors:%lu", sectors);
    PROGRAMM_LOG_INFO("%s", progress_buf);

    for (uint32_t sector = 0; sector < sectors; sector++) {
        uint32_t addr = dst_offset + sector * QSPI_SECTOR_SIZE;

        if (!QSPI_EraseSector(addr)) {
            PROGRAMM_LOG_ERROR("Erase B fail at %lu", sector);
            return 0;
        }

        if (sector % 100 == 0 || sector == sectors - 1) {
            snprintf(progress_buf, sizeof(progress_buf), "%lu/%lu", sector + 1, sectors);
            PROGRAMM_LOG_PROGRESS("%s", progress_buf);
        }
    }
    PROGRAMM_LOG_OK("Erase B done");

    // Копируем данные
    PROGRAMM_LOG_INFO("Copy A → B...");
    while (offset < size) {
        chunk = (size - offset > sizeof(read_buffer)) ? sizeof(read_buffer) : (size - offset);

        if (!QSPI_Read(src_offset + offset, read_buffer, chunk)) {
            PROGRAMM_LOG_ERROR("Read A fail at %luKB", offset / 1024);
            return 0;
        }

        if (!QSPI_Write(dst_offset + offset, read_buffer, chunk)) {
            PROGRAMM_LOG_ERROR("Backup fail at %luKB", offset / 1024);
            return 0;
        }

        offset += chunk;

        if (offset % (1024 * 1024) == 0 || offset == size) {
            snprintf(progress_buf, sizeof(progress_buf), "%luMB/%luMB",
                     offset / (1024*1024), size / (1024*1024));
            PROGRAMM_LOG_PROGRESS("%s", progress_buf);
        }
    }
    PROGRAMM_LOG_OK("Backup A → B done!");
    return 1;
}

/* Копировать раздел B в A (восстановление после сбоя) */
static uint8_t CopyPartitionBtoA(void)
{
    uint32_t src_offset = QSPI_PARTITION_B_OFFSET;
    uint32_t dst_offset = QSPI_PARTITION_A_OFFSET;
    uint32_t size = QSPI_PARTITION_SIZE;
    uint32_t offset = 0;
    uint32_t chunk;
    uint32_t sectors = size / QSPI_SECTOR_SIZE;

    PROGRAMM_LOG_WARNING("Restore B → A...");

    // Стираем раздел A
    PROGRAMM_LOG_INFO("Erase A...");
    snprintf(progress_buf, sizeof(progress_buf), "Sectors:%lu", sectors);
    PROGRAMM_LOG_INFO("%s", progress_buf);

    for (uint32_t sector = 0; sector < sectors; sector++) {
        uint32_t addr = dst_offset + sector * QSPI_SECTOR_SIZE;

        if (!QSPI_EraseSector(addr)) {
            PROGRAMM_LOG_ERROR("Erase fail at %lu", sector);
            return 0;
        }

        if (sector % 100 == 0 || sector == sectors - 1) {
            snprintf(progress_buf, sizeof(progress_buf), "%lu/%lu", sector + 1, sectors);
            PROGRAMM_LOG_PROGRESS("%s", progress_buf);
        }
    }
    PROGRAMM_LOG_OK("Erase done");

    // Копируем данные
    PROGRAMM_LOG_INFO("Copy data...");
    while (offset < size) {
        chunk = (size - offset > sizeof(read_buffer)) ? sizeof(read_buffer) : (size - offset);

        if (!QSPI_Read(src_offset + offset, read_buffer, chunk)) {
            PROGRAMM_LOG_ERROR("Read B fail at %luKB", offset / 1024);
            return 0;
        }

        if (!QSPI_Write(dst_offset + offset, read_buffer, chunk)) {
            PROGRAMM_LOG_ERROR("Copy fail at %luKB", offset / 1024);
            return 0;
        }

        offset += chunk;

        if (offset % (1024 * 1024) == 0 || offset == size) {
            snprintf(progress_buf, sizeof(progress_buf), "%luMB/%luMB",
                     offset / (1024*1024), size / (1024*1024));
            PROGRAMM_LOG_PROGRESS("%s", progress_buf);
        }
    }
    PROGRAMM_LOG_OK("Copy done!");
    return 1;
}

// ==================================================================
// ==================== ВСПОМОГАТЕЛЬНЫЕ ОПЕРАЦИИ ====================
// ==================================================================

/* Удалить UPD.BIN с SD карты */
static uint8_t Update_DeleteUpdateBin(void)
{
    if (!SD_Delete_File(UPDATE_BIN_PATH)) {
        PROGRAMM_LOG_ERROR("Del fail");
        return 0;
    }
    PROGRAMM_LOG_OK("UPD deleted");
    return 1;
}

/* Перезагрузка контроллера */
static void Update_Restart(void)
{
    PROGRAMM_LOG_WARNING("Restart...");
    HAL_Delay(1000);
    NVIC_SystemReset();
}

// ==========================================================
// ==================== ОСНОВНАЯ ФУНКЦИЯ ====================
// ==========================================================

/* Проверить наличие обновлений и выполнить их */
uint8_t Update_CheckAndStart(void)
{
    FirmwareFooter_t footer = {0};
    FirmwareFooter_t footer_a = {0};
    FirmwareFooter_t footer_b = {0};
    char new_version[VERSION_BUFFER_SIZE] = {0};
    uint8_t result;
    uint8_t is_first = 0;
    uint8_t restore_flag;

    // Проверяем флаг восстановления
    restore_flag = EEPROM_ReadRestoreFlag();
    if (restore_flag == 1) {
        PROGRAMM_LOG_WARNING("=== RESTORE FROM B ===");
        PROGRAMM_LOG_INFO("Restoring partition A from B...");

        if (CopyPartitionBtoA()) {
            EEPROM_ReadPartitionInfo(PARTITION_B, &footer_b);

            EEPROM_ClearPage(EEPROM_MAGIC_A_ADDR);
            EEPROM_ClearPage(EEPROM_MAJOR_A_ADDR);
            EEPROM_ClearPage(EEPROM_MINOR_A_ADDR);
            EEPROM_ClearPage(EEPROM_PATCH_A_ADDR);
            EEPROM_ClearPage(EEPROM_CRC_A_ADDR);

            EEPROM_WritePartitionInfo(PARTITION_A, &footer_b);
            EEPROM_ClearBootAttempts();
            EEPROM_ClearRestoreFlag();

            PROGRAMM_LOG_OK("Restore done!");
            PROGRAMM_LOG_OK("Version restored from B");
            HAL_Delay(1500);
            NVIC_SystemReset();
        } else {
            PROGRAMM_LOG_ERROR("Restore FAILED!");
            while(1) LED_Blink(100, 100);
        }
    }

    PROGRAMM_LOG_INFO("Check update...");

    if (!Update_CheckFilesExist()) {
        return 0;
    }

    PROGRAMM_LOG_WARNING("=== UPDATE ===");

    // Инициализация EEPROM
    if (!EEPROM_IsInitialized()) {
        PROGRAMM_LOG_WARNING("First init EEPROM...");
        if (!EEPROM_InitFirst()) {
            PROGRAMM_LOG_ERROR("EEPROM init fail");
            return 0;
        }
        PROGRAMM_LOG_OK("EEPROM init OK");
        is_first = 1;
    }

    QSPI_GetParams(&flash_params);
    Version_ShowInfo();

    // Чтение футера из файла
    if (!Update_ReadFooterFromFile(&footer)) {
        PROGRAMM_LOG_ERROR("FAIL (footer)");
        return 0;
    }

    if (footer.magic != FIRMWARE_MAGIC) {
        PROGRAMM_LOG_ERROR("Bad MAGIC");
        return 0;
    }
    PROGRAMM_LOG_OK("Magic OK");

    // Формируем новую версию
    snprintf(new_version, VERSION_BUFFER_SIZE, "%d.%d.%d",
             footer.major, footer.minor, footer.patch);

    // Читаем текущие версии
    EEPROM_ReadPartitionInfo(PARTITION_A, &footer_a);
    EEPROM_ReadPartitionInfo(PARTITION_B, &footer_b);

    char ver_a[VERSION_BUFFER_SIZE] = {0};
    char ver_b[VERSION_BUFFER_SIZE] = {0};
    FooterToVersion(&footer_a, ver_a, VERSION_BUFFER_SIZE);
    FooterToVersion(&footer_b, ver_b, VERSION_BUFFER_SIZE);

    // Проверка версии
    if (!is_first) {
        if (Version_Compare(new_version, ver_a) <= 0) {
            PROGRAMM_LOG_WARNING("Version not newer");
            PROGRAMM_LOG_INFO("Current: %s", ver_a);
            PROGRAMM_LOG_INFO("New: %s", new_version);
            return 0;
        }
        PROGRAMM_LOG_OK("New version higher!");
    } else {
        PROGRAMM_LOG_OK("First install: %s", new_version);
    }

    // CRC проверка
    uint32_t file_crc = Update_CalculateFileCRC(1);
    PROGRAMM_LOG_INFO("CRC:0x%08lX", file_crc);
    PROGRAMM_LOG_INFO("FCRC:0x%08lX", footer.crc);

    if (file_crc != footer.crc) {
        PROGRAMM_LOG_ERROR("CRC MISMATCH");
        PROGRAMM_LOG_ERROR("ABORTED");
        return 0;
    }
    PROGRAMM_LOG_OK("CRC OK");

    PROGRAMM_LOG_INFO("New: %s", new_version);

    // ============================================================
    // ПЕРВЫЙ ЗАПУСК - пишем в оба раздела
    // ============================================================
    if (is_first) {
        PROGRAMM_LOG_WARNING("First install: writing to A and B");

        uint32_t sectors = QSPI_FLASH_SIZE / QSPI_SECTOR_SIZE;
        for (uint32_t sector = 0; sector < sectors; sector++) {
            if (!QSPI_EraseSector(sector * QSPI_SECTOR_SIZE)) {
                PROGRAMM_LOG_ERROR("Erase fail at %lu", sector);
                return 0;
            }
            if (sector % 100 == 0 || sector == sectors - 1) {
                PROGRAMM_LOG_PROGRESS("%lu/%lu", sector + 1, sectors);
            }
        }
        PROGRAMM_LOG_OK("QSPI erased");

        if (!Update_WriteBinToPartition(QSPI_PARTITION_A_OFFSET)) {
            PROGRAMM_LOG_ERROR("FAIL (write A)");
            return 0;
        }

        if (!Update_WriteBinToPartition(QSPI_PARTITION_B_OFFSET)) {
            PROGRAMM_LOG_ERROR("FAIL (write B)");
            return 0;
        }

        if (EEPROM_WritePartitionInfo(PARTITION_A, &footer) != EEPROM_OK) {
            PROGRAMM_LOG_ERROR("FAIL (write EEPROM A)");
            return 0;
        }
        if (EEPROM_WritePartitionInfo(PARTITION_B, &footer) != EEPROM_OK) {
            PROGRAMM_LOG_ERROR("FAIL (write EEPROM B)");
            return 0;
        }

        EEPROM_ClearBootAttempts();
        EEPROM_SaveUpdateCount(0);
        PROGRAMM_LOG_OK("First install complete!");

    // ============================================================
    // ОБЫЧНОЕ ОБНОВЛЕНИЕ - бэкап A→B, затем обновление A
    // ============================================================
    } else {
        // 1. Бэкап A → B
        PROGRAMM_LOG_WARNING("Backup current A to B...");
        if (!CopyPartitionAtoB()) {
            PROGRAMM_LOG_ERROR("FAIL (backup A→B)");
            return 0;
        }

        // 2. Стираем раздел A
        PROGRAMM_LOG_WARNING("Update: writing only to A");
        if (!Update_ErasePartition(QSPI_PARTITION_A_OFFSET)) {
            PROGRAMM_LOG_ERROR("FAIL (erase A)");
            return 0;
        }

        // 3. Пишем новую прошивку в A
        if (!Update_WriteBinToPartition(QSPI_PARTITION_A_OFFSET)) {
            PROGRAMM_LOG_ERROR("FAIL (write A)");
            return 0;
        }

        // 4. Очищаем EEPROM раздела A
        PROGRAMM_LOG_WARNING("Clear EEPROM A...");
        result = EEPROM_ClearPage(EEPROM_MAGIC_A_ADDR);
        if (result != EEPROM_OK) {
            PROGRAMM_LOG_ERROR("Clear magic fail");
            return 0;
        }
        result = EEPROM_ClearPage(EEPROM_MAJOR_A_ADDR);
        if (result != EEPROM_OK) {
            PROGRAMM_LOG_ERROR("Clear major fail");
            return 0;
        }
        result = EEPROM_ClearPage(EEPROM_MINOR_A_ADDR);
        if (result != EEPROM_OK) {
            PROGRAMM_LOG_ERROR("Clear minor fail");
            return 0;
        }
        result = EEPROM_ClearPage(EEPROM_PATCH_A_ADDR);
        if (result != EEPROM_OK) {
            PROGRAMM_LOG_ERROR("Clear patch fail");
            return 0;
        }
        result = EEPROM_ClearPage(EEPROM_CRC_A_ADDR);
        if (result != EEPROM_OK) {
            PROGRAMM_LOG_ERROR("Clear crc fail");
            return 0;
        }

        // 5. Пишем новую версию в EEPROM
        if (EEPROM_WritePartitionInfo(PARTITION_A, &footer) != EEPROM_OK) {
            PROGRAMM_LOG_ERROR("FAIL (write EEPROM A)");
            return 0;
        }

        // 6. Обновляем счётчики
        EEPROM_ClearBootAttempts();
        uint32_t count = EEPROM_ReadUpdateCount();
        EEPROM_SaveUpdateCount(count + 1);

        PROGRAMM_LOG_OK("Update complete!");
    }

    Version_ShowInfo();

    if (!Update_DeleteUpdateBin()) {
        PROGRAMM_LOG_ERROR("FAIL (delete)");
        return 0;
    }

    PROGRAMM_LOG_OK("=== DONE ===");
    Update_Restart();
    return 1;
}

// =============================================================
// ==================== ПРЫЖОК В ПРИЛОЖЕНИЕ ====================
// =============================================================

/* Загрузка и выполнение приложения из раздела A */
void JumpToApplication(void)
{
    uint32_t boot_address = QSPI_PARTITION_A_ADDR;
    uint32_t app_reset;
    uint8_t attempts;

    if (!EEPROM_IsInitialized()) {
        PROGRAMM_LOG_ERROR("EEPROM not init!");
        if (!EEPROM_InitFirst()) {
            while (1) LED_Blink(100, 100);
        }
    }

    attempts = EEPROM_ReadBootAttempts();

    PROGRAMM_LOG_INFO("Boot from A");
    PROGRAMM_LOG_INFO("Boot attempts: %d", attempts);

    app_reset = *(volatile uint32_t*)(boot_address + 4);

    if ((app_reset < QSPI_BASE_ADDR) ||
        (app_reset > (QSPI_BASE_ADDR + QSPI_FLASH_SIZE))) {

        PROGRAMM_LOG_ERROR("Invalid APP!");

        attempts++;
        EEPROM_SaveBootAttempts(attempts);
        PROGRAMM_LOG_WARNING("Attempt %d/%d", attempts, BOOT_ATTEMPTS_MAX);

        if (attempts >= BOOT_ATTEMPTS_MAX) {
            PROGRAMM_LOG_ERROR("3 failed boots!");

            // Ставим флаг восстановления и перезагружаемся
            EEPROM_SaveRestoreFlag(1);
            PROGRAMM_LOG_WARNING("Restore flag set");

            HAL_Delay(1000);
            NVIC_SystemReset();
        }

        HAL_Delay(1000);
        NVIC_SystemReset();
    }

    // Успешная загрузка - сбрасываем счётчик попыток
    if (attempts > 0) {
        EEPROM_ClearBootAttempts();
        PROGRAMM_LOG_OK("Boot attempts cleared");
    }

    PROGRAMM_LOG_OK("Jump to APP");
    HAL_Delay(1000);

    SCB->VTOR = boot_address;

    void (*app_entry)(void) = (void (*)(void))app_reset;
    app_entry();

    while (1)
    {
    	ErrorCode = 20;
        LED_Blink(50, 500);
        Error_Handler();
    }
}
