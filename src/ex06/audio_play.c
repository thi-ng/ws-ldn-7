#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_lcd.h"
#include "ex04/audio_play.h"
#include "ctss/synth.h"
#include "ctss/adsr.h"
#include "ctss/iir.h"
#include "ctss/biquad.h"
#include "ctss/panning.h"
#include "ctss/osc.h"
#include "ctss/pluck.h"
#include "ctss/node_ops.h"
#include "ctss/delay.h"
#include "macros.h"
#include "ctss/ctss_math.h"
#include "gui/gui.h"
#include "gui/accentknob_pink48_8.h"
#include "gui/accentknob_cyan48_8.h"
#include "gui/bt_accent_pink48.h"
#include "ex04/thing64_16.h"

static __IO DMABufferState bufferState = BUFFER_OFFSET_NONE;

static CTSS_Synth synth;
static CTSS_DSPNodeHandler oscFunctions[] = { ctss_process_osc_spiral,
		ctss_process_osc_sin, ctss_process_osc_square, ctss_process_osc_saw,
		ctss_process_osc_sawsin, ctss_process_osc_tri, ctss_process_pluck,
		ctss_process_pluck };

static CTSS_BiquadType filterTypes[] = { LPF, HPF, BPF, PEQ };

static const uint8_t scale[] = { 36, 40, 43, 45, 55, 52, 48, 60, 52, 55, 45, 48,
		36, 43, 31, 33 };

uint32_t noteID = 0;
uint32_t voiceID = 0;
uint32_t lastNote = 0;

static SynthPreset synthPresets[] = { { .osc1Gain = 0, .osc2Gain = 0.2f,
		.detune = 0.5066f, .filterCutoff = 8000.0f, .filterQ = 0.9f, .feedback =
				0.5f, .width = 0.66f, .osc1Fn = 0, .osc2Fn = 0, .filterType = 0,
		.tempo = 150, .attack = 0.01f, .decay = 0.05f, .sustain = 0.25f,
		.release = 0.66f, .string = 0.02f, .volume = VOLUME } };

static SynthPreset *preset = &synthPresets[0];

static uint8_t audioBuf[AUDIO_DMA_BUFFER_SIZE];

static SpriteSheet dialSheet = { .pixels = accentknob_pink48_8_argb4444,
		.spriteWidth = 48, .spriteHeight = 48, .numSprites = 8, .format =
		CM_ARGB4444 };

static SpriteSheet buttonSheet = { .pixels = bt_accent_pink48_argb4444,
		.spriteWidth = 48, .spriteHeight = 24, .numSprites = 2, .format =
		CM_ARGB4444 };

static SpriteSheet cyanSheet = { .pixels = accentknob_cyan48_8_argb4444,
		.spriteWidth = 48, .spriteHeight = 48, .numSprites = 8, .format =
		CM_ARGB4444 };

uint8_t stepModes[][16] = { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
		15 }, { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 }, { 0, 1,
		2, 3, 7, 6, 5, 4, 8, 9, 10, 11, 15, 14, 13, 12 }, { 0, 1, 4, 8, 5, 2, 3,
		6, 9, 12, 13, 10, 7, 11, 14, 15 } };

typedef struct {
	uint8_t steps[16];
	uint8_t numSteps;
	uint8_t stepMode;
	uint8_t nextMode;
	uint8_t pos;
} Sequencer;

static Sequencer sequencer = { .steps = { 15, 15, 15, 15, 15, 15, 15, 15, 15,
		15, 15, 15, 15, 15, 15, 15 }, .numSteps = 16, .stepMode = 0, .pos = 0 };

static GUI *gui[2];
volatile uint32_t guiID = 1;

extern __IO GUITouchState touchState;

static void initSynth();
static void updateAudioBuffer();
static void patchStackPluckOsc(CTSS_DSPStack *stack, uint8_t oscID);
static void stackUseCommonOsc(CTSS_DSPStack *stack, uint8_t oscID);

static GUI* initSynthGUI();
static GUI* initSequencerGUI();

void toggleGUI();
static void triggerGUIredraw();
static void handleGUI(GUI *gui);
static void gui_cb_setSeqStep(GUIElement *e);
static void gui_cb_setSeqMode(GUIElement *e);
static void gui_cb_setVolume(GUIElement *e);
static void gui_cb_setOsc1Gain(GUIElement *e);
static void gui_cb_setOsc2Gain(GUIElement *e);
static void gui_cb_setOsc1Fn(GUIElement *e);
static void gui_cb_setOsc2Fn(GUIElement *e);
static void gui_cb_setDetune(GUIElement *e);
static void gui_cb_setFilterCutOff(GUIElement *e);
static void gui_cb_setFilterQ(GUIElement *e);
static void gui_cb_setFilterType(GUIElement *e);
static void gui_cb_setFeedback(GUIElement *e);
static void gui_cb_setTempo(GUIElement *e);
static void gui_cb_setWidth(GUIElement *e);
static void gui_cb_setAttack(GUIElement *e);
static void gui_cb_setDecay(GUIElement *e);
static void gui_cb_setSustain(GUIElement *e);
static void gui_cb_setRelease(GUIElement *e);
static void gui_cb_setString(GUIElement *e);

void demoAudioPlayback(void) {
	BSP_LCD_SelectLayer(0);
	BSP_LCD_Clear(UI_BG_COLOR);
	BSP_LCD_SetTransparency(0, 255);
	BSP_LCD_SetTransparency(1, 255);
	BSP_LCD_SelectLayer(1);
	gui[0] = initSynthGUI();
	gui[1] = initSequencerGUI();

	initSynth();
	if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, 70, SAMPLE_RATE) != 0) {
		Error_Handler();
	}
	BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
	BSP_AUDIO_OUT_SetVolume(preset->volume);
	BSP_AUDIO_OUT_Play((uint16_t *) audioBuf, AUDIO_DMA_BUFFER_SIZE);

	triggerGUIredraw();

	while (1) {
		handleGUI(gui[guiID]);
		HAL_Delay(16);
	}

	if (BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW) != AUDIO_OK) {
		Error_Handler();
	}
}

static GUI* initSynthGUI() {
	GUI *gui = initGUI(22, &UI_FONT, UI_BG_COLOR, UI_TEXT_COLOR);
	gui->items[0] = guiDialButton(0, "MASTER", 15, 10,
			(float) (preset->volume) / 80.0f,
			UI_SENSITIVITY, &dialSheet, gui_cb_setVolume);
	gui->items[1] = guiDialButton(1, "OSC1", 95, 10, preset->osc1Gain / 0.2f,
	UI_SENSITIVITY, &dialSheet, gui_cb_setOsc1Gain);
	gui->items[2] = guiDialButton(2, "OSC2", 175, 10, preset->osc2Gain / 0.2f,
	UI_SENSITIVITY, &dialSheet, gui_cb_setOsc2Gain);
	gui->items[3] = guiDialButton(3, "FREQ", 255, 10,
			preset->filterCutoff / 8000.0f,
			UI_SENSITIVITY, &dialSheet, gui_cb_setFilterCutOff);
	gui->items[4] = guiDialButton(4, "RES", 335, 10, 0.9f - preset->filterQ,
	UI_SENSITIVITY, &dialSheet, gui_cb_setFilterQ);
	gui->items[5] = guiDialButton(5, "DELAY", 415, 10, preset->feedback / 0.9f,
	UI_SENSITIVITY, &dialSheet, gui_cb_setFeedback);
	// OSC1 types
	gui->items[6] = guiToggleButton(6, NULL, 95, 90, 1.0f, &buttonSheet,
			gui_cb_setOsc1Fn);
	gui->items[7] = guiToggleButton(7, NULL, 95, 120, 2.0f, &buttonSheet,
			gui_cb_setOsc1Fn);
	gui->items[8] = guiToggleButton(8, NULL, 95, 150, 4.0f, &buttonSheet,
			gui_cb_setOsc1Fn);
	// OSC2 types
	gui->items[9] = guiToggleButton(9, NULL, 175, 90, 1.0f, &buttonSheet,
			gui_cb_setOsc2Fn);
	gui->items[10] = guiToggleButton(10, NULL, 175, 120, 2.0f, &buttonSheet,
			gui_cb_setOsc2Fn);
	gui->items[11] = guiToggleButton(11, NULL, 175, 150, 4.0f, &buttonSheet,
			gui_cb_setOsc2Fn);
	// Filter types
	gui->items[12] = guiToggleButton(12, NULL, 255, 90, 1.0f, &buttonSheet,
			gui_cb_setFilterType);
	gui->items[13] = guiToggleButton(13, NULL, 255, 120, 2.0f, &buttonSheet,
			gui_cb_setFilterType);
	// Tempo
	gui->items[14] = guiDialButton(14, "TEMPO", 15, 90,
			1.0f - preset->tempo / 900.0f,
			UI_SENSITIVITY, &dialSheet, gui_cb_setTempo);
	// Panning width
	gui->items[15] = guiDialButton(15, "WIDTH", 415, 90, preset->width,
	UI_SENSITIVITY, &dialSheet, gui_cb_setWidth);
	// Envelope
	gui->items[16] = guiDialButton(16, "ATTACK", 95, 180, preset->attack,
	UI_SENSITIVITY, &dialSheet, gui_cb_setAttack);
	gui->items[17] = guiDialButton(17, "DECAY", 175, 180, preset->decay,
	UI_SENSITIVITY, &dialSheet, gui_cb_setDecay);
	gui->items[18] = guiDialButton(18, "SUSTAIN", 255, 180, preset->sustain,
	UI_SENSITIVITY, &dialSheet, gui_cb_setSustain);
	gui->items[19] = guiDialButton(19, "RELEASE", 335, 180, preset->release,
	UI_SENSITIVITY, &dialSheet, gui_cb_setRelease);
	// OSC2 detune
	gui->items[20] = guiDialButton(20, "DETUNE", 335, 90,
			(preset->detune - 0.5f) * 50.0f,
			UI_SENSITIVITY, &dialSheet, gui_cb_setDetune);
	// Karplus-Strong param
	gui->items[21] = guiDialButton(21, "STRING", 415, 180,
			preset->string * 33.0f,
			UI_SENSITIVITY, &dialSheet, gui_cb_setString);
	guiForceRedraw(gui);
	return gui;
}

static GUI* initSequencerGUI() {
	GUI *gui = initGUI(20, &UI_FONT, UI_BG_COLOR, UI_TEXT_COLOR);
	for (uint32_t i = 0, y = 0, x = 0; i < 16; i++) {
		uint16_t xx = 208 + x * 64;
		uint16_t yy = 16 + y * 64;
		float v = sequencer.steps[i] / 15.f;
		gui->items[i] = guiDialButton(i, NULL, xx, yy, v,
		UI_SENSITIVITY, &dialSheet, gui_cb_setSeqStep);
		if (++x == 4) {
			x = 0;
			y++;
		}
	}
	for (uint32_t i = 0; i < 4; i++) {
		gui->items[16 + i] = guiRadioButton(16 + i, NULL, 144, 30 + i * 30,
				(float) i, &buttonSheet, gui_cb_setSeqMode);
	}
	return gui;
}

void toggleGUI() {
	guiID ^= 1;
	triggerGUIredraw();
}

static void triggerGUIredraw() {
	BSP_LCD_Clear(0xffffff);
	guiForceRedraw(gui[guiID]);
}

static void gui_cb_setSeqStep(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	sequencer.steps[e->id] = (uint8_t) (db->value * 15.0f + 0.5f);
}

static void gui_cb_setSeqMode(GUIElement *e) {
	if ((e->state & GUI_ONOFF_MASK) == GUI_ON) {
		uint8_t id = (uint8_t) (((PushButtonState *) (e->userData))->value);
		sequencer.nextMode = id;
		for (uint32_t i = 0; i < 4; i++) {
			gui[1]->items[16 + i]->state = GUI_OFF | GUI_DIRTY;
		}
		gui[1]->items[16 + id]->state = GUI_ON | GUI_DIRTY;
		gui[1]->redraw = 1;
	}
}

static void gui_cb_setVolume(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->volume = (uint8_t) (db->value * 90.0f);
	BSP_AUDIO_OUT_SetVolume(preset->volume);
}

static void gui_cb_setTempo(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->tempo = 150 + (uint16_t) ((1.0f - db->value) * 5.0f) * 150;
}

static void gui_cb_setOsc1Gain(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->osc1Gain = expf(4.5f * db->value - 3.5f) / 2.7f * 0.2f;
}

static void gui_cb_setOsc2Gain(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->osc2Gain = expf(4.5f * db->value - 3.5f) / 2.7f * 0.2f;
}

static void gui_cb_setDetune(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->detune = 0.5f + db->value * 0.02f;
}

static void gui_cb_setOsc1Fn(GUIElement *e) {
	uint8_t id = (uint8_t) ((PushButtonState *) (e->userData))->value;
	if (e->state & GUI_ON) {
		preset->osc1Fn |= id;
	} else {
		preset->osc1Fn &= ~id;
	}
}

static void gui_cb_setOsc2Fn(GUIElement *e) {
	uint8_t id = (uint8_t) ((PushButtonState *) (e->userData))->value;
	if (e->state & GUI_ON) {
		preset->osc2Fn |= id;
	} else {
		preset->osc2Fn &= ~id;
	}
}

static void gui_cb_setFilterCutOff(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->filterCutoff = 220.0f
			+ expf(4.5f * db->value - 3.5f) / 2.7f * 8000.0f;
}

static void gui_cb_setFilterQ(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->filterQ = 1.0f - db->value * 0.9f;
}

static void gui_cb_setFilterType(GUIElement *e) {
	uint8_t id = (uint8_t) ((PushButtonState *) (e->userData))->value;
	if (e->state & GUI_ON) {
		preset->filterType |= id;
	} else {
		preset->filterType &= ~id;
	}
}

static void gui_cb_setFeedback(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->feedback = db->value * 0.95f;
}

static void gui_cb_setWidth(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->width = db->value;
}

static void gui_cb_setAttack(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->attack = 0.002f + db->value * 0.99f;
}

static void gui_cb_setDecay(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->decay = 0.005f + db->value * 0.5f;
}

static void gui_cb_setSustain(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->sustain = db->value * 0.99f;
}

static void gui_cb_setRelease(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->release = 0.002f + db->value * 0.99f;
}

static void gui_cb_setString(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	preset->string = 0.001f + db->value * 0.03f;
}
static void handleGUI(GUI *gui) {
	if (touchState.touchUpdate) {
		getTouchState(&touchState);
		guiUpdate(gui, &touchState);
		guiDraw(gui);
		touchState.touchUpdate = 0;
		drawBitmapRaw(15, 180, 64, 64, thing64_16, CM_ARGB4444);
	} else if (gui->redraw) {
		guiDraw(gui);
		drawBitmapRaw(15, 180, 64, 64, thing64_16, CM_ARGB4444);
	}
}

static void patchStackPluckOsc(CTSS_DSPStack *stack, uint8_t oscID) {
	CTSS_DSPNode *pluck = NODE_ID(stack, oscID ? "bp" : "ap");
	CTSS_NodeOp4State *sum = NODE_ID_STATE(CTSS_NodeOp4State, stack, "s");
	if (oscID) {
		sum->bufC = pluck->buf;
	} else {
		sum->bufA = pluck->buf;
	}
	NODE_ID(stack, oscID ? "b" : "a")->flags = 0;
	pluck->flags = NODE_ACTIVE;
}

static void stackUseCommonOsc(CTSS_DSPStack *stack, uint8_t oscID) {
	CTSS_DSPNode *osc = NODE_ID(stack, oscID ? "b" : "a");
	CTSS_NodeOp4State *sum = NODE_ID_STATE(CTSS_NodeOp4State, stack, "s");
	if (oscID) {
		sum->bufC = osc->buf;
	} else {
		sum->bufA = osc->buf;
	}
	NODE_ID(stack, oscID ? "bp" : "ap")->flags = 0;
	osc->flags = NODE_ACTIVE;
}

static void initStack(CTSS_DSPStack *stack, float freq) {
	float f1 = HZ_TO_RAD(freq);
	float f2 = HZ_TO_RAD(freq * preset->detune);
	CTSS_DSPNode *env = ctss_adsr("e", synth.lfo[0], preset->attack,
			preset->decay, preset->release, 1.0f, preset->sustain);
	CTSS_DSPNode *osc1 = ctss_osc("a", oscFunctions[preset->osc1Fn], 0.0f, f1,
			preset->osc1Gain, 0.0f);
	CTSS_DSPNode *osc2 = ctss_osc("b", oscFunctions[preset->osc2Fn], 0.0f, f2,
			preset->osc2Gain, 0.0f);
	CTSS_DSPNode *pluck1 = ctss_osc_pluck("ap", f1, 0.005f, preset->osc1Gain,
			0.0f);
	pluck1->flags = 0;
	CTSS_DSPNode *pluck2 = ctss_osc_pluck("bp", f2, 0.005f, preset->osc2Gain,
			0.0f);
	pluck2->flags = 0;
	CTSS_DSPNode *sum = ctss_op4("s", osc1, env, osc2, env, ctss_process_madd);
	CTSS_DSPNode *filter = ctss_filter_biquad("f",
			filterTypes[preset->filterType], sum, preset->filterCutoff, 12.0f,
			preset->filterQ);
	CTSS_DSPNode *delay = ctss_delay("d", filter, (int) (SAMPLE_RATE * 0.375f),
			preset->feedback, 1);
	CTSS_DSPNode *pan = ctss_panning("p", delay, NULL, 0.5f);
	CTSS_DSPNode *nodes[] = { env, osc1, osc2, pluck1, pluck2, sum, filter,
			delay, pan };
	ctss_init_stack(stack);
	ctss_build_stack(stack, nodes, 9);
}

static void initSynth() {
	ctss_init(&synth, 3);
	synth.lfo[0] = ctss_osc("lfo1", ctss_process_osc_sin, 0.0f,
			HZ_TO_RAD(1 / 24.0f), 0.6f, 1.0f);
	synth.numLFO = 1;
	for (uint8_t i = 0; i < synth.numStacks; i++) {
		initStack(&synth.stacks[i], 110.0f);
	}
	ctss_collect_stacks(&synth);
}

uint8_t sequencer_get_step(Sequencer *seq) {
	uint8_t id = stepModes[seq->stepMode][seq->pos];
	uint8_t step = seq->steps[id];
	GUIElement *e = gui[1]->items[id];
	if (seq->pos < seq->numSteps - 1 || seq->stepMode == seq->nextMode) {
		e->state |= GUI_DIRTY;
		e->sprite = &cyanSheet;
	}
	e = gui[1]->items[stepModes[seq->stepMode][
			seq->pos ? seq->pos - 1 : seq->numSteps - 1]];
	e->state |= GUI_DIRTY;
	e->sprite = &dialSheet;
	gui[1]->redraw = 1;
	if (++seq->pos == seq->numSteps) {
		seq->pos = 0;
		seq->stepMode = seq->nextMode;
	}
	return step;
}

void renderAudio(int16_t *ptr, uint32_t frames) {
	if (HAL_GetTick() - lastNote >= preset->tempo) {
		lastNote = HAL_GetTick();
		noteID = sequencer_get_step(&sequencer);
		if (noteID) {
			CTSS_DSPStack *s = &synth.stacks[voiceID];
			CTSS_DSPNode *env = NODE_ID(s, "e");
			float f1 = ctss_notes[scale[noteID]];
			float f2 = ctss_notes[scale[noteID]] * preset->detune;
			if (preset->osc1Fn >= 6) {
				patchStackPluckOsc(s, 0);
				CTSS_DSPNode *a = NODE_ID(s, "ap");
				ctss_reset_pluck(a, f1, preset->string, 0.95f);
				((CTSS_PluckOsc *) a->state)->gain = preset->osc1Gain;
			} else {
				stackUseCommonOsc(s, 0);
				CTSS_DSPNode *a = NODE_ID(s, "a");
				CTSS_OscState *osc1 = (CTSS_OscState *) a->state;
				osc1->freq = HZ_TO_RAD(f1);
				osc1->phase = 0;
				osc1->gain = preset->osc1Gain;
				a->handler = oscFunctions[MIN(preset->osc1Fn, 5)];
			}
			if (preset->osc2Fn >= 6) {
				patchStackPluckOsc(s, 1);
				CTSS_DSPNode *b = NODE_ID(s, "bp");
				ctss_reset_pluck(b, f2, preset->string, 0.95f);
				((CTSS_PluckOsc *) b->state)->gain = preset->osc2Gain;
			} else {
				stackUseCommonOsc(s, 1);
				CTSS_DSPNode *b = NODE_ID(s, "b");
				CTSS_OscState *osc2 = (CTSS_OscState *) b->state;
				osc2->freq = HZ_TO_RAD(f2);
				osc2->phase = 0;
				osc2->gain = preset->osc2Gain;
				b->handler = oscFunctions[MIN(preset->osc2Fn, 5)];
			}
			ctss_configure_adsr(env, preset->attack, preset->decay,
					preset->release, 1.0f, preset->sustain);
			ctss_reset_adsr(env);
			ctss_calculate_biquad_coeff(NODE_ID(s, "f"),
					filterTypes[preset->filterType], preset->filterCutoff,
					12.0f, preset->filterQ);
			NODE_ID_STATE(CTSS_DelayState, s, "d")->feedback = preset->feedback;
			NODE_ID_STATE(CTSS_PanningState, s, "p")->pos = 0.5f
					+ 0.49f * ((voiceID % 2) ? -preset->width : preset->width);
			ctss_activate_stack(s);
			noteID = (noteID + 1) % 16;
			voiceID = (voiceID + 1) % synth.numStacks;
		}
	}
	ctss_update_mix_stereo_i16(&synth, ctss_mixdown_i16, frames, ptr);
}

static void updateAudioBuffer() {
	if (bufferState == BUFFER_OFFSET_HALF) {
		int16_t *ptr = (int16_t*) &audioBuf[0];
		renderAudio(ptr, AUDIO_DMA_BUFFER_SIZE8);
		bufferState = BUFFER_OFFSET_NONE;
	} else if (bufferState == BUFFER_OFFSET_FULL) {
		int16_t *ptr = (int16_t*) &audioBuf[0] + AUDIO_DMA_BUFFER_SIZE4;
		renderAudio(ptr, AUDIO_DMA_BUFFER_SIZE8);
		bufferState = BUFFER_OFFSET_NONE;
	}
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void) {
	bufferState = BUFFER_OFFSET_HALF;
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
	bufferState = BUFFER_OFFSET_FULL;
}

void BSP_AUDIO_OUT_Error_CallBack(void) {
	Error_Handler();
}

// Callback function run whenever timer caused interrupt
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	UNUSED(htim);
	//__disable_irq();
	updateAudioBuffer();
	//__enable_irq();
}
