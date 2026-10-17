/*
 * sd.c
 * SD Card driver with FATFS
 */

#include "sd.h"
#include <string.h>

#ifdef SD_DEBUG
    #include "DTP.h"

    /* Вспомогательная функция для вывода на LCD */
    static void SD_Send_String(const char* str)
    {
        if (str) {
            DTP_PrintLine(RED, str);
        }
    }
#endif
// ====================================================
// ==================== ПЕРЕМЕННЫЕ ====================
// ====================================================

extern SD_HandleTypeDef hsd1;
extern char SDPath[4];

static FATFS SDFatFs;
static DIR current_dir;
static FIL current_file;
static uint8_t dir_open = 0;
static uint8_t file_open = 0;
static uint8_t sd_initialized = 0;
static uint8_t sd_card_present = 0;

// =========================================================
// ==================== ОШИБКИ ============================
// =========================================================

const char* SD_Get_Error_String(FRESULT res)
{
    switch (res) {
        case FR_OK:                 return "Success";
        case FR_DISK_ERR:           return "Disk error";
        case FR_INT_ERR:            return "Internal error";
        case FR_NOT_READY:          return "Drive not ready";
        case FR_NO_FILE:            return "No file";
        case FR_NO_PATH:            return "No path";
        case FR_INVALID_NAME:       return "Invalid name";
        case FR_DENIED:             return "Access denied";
        case FR_EXIST:              return "File exists";
        case FR_INVALID_OBJECT:     return "Invalid object";
        case FR_WRITE_PROTECTED:    return "Write protected";
        case FR_INVALID_DRIVE:      return "Invalid drive";
        case FR_NOT_ENABLED:        return "Not enabled";
        case FR_NO_FILESYSTEM:      return "No filesystem";
        case FR_MKFS_ABORTED:       return "MKFS aborted";
        case FR_TIMEOUT:            return "Timeout";
        case FR_LOCKED:             return "Locked";
        case FR_NOT_ENOUGH_CORE:    return "Not enough core";
        case FR_TOO_MANY_OPEN_FILES:return "Too many open files";
        default:                    return "Unknown error";
    }
}

#ifdef SD_DEBUG
void SD_Print_Error(FRESULT res)
{
    char msg[64];
    sprintf(msg, "FATFS Error: %s (%d)", SD_Get_Error_String(res), res);
    SD_Send_String(msg);
}
#endif

// =========================================================
// ==================== ВСПОМОГАТЕЛЬНЫЕ ====================
// =========================================================

/* Проверить, валидное ли состояние для операций */
static uint8_t SD_IsReady(void)
{
    return (sd_initialized && sd_card_present);
}

/* Закрыть все открытые ресурсы (при ошибке) */
static void SD_Cleanup(void)
{
    if (file_open) {
        f_close(&current_file);
        file_open = 0;
    }
    if (dir_open) {
        f_closedir(&current_dir);
        dir_open = 0;
    }
}

// =========================================================
// ==================== ИНИЦИАЛИЗАЦИЯ ======================
// =========================================================

uint8_t SD_IsDetected(void)
{
    if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER)
        return 1;
    if (hsd1.Instance->POWER & SDMMC_POWER_PWRCTRL)
        return 1;
    return 0;
}

uint8_t SD_IsInitialized(void)
{
    return sd_initialized;
}

uint8_t SD_Init(void)
{
    FRESULT res;
    HAL_SD_CardInfoTypeDef cardInfo;

    if (sd_initialized)
        return 1;

    if (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
        sd_card_present = 0;
        sd_initialized = 0;
        return 0;
    }

    sd_card_present = 1;

    /* Вывод информации о карте (только для дебага) */
    #ifdef SD_DEBUG
    if (HAL_SD_GetCardInfo(&hsd1, &cardInfo) == HAL_OK) {
        char msg[64];
        uint32_t card_size = (cardInfo.LogBlockNbr * cardInfo.LogBlockSize) / (1024 * 1024);
        sprintf(msg, "Card size: %lu MB", card_size);
        SD_Send_String(msg);
    }
    #endif

    res = f_mount(&SDFatFs, SDPath, 1);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        sd_initialized = 0;
        sd_card_present = 0;
        return 0;
    }

    sd_initialized = 1;
    return 1;
}

// =========================================================
// ==================== РАБОТА С ДИСКОМ ====================
// =========================================================

SD_Result_t SD_GetDiskInfo(SD_DiskInfo_t* info)
{
    FATFS* fs;
    DWORD free_clusters;
    FRESULT res;

    if (!SD_IsReady())
        return SD_NOT_INITIALIZED;

    res = f_getfree("", &free_clusters, &fs);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return SD_ERROR;
    }

    info->total_clusters = (fs->n_fatent - 2);
    info->free_clusters = free_clusters;
    info->sectors_per_cluster = fs->csize;
    info->bytes_per_sector = 512;

    info->total_mb = (uint32_t)(((uint64_t)info->total_clusters *
                                 info->sectors_per_cluster *
                                 info->bytes_per_sector) / (1024 * 1024));
    info->free_mb = (uint32_t)(((uint64_t)info->free_clusters *
                                info->sectors_per_cluster *
                                info->bytes_per_sector) / (1024 * 1024));

    return SD_OK;
}

SD_Result_t SD_Format(void)
{
    // TODO: Реализовать форматирование
    return SD_ERROR;
}

// =========================================================
// ==================== РАБОТА С ПАПКАМИ ===================
// =========================================================

uint8_t SD_Create_Directory(const char* path)
{
    FRESULT res;

    if (!SD_IsReady()) return 0;

    res = f_stat(path, NULL);
    if (res == FR_OK) return 1; // уже существует

    res = f_mkdir(path);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    return 1;
}

uint8_t SD_Delete_Directory(const char* path)
{
    FRESULT res;

    if (!SD_IsReady()) return 0;

    res = f_unlink(path);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    return 1;
}

uint8_t SD_Directory_Exists(const char* path)
{
    FILINFO fno;

    if (!SD_IsReady()) return 0;

    if (f_stat(path, &fno) == FR_OK && (fno.fattrib & AM_DIR))
        return 1;

    return 0;
}

uint8_t SD_Directory_IsEmpty(const char* path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;

    if (!SD_IsReady()) return 0;

    res = f_opendir(&dir, path);
    if (res != FR_OK) return 0;

    res = f_readdir(&dir, &fno);
    f_closedir(&dir);

    if (res != FR_OK) return 0;
    return (fno.fname[0] == 0);
}

// =========================================================
// ==================== НАВИГАЦИЯ ===========================
// =========================================================

uint8_t SD_Open_Directory(const char* path)
{
    FRESULT res;

    if (!SD_IsReady()) return 0;

    /* Закрываем предыдущую директорию */
    if (dir_open) {
        f_closedir(&current_dir);
        dir_open = 0;
    }

    res = f_opendir(&current_dir, path);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    dir_open = 1;
    return 1;
}

uint8_t SD_Close_Directory(void)
{
    if (dir_open) {
        f_closedir(&current_dir);
        dir_open = 0;
    }
    return 1;
}

uint8_t SD_Rewind_Directory(void)
{
    if (!dir_open) return 0;
    f_rewinddir(&current_dir);
    return 1;
}

uint8_t SD_Read_Directory(SD_FileInfo_t* file_info, uint8_t* file_found)
{
    FRESULT res;
    FILINFO fno;

    if (!SD_IsReady() || !dir_open) {
        *file_found = 0;
        return 0;
    }

    res = f_readdir(&current_dir, &fno);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        *file_found = 0;
        return 0;
    }

    if (fno.fname[0] == 0) {
        *file_found = 0;
        return 1;
    }

    strncpy(file_info->name, fno.fname, sizeof(file_info->name) - 1);
    file_info->name[sizeof(file_info->name) - 1] = '\0';
    file_info->size = fno.fsize;
    file_info->is_directory = (fno.fattrib & AM_DIR) ? 1 : 0;

    file_info->year = 1980 + ((fno.fdate >> 9) & 0x7F);
    file_info->month = (fno.fdate >> 5) & 0x0F;
    file_info->day = fno.fdate & 0x1F;
    file_info->hour = (fno.ftime >> 11) & 0x1F;
    file_info->minute = (fno.ftime >> 5) & 0x3F;
    file_info->second = (fno.ftime & 0x1F) * 2;

    *file_found = 1;
    return 1;
}

// =========================================================
// ==================== ФАЙЛЫ: СОЗДАНИЕ/УДАЛЕНИЕ ===========
// =========================================================

uint8_t SD_Create_File(const char* path)
{
    FIL file;
    FRESULT res;

    if (!SD_IsReady()) return 0;

    res = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);
    if (res == FR_EXIST) return 1; // уже существует
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    f_close(&file);
    return 1;
}

uint8_t SD_Delete_File(const char* path)
{
    FRESULT res;

    if (!SD_IsReady()) return 0;

    res = f_unlink(path);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    return 1;
}

uint8_t SD_File_Exists(const char* path)
{
    FILINFO fno;

    if (!SD_IsReady()) return 0;

    if (f_stat(path, &fno) == FR_OK && !(fno.fattrib & AM_DIR))
        return 1;

    return 0;
}

uint8_t SD_File_IsEmpty(const char* path)
{
    return (SD_Get_File_Size(path) == 0);
}

uint8_t SD_Clear_File(const char* path)
{
    FIL file;
    FRESULT res;

    if (!SD_IsReady()) return 0;

    res = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    f_close(&file);
    return 1;
}

// =========================================================
// ==================== ФАЙЛЫ: ОТКРЫТИЕ/ЗАКРЫТИЕ ==========
// =========================================================

uint8_t SD_Open_File(const char* path, SD_FileMode_t mode)
{
    FRESULT res;
    BYTE fatfs_mode;

    if (!SD_IsReady()) return 0;

    /* Закрываем предыдущий файл */
    if (file_open) {
        f_close(&current_file);
        file_open = 0;
    }

    switch (mode) {
        case SD_MODE_READ:       fatfs_mode = FA_READ; break;
        case SD_MODE_WRITE:      fatfs_mode = FA_CREATE_ALWAYS | FA_WRITE; break;
        case SD_MODE_APPEND:     fatfs_mode = FA_OPEN_APPEND | FA_WRITE; break;
        case SD_MODE_READ_WRITE: fatfs_mode = FA_READ | FA_WRITE; break;
        default: return 0;
    }

    res = f_open(&current_file, path, fatfs_mode);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    file_open = 1;
    return 1;
}

uint8_t SD_Close_File(void)
{
    if (file_open) {
        f_close(&current_file);
        file_open = 0;
    }
    return 1;
}

uint8_t SD_Is_File_Open(void)
{
    return file_open;
}

uint32_t SD_Get_File_Position(void)
{
    if (!file_open) return 0;
    return f_tell(&current_file);
}

uint8_t SD_Seek_File(uint32_t offset)
{
    if (!file_open) return 0;
    if (f_lseek(&current_file, offset) != FR_OK) return 0;
    return 1;
}

uint8_t SD_Rewind_File(void)
{
    return SD_Seek_File(0);
}

uint8_t SD_Truncate_File(void)
{
    if (!file_open) return 0;
    if (f_truncate(&current_file) != FR_OK) return 0;
    return 1;
}

// =========================================================
// ==================== ФАЙЛЫ: ИНФОРМАЦИЯ ==================
// =========================================================

uint32_t SD_Get_File_Size(const char* path)
{
    FILINFO fno;

    if (!SD_IsReady()) return 0;

    if (f_stat(path, &fno) != FR_OK) return 0;
    return fno.fsize;
}

uint32_t SD_Get_Open_File_Size(void)
{
    if (!file_open) return 0;
    return f_size(&current_file);
}

uint8_t SD_Get_File_Info(const char* path, SD_FileInfo_t* info)
{
    FILINFO fno;
    FRESULT res;

    if (!SD_IsReady() || !info) return 0;

    res = f_stat(path, &fno);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    strncpy(info->name, fno.fname, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->size = fno.fsize;
    info->is_directory = (fno.fattrib & AM_DIR) ? 1 : 0;

    info->year = 1980 + ((fno.fdate >> 9) & 0x7F);
    info->month = (fno.fdate >> 5) & 0x0F;
    info->day = fno.fdate & 0x1F;
    info->hour = (fno.ftime >> 11) & 0x1F;
    info->minute = (fno.ftime >> 5) & 0x3F;
    info->second = (fno.ftime & 0x1F) * 2;

    return 1;
}

// =========================================================
// ==================== ФАЙЛЫ: ЗАПИСЬ ======================
// =========================================================

uint8_t SD_Write_To_File(const uint8_t* data, uint32_t size, uint32_t* bytes_written)
{
    UINT bw;
    FRESULT res;

    if (!SD_IsReady() || !file_open || !data)
        return 0;

    res = f_write(&current_file, data, size, &bw);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    if (bytes_written) *bytes_written = bw;
    return (bw == size);
}

uint8_t SD_Write_String_To_File(const char* str)
{
    if (!str) return 0;
    return SD_Write_To_File((const uint8_t*)str, strlen(str), NULL);
}

uint8_t SD_Write_Line_To_File(const char* str)
{
    char buffer[512];

    if (!str) return 0;

    snprintf(buffer, sizeof(buffer), "%s\r\n", str);
    return SD_Write_To_File((const uint8_t*)buffer, strlen(buffer), NULL);
}

// =========================================================
// ==================== ФАЙЛЫ: ЧТЕНИЕ ======================
// =========================================================

uint8_t SD_Read_From_File(uint8_t* buffer, uint32_t buffer_size, uint32_t* bytes_read)
{
    UINT br;
    FRESULT res;

    if (!SD_IsReady() || !file_open || !buffer)
        return 0;

    res = f_read(&current_file, buffer, buffer_size, &br);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    if (bytes_read) *bytes_read = br;
    return 1;
}

uint8_t SD_Read_Line_From_File(char* buffer, uint32_t buffer_size, uint32_t* bytes_read)
{
    char ch;
    uint32_t pos = 0;

    if (!SD_IsReady() || !file_open || !buffer || buffer_size < 2)
        return 0;

    while (pos < buffer_size - 1) {
        if (!SD_Read_From_File((uint8_t*)&ch, 1, NULL))
            break;

        if (ch == '\n') break;
        if (ch != '\r') buffer[pos++] = ch;
    }

    buffer[pos] = '\0';
    if (bytes_read) *bytes_read = pos;

    return 1;
}

uint8_t SD_Read_Entire_File(const char* path, uint8_t* buffer, uint32_t buffer_size, uint32_t* bytes_read)
{
    FRESULT res;
    FIL file;
    UINT br;

    if (!SD_IsReady() || !buffer) return 0;

    res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    res = f_read(&file, buffer, buffer_size, &br);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        f_close(&file);
        return 0;
    }

    f_close(&file);

    if (bytes_read) *bytes_read = br;
    return 1;
}

// =========================================================
// ==================== УДОБНЫЕ ОБЁРТКИ ====================
// =========================================================

uint8_t SD_Write_String(const char* path, const char* str)
{
    uint8_t result = 0;

    if (SD_Open_File(path, SD_MODE_WRITE)) {
        result = SD_Write_String_To_File(str);
        SD_Close_File();
    }

    return result;
}

uint8_t SD_Append_String(const char* path, const char* str)
{
    uint8_t result = 0;

    if (SD_Open_File(path, SD_MODE_APPEND)) {
        result = SD_Write_String_To_File(str);
        SD_Close_File();
    }

    return result;
}

uint8_t SD_Copy_File(const char* src, const char* dst)
{
    uint8_t buffer[512];
    UINT bytes_read, bytes_written;
    FRESULT res;
    FIL fsrc, fdst;

    if (!SD_IsReady()) return 0;

    res = f_open(&fsrc, src, FA_READ);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    res = f_open(&fdst, dst, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        f_close(&fsrc);
        return 0;
    }

    while (1) {
        res = f_read(&fsrc, buffer, sizeof(buffer), &bytes_read);
        if (res != FR_OK || bytes_read == 0) break;

        res = f_write(&fdst, buffer, bytes_read, &bytes_written);
        if (res != FR_OK || bytes_written != bytes_read) break;
    }

    f_close(&fsrc);
    f_close(&fdst);

    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    return 1;
}

uint8_t SD_Rename_File(const char* old_name, const char* new_name)
{
    FRESULT res;

    if (!SD_IsReady()) return 0;

    res = f_rename(old_name, new_name);
    if (res != FR_OK) {
        SD_LOG_ERROR(res);
        return 0;
    }

    return 1;
}
