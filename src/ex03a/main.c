#include <math.h>
#include "ex03a/main.h"
#include "gui/bt_blackangle64_16.h"
#include "macros.h"

static void touchScreenError();
static void initAppGUI();
static void drawGUI();
static void updateAudioBuffer();

const float ct_synth_notes[] = {
		// c1
		32.70319566f, 34.64782887f, 36.70809599f, 38.89087297f, 41.20344461f,
		43.65352893f, 46.24930284f, 48.99942950f, 51.91308720f, 55.00000000f,
		58.27047019f, 61.73541266f,
		// c2
		65.40639133f, 69.29565774f, 73.41619198f, 77.78174593f, 82.40688923f,
		87.30705786f, 92.49860568f, 97.99885900f, 103.82617439f, 110.00000000f,
		116.54094038f, 123.47082531f,
		// c3
		130.81278265f, 138.59131549f, 146.83238396f, 155.56349186f,
		164.81377846f, 174.61411572f, 184.99721136f, 195.99771799f,
		207.65234879f, 220.00000000f, 233.08188076f, 246.94165063f,
		// c4
		261.62556530f, 277.18263098f, 293.66476792f, 311.12698372f,
		329.62755691f, 349.22823143f, 369.99442271f, 391.99543598f,
		415.30469758f, 440.00000000f, 466.16376152f, 493.88330126f,
		// c5
		523.25113060f, 554.36526195f, 587.32953583f, 622.25396744f,
		659.25511383f, 698.45646287f, 739.98884542f, 783.99087196f,
		830.60939516f, 880.00000000f, 932.32752304f, 987.76660251f,
		// c6
		1046.50226120f, 1108.73052391f, 1174.65907167f, 1244.50793489f,
		1318.51022765f, 1396.91292573f, 1479.97769085f, 1567.98174393f,
		1661.21879032f, 1760.00000000f, 1864.65504607f, 1975.53320502f,
		// c7
		2093.00452240f, 2217.46104781f, 2349.31814334f, 2489.01586978,
		2637.02045530f, 2793.82585146f, 2959.95538169f, 3135.96348785,
		3322.43758064f, 3520.00000000f, 3729.31009214f, 3951.06641005,
		// c8
		4186.00904481f, 4434.92209563f, 4698.63628668f, 4978.03173955f,
		5274.04091061f, 5587.65170293f, 5919.91076339f, 6271.92697571f,
		6644.87516128f, 7040.00000000f, 7458.62018429f, 7902.13282010f };

TIM_HandleTypeDef TimHandle;

static TS_StateTypeDef rawTouchState;
static GUITouchState touchState;

static SpriteSheet dialSheet = { .pixels = bt_blackangle64_16_argb8888,
		.spriteWidth = 64, .spriteHeight = 64, .numSprites = 16, .format =
		CM_ARGB8888 };

static GUI *gui;

static __IO DMABufferState bufferState = BUFFER_OFFSET_NONE;
static uint8_t audioBuf[AUDIO_DMA_BUFFER_SIZE];

static Oscillator osc = { .phase = 0.0f, .freq = HZ_TO_RAD(22050.0f),
		.modPhase = 0.0f, .modAmp = 4.0f, .type = 0 };

int main() {
	CPU_CACHE_Enable();
	HAL_Init();
	SystemClock_Config();
	BSP_LCD_Init();
	if (BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize()) == TS_OK) {
		BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER, LCD_FRAME_BUFFER);
		BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER);

		if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, VOLUME, SAMPLE_RATE)
				!= 0) {
			Error_Handler();
		}
		BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
		BSP_AUDIO_OUT_SetVolume(VOLUME);
		BSP_AUDIO_OUT_Play((uint16_t *) audioBuf, AUDIO_DMA_BUFFER_SIZE);

		initAppGUI();

		while (1) {
			drawGUI();
			HAL_Delay(30);
		}

		if (BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW) != AUDIO_OK) {
			Error_Handler();
		}
	} else {
		touchScreenError();
	}
	return 0;
}

static void touchScreenError() {
	BSP_LCD_SetFont(&LCD_DEFAULT_FONT);
	BSP_LCD_SetBackColor(LCD_COLOR_RED);
	BSP_LCD_Clear(LCD_COLOR_RED);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() / 2 - 24,
			(uint8_t *) "Touchscreen error!", CENTER_MODE);
	while (1) {
	}
}

static float mix(float a, float b, float t) {
	return a + (b - a) * t;
}

static float mapValue(float x, float a, float b, float c, float d) {
	return mix(c, d, (x - a) / (b - a));
}

static void setVolume(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	BSP_AUDIO_OUT_SetVolume((uint8_t) (db->value * 100));
}

static void setOscFreq(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	//osc.freq = HZ_TO_RAD(mix(50.f, 1760.f, db->value));
	osc.freq = HZ_TO_RAD(ct_synth_notes[(uint8_t )(db->value * 48.f)]);
}

static void setModAmp(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	osc.modAmp = db->value * 16.f;
}

static void setOscType(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	osc.type = (uint8_t) (db->value * 5);
	//db->value = osc.type * 0.2;
}

static void initAppGUI() {
	gui = initGUI(4, &UI_FONT, LCD_COLOR_BLACK, LCD_COLOR_LIGHTGRAY);
	gui->items[0] = guiDialButton(0, "Volume", 10, 10, (float) VOLUME / 100.0f,
			0.025f, &dialSheet, setVolume);
	gui->items[1] = guiDialButton(1, "Freq", 80, 10, osc.freq / 5000.0f, 0.025f,
			&dialSheet, setOscFreq);
	gui->items[2] = guiDialButton(2, "Mod Amp", 150, 10, 0.0f, 0.025f,
			&dialSheet, setModAmp);
	gui->items[3] = guiDialButton(2, "Type", 220, 10, 0.0f, 0.025f, &dialSheet,
			setOscType);
}

static void drawGUI() {
	BSP_LCD_Clear(gui->bgColor);
	uint16_t w = MIN(BSP_LCD_GetXSize(), AUDIO_DMA_BUFFER_SIZE2);
	uint16_t h = BSP_LCD_GetYSize() / 2;
	int16_t *ptr = (int16_t*) &audioBuf[0];
	for (uint16_t x = 0; x < w; x++) {
		float y = (float) (ptr[x]) / (32767.0 * 1.1f);
		//BSP_LCD_DrawPixel(x, (uint16_t) (h + h * y), LCD_COLOR_CYAN);
		uint32_t col = 0x7f00ff | (uint32_t)((fabs(y) * 0.9f + 0.1f) * 255) << 24;
		BSP_LCD_SetTextColor(col);
		if (y >= 0.0f) {
			BSP_LCD_DrawVLine(x, h, (uint16_t) h * y);
		} else {
			BSP_LCD_DrawVLine(x, (uint16_t) (h + h * y), (uint16_t) h * fabs(y));
		}
	}
	BSP_TS_GetState(&rawTouchState);
	guiUpdateTouch(&rawTouchState, &touchState);
	guiUpdate(gui, &touchState);
	guiForceRedraw(gui);
	guiDraw(gui);
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void) {
	bufferState = BUFFER_OFFSET_HALF;
	updateAudioBuffer();
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
	bufferState = BUFFER_OFFSET_FULL;
	updateAudioBuffer();
}

void BSP_AUDIO_OUT_Error_CallBack(void) {
	Error_Handler();
}

static void renderAudio(int16_t *ptr) {
	uint32_t len = AUDIO_DMA_BUFFER_SIZE8;
	while (len--) {
		osc.phase += osc.freq;
		if (osc.phase >= TAU) {
			osc.phase -= TAU;
		}
		osc.modPhase += HZ_TO_RAD(1.0f);
		if (osc.modPhase >= TAU) {
			osc.modPhase -= TAU;
		}
		float m = osc.modAmp * cosf(osc.modPhase);
		float y;
		switch (osc.type) {
		case 0: // sin
			y = sinf(osc.phase + m);
			break;
		case 1: // saw
			y = mapValue(osc.phase + m, 0.0f, TAU, -1.f, 1.f);
			break;
		case 2: // square
			y = (osc.phase + m) < PI ? -1.0f : 1.0f;
			break;
		case 3: // triangle
			if (osc.phase + m < PI) {
				y = mapValue(osc.phase + m, 0.0f, PI, -1.f, 1.f);
			} else {
				y = mapValue(osc.phase + m, PI, TAU, 1.f, -1.f);
			}
			break;
		case 4: // tri + sin
		default:
			if (osc.phase + m < PI) {
				y = mapValue(osc.phase + m, 0.0f, PI, -1.f, 1.f);
			} else {
				y = sinf(osc.phase + m);
			}
		}
		int16_t yi = ct_clamp16((int32_t) (y * 32767));
		*ptr++ = yi;
		*ptr++ = yi;
	}
}

static void updateAudioBuffer() {
	if (bufferState == BUFFER_OFFSET_HALF) {
		int16_t *ptr = (int16_t*) &audioBuf[0];
		renderAudio(ptr);
		bufferState = BUFFER_OFFSET_NONE;
	} else if (bufferState == BUFFER_OFFSET_FULL) {
		int16_t *ptr = (int16_t*) &audioBuf[0] + AUDIO_DMA_BUFFER_SIZE4;
		renderAudio(ptr);
		bufferState = BUFFER_OFFSET_NONE;
	}
}
