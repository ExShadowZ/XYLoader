/*
 * eprom.c
 * Universal EEPROM driver for AT24Cxx family
 */

#include "eprom.h"
#include <string.h>

// ====================================================
// ==================== ПЕРЕМЕННЫЕ ====================
// ====================================================

extern I2C_HandleTypeDef EEPROM_I2C;
extern CRC_HandleTypeDef EEPROM_CRC;

static uint8_t data_buffer[EEPROM_PAGE_SIZE];

// =========================================================
// ==================== ВСПОМОГАТЕЛЬНЫЕ ====================
// =========================================================

/* Ожидание завершения внутренней записи */
static void EEPROM_WaitForWriteComplete(void)
{
    HAL_Delay(5);
}

// =========================================================
// ===================== ИНИЦИАЛИЗАЦИЯ =====================
// =========================================================

/* Проверить наличие EEPROM на шине I2C */
uint8_t EEPROM_Init(void)
{
    uint8_t test_data = 0;

    if (EEPROM_ReadByte(0, &test_data) == EEPROM_OK) {
        return EEPROM_OK;
    }

    return EEPROM_ERROR_TIMEOUT;
}

// =============================================================
// ==================== ПОБАЙТОВЫЕ ОПЕРАЦИИ ====================
// =============================================================

/* Запись одного байта */
uint8_t EEPROM_WriteByte(uint16_t address, uint8_t data)
{
    uint8_t buffer[EEPROM_ADDR_SIZE + 1];
    uint8_t idx = 0;

    if (address >= EEPROM_SIZE) {
        return EEPROM_ERROR_SIZE;
    }

    EEPROM_WP_DISABLE(); // Отключаем защиту (если используется)

    if (EEPROM_ADDR_SIZE == 1) {
        buffer[idx++] = (uint8_t)(address & 0xFF);
    } else {
        buffer[idx++] = (uint8_t)((address >> 8) & 0xFF);
        buffer[idx++] = (uint8_t)(address & 0xFF);
    }
    buffer[idx++] = data;

    if (HAL_I2C_Master_Transmit(&EEPROM_I2C, EEPROM_I2C_ADDR, buffer, idx, 100) != HAL_OK) {
        EEPROM_WP_ENABLE(); // Включаем защиту (если используется)
        return EEPROM_ERROR_TIMEOUT;
    }

    EEPROM_WaitForWriteComplete();
    EEPROM_WP_ENABLE(); // Включаем защиту (если используется)

    return EEPROM_OK;
}

/* Чтение одного байта */
uint8_t EEPROM_ReadByte(uint16_t address, uint8_t *data)
{
    uint8_t addr_buf[EEPROM_ADDR_SIZE];

    if (address >= EEPROM_SIZE) {
        return EEPROM_ERROR_SIZE;
    }

    EEPROM_WP_ENABLE(); // Включаем защиту (если используется)

    if (EEPROM_ADDR_SIZE == 1) {
        addr_buf[0] = (uint8_t)(address & 0xFF);
    } else {
        addr_buf[0] = (uint8_t)((address >> 8) & 0xFF);
        addr_buf[1] = (uint8_t)(address & 0xFF);
    }

    if (HAL_I2C_Master_Transmit(&EEPROM_I2C, EEPROM_I2C_ADDR, addr_buf, EEPROM_ADDR_SIZE, 100) != HAL_OK) {
        return EEPROM_ERROR_TIMEOUT;
    }

    if (HAL_I2C_Master_Receive(&EEPROM_I2C, EEPROM_I2C_ADDR | 0x01, data, 1, 100) != HAL_OK) {
        return EEPROM_ERROR_TIMEOUT;
    }

    return EEPROM_OK;
}

// ===========================================================
// ==================== БУФЕРНЫЕ ОПЕРАЦИИ ====================
// ===========================================================

/* Запись массива данных с учётом страничной организации */
uint8_t EEPROM_WriteBuffer(uint16_t address, uint8_t *data, uint16_t size)
{
    uint16_t bytes_written = 0;
    uint16_t bytes_to_write;
    uint16_t current_addr = address;
    uint8_t buffer[EEPROM_PAGE_SIZE + EEPROM_ADDR_SIZE];

    if (address + size > EEPROM_SIZE) {
        return EEPROM_ERROR_SIZE;
    }

    EEPROM_WP_DISABLE(); // Отключаем защиту (если используется)

    while (bytes_written < size) {
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        bytes_to_write = EEPROM_PAGE_SIZE - page_offset;

        if (bytes_to_write > (size - bytes_written)) {
            bytes_to_write = size - bytes_written;
        }

        uint8_t idx = 0;
        if (EEPROM_ADDR_SIZE == 1) {
            buffer[idx++] = (uint8_t)(current_addr & 0xFF);
        } else {
            buffer[idx++] = (uint8_t)((current_addr >> 8) & 0xFF);
            buffer[idx++] = (uint8_t)(current_addr & 0xFF);
        }

        memcpy(&buffer[idx], &data[bytes_written], bytes_to_write);

        if (HAL_I2C_Master_Transmit(&EEPROM_I2C, EEPROM_I2C_ADDR,
                                   buffer, idx + bytes_to_write, 100) != HAL_OK) {
            EEPROM_WP_ENABLE(); // Включаем защиту (если используется)
            return EEPROM_ERROR_TIMEOUT;
        }

        EEPROM_WaitForWriteComplete();

        bytes_written += bytes_to_write;
        current_addr += bytes_to_write;
    }

    EEPROM_WP_ENABLE(); // Включаем защиту (если используется)
    return EEPROM_OK;
}

/* Чтение массива данных */
uint8_t EEPROM_ReadBuffer(uint16_t address, uint8_t *buffer, uint16_t size)
{
    uint8_t addr_buf[EEPROM_ADDR_SIZE];

    if (address + size > EEPROM_SIZE) {
        return EEPROM_ERROR_SIZE;
    }

    EEPROM_WP_ENABLE(); // Включаем защиту (если используется)

    if (EEPROM_ADDR_SIZE == 1) {
        addr_buf[0] = (uint8_t)(address & 0xFF);
    } else {
        addr_buf[0] = (uint8_t)((address >> 8) & 0xFF);
        addr_buf[1] = (uint8_t)(address & 0xFF);
    }

    if (HAL_I2C_Master_Transmit(&EEPROM_I2C, EEPROM_I2C_ADDR, addr_buf, EEPROM_ADDR_SIZE, 100) != HAL_OK) {
        return EEPROM_ERROR_TIMEOUT;
    }

    if (HAL_I2C_Master_Receive(&EEPROM_I2C, EEPROM_I2C_ADDR | 0x01, buffer, size, 100) != HAL_OK) {
        return EEPROM_ERROR_TIMEOUT;
    }

    return EEPROM_OK;
}

// ===========================================================
// ========================= ОЧИСТКА =========================
// ===========================================================

/* Очистка всей EEPROM (заполнение 0xFF) */
uint8_t EEPROM_EraseChip(void)
{
    EEPROM_WP_DISABLE(); // Отключаем защиту (если используется)

    memset(data_buffer, 0xFF, EEPROM_PAGE_SIZE);

    for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += EEPROM_PAGE_SIZE) {
        if (EEPROM_WriteBuffer(addr, data_buffer, EEPROM_PAGE_SIZE) != EEPROM_OK) {
            EEPROM_WP_ENABLE(); // Включаем защиту (если используется)
            return EEPROM_ERROR;
        }
    }

    EEPROM_WP_ENABLE(); // Включаем защиту (если используется)
    return EEPROM_OK;
}

/* Очистка всей EEPROM (заполнение нулями) - для совместимости */
uint8_t EEPROM_Clear(void)
{
    uint8_t zero_page[EEPROM_PAGE_SIZE];
    uint16_t address;
    uint8_t result;

    memset(zero_page, 0, EEPROM_PAGE_SIZE);

    EEPROM_WP_DISABLE(); // Отключаем защиту (если используется)

    for (address = 0; address < EEPROM_SIZE; address += EEPROM_PAGE_SIZE) {
        result = EEPROM_WriteBuffer(address, zero_page, EEPROM_PAGE_SIZE);
        if (result != EEPROM_OK) {
            EEPROM_WP_ENABLE(); // Включаем защиту (если используется)
            return result;
        }
    }

    EEPROM_WP_ENABLE(); // Включаем защиту (если используется)
    return EEPROM_OK;
}

// =========================================================
// ====================== ФУНКЦИИ С CRC ====================
// =========================================================

/* Запись данных с проверкой CRC32 */
uint8_t EEPROM_WriteData(uint16_t address, uint8_t *data, uint8_t len)
{
    uint32_t crc32 = 0;
    uint32_t data_check = 0;
    uint32_t crc_check = 0;
    uint32_t import_data = 0;
    uint8_t result;

    if (address + len + 4 > EEPROM_SIZE) {
        return EEPROM_ERROR_SIZE;
    }

    // Упаковываем данные в 32-битное слово
    for (int i = 0; i < len; i++) {
        *((uint8_t*)&import_data + i) = *(data + i);
    }

    // Вычисляем CRC
    crc32 = HAL_CRC_Calculate(&EEPROM_CRC, (uint32_t*)&import_data, 1);

    EEPROM_WP_DISABLE(); // Отключаем защиту (если используется)

    // Пишем данные
    result = EEPROM_WriteBuffer(address, data, len);
    if (result != EEPROM_OK) {
        EEPROM_WP_ENABLE();
        return result;
    }

    // Пишем CRC
    result = EEPROM_WriteBuffer(address + len, (uint8_t*)&crc32, 4);
    if (result != EEPROM_OK) {
        EEPROM_WP_ENABLE();
        return result;
    }

    // Проверяем
    result = EEPROM_ReadBuffer(address, (uint8_t*)&data_check, len);
    if (result != EEPROM_OK) {
        EEPROM_WP_ENABLE();
        return result;
    }

    result = EEPROM_ReadBuffer(address + len, (uint8_t*)&crc_check, 4);
    if (result != EEPROM_OK) {
        EEPROM_WP_ENABLE();
        return result;
    }

    EEPROM_WP_ENABLE(); // Включаем защиту (если используется)

    if ((import_data == data_check) && (crc32 == crc_check)) {
        return EEPROM_OK;
    }

    return EEPROM_ERROR_CRC;
}

/* Чтение данных с проверкой CRC32 */
uint8_t EEPROM_ReadData(uint16_t address, uint8_t *data, uint8_t len)
{
    uint32_t crc_check = 0;
    uint32_t import_data = 0;
    uint8_t result;

    if (address + len + 4 > EEPROM_SIZE) {
        return EEPROM_ERROR_SIZE;
    }

    EEPROM_WP_ENABLE(); // Включаем защиту (если используется)

    // Читаем данные
    result = EEPROM_ReadBuffer(address, data, len);
    if (result != EEPROM_OK) return result;

    // Читаем CRC
    result = EEPROM_ReadBuffer(address + len, (uint8_t*)&crc_check, 4);
    if (result != EEPROM_OK) return result;

    // Упаковываем данные
    for (int i = 0; i < len; i++) {
        *((uint8_t*)&import_data + i) = *(data + i);
    }

    // Проверяем CRC
    if (HAL_CRC_Calculate(&EEPROM_CRC, (uint32_t*)&import_data, 1) == crc_check) {
        return EEPROM_OK;
    }

    return EEPROM_ERROR_CRC;
}
