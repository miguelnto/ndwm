#ifndef NDWM_SYSTRAY_H
#define NDWM_SYSTRAY_H

#include "types/client.h"

typedef struct {
    Window win;
    Client *icons; 
} Systray;

#endif

