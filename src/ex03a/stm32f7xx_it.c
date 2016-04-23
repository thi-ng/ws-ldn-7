#include "ex03a/main.h"
#include "ex03a/stm32f7xx_it.h"

extern SAI_HandleTypeDef haudio_out_sai;

void Error_Handler(void) {
	BSP_LED_On(LED_GREEN);
	while (1) {
	}
}

void SysTick_Handler(void) {
	HAL_IncTick();
}

void AUDIO_OUT_SAIx_DMAx_IRQHandler(void) {
	HAL_DMA_IRQHandler(haudio_out_sai.hdmatx);
}
