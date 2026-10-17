/*
 * DTP.c
 *
 *  Created on: Jun 11, 2026
 *      Author: ExShadow
 */

#include "DTP.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// Структура для хранения строки с её цветом
typedef struct {
    char text[DTP_MAX_LINE_LENGTH + 1];
    uint16_t color;
} DTP_Line_t;

// Буфер строк с цветами
static DTP_Line_t line_buffer[DTP_MAX_LINES];
static uint16_t default_color = WHITE;
static uint8_t is_initialized = 0;

// Начальная позиция X для текста (отступ)
#define TEXT_X 4
// Начальная позиция Y для первой строки
#define TEXT_Y_START 4
// Высота строки (шрифт 16 пикселей)
#define TEXT_LINE_HEIGHT 16

// Вспомогательная функция для отрисовки всех строк
static void DTP_RenderAll(void)
{
    uint8_t i;
    uint16_t y_pos;
    uint16_t original_color;

    // Сохраняем текущий цвет
    original_color = POINT_COLOR;

    // Очищаем область вывода текста
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0,
                               ST7735Ctx.Width, DTP_MAX_LINES * TEXT_LINE_HEIGHT + TEXT_Y_START,
                               BLACK);

    // Рисуем каждую строку из буфера
    for (i = 0; i < DTP_MAX_LINES; i++)
    {
        // Пропускаем пустые строки (первый символ '\0')
        if (line_buffer[i].text[0] == '\0')
            continue;

        y_pos = TEXT_Y_START + (i * TEXT_LINE_HEIGHT);

        // Устанавливаем цвет строки
        POINT_COLOR = line_buffer[i].color;

        // Выводим текст
        LCD_ShowString(TEXT_X, y_pos, ST7735Ctx.Width - TEXT_X - 4,
                       TEXT_LINE_HEIGHT, 16, (uint8_t*)line_buffer[i].text);
    }

    // Восстанавливаем цвет
    POINT_COLOR = original_color;
}

// Инициализация терминала
void DTP_Init(void)
{
    uint8_t i;

    // Очищаем буфер
    for (i = 0; i < DTP_MAX_LINES; i++)
    {
        memset(line_buffer[i].text, 0, DTP_MAX_LINE_LENGTH + 1);
        line_buffer[i].color = default_color;
    }

    is_initialized = 1;
    default_color = WHITE;

    // Очищаем экран
    DTP_Clear();
}

// Очистить весь экран
void DTP_Clear(void)
{
    uint8_t i;

    // Очищаем буфер
    for (i = 0; i < DTP_MAX_LINES; i++)
    {
        memset(line_buffer[i].text, 0, DTP_MAX_LINE_LENGTH + 1);
        line_buffer[i].color = default_color;
    }

    // Очищаем экран
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0,
                               ST7735Ctx.Width, ST7735Ctx.Height,
                               BLACK);
}

// Прокрутка буфера вверх на одну строку
void DTP_Scroll(void)
{
    uint8_t i;

    // Сдвигаем все строки вверх: строка 1 -> строка 0, строка 2 -> строка 1, и т.д.
    for (i = 1; i < DTP_MAX_LINES; i++)
    {
        strcpy(line_buffer[i-1].text, line_buffer[i].text);
        line_buffer[i-1].color = line_buffer[i].color;
    }

    // Очищаем последнюю строку
    memset(line_buffer[DTP_MAX_LINES-1].text, 0, DTP_MAX_LINE_LENGTH + 1);
    line_buffer[DTP_MAX_LINES-1].color = default_color;

    // Перерисовываем
    DTP_RenderAll();
}

// Добавить строку в буфер (внутренняя функция)
static void DTP_AddLine(const char* text, uint16_t color)
{
    uint8_t i;
    uint8_t first_empty = DTP_MAX_LINES; // Индекс первой пустой строки

    // Ищем первую пустую строку
    for (i = 0; i < DTP_MAX_LINES; i++)
    {
        if (line_buffer[i].text[0] == '\0')
        {
            first_empty = i;
            break;
        }
    }

    // Если все строки заняты - делаем прокрутку
    if (first_empty == DTP_MAX_LINES)
    {
        // Прокручиваем
        for (i = 1; i < DTP_MAX_LINES; i++)
        {
            strcpy(line_buffer[i-1].text, line_buffer[i].text);
            line_buffer[i-1].color = line_buffer[i].color;
        }
        // Добавляем новую строку в последнюю позицию
        strncpy(line_buffer[DTP_MAX_LINES-1].text, text, DTP_MAX_LINE_LENGTH);
        line_buffer[DTP_MAX_LINES-1].text[DTP_MAX_LINE_LENGTH] = '\0';
        line_buffer[DTP_MAX_LINES-1].color = color;
    }
    else
    {
        // Добавляем в первую пустую строку
        strncpy(line_buffer[first_empty].text, text, DTP_MAX_LINE_LENGTH);
        line_buffer[first_empty].text[DTP_MAX_LINE_LENGTH] = '\0';
        line_buffer[first_empty].color = color;
    }

    // Перерисовываем
    DTP_RenderAll();
}

// Вывод строки с новой строки
void DTP_PrintLine(uint16_t color, const char* format, ...)
{
    char buffer[DTP_MAX_LINE_LENGTH + 1];
    va_list args;

    // Форматируем строку
    va_start(args, format);
    vsnprintf(buffer, DTP_MAX_LINE_LENGTH, format, args);
    va_end(args);

    // Добавляем в буфер с указанным цветом
    DTP_AddLine(buffer, color);
}

// Вывод строки без перехода на новую строку (дописывание к последней строке)
void DTP_Print(uint16_t color, const char* format, ...)
{
    char buffer[DTP_MAX_LINE_LENGTH + 1];
    char temp_buffer[DTP_MAX_LINE_LENGTH * 2 + 1];
    va_list args;
    uint8_t last_nonempty = 0;
    uint8_t i;

    // Находим последнюю непустую строку
    for (i = 0; i < DTP_MAX_LINES; i++)
    {
        if (line_buffer[i].text[0] != '\0')
            last_nonempty = i;
    }

    // Если нет ни одной строки, просто выводим новую
    if (last_nonempty == 0 && line_buffer[0].text[0] == '\0')
    {
        DTP_PrintLine(color, format, args);
        return;
    }

    // Форматируем строку
    va_start(args, format);
    vsnprintf(buffer, DTP_MAX_LINE_LENGTH, format, args);
    va_end(args);

    // Добавляем к последней непустой строке
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%s",
             line_buffer[last_nonempty].text, buffer);

    // Обновляем строку
    strncpy(line_buffer[last_nonempty].text, temp_buffer, DTP_MAX_LINE_LENGTH);
    line_buffer[last_nonempty].text[DTP_MAX_LINE_LENGTH] = '\0';
    line_buffer[last_nonempty].color = color;  // Обновляем цвет последней строки

    // Перерисовываем
    DTP_RenderAll();
}

// Установить цвет по умолчанию
void DTP_SetDefaultColor(uint16_t color)
{
    default_color = color;
}

// Получить цвет по умолчанию
uint16_t DTP_GetDefaultColor(void)
{
    return default_color;
}
