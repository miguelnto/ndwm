#include "systray.h"
#include <stdlib.h>

Systray *systray = NULL;

int get_systray_width(void)
{
	unsigned int systray_width = 0;
	for(Client *icon = systray->icons; icon; systray_width += icon->w + systrayspacing, icon = icon->next);
	return (systray_width != 0) ? systray_width + systrayspacing : 1;
}

void remove_systray_icon(Client *i)
{
	Client **ii;

	if (!i) {
		return;
	}
	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
	if (ii) {
		*ii = i->next;
	}
	free(i);
}

