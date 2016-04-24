#pragma once

#include <stdlib.h>
#include <math.h>
#include "stm32f7xx_hal.h"
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_audio.h"
#include "ex08/stm32f7xx_it.h"
#include "ex08/sequencer.h"

#include "macros.h"
#include "clockconfig.h"

#include "synth/synth.h"
#include "synth/adsr.h"
#include "synth/biquad.h"
#include "synth/delay.h"
#include "synth/node_ops.h"
#include "synth/osc.h"
#include "synth/panning.h"

#define VOLUME 70
#define NUM_VOICES 4

#define AUDIO_DMA_BUFFER_SIZE 1024
#define AUDIO_DMA_BUFFER_SIZE2 (AUDIO_DMA_BUFFER_SIZE >> 1)
#define AUDIO_DMA_BUFFER_SIZE4 (AUDIO_DMA_BUFFER_SIZE >> 2)
#define AUDIO_DMA_BUFFER_SIZE8 (AUDIO_DMA_BUFFER_SIZE >> 3)

typedef enum {
	BUFFER_OFFSET_NONE = 0, BUFFER_OFFSET_HALF, BUFFER_OFFSET_FULL
} DMABufferState;

typedef struct {
	float osc1Gain;
	float osc2Gain;
	float detune;
	float impFreq;
	float filterCutoff;
	float filterQ;
	float feedback;
	float width;
	float attack;
	float decay;
	float sustain;
	float release;
	float string;
	uint16_t tempo;
	uint8_t osc1Fn;
	uint8_t osc2Fn;
	uint8_t filterType;
	uint8_t volume;
} SynthPreset;
