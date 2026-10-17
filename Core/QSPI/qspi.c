/*
 * qspi.c
 * QSPI Flash driver for STM32H7
 */

#include "../QSPI/qspi.h"
#include <string.h>

// ====================================================
// ==================== ПЕРЕМЕННЫЕ ====================
// ====================================================

extern OSPI_HandleTypeDef hospi1;

static uint8_t initialized = 0;
static QSPI_FlashParams_t flash_params = {0};
extern uint32_t ErrorCode;

// =========================================================
// ==================== ВСПОМОГАТЕЛЬНЫЕ ====================
// =========================================================

/* Отправить команду в QSPI */
static uint8_t QSPI_SendCommand(uint8_t cmd)
{
    OSPI_RegularCmdTypeDef sCmd = {0};
    sCmd.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    sCmd.FlashId = HAL_OSPI_FLASH_ID_1;
    sCmd.Instruction = cmd;
    sCmd.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    sCmd.AddressMode = HAL_OSPI_ADDRESS_NONE;
    sCmd.DataMode = HAL_OSPI_DATA_NONE;
    sCmd.DummyCycles = 0;
    sCmd.DQSMode = HAL_OSPI_DQS_DISABLE;
    sCmd.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (HAL_OSPI_Command(&hospi1, &sCmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return 0;
    }
    return 1;
}

/* Разрешить запись в QSPI */
static uint8_t QSPI_WriteEnable(void)
{
    return QSPI_SendCommand(QSPI_CMD_WRITE_ENABLE);
}

/* Прочитать регистр статуса */
static uint8_t QSPI_ReadStatus(uint8_t *status)
{
    if (!status) return 0;

    OSPI_RegularCmdTypeDef sCmd = {0};
    sCmd.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    sCmd.FlashId = HAL_OSPI_FLASH_ID_1;
    sCmd.Instruction = QSPI_CMD_READ_STATUS_REG;
    sCmd.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    sCmd.AddressMode = HAL_OSPI_ADDRESS_NONE;
    sCmd.DataMode = HAL_OSPI_DATA_1_LINE;
    sCmd.NbData = 1;
    sCmd.DummyCycles = 0;
    sCmd.DQSMode = HAL_OSPI_DQS_DISABLE;
    sCmd.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (HAL_OSPI_Command(&hospi1, &sCmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return 0;
    }

    if (HAL_OSPI_Receive(&hospi1, status, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return 0;
    }

    return 1;
}

/* Ожидать готовности QSPI (бит BUSY = 0) */
static uint8_t QSPI_WaitReady(void)
{
    uint8_t status;
    uint32_t timeout = 1000000;

    do {
        if (!QSPI_ReadStatus(&status)) {
            return 0;
        }
        if (--timeout == 0) {
            return 0;
        }
    } while (status & 0x01);

    return 1;
}

// ==========================================================
// ==================== ОСНОВНЫЕ ФУНКЦИИ ====================
// ==========================================================

/* Инициализация QSPI */
uint8_t QSPI_Init(void)
{
    if (initialized) return 1;

    if (hospi1.Instance == NULL) {
        return 0;
    }

    flash_params.size = QSPI_FLASH_SIZE;
    flash_params.page_size = QSPI_PAGE_SIZE;
    flash_params.sector_size = QSPI_SECTOR_SIZE;
    flash_params.total_sectors = QSPI_TOTAL_SECTORS;
    flash_params.manufacturer_id = QSPI_MANUFACTURER;
    flash_params.memory_type = QSPI_MEMORY_TYPE;
    flash_params.capacity = QSPI_CAPACITY;
    flash_params.supports_4byte_addr = QSPI_4BYTE_ADDR;

    initialized = 1;
    return 1;
}

/* Получить параметры QSPI Flash */
void QSPI_GetParams(QSPI_FlashParams_t *params)
{
    if (params) {
        memcpy(params, &flash_params, sizeof(QSPI_FlashParams_t));
    }
}

/* Стереть один сектор (4KB) */
uint8_t QSPI_EraseSector(uint32_t address)
{
    if (!initialized) return 0;

    if (!QSPI_WriteEnable()) return 0;

    OSPI_RegularCmdTypeDef sCmd = {0};
    sCmd.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    sCmd.FlashId = HAL_OSPI_FLASH_ID_1;
    sCmd.Instruction = QSPI_CMD_SECTOR_ERASE;
    sCmd.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;

    if (QSPI_4BYTE_ADDR) {
        sCmd.AddressMode = HAL_OSPI_ADDRESS_4_LINES;
        sCmd.AddressSize = HAL_OSPI_ADDRESS_32_BITS;
        sCmd.Address = address;
    } else {
        sCmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
        sCmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
        sCmd.Address = address & 0x00FFFFFF;
    }

    sCmd.DataMode = HAL_OSPI_DATA_NONE;
    sCmd.DummyCycles = 0;
    sCmd.DQSMode = HAL_OSPI_DQS_DISABLE;
    sCmd.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (HAL_OSPI_Command(&hospi1, &sCmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return 0;
    }

    return QSPI_WaitReady();
}

/* Запись данных в QSPI */
uint8_t QSPI_Write(uint32_t address, uint8_t *buffer, uint32_t size)
{
    if (!initialized || !buffer || size == 0) return 0;

    uint32_t offset = 0;
    uint32_t page_size = flash_params.page_size;

    while (offset < size) {
        uint32_t remaining = size - offset;
        uint32_t page_offset = (address + offset) % page_size;
        uint32_t write_size = (remaining < (page_size - page_offset)) ?
                              remaining : (page_size - page_offset);

        if (!QSPI_WriteEnable()) return 0;

        OSPI_RegularCmdTypeDef sCmd = {0};
        sCmd.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
        sCmd.FlashId = HAL_OSPI_FLASH_ID_1;
        sCmd.Instruction = QSPI_CMD_PAGE_PROGRAM;
        sCmd.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
        sCmd.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;

        if (QSPI_4BYTE_ADDR) {
            sCmd.AddressMode = HAL_OSPI_ADDRESS_4_LINES;
            sCmd.AddressSize = HAL_OSPI_ADDRESS_32_BITS;
            sCmd.Address = address + offset;
        } else {
            sCmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
            sCmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
            sCmd.Address = (address + offset) & 0x00FFFFFF;
        }

        sCmd.DataMode = HAL_OSPI_DATA_1_LINE;
        sCmd.NbData = write_size;
        sCmd.DummyCycles = 0;
        sCmd.DQSMode = HAL_OSPI_DQS_DISABLE;
        sCmd.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

        if (HAL_OSPI_Command(&hospi1, &sCmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
            return 0;
        }

        if (HAL_OSPI_Transmit(&hospi1, buffer + offset, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
            return 0;
        }

        if (!QSPI_WaitReady()) return 0;

        offset += write_size;
    }

    return 1;
}

/* Чтение данных из QSPI (Indirect режим) */
uint8_t QSPI_Read(uint32_t address, uint8_t *buffer, uint32_t size)
{
    if (!initialized || !buffer || size == 0) return 0;

    OSPI_RegularCmdTypeDef sCmd = {0};
    sCmd.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    sCmd.FlashId = HAL_OSPI_FLASH_ID_1;
    sCmd.Instruction = QSPI_CMD_FAST_READ;
    sCmd.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;

    if (QSPI_4BYTE_ADDR) {
        sCmd.AddressMode = HAL_OSPI_ADDRESS_4_LINES;
        sCmd.AddressSize = HAL_OSPI_ADDRESS_32_BITS;
        sCmd.Address = address;
    } else {
        sCmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
        sCmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
        sCmd.Address = address & 0x00FFFFFF;
    }

    sCmd.DataMode = HAL_OSPI_DATA_1_LINE;
    sCmd.NbData = size;
    sCmd.DummyCycles = 8;
    sCmd.DQSMode = HAL_OSPI_DQS_DISABLE;
    sCmd.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (HAL_OSPI_Command(&hospi1, &sCmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return 0;
    }

    if (HAL_OSPI_Receive(&hospi1, buffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return 0;
    }

    return 1;
}

/* Включить Memory-Mapped режим */
void QSPI_EnableMemoryMapped(void)
{
    OSPI_RegularCmdTypeDef sCommand = {0};
    sCommand.OperationType = HAL_OSPI_OPTYPE_READ_CFG;
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;
    sCommand.Instruction = QSPI_CMD_FAST_READ;
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    sCommand.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
    sCommand.DataMode = HAL_OSPI_DATA_1_LINE;
    sCommand.NbData = 0;
    sCommand.DummyCycles = 8;
    sCommand.DQSMode = HAL_OSPI_DQS_DISABLE;
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    	ErrorCode = 17;
        Error_Handler();
    }

    sCommand.OperationType = HAL_OSPI_OPTYPE_WRITE_CFG;
    sCommand.Instruction = QSPI_CMD_PAGE_PROGRAM;
    sCommand.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    sCommand.DummyCycles = 0;
    sCommand.DataMode = HAL_OSPI_DATA_1_LINE;

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    	ErrorCode = 18;
        Error_Handler();
    }

    OSPI_MemoryMappedTypeDef sMemMappedCfg = {0};
    sMemMappedCfg.TimeOutActivation = HAL_OSPI_TIMEOUT_COUNTER_DISABLE;

    if (HAL_OSPI_MemoryMapped(&hospi1, &sMemMappedCfg) != HAL_OK) {
    	ErrorCode = 19;
        Error_Handler();
    }
}
