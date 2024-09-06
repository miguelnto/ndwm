#ifndef NDWM_SYSTRAY_H
#define NDWM_SYSTRAY_H

#include "types/client.h"

typedef struct {
	Window win;
	Client *icons; 
} Systray;

/* Global varibles */ 
extern Systray *systray;

/* Constants */
static const unsigned int systrayspacing = 2; 

int get_systray_width(void);
void remove_systray_icon(Client *i);

#endif

