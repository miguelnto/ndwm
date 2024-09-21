#ifndef NDWM_KEY_H
#define NDWM_KEY_H

#include "arg.h"
#include <X11/Xutil.h>

typedef struct {
	unsigned int mod;
	KeySym key_symbol;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

#endif

