/*
 * bitmaps.h
 *
 *  Created on: Mar. 28, 2023
 *      Author: alan
 */

#ifndef INC_BITMAPS_H_
#define INC_BITMAPS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
	const uint8_t* bitmap;
	const int16_t width;
	const int16_t height;
} bitmap_t;


extern const uint8_t number_idx;
extern const uint8_t alpha_idx;
extern const uint8_t cad_idx;
extern const uint8_t colon_idx;
extern const uint8_t degree_idx;
extern const uint8_t dst_idx;
extern const uint8_t max_idx;
extern const uint8_t odo_idx;
extern const uint8_t period_idx;
extern const uint8_t rarrow_idx;
extern const uint8_t uarrow_idx;

//extern const unsigned char epd_bitmap_splash_128x128 [];
extern const bitmap_t bitmaps32[];


// 32 bit tall bitmaps
//extern const int epd_bitmap_20x32_LEN;
//extern const unsigned char* epd_bitmap_20x32[10];
//extern const int epd_bitmap_10x32_LEN;
//extern const unsigned char* epd_bitmap_10x32[3];
//extern const unsigned char* epd_bitmap_4x32[1];

#if 0
// 40 bit tall bitmaps
extern const int epd_bitmap_24x40_LEN;
extern const unsigned char* epd_bitmap_24x40[10];

// 60 bit tall bitmaps
extern const int epd_bitmap_36x60_LEN;
extern const unsigned char* epd_bitmap_36x60[10];

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 128)
extern const int epd_bitmap_allArray_LEN;
extern const unsigned char* epd_bitmap_allArray[2];
#endif

#ifdef __cplusplus
}
#endif

#endif /* INC_BITMAPS_H_ */
