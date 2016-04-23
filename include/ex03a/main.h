#pragma once
#include <stdint.h>
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_ts.h"
#include "stm32746g_discovery_audio.h"
#include "ex03a/stm32f7xx_it.h"
#include "gui/gui.h"
#include "clockconfig.h"
#include "ct_math.h"

#define LCD_FRAME_BUFFER SDRAM_DEVICE_ADDR

#define VOLUME 50
#define SAMPLE_RATE 44100

#define AUDIO_DMA_BUFFER_SIZE 1024 // bytes
#define AUDIO_DMA_BUFFER_SIZE2 (AUDIO_DMA_BUFFER_SIZE >> 1) // 16bit words
#define AUDIO_DMA_BUFFER_SIZE4 (AUDIO_DMA_BUFFER_SIZE >> 2) // half buffer size (in 16bit words)
#define AUDIO_DMA_BUFFER_SIZE8 (AUDIO_DMA_BUFFER_SIZE >> 3) // number of stereo samples (16bit words)

#define TAU_RATE (TAU / (float)SAMPLE_RATE)
#define HZ_TO_RAD(freq) ((freq)*TAU_RATE)

typedef enum {
	BUFFER_OFFSET_NONE = 0, BUFFER_OFFSET_HALF, BUFFER_OFFSET_FULL
} DMABufferState;

typedef struct {
	float phase;
	float freq;
	float modPhase;
	float modAmp;
	uint8_t type;
} Oscillator;
