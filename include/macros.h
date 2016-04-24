#pragma once

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define CLAMP(x,lo,hi) (MAX(MIN((x),(hi)),(lo)))

#define FASTFLOOR(x)  (((x) >= 0) ? (int)(x) : (int)(x)-1)
