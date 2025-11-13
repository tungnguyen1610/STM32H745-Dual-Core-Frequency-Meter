/** 
 * 8x8 monochrome bitmap fonts for rendering
 * Author: Daniel Hepper <daniel@hepper.net>
 * 
 * License: Public Domain
 * 
 * Based on:
 * // Summary: font8x8.h
 * // 8x8 monochrome bitmap fonts for rendering
 * //
 * // Author:
 * //     Marcel Sondaar
 * //     International Business Machines (public domain VGA fonts)
 * //
 * // License:
 * //     Public Domain
 * 
 * Fetched from: http://dimensionalrift.homelinux.net/combuster/mos3/?p=viewsource&file=/modules/gfx/font8_8.asm
 **/

// Constant: font8x8_2500
// Contains an 8x8 font map for unicode points U+2500 - U+257F (box drawing)
#ifndef __FONT8X8_H
#define __FONT8X8_H

#include <stdint.h>

// Dimensions of the font
#define FONT_WIDTH  8
#define FONT_HEIGHT 8
extern const uint8_t font8x8_basic[128][8];
#endif