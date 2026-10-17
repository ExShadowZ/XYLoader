/*
 * DTP.h
 *
 *  Created on: Jun 11, 2026
 *      Author: ExShadow
 */

#ifndef __DTP_H
#define __DTP_H

#include "main.h"
#include "lcd.h"

// Количество строк в терминале
#define DTP_MAX_LINES 4  // Изменил на 5, т.к. шрифт 16px, экран 80px высотой -> 5 строк

// Максимальная длина строки (в символах)
#define DTP_MAX_LINE_LENGTH 20

// Инициализация терминала
void DTP_Init(void);

// Очистить весь экран и сбросить буфер
void DTP_Clear(void);

// Вывод строки с автоматическим переходом на новую строку
// color - цвет текста (используйте константы из lcd.h: RED, GREEN, WHITE, YELLOW и т.д.)
// format - форматированная строка (как в printf)
void DTP_PrintLine(uint16_t color, const char* format, ...);

// Вывод строки без перехода на новую строку (дописывает к последней строке)
void DTP_Print(uint16_t color, const char* format, ...);

// Прокрутка буфера вверх на одну строку
void DTP_Scroll(void);

// Установить цвет по умолчанию для последующих вызовов
void DTP_SetDefaultColor(uint16_t color);

// Получить текущий цвет по умолчанию
uint16_t DTP_GetDefaultColor(void);

#endif /* __DTP_H */
