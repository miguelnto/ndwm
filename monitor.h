#ifndef NDWM_MONITOR_H
#define NDWM_MONITOR_H

#include <X11/Xutil.h>
#include "types/client.h"

typedef struct Monitor Monitor;

/* Constants */
#define TAGS_LEN 9
static const unsigned int snap = 32; /* Snap pixel */

/* Global variables */
extern Monitor *first_monitor;

typedef struct {
	void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;

struct Monitor {
	float master_factor;
	int by;               /* Bar geometry */
	int mx, my, mw, mh;   /* Screen size */
	int wx, wy, ww, wh;   /* Window area  */
	
	unsigned int seltags;
	unsigned int selected_layout;
	unsigned int tagset[2];
	bool top_bar;
	Client *clients;
	Client *selected_client;
	Client *stack;
	Monitor *next;
	Window bar_win;
	const Layout *lt[2];
	Pertag *pertag;
};

struct Pertag {
	unsigned int current_tag, previous_tag; /* Current and previous tag */
	float master_factors[TAGS_LEN + 1]; /* master_factors per tag */
        unsigned int selected_layouts[TAGS_LEN + 1]; /* Selected layouts */
        const Layout *ltidxs[TAGS_LEN + 1][2]; /* Matrix of tags and layouts indexes  */
};

void update_bar_pos(Monitor *m, int bar_height);
Client *window_to_client(Window w);
Client *window_to_systray_icon(Window w); 
void arrange_monitor(Monitor *m);
void attach(Client *c);
void attach_stack(Client *c);

#endif

