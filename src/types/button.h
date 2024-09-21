#ifndef NDWM_BUTTON_H
#define NDWM_BUTTON_H

#include "arg.h"

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
} Button;

#endif

