#include <stdlib.h>
#include "screen.h"

Screen* ct_screen_init() {
	BSP_LCD_Init();
	Screen *screen = (Screen*) malloc(sizeof(Screen));
	screen->width = BSP_LCD_GetXSize();
	screen->height = BSP_LCD_GetYSize();
	screen->addr[0] = LCD_FB_START_ADDRESS;
	screen->addr[1] = LCD_FB_START_ADDRESS + screen->width * screen->height * 4;
	screen->front = 1;
	BSP_LCD_LayerDefaultInit(0, screen->addr[0]);
	BSP_LCD_LayerDefaultInit(1, screen->addr[1]);
	BSP_LCD_SetLayerVisible(0, DISABLE);
	BSP_LCD_SetLayerVisible(1, ENABLE);
	BSP_LCD_Clear(LCD_COLOR_RED);
	BSP_LCD_SelectLayer(0);
	return screen;
}

void ct_screen_flip_buffers(Screen *screen) {
	while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS));
	BSP_LCD_SetLayerVisible(screen->front, DISABLE);
	screen->front ^= 1;
	BSP_LCD_SetLayerVisible(screen->front, ENABLE);
	BSP_LCD_SelectLayer(ct_screen_backbuffer_id(screen));
}

uint32_t* ct_screen_backbuffer_ptr(Screen *screen) {
	return (uint32_t*)(screen->addr[ct_screen_backbuffer_id(screen)]);
}
