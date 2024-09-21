#ifndef NDWM_CLIENT_H 
#define NDWM_CLIENT_H

#include <X11/Xutil.h>
#include "stdbool.h"

typedef struct Client Client;

struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int sfx, sfy, sfw, sfh; /* Stored float geometry, used on mode revert */
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	int oldstate;
	bool is_floating, is_fixed, is_urgent, is_fullscreen, never_focus;
	Client *next;
	Client *stack_next;
	Window win;
};

#endif

