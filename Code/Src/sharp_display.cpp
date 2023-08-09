/*
 * sharp_display.cpp
 *
 *  Created on: May 5, 2023
 *      Author: alan
 */

#include "sharp_display.h"

#include "main.h"
//#include <string.h>
#include <cstring>
#include <stdlib.h>


#define SHARPMEM_BIT_WRITECMD (0x01) // 0x80 in LSB format
#define SHARPMEM_BIT_VCOM (0x02)     // 0x40 in LSB format
#define SHARPMEM_BIT_CLEAR (0x04)    // 0x20 in LSB format

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                    \
  {                                                                            \
    int16_t t = a;                                                             \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif
#ifndef _swap_uint16_t
#define _swap_uint16_t(a, b)                                                   \
  {                                                                            \
    uint16_t t = a;                                                            \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

/**************************************************************************
    Sharp Memory Display Connector
    -----------------------------------------------------------------------
    Pin   Function        Notes
    ===   ==============  ===============================
      1   VIN             3.3-5.0V (into LDO supply)
      2   3V3             3.3V out
      3   GND
      4   SCLK            Serial Clock
      5   MOSI            Serial Data Input
      6   CS              Serial Chip Select
      9   EXTMODE         COM Inversion Select (Low = SW clock/serial)
      7   EXTCOMIN        External COM Inversion Signal
      8   DISP            Display On(High)/Off(Low)

 **************************************************************************/

#define TOGGLE_VCOM                                                            \
  do {                                                                         \
    _sharpmem_vcom = _sharpmem_vcom ? 0x00 : SHARPMEM_BIT_VCOM;                \
  } while (0);


// 1<<n is a costly operation on AVR -- table usu. smaller & faster
static const uint8_t set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,  (uint8_t)~2,  (uint8_t)~4,
                                      (uint8_t)~8,  (uint8_t)~16, (uint8_t)~32,
                                      (uint8_t)~64, (uint8_t)~128};


sharp_display::sharp_display(GPIO_TypeDef *_cs_port, uint16_t _cs_pin, SPI_HandleTypeDef* _hspi1,
		uint16_t w, uint16_t h)
{
	sharpmem_buffer = (uint8_t *)malloc((w * h) / 8);;
	cs_port = _cs_port;
	cs_pin = _cs_pin;
	spi = _hspi1;
	width = w;
	height = h;
	_sharpmem_vcom = SHARPMEM_BIT_VCOM;
	rotation = 0;
}


/**************************************************************************/
/*!
    @brief Draws a single pixel in image buffer

    @param[in]  x
                The x position (0 based)
    @param[in]  y
                The y position (0 based)
    @param color The color to set:
    * **0**: Black
    * **1**: White
*/
/**************************************************************************/
void sharp_display::drawPixel(int16_t x, int16_t y, uint16_t color)
{
	if ((x < 0) || (x >= width) || (y < 0) || (y >= height))
		return;

	switch (rotation) {
	case 1:
		_swap_int16_t(x, y);
		x = width - 1 - x;
		break;
	case 2:
		x = width - 1 - x;
		y = height - 1 - y;
		break;
	case 3:
		_swap_int16_t(x, y);
		y = height - 1 - y;
		break;
	}

	if (color) {
		sharpmem_buffer[(y * width + x) / 8] |= set[x & 7];
	} else {
		sharpmem_buffer[(y * width + x) / 8] &= clr[x & 7];
	}
}

/**************************************************************************/
/*!
    @brief Gets the value (1 or 0) of the specified pixel from the buffer

    @param[in]  x
                The x position (0 based)
    @param[in]  y
                The y position (0 based)

    @return     1 if the pixel is enabled, 0 if disabled
*/
/**************************************************************************/
uint8_t sharp_display::getPixel(uint16_t x, uint16_t y)
{
	if ((x >= width) || (y >= height))
		return 0; // <0 test not needed, unsigned

	switch (rotation) {
	case 1:
		_swap_uint16_t(x, y);
		x = width - 1 - x;
		break;
	case 2:
		x = width - 1 - x;
		y = height - 1 - y;
		break;
	case 3:
		_swap_uint16_t(x, y);
		y = height - 1 - y;
		break;
	}

	return sharpmem_buffer[(y * width + x) / 8] & set[x & 7] ? 1 : 0;
}

/**************************************************************************/
/*!
    @brief Clears the screen
*/
/**************************************************************************/
void sharp_display::clearDisplay()
{
	memset(sharpmem_buffer, 0xff, (width * height) / 8);

	//spidev->beginTransaction();
	// Send the clear screen command rather than doing a HW refresh (quicker)
	//digitalWrite(_cs, HIGH);
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

	uint8_t clear_data[2] = {(uint8_t)(_sharpmem_vcom | SHARPMEM_BIT_CLEAR), 0x00};
	//spidev->transfer(clear_data, 2);
	HAL_SPI_Transmit(spi, clear_data, 2, 100);

	TOGGLE_VCOM;
	//digitalWrite(_cs, LOW);
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
	//spidev->endTransaction();
}

/**************************************************************************/
/*!
    @brief Renders the contents of the pixel buffer on the LCD
*/
/**************************************************************************/
void sharp_display::refresh(void)
{
	uint16_t i, currentline;

//	spidev->beginTransaction();
	// Send the write command
//	digitalWrite(_cs, HIGH);
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

//	spidev->transfer(_sharpmem_vcom | SHARPMEM_BIT_WRITECMD);
	uint8_t tmp = _sharpmem_vcom | SHARPMEM_BIT_WRITECMD;
	HAL_SPI_Transmit(spi, &tmp, 1, 100);
	TOGGLE_VCOM;

	uint8_t bytes_per_line = width / 8;
	uint16_t totalbytes = (width * height) / 8;

	for (i = 0; i < totalbytes; i += bytes_per_line) {
		uint8_t line[bytes_per_line + 2];

		// Send address byte
		currentline = ((i + 1) / (width / 8)) + 1;
		line[0] = currentline;
		// copy over this line
		memcpy(line + 1, sharpmem_buffer + i, bytes_per_line);
		// Send end of line
		line[bytes_per_line + 1] = 0x00;
		// send it!
//		spidev->transfer(line, bytes_per_line + 2);
		HAL_SPI_Transmit(spi, line, bytes_per_line + 2, 100);
	}

	// Send another trailing 8 bits for the last line
//	spidev->transfer(0x00);
	tmp = 0x00;
	HAL_SPI_Transmit(spi, &tmp, 1, 100);
//	digitalWrite(_cs, LOW);
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
//	spidev->endTransaction();
}

/**************************************************************************/
/*!
    @brief Clears the display buffer without outputting to the display
*/
/**************************************************************************/
void sharp_display::clearDisplayBuffer()
{
	memset(sharpmem_buffer, 0xff, (width * height) / 8);
}

/**************************************************************************/
/*!
   @brief      Draw a PROGMEM-resident 1-bit image at the specified (x,y)
   position, using the specified foreground color (unset bits are transparent).
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void sharp_display::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
		int16_t w, int16_t h, uint16_t color)
{
	int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
	uint8_t byte = 0;

	for (int16_t j = 0; j < h; j++, y++) {
		for (int16_t i = 0; i < w; i++) {
			if (i & 7)
				byte <<= 1;
			else
				byte = bitmap[j * byteWidth + i / 8];
			if (byte & 0x80)
				sharp_display::drawPixel(x + i, y, color);
		}
	}
}

/**************************************************************************/
/*!
   @brief      Draw a PROGMEM-resident 1-bit image at the specified (x,y)
   position, using the specified foreground color (unset bits are transparent).
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void sharp_display::drawBitmap(int16_t *x, int16_t y, const bitmap_t *bitmap)
{
	int16_t w = bitmap->width;
	int16_t h = bitmap->height;
	int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
	uint8_t byte = 0;

	//  startWrite();
	for (int16_t j = 0; j < h; j++, y++) {
		for (int16_t i = 0; i < w; i++) {
			if (i & 7)
				byte <<= 1;
			else
				byte = bitmap->bitmap[j * byteWidth + i / 8];
			if (byte & 0x80)
				sharp_display::drawPixel(*x + i, y, 0);
		}
	}
	*x = *x + w;
	//  endWrite();
}

void sharp_display::clearBlock(int16_t x, int16_t y, int16_t w, int16_t h)
{
	for (int16_t j = 0; j < h; j++, y++)
		for (int16_t i = 0; i < w; i++)
			sharp_display::drawPixel(x + i, y, 1);
}

void sharp_display::drawString(int16_t x, int16_t y, const bitmap_t *bitmap, char *s)
{
	int i = 0;
	while (s[i] != '\0') {
		if ((s[i] >= 'A') && (s[i] <= 'Z'))
			drawBitmap(&x, y, &bitmap[alpha_idx + s[i] - 'A']);
		else if ((s[i] >= '0') && (s[i] <= '9'))
			drawBitmap(&x, y, &bitmap[number_idx + s[i] - '0']);
		else if (s[i] == ':')
			drawBitmap(&x, y, &bitmap[colon_idx]);
		else if (s[i] == '.')
			drawBitmap(&x, y, &bitmap[period_idx]);
		i++;
	}
}
