#include <string.h>
#include <stdio.h>
#include <math.h>
#include "ex07/main.h"
#include "ex07/texture.h"
#include "screen.h"

static char buf[8];
static float theta, zoom;

int main(void) {
	HAL_Init();
	SystemClock_Config();
	Screen *screen = ct_screen_init();

	while (1) {
		ct_screen_flip_buffers(screen);
		uint32_t t0 = HAL_GetTick();
		theta += 0.02f;
		zoom += 0.03f;
		float scale = 5.0f + sinf(zoom) * 4.85f;
		register float du = cosf(theta) * scale;
		register float dv = sinf(theta) * scale;
		register float u = 0, v = 0;
		float uu = 0, vv = 0;
		uint32_t *dest = ct_screen_backbuffer_ptr(screen);
		uint32_t *img = (uint32_t*)texture128_argb8888;
		for (uint32_t y = screen->height; y--;) {
			for (uint32_t x = screen->width; x--;) {
				u += du;
				v += dv;
				*dest++ = img[(((int32_t)v & 0x7f) << 7) + ((int32_t)u & 0x7f)];
			}
			u = (uu -= dv);
			v = (vv += du);
		}
		snprintf(buf, 8, "%02ums", (unsigned int) (HAL_GetTick() - t0));
		BSP_LCD_DisplayStringAtLine(0, (uint8_t*)buf);
	}
}
