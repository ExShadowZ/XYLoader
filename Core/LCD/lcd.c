#include "lcd.h"
#include "font.h"
#include <string.h>
#include <stdio.h>
#include "main.h"

#ifdef TFT96
extern const unsigned char WeActStudiologo_160_80[];
#elif TFT18
extern const unsigned char WeActStudiologo_128_160[];
#endif

extern SPI_HandleTypeDef hspi4;
extern TIM_HandleTypeDef htim1;

// Пины LCD
#define LCD_RS_SET      HAL_GPIO_WritePin(LCD_WR_RS_GPIO_Port, LCD_WR_RS_Pin, GPIO_PIN_SET)
#define LCD_RS_RESET    HAL_GPIO_WritePin(LCD_WR_RS_GPIO_Port, LCD_WR_RS_Pin, GPIO_PIN_RESET)
#define LCD_CS_SET      HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define LCD_CS_RESET    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)

#define SPI_Drv         (&hspi4)
#define LCD_Brightness_timer   &htim1
#define LCD_Brightness_channel TIM_CHANNEL_2

// Прототипы
static int32_t lcd_init(void);
static int32_t lcd_gettick(void);
static int32_t lcd_writereg(uint8_t reg, uint8_t *pdata, uint32_t length);
static int32_t lcd_readreg(uint8_t reg, uint8_t *pdata);
static int32_t lcd_senddata(uint8_t *pdata, uint32_t length);
static int32_t lcd_recvdata(uint8_t *pdata, uint32_t length);

// Глобальные переменные
ST7735_Object_t st7735_pObj;
uint32_t st7735_id;
uint16_t POINT_COLOR = 0xFFFF;
uint16_t BACK_COLOR = BLACK;

ST7735_IO_t st7735_pIO = {
    lcd_init,
    NULL,
    0,
    lcd_writereg,
    lcd_readreg,
    lcd_senddata,
    lcd_recvdata,
    lcd_gettick
};

// МИНИМАЛЬНАЯ ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ
void LCD_Test(void)
{
#ifdef TFT96
    ST7735Ctx.Orientation = ST7735_ORIENTATION_LANDSCAPE_ROT180;
    ST7735Ctx.Panel = HannStar_Panel;
    ST7735Ctx.Type = ST7735_0_9_inch_screen;
#elif TFT18
    ST7735Ctx.Orientation = ST7735_ORIENTATION_PORTRAIT;
    ST7735Ctx.Panel = BOE_Panel;
    ST7735Ctx.Type = ST7735_1_8a_inch_screen;
#else
    #error "Unknown Screen"
#endif

    ST7735_RegisterBusIO(&st7735_pObj, &st7735_pIO);
    ST7735_LCD_Driver.Init(&st7735_pObj, ST7735_FORMAT_RBG565, &ST7735Ctx);
    ST7735_LCD_Driver.ReadID(&st7735_pObj, &st7735_id);

    // Включаем подсветку на 100%
    LCD_SetBrightness(100);

    // Очищаем экран черным
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, ST7735Ctx.Width, ST7735Ctx.Height, BLACK);

    // Выводим минимальную информацию
    uint8_t text[20];
    snprintf((char *)&text, sizeof(text), "LCD ID:0x%lX", st7735_id);
    LCD_ShowString(2, 2, ST7735Ctx.Width, 16, 16, text);
    snprintf((char *)&text, sizeof(text), "BOOT v1.0");
    LCD_ShowString(2, 20, ST7735Ctx.Width, 16, 16, text);
}

void LCD_SetBrightness(uint32_t Brightness)
{
    __HAL_TIM_SetCompare(LCD_Brightness_timer, LCD_Brightness_channel, Brightness);
}

uint32_t LCD_GetBrightness(void)
{
    return __HAL_TIM_GetCompare(LCD_Brightness_timer, LCD_Brightness_channel);
}

void LCD_Light(uint32_t Brightness_Dis, uint32_t time)
{
    uint32_t Brightness_Now = LCD_GetBrightness();
    if (Brightness_Now == Brightness_Dis || time == 0) return;

    float k = (float)(Brightness_Now - Brightness_Dis) / (0 - time);
    uint32_t tick = HAL_GetTick();

    while (1) {
        HAL_Delay(1);
        uint32_t time_now = HAL_GetTick() - tick;
        uint32_t set = (uint32_t)(time_now * k + Brightness_Now);
        LCD_SetBrightness(set);
        if (time_now >= time) break;
    }
}

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t mode)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint16_t x0 = x;
    uint16_t colortemp = POINT_COLOR;
    uint32_t h, w;

    uint16_t write[size][size == 12 ? 6 : 8];
    uint16_t count = 0;

    ST7735_GetXSize(&st7735_pObj, &w);
    ST7735_GetYSize(&st7735_pObj, &h);

    num = num - ' ';

    for (t = 0; t < size; t++) {
        if (size == 12)
            temp = asc2_1206[num][t];
        else
            temp = asc2_1608[num][t];

        for (t1 = 0; t1 < 8; t1++) {
            if (temp & 0x80)
                POINT_COLOR = (colortemp & 0xFF) << 8 | colortemp >> 8;
            else
                POINT_COLOR = (BACK_COLOR & 0xFF) << 8 | BACK_COLOR >> 8;

            write[count][t / 2] = POINT_COLOR;
            count++;
            if (count >= size) count = 0;

            temp <<= 1;
            y++;
            if (y >= h) { POINT_COLOR = colortemp; return; }
            if ((y - y0) == size) {
                y = y0;
                x++;
                if (x >= w) { POINT_COLOR = colortemp; return; }
                break;
            }
        }
    }

    ST7735_FillRGBRect(&st7735_pObj, x0, y0, (uint8_t *)&write, size == 12 ? 6 : 8, size);
    POINT_COLOR = colortemp;
}

void LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, uint8_t *p)
{
    uint8_t x0 = x;
    width += x;
    height += y;
    while ((*p <= '~') && (*p >= ' ')) {
        if (x >= width) {
            x = x0;
            y += size;
        }
        if (y >= height) break;
        LCD_ShowChar(x, y, *p, size, 0);
        x += size / 2;
        p++;
    }
}

// ============ SPI ФУНКЦИИ ============
static int32_t lcd_init(void)
{
    HAL_TIMEx_PWMN_Start(LCD_Brightness_timer, LCD_Brightness_channel);
    return ST7735_OK;
}

static int32_t lcd_gettick(void)
{
    return HAL_GetTick();
}

static int32_t lcd_writereg(uint8_t reg, uint8_t *pdata, uint32_t length)
{
    int32_t result = ST7735_OK;
    LCD_CS_RESET;
    LCD_RS_RESET;
    if (HAL_SPI_Transmit(SPI_Drv, &reg, 1, 100) != HAL_OK) result = ST7735_ERROR;
    LCD_RS_SET;
    if (length > 0 && HAL_SPI_Transmit(SPI_Drv, pdata, length, 500) != HAL_OK) result = ST7735_ERROR;
    LCD_CS_SET;
    return result;
}

static int32_t lcd_readreg(uint8_t reg, uint8_t *pdata)
{
    int32_t result = ST7735_OK;
    LCD_CS_RESET;
    LCD_RS_RESET;
    if (HAL_SPI_Transmit(SPI_Drv, &reg, 1, 100) != HAL_OK) result = ST7735_ERROR;
    LCD_RS_SET;
    if (HAL_SPI_Receive(SPI_Drv, pdata, 1, 500) != HAL_OK) result = ST7735_ERROR;
    LCD_CS_SET;
    return result;
}

static int32_t lcd_senddata(uint8_t *pdata, uint32_t length)
{
    int32_t result = ST7735_OK;
    LCD_CS_RESET;
    if (HAL_SPI_Transmit(SPI_Drv, pdata, length, 100) != HAL_OK) result = ST7735_ERROR;
    LCD_CS_SET;
    return result;
}

static int32_t lcd_recvdata(uint8_t *pdata, uint32_t length)
{
    int32_t result = ST7735_OK;
    LCD_CS_RESET;
    if (HAL_SPI_Receive(SPI_Drv, pdata, length, 500) != HAL_OK) result = ST7735_ERROR;
    LCD_CS_SET;
    return result;
}
