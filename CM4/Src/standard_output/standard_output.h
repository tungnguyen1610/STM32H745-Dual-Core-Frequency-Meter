#ifndef STANDARD_OUTPUT_STANDRAD_OUTPUT
#define STANDARD_OUTPUT_STANDRAD_OUTPUT

#include "term_colors.h"
#include <stdint.h>
void MSG(const char *format, ...); // custom printf() replacement
void MSGchar(int c); // print a single character
void MSGraw(const char * str); // print a raw string
void MSGVariable(const uint32_t * data, uint16_t len); // print raw variable data
#endif /* STANDARD_OUTPUT_STANDRAD_OUTPUT */
