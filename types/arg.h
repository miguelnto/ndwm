#ifndef NDWM_ARG_H
#define NDWM_ARG_H

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

#endif

