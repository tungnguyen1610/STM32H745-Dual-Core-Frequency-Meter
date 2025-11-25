#ifndef INC_GC9A01_H_
#define INC_GC9A01_H_

#include "main.h"
#include <stdint.h>
#include "font8x8.h"
// ==== USER CONFIGURATIONS ====
#define GC9A01_SPI           hspi5
#define GC9A01_SPI_TIMEOUT   500
#define USE_DMA              0  // Enable/disable DMA

#define GC9A01_CS_PORT       GPIOA
#define GC9A01_CS_PIN        GPIO_PIN_6

#define GC9A01_DC_PORT       GPIOA
#define GC9A01_DC_PIN        GPIO_PIN_4

#define GC9A01_RST_PORT      GPIOA
#define GC9A01_RST_PIN       GPIO_PIN_3

#define GC9A01_WIDTH  240
#define GC9A01_HEIGHT 240



void GC9A01_FlushReady(void);
// ==== API ====
void GC9A01_Init(void);
void GC9A01_Reset(void);
void GC9A01_WriteCommand(uint8_t cmd);
void GC9A01_WriteData(uint8_t data);
void GC9A01_WriteDataBuffer(uint8_t *data, uint32_t size);
void GC9A01_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void GC9A01_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void GC9A01_Flush(const void *color_map, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void GC9A01_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
// Function Prototypes for text drawing
void GC9A01_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t background_color);
void GC9A01_WriteString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t background_color);
void GC9A01_DisplayStats(int ch0_freq, int ch1_freq, int ch2_freq, int ch3_freq);
void  GC9A01_DisplayUpdate(int ch0_freq, int ch1_freq, int ch2_freq, int ch3_freq);
#endif
