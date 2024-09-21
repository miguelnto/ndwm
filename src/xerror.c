#include "xerror.h"
#include "utils.h"

int xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

int xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("ndwm: another window manager is already running");
	return -1;
}

