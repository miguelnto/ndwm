#ifndef NDWM_XERROR_H
#define NDWM_XERROR_H

#include <X11/Xutil.h>

// Xerror utils
int xerrordummy(Display *dpy, XErrorEvent *ee);
int xerrorstart(Display *dpy, XErrorEvent *ee);

#endif
