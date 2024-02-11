// Example header guard
#ifndef AUDIO_SD_H
#define AUDIO_SD_H

#include "stdio.h"
#include <string.h>
#include <stdarg.h> //for va_list var arg functions

#include "fatfs.h"

#define WAV_WRITE_SAMPLE_COUNT 2048

extern void myprintf(const char *fmt, ...);

void sd_card_init(void);
void sd_demo(void);
void dump_audio_content(uint8_t *data, uint16_t data_size);

#endif //AUDIO_SD_H