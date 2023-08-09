/*
 * sharp_display.h
 *
 *  Created on: May 5, 2023
 *      Author: alan
 */

#ifndef INC_SHARP_DISPLAY_H_
#define INC_SHARP_DISPLAY_H_

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"
#include "bitmaps.h"

class sharp_display {
public:
	sharp_display(GPIO_TypeDef *cs_port, uint16_t cs_pin, SPI_HandleTypeDef *spi,
			uint16_t w = 128, uint16_t h = 128);
	void drawPixel(int16_t x, int16_t y, uint16_t color);
	uint8_t getPixel(uint16_t x, uint16_t y);
	void clearDisplay();
	void refresh(void);
	void clearDisplayBuffer();

	void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
			int16_t w, int16_t h, uint16_t color);
	void drawBitmap(int16_t *x, int16_t y, const bitmap_t* bitmap);
	void clearBlock(int16_t x, int16_t y, int16_t w, int16_t h);

	void drawString(int16_t x, int16_t y, const bitmap_t *bitmap, char *s);

private:
	uint8_t *sharpmem_buffer;
	GPIO_TypeDef *cs_port;
	uint16_t cs_pin;
	SPI_HandleTypeDef *spi;
	uint16_t width;
	uint16_t height;
	uint8_t _sharpmem_vcom;
	uint8_t rotation;
};

#endif /* INC_SHARP_DISPLAY_H_ */
