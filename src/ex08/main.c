#include "ex08/main.h"

__IO DMABufferState bufferState = BUFFER_OFFSET_NONE;
__IO int32_t isPressed = 0;

static uint8_t audioBuffer[AUDIO_DMA_BUFFER_SIZE];

static tinymt32_t rng;
static __IO uint32_t voiceID = 0;
static __IO uint32_t transposeID = 0;

static CT_Synth synth;
static SeqTrack* tracks[2];

static uint8_t transpose[] = { 0, 5, 7, 9, 12, 17, 19, 24 };
static int8_t notes1[] = { 36, -1, 12, 12, -1, -1, -1, -1, 48, -1, 17, 12, -1, -1,
			-1, 24 };
static int8_t notes2[] = { 12, 17, -1, 24, 36, -1 };

static SynthPreset synthPresets[] = { {
		.osc1Gain = 0.25f,
		.osc2Gain = 0.25f,
		.detune = 0.5066f,
		.impFreq = 40.0f,
		.filterCutoff = 8000.0f,
		.filterQ = 0.9f,
		.feedback = 0.5f,
		.width = 0.66f,
		.osc1Fn = 2,
		.osc2Fn = 1,
		.filterType = 0,
		.tempo = 150,
		.attack = 0.01f,
		.decay = 0.025f,
		.sustain = 0.25f,
		.release = 0.5f,
		.string = 0.02f,
		.volume = VOLUME } };

static SynthPreset *preset = &synthPresets[0];

static CT_BiquadType filterTypes[] = { LPF, HPF, BPF, PEQ };

static CT_DSPNodeHandler oscFunctions[] = { ct_synth_process_osc_spiral,
		ct_synth_process_osc_sin, ct_synth_process_osc_square,
		ct_synth_process_osc_saw, ct_synth_process_osc_sawsin,
		ct_synth_process_osc_tri };

static void initSynth();
static void initAudio();
static void initStack(CT_DSPStack *stack, float freq);
static void initSequencer();
static void playNote(CT_Synth* synth, SeqTrack *track, int8_t note, uint32_t tick);
static void updateAudioBuffer(CT_Synth *synth);

int main(void) {
	CPU_CACHE_Enable();
	HAL_Init();
	SystemClock_Config();
	BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);
	HAL_Delay(1000);

	if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, VOLUME, SAMPLE_RATE) != 0) {
		Error_Handler();
	}

	tinymt32_init(&rng, 0x12345678);
	initSequencer();
	initAudio();

	while (1) {
		uint32_t tick = HAL_GetTick();
		updateAllTracks(&synth, tracks, 2, tick);
		updateAudioBuffer(&synth);
	}
}

static void initStack(CT_DSPStack *stack, float freq) {
	float f1 = HZ_TO_RAD(freq);
	float f2 = HZ_TO_RAD(freq * preset->detune);
	CT_DSPNode *env = ct_synth_adsr("e", synth.lfo[0], preset->attack,
			preset->decay, preset->release, 1.0f, preset->sustain);

	CT_DSPNode *imp = ct_synth_osc("imp", ct_synth_process_osc_impulse, 0.0f, HZ_TO_RAD(preset->impFreq), HZ_TO_RAD(freq * 6.0f), 0.0f);

	CT_DSPNode *osc1 = ct_synth_osc("a", oscFunctions[preset->osc1Fn], 0.0f, f1,
			preset->osc1Gain, 0.0f);

	CT_DSPNode *osc2 = ct_synth_osc("b", oscFunctions[preset->osc2Fn], 0.0f, f2,
			preset->osc2Gain, 0.0f);

	ct_synth_set_osc_lfo(osc1, imp, 1.0f);
	ct_synth_set_osc_lfo(osc2, imp, 1.0f);

	CT_DSPNode *sum = ct_synth_op4("s", osc1, env, osc2, env,
			ct_synth_process_madd);

	CT_DSPNode *filter = ct_synth_filter_biquad("f",
			filterTypes[preset->filterType], sum, preset->filterCutoff, 12.0f,
			preset->filterQ);

	CT_DSPNode *delay = ct_synth_delay("d", filter,
			(int) (SAMPLE_RATE * 0.375f), preset->feedback, 1);

	CT_DSPNode *pan = ct_synth_panning("p", delay, NULL, 0.5f);

	CT_DSPNode *nodes[] = { env, imp, osc1, osc2, sum, filter, delay, pan };
	ct_synth_init_stack(stack);
	ct_synth_build_stack(stack, nodes, 8);
}

static void initSynth() {
	ct_math_init();
	ct_synth_init(&synth, NUM_VOICES);
	synth.lfo[0] = ct_synth_osc("lfo1", ct_synth_process_osc_sin, 0.0f,
			HZ_TO_RAD(1 / 24.0f), 0.6f, 1.0f);
	synth.numLFO = 1;
	for (uint8_t i = 0; i < synth.numStacks; i++) {
		initStack(&synth.stacks[i], 110.0f);
	}
	ct_synth_collect_stacks(&synth);
}

void initSequencer(void) {
	tracks[0] = initTrack(0, (SeqTrack*) malloc(sizeof(SeqTrack)), playNote,
			notes1, 16, 250, 1.0f);
	tracks[1] = initTrack(1, (SeqTrack*) malloc(sizeof(SeqTrack)), playNote,
			notes2, 6, 250, 1.0f);
	tracks[0]->attack = 0.005f;
	tracks[0]->decay = 0.05f;
	tracks[1]->attack = 0.005f;
	tracks[1]->decay = 0.05f;
}

void initAudio(void) {
	initSynth();
	if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, VOLUME, SAMPLE_RATE) != 0) {
		Error_Handler();
	}
	BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
	BSP_AUDIO_OUT_Play((uint16_t*) &audioBuffer[0], AUDIO_DMA_BUFFER_SIZE);
}

static void playNote(CT_Synth* synth, SeqTrack *track, int8_t note, uint32_t tick) {
	CT_DSPStack *s = &synth->stacks[voiceID];
	CT_DSPNode *env = NODE_ID(s, "e");
	float f1 = ct_synth_notes[note + transpose[transposeID]] * 0.5f;
	float f2 = f1 * preset->detune;
	CT_DSPNode *imp = NODE_ID(s, "imp");
	CT_OscState *impstate = (CT_OscState *) imp->state;
	impstate->freq = HZ_TO_RAD(preset->impFreq);
	impstate->phase = 0.0f;
	CT_DSPNode *a = NODE_ID(s, "a");
	CT_OscState *osc1 = (CT_OscState *) a->state;
	osc1->freq = HZ_TO_RAD(f1);
	osc1->phase = 0;
	osc1->gain = preset->osc1Gain;
	a->handler = oscFunctions[MIN(preset->osc1Fn, 5)];
	CT_DSPNode *b = NODE_ID(s, "b");
	CT_OscState *osc2 = (CT_OscState *) b->state;
	osc2->freq = HZ_TO_RAD(f2);
	osc2->phase = 0;
	osc2->gain = preset->osc2Gain;
	b->handler = oscFunctions[MIN(preset->osc2Fn, 5)];
	ct_synth_configure_adsr(env, track->attack, preset->decay, preset->release,
			1.0f, preset->sustain);
	ct_synth_reset_adsr(env);
	ct_synth_calculate_biquad_coeff(NODE_ID(s, "f"),
			filterTypes[preset->filterType], track->cutoff, 12.0f,
			track->resonance);
	NODE_ID_STATE(CT_DelayState, s, "d")->feedback = track->damping;
	NODE_ID_STATE(CT_PanningState, s, "p")->pos = 0.5f
			+ 0.49f * ((track->id % 2) ? -preset->width : preset->width);
	ct_synth_activate_stack(s);
	voiceID = (voiceID + 1) % synth->numStacks;
}

static void renderAudio(int16_t *ptr, uint32_t frames) {
	ct_synth_update_mix_stereo_i16(&synth, frames, ptr);
}

void updateAudioBuffer(CT_Synth *synth) {
	if (bufferState == BUFFER_OFFSET_HALF) {
		int16_t *ptr = (int16_t*) &audioBuffer[0];
		renderAudio(ptr, AUDIO_DMA_BUFFER_SIZE8);
		bufferState = BUFFER_OFFSET_NONE;
	}

	if (bufferState == BUFFER_OFFSET_FULL) {
		int16_t *ptr = (int16_t*) &audioBuffer[0] + AUDIO_DMA_BUFFER_SIZE4;
		renderAudio(ptr, AUDIO_DMA_BUFFER_SIZE8);
		bufferState = BUFFER_OFFSET_NONE;
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t pin) {
	if (pin == KEY_BUTTON_PIN) {
		if (!isPressed) {
			BSP_LED_Toggle(LED_GREEN);
			transposeID = (transposeID + 1) % 8;
			tracks[0]->direction *= -1;
			tracks[1]->direction *= -1;
			isPressed = 1;
		} else {
			isPressed = 0;
		}
	}
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void) {
	bufferState = BUFFER_OFFSET_HALF;
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
	bufferState = BUFFER_OFFSET_FULL;
}

void BSP_AUDIO_OUT_Error_CallBack(void) {
	BSP_LED_On(LED_GREEN);
	while (1) {
	}
}
