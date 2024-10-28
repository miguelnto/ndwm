#ifndef NDWM_MONITOR_H
#define NDWM_MONITOR_H

#include <X11/Xutil.h>
#include "types/client.h"

typedef struct Monitor Monitor;

/* Constants */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
#define TAGS_LEN 9

typedef struct Pertag Pertag;

struct Monitor {
    float master_factor;
    int mx, my, mw, mh;   /* Screen size */
    int wx, wy, ww, wh;   /* Window area  */
    
    unsigned int seltags;
    unsigned int tagset[2];
    bool top_bar;
    Client *clients;
    Client *selected_client;
    Client *stack;
    Window bar_win;
    int by, bh;               /* Bar geometry */
    Pertag *pertag;
};

struct Pertag {
    unsigned int current_tag, previous_tag; /* Current and previous tag */
    float master_factors[TAGS_LEN + 1]; /* master_factors per tag */
};

#endif

