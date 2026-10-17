#ifndef SD_H
#define SD_H

#include "main.h"
#include "fatfs.h"
#include <stdint.h>
#include <stdio.h>

// ===========================================================
// ==================== НАСТРОЙКА РЕЖИМОВ ====================
// ===========================================================

// #define SD_DEBUG

// ===========================================================
// ==================== ОБРАБОТКА ОШИБОК ====================
// ===========================================================

/* Получить строковое описание ошибки FATFS (всегда доступно) */
const char* SD_Get_Error_String(FRESULT res);

#ifdef SD_DEBUG
    /* Вывести ошибку на LCD (только при включённом дебаге) */
    void SD_Print_Error(FRESULT res);

    /* Макрос для логирования ошибок */
    #define SD_LOG_ERROR(res) do { \
        if ((res) != FR_OK) { \
            SD_Print_Error(res); \
        } \
    } while(0)
#else
    /* Заглушка — компилятор выкинет этот код */
    static inline void SD_Print_Error(FRESULT res) { (void)res; }
    #define SD_LOG_ERROR(res) ((void)(res))
#endif

// ===========================================================
// ==================== СТРУКТУРЫ ============================
// ===========================================================

typedef enum {
    SD_MODE_READ = 0,
    SD_MODE_WRITE = 1,
    SD_MODE_APPEND = 2,
    SD_MODE_READ_WRITE = 3
} SD_FileMode_t;

typedef enum {
    SD_OK = 0,
    SD_ERROR = 1,
    SD_FILE_NOT_FOUND = 2,
    SD_ACCESS_DENIED = 3,
    SD_DISK_FULL = 4,
    SD_NOT_INITIALIZED = 5
} SD_Result_t;

typedef struct {
    char name[256];
    char full_path[512];
    uint32_t size;
    uint8_t is_directory;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} SD_FileInfo_t;

typedef struct {
    uint32_t total_clusters;
    uint32_t free_clusters;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sector;
    uint32_t total_mb;
    uint32_t free_mb;
} SD_DiskInfo_t;

// ===========================================================
// ==================== ПРОТОТИПЫ ============================
// ===========================================================

/* Инициализация */
uint8_t SD_Init(void);
uint8_t SD_IsInitialized(void);
uint8_t SD_IsDetected(void);

/* Работа с диском */
SD_Result_t SD_GetDiskInfo(SD_DiskInfo_t* info);
SD_Result_t SD_Format(void);

/* Работа с папками */
uint8_t SD_Create_Directory(const char* path);
uint8_t SD_Delete_Directory(const char* path);
uint8_t SD_Directory_Exists(const char* path);
uint8_t SD_Directory_IsEmpty(const char* path);
uint8_t SD_Open_Directory(const char* path);
uint8_t SD_Close_Directory(void);
uint8_t SD_Read_Directory(SD_FileInfo_t* file_info, uint8_t* file_found);
uint8_t SD_Rewind_Directory(void);

/* Работа с файлами */
uint8_t SD_Create_File(const char* path);
uint8_t SD_Delete_File(const char* path);
uint8_t SD_File_Exists(const char* path);
uint8_t SD_File_IsEmpty(const char* path);
uint8_t SD_Clear_File(const char* path);

uint8_t SD_Open_File(const char* path, SD_FileMode_t mode);
uint8_t SD_Close_File(void);
uint8_t SD_Is_File_Open(void);
uint32_t SD_Get_File_Position(void);
uint8_t SD_Seek_File(uint32_t offset);
uint8_t SD_Rewind_File(void);
uint8_t SD_Truncate_File(void);

uint32_t SD_Get_File_Size(const char* path);
uint32_t SD_Get_Open_File_Size(void);
uint8_t SD_Get_File_Info(const char* path, SD_FileInfo_t* info);

/* Чтение/запись */
uint8_t SD_Write_To_File(const uint8_t* data, uint32_t size, uint32_t* bytes_written);
uint8_t SD_Write_String_To_File(const char* str);
uint8_t SD_Write_Line_To_File(const char* str);
uint8_t SD_Read_From_File(uint8_t* buffer, uint32_t buffer_size, uint32_t* bytes_read);
uint8_t SD_Read_Line_From_File(char* buffer, uint32_t buffer_size, uint32_t* bytes_read);
uint8_t SD_Read_Entire_File(const char* path, uint8_t* buffer, uint32_t buffer_size, uint32_t* bytes_read);

/* Удобные обёртки */
uint8_t SD_Write_String(const char* path, const char* str);
uint8_t SD_Append_String(const char* path, const char* str);
uint8_t SD_Copy_File(const char* src, const char* dst);
uint8_t SD_Rename_File(const char* old_name, const char* new_name);

#endif
