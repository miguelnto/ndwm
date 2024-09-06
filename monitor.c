#include "monitor.h"
#include "systray.h"

Monitor *first_monitor = NULL; 

void update_bar_pos(Monitor *m, int bar_height)
{
	m->wy = m->my;
	m->wh = m->mh;
	m->wh -= bar_height;
	m->by = m->top_bar ? m->wy : m->wy + m->wh;
	m->wy = m->top_bar ? m->wy + bar_height : m->wy;
}

Client *window_to_client(Window w)
{
	for (Client *client = first_monitor->clients; client; client = client->next) {
		if (client->win == w) {
			return client;
		}
	}
	return NULL;
}

Client *window_to_systray_icon(Window w) 
{
	Client *client = NULL;

	if (!w) { 
		return client;
	}
	for (client = systray->icons; client && client->win != w; client = client->next);
	return client;
}

void arrange_monitor(Monitor *m)
{
	if (m->lt[m->selected_layout]->arrange) {
		m->lt[m->selected_layout]->arrange(m);
	}
}

void attach(Client *c)
{
	c->next = first_monitor->clients;
	first_monitor->clients = c;
}

void attach_stack(Client *c)
{
	c->stack_next = first_monitor->stack;
	first_monitor->stack = c;
}

