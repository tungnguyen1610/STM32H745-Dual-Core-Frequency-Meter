#include "gc9a01.h"
#include "main.h"
#include "stdio.h"
#include <string.h>
#include "font8x8.h"
 #include <cmsis_os2.h>
 
extern SPI_HandleTypeDef GC9A01_SPI;
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF

// Adjusted constants for centered display on a circle
#define HORIZONTAL_PADDING 30 // Push content inward to avoid clipped corners
#define START_Y_OFFSET     60 // Start further down from the top clipped area
#define SEPARATOR_THICKNESS 2
#define LINE_HEIGHT_SPACED (FONT_HEIGHT + 4) // 8 + 4 = 12 pixels per line

#if USE_DMA
static volatile uint8_t tx_busy = 0;
#endif

// === INTERNAL GPIO CONTROL ===
static void GC9A01_Select(void)   { HAL_GPIO_WritePin(GC9A01_CS_PORT, GC9A01_CS_PIN, GPIO_PIN_RESET); }
static void GC9A01_Unselect(void) { HAL_GPIO_WritePin(GC9A01_CS_PORT, GC9A01_CS_PIN, GPIO_PIN_SET);   }
static void GC9A01_DC_Command(void) { HAL_GPIO_WritePin(GC9A01_DC_PORT, GC9A01_DC_PIN, GPIO_PIN_RESET); }
static void GC9A01_DC_Data(void)    { HAL_GPIO_WritePin(GC9A01_DC_PORT, GC9A01_DC_PIN, GPIO_PIN_SET);   }

// === RESET ===
void GC9A01_Reset(void)
{
    HAL_GPIO_WritePin(GC9A01_RST_PORT, GC9A01_RST_PIN, GPIO_PIN_RESET);
    //Change from Hal_Delay();
	osDelay(20);
    HAL_GPIO_WritePin(GC9A01_RST_PORT, GC9A01_RST_PIN, GPIO_PIN_SET);
    osDelay(120);
}

// === COMMAND/DATA ===
void GC9A01_WriteCommand(uint8_t cmd)
{
    GC9A01_Select();
    GC9A01_DC_Command();
    HAL_SPI_Transmit(&GC9A01_SPI, &cmd, 1, GC9A01_SPI_TIMEOUT);
    GC9A01_Unselect();
}

// Blocking write for single byte
void GC9A01_WriteData(uint8_t data)
{
    GC9A01_Select();
    GC9A01_DC_Data();
    HAL_SPI_Transmit(&GC9A01_SPI, &data, 1, GC9A01_SPI_TIMEOUT);
    GC9A01_Unselect();
}
void GC9A01_WriteDataBuffer(uint8_t *data, uint32_t size)
{
	GC9A01_Select();
	GC9A01_DC_Data();
#if USE_DMA
	tx_busy = 1;
	HAL_SPI_Transmit_DMA(&GC9A01_SPI, data, size);
#else
	HAL_SPI_Transmit(&GC9A01_SPI, data, size, GC9A01_SPI_TIMEOUT);
	GC9A01_Unselect();
#endif
}

// === WRITE DATA BUFFER (DMA) ===
void GC9A01_WriteDataBuffer_DMA(uint8_t *data, uint32_t size)
{
#if USE_DMA
    // Wait for any previous DMA transfer
    while(tx_busy) HAL_Delay(1);

    tx_busy = 1;
    GC9A01_Select();
    GC9A01_DC_Data();
    HAL_SPI_Transmit_DMA(&GC9A01_SPI, data, size);
#else
    GC9A01_Select();
    GC9A01_DC_Data();
    HAL_SPI_Transmit(&GC9A01_SPI, data, size, GC9A01_SPI_TIMEOUT);
    GC9A01_Unselect();
#endif
}

// === DMA CALLBACK ===
#if USE_DMA
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi == &GC9A01_SPI) {
        GC9A01_Unselect();  // Deassert CS only after full transfer
        tx_busy = 0;
        GC9A01_FlushReady(); // Optional hook
    }
}
#endif

// === SET ADDRESS WINDOW ===
void GC9A01_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    GC9A01_WriteCommand(0x2A); // Column
    uint8_t col_data[] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    GC9A01_WriteDataBuffer_DMA(col_data, sizeof(col_data));

    GC9A01_WriteCommand(0x2B); // Row
    uint8_t row_data[] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    GC9A01_WriteDataBuffer_DMA(row_data, sizeof(row_data));

    GC9A01_WriteCommand(0x2C); // Write
}
// === DRAW PIXEL ===
void GC9A01_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
	GC9A01_SetAddressWindow(x, y, x, y);
	uint8_t data[] = {color >> 8, color & 0xFF};
	GC9A01_WriteDataBuffer(data, 2);
#if USE_DMA
	while (tx_busy);
#endif
}
// === FILL RECTANGLE (DMA safe) ===
void GC9A01_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    GC9A01_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    uint32_t size = w * h;
    uint8_t buf[128]; // Chunk buffer
    for(size_t i = 0; i < sizeof(buf); i += 2) {
        buf[i]   = color >> 8;
        buf[i+1] = color & 0xFF;
    }

    while(size > 0) {
        uint16_t chunk = (size > (sizeof(buf)/2)) ? (sizeof(buf)/2) : size;
        GC9A01_WriteDataBuffer_DMA(buf, chunk*2);

#if USE_DMA
        while(tx_busy) osDelay(1); // wait until chunk transmitted
#endif
        size -= chunk;
    }
}

// === FLUSH FOR LVGL ===
void GC9A01_Flush(const void *color_map, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    uint32_t size = (x2-x1+1) * (y2-y1+1) * 2;
    GC9A01_SetAddressWindow(x1, y1, x2, y2);

    GC9A01_WriteDataBuffer_DMA((uint8_t*)color_map, size);

#if USE_DMA
    while(tx_busy) osDelay(1); // wait for full frame
#else
    GC9A01_FlushReady();
#endif
}

// === OPTIONAL WEAK CALLBACK ===
__weak void GC9A01_FlushReady(void) { /* User hook */ }


// === DISPLAY INIT ===
void GC9A01_Init(void)
{
	GC9A01_Reset();

	GC9A01_WriteCommand(0xEF);
	GC9A01_WriteCommand(0xEB);
	GC9A01_WriteData(0x14);

	GC9A01_WriteCommand(0xFE);
	GC9A01_WriteCommand(0xEF);

	GC9A01_WriteCommand(0xEB);
	GC9A01_WriteData(0x14);

	GC9A01_WriteCommand(0x84);
	GC9A01_WriteData(0x40);

	GC9A01_WriteCommand(0x85);
	GC9A01_WriteData(0xFF);

	GC9A01_WriteCommand(0x86);
	GC9A01_WriteData(0xFF);

	GC9A01_WriteCommand(0x87);
	GC9A01_WriteData(0xFF);

	GC9A01_WriteCommand(0x88);
	GC9A01_WriteData(0x0A);

	GC9A01_WriteCommand(0x89);
	GC9A01_WriteData(0x21);

	GC9A01_WriteCommand(0x8A);
	GC9A01_WriteData(0x00);

	GC9A01_WriteCommand(0x8B);
	GC9A01_WriteData(0x80);

	GC9A01_WriteCommand(0x8C);
	GC9A01_WriteData(0x01);

	GC9A01_WriteCommand(0x8D);
	GC9A01_WriteData(0x01);

	GC9A01_WriteCommand(0x8E);
	GC9A01_WriteData(0xFF);

	GC9A01_WriteCommand(0x8F);
	GC9A01_WriteData(0xFF);


	GC9A01_WriteCommand(0xB6);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x20);

	GC9A01_WriteCommand(0x36);
	GC9A01_WriteData(0x00);//Set as normal orientation

	GC9A01_WriteCommand(0x3A);
	GC9A01_WriteData(0x05); // 16-bit color mode


	GC9A01_WriteCommand(0x90);
	GC9A01_WriteData(0x08);
	GC9A01_WriteData(0x08);
	GC9A01_WriteData(0x08);
	GC9A01_WriteData(0x08);

	GC9A01_WriteCommand(0xBD);
	GC9A01_WriteData(0x06);

	GC9A01_WriteCommand(0xBC);
	GC9A01_WriteData(0x00);

	GC9A01_WriteCommand(0xFF);
	GC9A01_WriteData(0x60);
	GC9A01_WriteData(0x01);
	GC9A01_WriteData(0x04);

	GC9A01_WriteCommand(0xC3);
	GC9A01_WriteData(0x13);
	GC9A01_WriteCommand(0xC4);
	GC9A01_WriteData(0x13);

	GC9A01_WriteCommand(0xC9);
	GC9A01_WriteData(0x22);

	GC9A01_WriteCommand(0xBE);
	GC9A01_WriteData(0x11);

	GC9A01_WriteCommand(0xE1);
	GC9A01_WriteData(0x10);
	GC9A01_WriteData(0x0E);

	GC9A01_WriteCommand(0xDF);
	GC9A01_WriteData(0x21);
	GC9A01_WriteData(0x0c);
	GC9A01_WriteData(0x02);

	GC9A01_WriteCommand(0xF0);
	GC9A01_WriteData(0x45);
	GC9A01_WriteData(0x09);
	GC9A01_WriteData(0x08);
	GC9A01_WriteData(0x08);
	GC9A01_WriteData(0x26);
	GC9A01_WriteData(0x2A);

	GC9A01_WriteCommand(0xF1);
	GC9A01_WriteData(0x43);
	GC9A01_WriteData(0x70);
	GC9A01_WriteData(0x72);
	GC9A01_WriteData(0x36);
	GC9A01_WriteData(0x37);
	GC9A01_WriteData(0x6F);


	GC9A01_WriteCommand(0xF2);
	GC9A01_WriteData(0x45);
	GC9A01_WriteData(0x09);
	GC9A01_WriteData(0x08);
	GC9A01_WriteData(0x08);
	GC9A01_WriteData(0x26);
	GC9A01_WriteData(0x2A);

	GC9A01_WriteCommand(0xF3);
	GC9A01_WriteData(0x43);
	GC9A01_WriteData(0x70);
	GC9A01_WriteData(0x72);
	GC9A01_WriteData(0x36);
	GC9A01_WriteData(0x37);
	GC9A01_WriteData(0x6F);

	GC9A01_WriteCommand(0xED);
	GC9A01_WriteData(0x1B);
	GC9A01_WriteData(0x0B);

	GC9A01_WriteCommand(0xAE);
	GC9A01_WriteData(0x77);

	GC9A01_WriteCommand(0xCD);
	GC9A01_WriteData(0x63);


	GC9A01_WriteCommand(0x70);
	GC9A01_WriteData(0x07);
	GC9A01_WriteData(0x07);
	GC9A01_WriteData(0x04);
	GC9A01_WriteData(0x0E);
	GC9A01_WriteData(0x0F);
	GC9A01_WriteData(0x09);
	GC9A01_WriteData(0x07);
	GC9A01_WriteData(0x08);
	GC9A01_WriteData(0x03);

	GC9A01_WriteCommand(0xE8);
	GC9A01_WriteData(0x34);

	GC9A01_WriteCommand(0x62);
	GC9A01_WriteData(0x18);
	GC9A01_WriteData(0x0D);
	GC9A01_WriteData(0x71);
	GC9A01_WriteData(0xED);
	GC9A01_WriteData(0x70);
	GC9A01_WriteData(0x70);
	GC9A01_WriteData(0x18);
	GC9A01_WriteData(0x0F);
	GC9A01_WriteData(0x71);
	GC9A01_WriteData(0xEF);
	GC9A01_WriteData(0x70);
	GC9A01_WriteData(0x70);

	GC9A01_WriteCommand(0x63);
	GC9A01_WriteData(0x18);
	GC9A01_WriteData(0x11);
	GC9A01_WriteData(0x71);
	GC9A01_WriteData(0xF1);
	GC9A01_WriteData(0x70);
	GC9A01_WriteData(0x70);
	GC9A01_WriteData(0x18);
	GC9A01_WriteData(0x13);
	GC9A01_WriteData(0x71);
	GC9A01_WriteData(0xF3);
	GC9A01_WriteData(0x70);
	GC9A01_WriteData(0x70);

	GC9A01_WriteCommand(0x64);
	GC9A01_WriteData(0x28);
	GC9A01_WriteData(0x29);
	GC9A01_WriteData(0xF1);
	GC9A01_WriteData(0x01);
	GC9A01_WriteData(0xF1);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x07);

	GC9A01_WriteCommand(0x66);
	GC9A01_WriteData(0x3C);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0xCD);
	GC9A01_WriteData(0x67);
	GC9A01_WriteData(0x45);
	GC9A01_WriteData(0x45);
	GC9A01_WriteData(0x10);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x00);

	GC9A01_WriteCommand(0x67);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x3C);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x01);
	GC9A01_WriteData(0x54);
	GC9A01_WriteData(0x10);
	GC9A01_WriteData(0x32);
	GC9A01_WriteData(0x98);

	GC9A01_WriteCommand(0x74);
	GC9A01_WriteData(0x10);
	GC9A01_WriteData(0x85);
	GC9A01_WriteData(0x80);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x00);
	GC9A01_WriteData(0x4E);
	GC9A01_WriteData(0x00);

	GC9A01_WriteCommand(0x98);
	GC9A01_WriteData(0x3e);
	GC9A01_WriteData(0x07);
	GC9A01_WriteCommand(0x35);
	GC9A01_WriteCommand(0x21);
	GC9A01_WriteCommand(0x11); // Sleep out
	osDelay(120);
	GC9A01_WriteCommand(0x29); // Display on
	osDelay(20);
	// Clear display to black
	GC9A01_FillRect(0, 0, 240, 240, 0x0000);
}

// Static buffer for 8x8 pixels (8 * 8 * 2 bytes/pixel = 128 bytes)
static uint8_t char_buffer[FONT_WIDTH * FONT_HEIGHT * 2];

/**
 * @brief Draws a single 8x8 character to the display.
 */
// Static buffer for 8x8 pixels (8 * 8 * 2 bytes/pixel = 128 bytes)
static uint8_t char_buffer[FONT_WIDTH * FONT_HEIGHT * 2];

void GC9A01_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t background_color)
{
    if (c < 32 || c > 126) c = ' ';

    const uint8_t *char_map = font8x8_basic[c];
    int screen_row = 0;

    // Standard Row Iteration: Top (0) to Bottom (7)
    for (int font_row = 0; font_row < FONT_HEIGHT; font_row++) { 
        uint8_t line_byte = char_map[font_row];
        
        int screen_col = 0;
        // Standard Bit Iteration: MSB-first (Left to Right)
        for (int font_col = 0; font_col < FONT_WIDTH; font_col++) {
            
            uint16_t pixel_color;

            // MSB-first logic: (0x80 >> font_col)
            if (line_byte & (0x01 << font_col)) { 
    		pixel_color = color;
			}else {
                pixel_color = background_color;
            }

            // Draw the pixel at the calculated screen position
            GC9A01_DrawPixel(x + screen_col, y + screen_row, pixel_color);
            
            screen_col++;
        }
        
        screen_row++;
    }
}


/**
 * @brief Draws a string of 8x8 characters.
 */
void GC9A01_WriteString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t background_color)
{
    while (*str) {
        GC9A01_DrawChar(x, y, *str++, color, background_color);
        x += FONT_WIDTH; // Move to the next horizontal position
        
        // Simple screen wrap protection (optional)
        if (x > GC9A01_WIDTH - FONT_WIDTH) break; 
    }
}
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF

#define MARGIN_X    10
#define START_Y     10
#define SEPARATOR_THICKNESS 2
#define FONT_SPACING (FONT_HEIGHT + 4) // Line height plus 4 pixels space

void GC9A01_DisplayStats(int ch0_freq, int ch1_freq, int ch2_freq, int ch3_freq)
{
    // Constants needed for centering
    const char *text = "HELLO WORLD";
    // Text length: 11 characters
    const uint16_t text_length = 11; 
    
    // Calculate required space in pixels
    // Assumes FONT_WIDTH = 8 (from your font8x8)
    uint16_t text_pixel_width = text_length * FONT_WIDTH; 
    
    // Calculate start X and Y positions
    // X = (Total Width / 2) - (Text Width / 2)
    uint16_t center_x = (GC9A01_WIDTH / 2) - (text_pixel_width / 2);
    // Y = (Total Height / 2) - (Font Height / 2)
    // Assumes FONT_HEIGHT = 8 (from your font8x8)
    uint16_t center_y = (GC9A01_HEIGHT / 2) - (FONT_HEIGHT / 2); 

    // --- 1. Clear Screen ---
    GC9A01_FillRect(0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, COLOR_BLACK); 

    // --- 2. Display Centered Text ---
    // Use COLOR_WHITE for text and COLOR_BLACK for background
    GC9A01_WriteString(center_x, center_y, text, COLOR_WHITE, COLOR_BLACK);
}

void GC9A01_DisplayUpdate(int ch0_freq, int ch1_freq, int ch2_freq, int ch3_freq)
{
    // 1. Clear Screen
    GC9A01_FillRect(0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, COLOR_BLACK);

    uint16_t current_y = START_Y_OFFSET;
    uint16_t separator_width = GC9A01_WIDTH - 2 * HORIZONTAL_PADDING;
    uint16_t text_color = COLOR_WHITE;
    uint16_t bg_color   = COLOR_BLACK;
    char buffer[50]; // Buffer for formatted strings
    
    // Calculate X position for horizontal centering of the separators
    uint16_t start_x = HORIZONTAL_PADDING;
    
    // --- Determine max text width for centering ---
    // The longest line is "External Timestamp Measurements" (31 characters)
    const uint16_t max_text_width = 31 * FONT_WIDTH; 
    
    // --- Line 1: Top Separator (Centered) ---
    GC9A01_FillRect(start_x, current_y, separator_width, SEPARATOR_THICKNESS, text_color);
    current_y += SEPARATOR_THICKNESS + LINE_HEIGHT_SPACED;

    // --- Line 2: Title (Centered) ---
    // Note: This title is 31 chars wide, so we place it centered
    GC9A01_WriteString(start_x, current_y, "Frequency Measurements", text_color, bg_color);
    current_y += LINE_HEIGHT_SPACED;

    // --- Line 3: Middle Separator (Centered) ---
    GC9A01_FillRect(start_x, current_y, separator_width, SEPARATOR_THICKNESS, text_color);
    current_y += SEPARATOR_THICKNESS + LINE_HEIGHT_SPACED; 

    GC9A01_WriteString(start_x, current_y, "Estimation", text_color, bg_color);
    current_y += LINE_HEIGHT_SPACED;

    // --- Line 4-7: Channel Data (Left-aligned relative to HORIZONTAL_PADDING) ---
    
    sprintf(buffer, "Ch0: %d Hz", ch0_freq);
    GC9A01_WriteString(start_x + FONT_WIDTH, current_y, buffer, text_color, bg_color); // Indent by one char
    current_y += LINE_HEIGHT_SPACED;

    sprintf(buffer, "Ch1: %d Hz", ch1_freq);
    GC9A01_WriteString(start_x + FONT_WIDTH, current_y, buffer, text_color, bg_color); // Indent by one char
    current_y += LINE_HEIGHT_SPACED;

    sprintf(buffer, "Ch2: %d Hz", ch2_freq);
    GC9A01_WriteString(start_x + FONT_WIDTH, current_y, buffer, text_color, bg_color); // Indent by one char
    current_y += LINE_HEIGHT_SPACED;

    sprintf(buffer, "Ch3: %d Hz", ch3_freq);
    GC9A01_WriteString(start_x + FONT_WIDTH, current_y, buffer, text_color, bg_color); // Indent by one char
    current_y += LINE_HEIGHT_SPACED;

    // --- Line 8: Bottom Separator (Centered) ---
    current_y += 5; 
    GC9A01_FillRect(start_x, current_y, separator_width, SEPARATOR_THICKNESS, text_color);
}
 