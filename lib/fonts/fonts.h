#ifndef CUBE_FONTS_H
#define CUBE_FONTS_H

#include <Adafruit_GFX.h>

/*
 * The GFX fonts the firmware uses, each defined once, in fonts.cpp.
 *
 * Include this rather than a font's own header. A font header defines its
 * data as `const` arrays, which in C++ are private to the file that
 * includes them, so every file including one compiled in its own copy:
 * three of FreeSansBold18pt7b at 5 KB each before this.
 */
extern const GFXfont FreeSansBold9pt7b;
extern const GFXfont FreeSansBold12pt7b;
extern const GFXfont FreeSansBold18pt7b;
extern const GFXfont LEMONMILK_Medium7pt7b;

#endif
