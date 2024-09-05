//#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "utils.h"
#include "arg.h"
#include "button.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define NUMTAGS					(LENGTH(tags))
#define TAGMASK     			((1 << NUMTAGS) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

#define SYSTEM_TRAY_REQUEST_DOCK    0

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10

#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2

#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkClientWin, ClkRootWin }; /* clicks */

typedef struct Monitor Monitor;
typedef struct Client Client;

struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	int oldstate;
	bool is_floating, is_fixed, is_urgent, is_fullscreen, never_focus;
	Client *next;
	Client *stack_next;
	Monitor *mon;
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym key_symbol;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

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

typedef struct Systray Systray;

struct Systray {
	Window win;
	Client *icons; 
};

/* function declarations */
static void apply_rules(Client *c);
static int apply_size_hints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrange_monitor(Monitor *m);
static void attach(Client *c);
static void attach_stack(Client *c);
static void button_press(XEvent *e);
static void tag(const Arg *arg);
static void check_another_wm_running(void);
static void cleanup(void);
static void cleanup_monitor(Monitor *m);
static void client_message(XEvent *e);
static void configure(Client *c);
static void configure_notify(XEvent *e);
static void configure_request(XEvent *e);
static Monitor *create_monitor(void);
static void destroy_notify(XEvent *e);
static void detach(Client *c);
static void detach_stack(Client *c);
static void draw_bar(Monitor *m);
static void enter_notify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focus_stack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int get_root_ptr(int *x, int *y);
static long get_state(Window w);
static unsigned int get_systray_width(void);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grab_buttons(Client *c, int focused);
static void grab_keys(void);
static void key_press(XEvent *e);
static void kill_client(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mapping_notify(XEvent *e);
static void map_request(XEvent *e);
static void motion_notify(XEvent *e);
static void move_with_mouse(const Arg *arg);
static Client *next_tiled_client(Client *c);
static void property_notify(XEvent *e);
static void quit(const Arg *arg);
static void remove_systray_icon(Client *i);
static void reset_layout(const Arg *arg);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resize_bar_win(Monitor *m);
static void resize_client(Client *c, int x, int y, int w, int h);
static void resize_with_mouse(const Arg *arg);
static void resize_request(XEvent *e);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int send_event(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void set_client_state(Client *c, long state);
static void set_focus(Client *c);
static void set_fullscreen(Client *c, int fullscreen);
static void set_master_factor(const Arg *arg);
static void setup(void);
static void set_urgent(Client *c, int urg);
static void shift_view_clients(const Arg *arg);
static void showhide(Client *c);
static void spawn(const Arg *arg);
static void toggle_fullscreen(const Arg *arg);
static void toggle_floating(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmap_notify(XEvent *e);
static void update_bar_pos(Monitor *m);
static void update_bars(void);
static void update_client_list(void);
static bool update_geometry(void);
static void update_numlock_mask(void);
static void update_size_hints(Client *c);
static void update_status(void);
static void update_systray(void);
static void update_systray_icon_geometry(Client *client, int w, int h);
static void update_systray_icon_state(Client *i, XPropertyEvent *ev);
static void update_title(Client *c);
static void update_window_type(Client *c);
static void update_wm_hints(Client *c);
static void view(const Arg *arg);
static Client *window_to_client(Window w);
static Client *window_to_systray_icon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void tile(Monitor *m);

static Systray *systray =  NULL;
static char stext[256];

static int screen;
static int sw, sh;           /* X display screen width and height */
static int bh, blw = 0;      /* Bar geometry */
static int lrpad;            /* Sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;

static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = button_press,
	[ClientMessage] = client_message,
	[ConfigureRequest] = configure_request,
	[ConfigureNotify] = configure_notify,
	[DestroyNotify] = destroy_notify,
	[EnterNotify] = enter_notify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = key_press,
	[MappingNotify] = mapping_notify,
	[MapRequest] = map_request,
	[MotionNotify] = motion_notify,
	[PropertyNotify] = property_notify,
	[ResizeRequest] = resize_request,
	[UnmapNotify] = unmap_notify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];

static bool running = true;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *first_monitor; 
static Window root, wmcheckwin;

/* Configuration, allows nested code to access above variables */
#include "config.h"

struct Pertag {
	unsigned int current_tag, previous_tag; /* Current and previous tag */
	float master_factors[LENGTH(tags) + 1]; /* master_factors per tag */
        unsigned int selected_layouts[LENGTH(tags) + 1]; /* Selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* Matrix of tags and layouts indexes  */
};

void apply_rules(Client *c)
{
	XClassHint class_hint = { NULL, NULL };

	/* Rule matching */
	c->is_floating = false;
	XGetClassHint(dpy, c->win, &class_hint);
	if (class_hint.res_class) {
		XFree(class_hint.res_class);
	}
	if (class_hint.res_name) {
		XFree(class_hint.res_name);
	}
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int apply_size_hints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* Set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw) {
			*x = sw - WIDTH(c);
		}
		if (*y > sh) {
			*y = sh - HEIGHT(c);
		}
		if (*x + *w + 2 * c->bw < 0) {
			*x = 0;
		}
		if (*y + *h + 2 * c->bw < 0) {
			*y = 0;
		}
	} else {
		if (*x >= m->wx + m->ww) {
			*x = m->wx + m->ww - WIDTH(c);
		}
		if (*y >= m->wy + m->wh) {
			*y = m->wy + m->wh - HEIGHT(c);
		}
		if (*x + *w + 2 * c->bw <= m->wx) {
			*x = m->wx;
		}
		if (*y + *h + 2 * c->bw <= m->wy) {
			*y = m->wy;
		}
	}
	if (*h < bh) {
		*h = bh;
	}
	if (*w < bh) {
		*w = bh;
	}
	if (c->is_floating || !c->mon->lt[c->mon->selected_layout]->arrange) {
		/* See last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { 
			/* Temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* Adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h) {
				*w = *h * c->maxa + 0.5;
			} else if (c->mina < (float)*h / *w) {
				*h = *w * c->mina + 0.5;
			}
		}
		if (baseismin) { 
			/* Increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* Adjust for increment value */
		if (c->incw) {
			*w -= *w % c->incw;
		}
		if (c->inch) {
			*h -= *h % c->inch;
		}
		/* Restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw) {
			*w = MIN(*w, c->maxw);
		}
		if (c->maxh) {
			*h = MIN(*h, c->maxh);
		}
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(Monitor *m)
{
	if (m) {
	     showhide(m->stack);
	} else for (m = first_monitor; m; m = m->next) {
		showhide(m->stack);
	}
	if (m) {
		arrange_monitor(m);
		restack(m);
	} else for (m = first_monitor; m; m = m->next)
		arrange_monitor(m);
}

void arrange_monitor(Monitor *m)
{
	if (m->lt[m->selected_layout]->arrange) {
		m->lt[m->selected_layout]->arrange(m);
	}
}

void attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void attach_stack(Client *c)
{
	c->stack_next = c->mon->stack;
	c->mon->stack = c;
}

void button_press(XEvent *e)
{
	unsigned int i, x, click;
	Arg arg = {0};
	Client *client;
	Monitor *mon;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;

	/* Focus monitor if necessary */
	if ((mon = first_monitor)) {
		unfocus(first_monitor->selected_client, 1);
		focus(NULL);
	}
	if (ev->window == first_monitor->bar_win) {
		i = x = 0;
		do {
			x += TEXTW(tags[i]);
		} while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} 
	} else if ((client = window_to_client(ev->window))) {
		focus(client);
		restack(first_monitor);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state)) {
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
		}
}

void check_another_wm_running(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);

	/* This causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout dummy_layout = { NULL };
	Monitor *mon;
	size_t i;

	view(&a);
	first_monitor->lt[first_monitor->selected_layout] = &dummy_layout;
	for (mon = first_monitor; mon; mon = mon->next) {
		while (mon->stack) {
			unmanage(mon->stack, 0);
		}
	}
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (first_monitor) {
		cleanup_monitor(first_monitor);
	}
	XUnmapWindow(dpy, systray->win);
	XDestroyWindow(dpy, systray->win);
	free(systray);
	for (i = 0; i < CurLast; i++) {
		drw_cur_free(drw, cursor[i]);
	}
	for (i = 0; i < LENGTH(colors); i++) {
		free(scheme[i]);
	}
	free(scheme);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void cleanup_monitor(Monitor *m)
{
	Monitor *mon;

	if (m == first_monitor) {
		first_monitor = first_monitor->next;
	} else {
		for (mon = first_monitor; mon && mon->next != m; mon = mon->next);
		mon->next = m->next;
	}
	XUnmapWindow(dpy, m->bar_win);
	XDestroyWindow(dpy, m->bar_win);
	free(m);
}

void client_message(XEvent *e)
{
	XWindowAttributes wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client *c = window_to_client(cme->window);
	unsigned int i;

	if (cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
		/* Add systray icons */
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
			if (!(c = (Client *)calloc(1, sizeof(Client)))) {
				die("fatal: could not malloc() %u bytes\n", sizeof(Client));
			}
			if (!(c->win = cme->data.l[2])) {
				free(c);
				return;
			}
			c->mon = first_monitor;
			c->next = systray->icons;
			systray->icons = c;
			if (!XGetWindowAttributes(dpy, c->win, &wa)) {
				/* Use sane defaults */
				wa.width = bh;
				wa.height = bh;
				wa.border_width = 0;
			}
			c->x = c->oldx = c->y = c->oldy = 0;
			c->w = c->oldw = wa.width;
			c->h = c->oldh = wa.height;
			c->oldbw = wa.border_width;
			c->bw = 0;
			c->is_floating = True;
			/* Reuse tags field as mapped status */
			c->tags = 1;
			update_size_hints(c);
			update_systray_icon_geometry(c, wa.width, wa.height);
			XAddToSaveSet(dpy, c->win);
			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
			XReparentWindow(dpy, c->win, systray->win, 0, 0);
			/* Use parents background color */
			swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
			send_event(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			/* FIXME: Not sure if I have to send these events, too */
			send_event(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			send_event(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			send_event(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			XSync(dpy, False);
			resize_bar_win(first_monitor);
			update_systray();
			set_client_state(c, NormalState);
		}
		return;
	}
	if (!c) {
		return;
	}
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen]) {
			set_fullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->is_fullscreen)));
		}
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		for (i = 0; i < LENGTH(tags) && !((1 << i) & c->tags); i++);
		if (i < LENGTH(tags)) {
			const Arg a = {.ui = 1 << i};
			view(&a);
			focus(c);
			restack(first_monitor);
		}
	}
}

void configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configure_notify(XEvent *e)
{
	Client *client;
	XConfigureEvent *ev = &e->xconfigure;
	bool dirty;

	/* TODO: update_geometry handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (update_geometry() || dirty) {
			drw_resize(drw, sw, bh);
			update_bars();
			for (client = first_monitor->clients; client; client = client->next) {
				if (client->is_fullscreen) {
					resize_client(client, first_monitor->mx, first_monitor->my, first_monitor->mw, first_monitor->mh);
				}
			}
			resize_bar_win(first_monitor);
			focus(NULL);
			arrange(NULL);
		}
	}
}

void configure_request(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = window_to_client(ev->window))) {
		if (ev->value_mask & CWBorderWidth) {
			c->bw = ev->border_width;
		} else if (c->is_floating || !first_monitor->lt[first_monitor->selected_layout]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->is_floating) {
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* Center in x direction */
			}
			if ((c->y + c->h) > m->my + m->mh && c->is_floating) {
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* Center in y direction */
			}
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight))) {
				configure(c);
			}
			if (ISVISIBLE(c)) {
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
			}
		} else {
			configure(c);
		}
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *create_monitor(void)
{
	Monitor *new_mon;
	unsigned int i;

	new_mon = ecalloc(1, sizeof(Monitor));
	new_mon->tagset[0] = new_mon->tagset[1] = 1;
	new_mon->master_factor = master_factor;
	new_mon->top_bar = top_bar;

	new_mon->lt[0] = &layouts[0];
	new_mon->lt[1] = &layouts[1 % LENGTH(layouts)];
	new_mon->pertag = ecalloc(1, sizeof(Pertag));
	new_mon->pertag->current_tag = new_mon->pertag->previous_tag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		new_mon->pertag->master_factors[i] = new_mon->master_factor;
		new_mon->pertag->ltidxs[i][0] = new_mon->lt[0];
		new_mon->pertag->ltidxs[i][1] = new_mon->lt[1];
		new_mon->pertag->selected_layouts[i] = new_mon->selected_layout;
	}
	return new_mon;
}

void destroy_notify(XEvent *e)
{
	Client *client;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((client = window_to_client(ev->window))) {
		unmanage(client, 1);
	} else if ((client = window_to_systray_icon(ev->window))) {
		remove_systray_icon(client);
		resize_bar_win(first_monitor);
		update_systray();
	}
}

void detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void detach_stack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->stack_next);
	*tc = c->stack_next;

	if (c == c->mon->selected_client) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->stack_next);
		c->mon->selected_client = t;
	}
}

void draw_bar(Monitor *m)
{
	int x, w, sw = 0, stw = 0;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *client;

	if (m == first_monitor) {
		stw = get_systray_width();

	        /* Draw status first so it can be overdrawn by tags later */
		drw_setscheme(drw, scheme[SchemeNorm]);
		sw = TEXTW(stext) - lrpad / 2 + 2; /* 2px right padding */
		drw_text(drw, m->ww - sw - stw, 0, sw, bh, lrpad / 2 - 2, stext, 0);
	}

	resize_bar_win(m);
	for (client = m->clients; client; client = client->next) {
		occ |= client->tags;
		if (client->is_urgent) {
			urg |= client->tags;
		}
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i) {
			drw_rect(drw, x + boxs, boxs, boxw, boxw,
				m == first_monitor && first_monitor->selected_client && first_monitor->selected_client->tags & 1 << i,
				urg & 1 << i);
		}
		x += w;
	}
	w = blw;
	drw_setscheme(drw, scheme[SchemeNorm]);

	if ((w = m->ww - sw - stw - x) > bh) {
	    drw_setscheme(drw, scheme[SchemeNorm]);
	    drw_rect(drw, x, 0, w, bh, 1, 1);
	}
	drw_map(drw, m->bar_win, 0, 0, m->ww - stw, bh);
}

void enter_notify(XEvent *e)
{
	Client *client;
	Monitor *mon;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root) {
		return;
	}
	client = window_to_client(ev->window);
	mon = client ? client->mon : first_monitor;
	if (mon != first_monitor) {
		unfocus(first_monitor->selected_client, 1);
	} else if (!client || client == first_monitor->selected_client) {
		return;
	}
	focus(client);
}

void expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = first_monitor)) {
		draw_bar(m);
		update_systray();
	}
}

void focus(Client *c)
{
	if (!c || !ISVISIBLE(c)) {
		for (c = first_monitor->stack; c && !ISVISIBLE(c); c = c->stack_next);
	}
	if (first_monitor->selected_client && first_monitor->selected_client != c) {
		unfocus(first_monitor->selected_client, 0);
	}
	if (c) {
		if (c->is_urgent) {
			set_urgent(c, 0);
		}
		detach_stack(c);
		attach_stack(c);
		grab_buttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		set_focus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	first_monitor->selected_client = c;
	draw_bar(first_monitor);
}

/* There are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (first_monitor->selected_client && ev->window != first_monitor->selected_client->win) {
		set_focus(first_monitor->selected_client);
	}
}

void focus_stack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!first_monitor->selected_client) {
		return;
	}
	if (arg->i > 0) {
	    for (c = first_monitor->selected_client->next; c && !ISVISIBLE(c); c = c->next);
	    if (!c) {
	        for (c = first_monitor->clients; c && !ISVISIBLE(c); c = c->next);
	    }
	} else {
	    for (i = first_monitor->clients; i != first_monitor->selected_client; i = i->next) {
	        if (ISVISIBLE(i)) {
		    c = i;
		}
	    }
	    if (!c) {
	        for (; i; i = i->next) {
		    if (ISVISIBLE(i)) {
		        c = i;
		    }
		}
	    }
	}
	if (c) {
	    focus(c);
	    restack(first_monitor);
	}
}

Atom getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;
	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

int get_root_ptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long get_state(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success) {
		return -1;
	}
	if (n != 0) {
		result = *p;
	}
	XFree(p);
	return result;
}

unsigned int get_systray_width(void)
{
	unsigned int systray_width = 0;
	for(Client *icon = systray->icons; icon; systray_width += icon->w + systrayspacing, icon = icon->next);
	return (systray_width != 0) ? systray_width + systrayspacing : 1;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0) {
		return 0;
	}
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems) {
		return 0;
	}
	if (name.encoding == XA_STRING) {
		strncpy(text, (char *)name.value, size - 1);
	} else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void grab_buttons(Client *c, int focused)
{
	update_numlock_mask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused) {
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		}
		for (i = 0; i < LENGTH(buttons); i++) {
			if (buttons[i].click == ClkClientWin) {
				for (j = 0; j < LENGTH(modifiers); j++) {
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
				}
			}
		}
	}
}

void grab_keys(void)
{
	update_numlock_mask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++) {
			if ((code = XKeysymToKeycode(dpy, keys[i].key_symbol))) {
				for (j = 0; j < LENGTH(modifiers); j++) {
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root, True, GrabModeAsync, GrabModeAsync);
				}
			}
		}
	}
}

void key_press(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++) {
		if (keysym == keys[i].key_symbol
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func) {
			keys[i].func(&(keys[i].arg));
		}
	}
}

void kill_client(const Arg *arg)
{
	if (!first_monitor->selected_client) {
		return;
	}
	if (!send_event(first_monitor->selected_client->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, first_monitor->selected_client->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	update_title(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = window_to_client(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = first_monitor;
		apply_rules(c);
	}

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww) {
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	}
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh) {
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	}
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	update_window_type(c);
	update_size_hints(c);
	update_wm_hints(c);
	c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
	c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
	c->sfx = c->x;
	c->sfy = c->y;
	c->sfw = c->w;
	c->sfh = c->h;
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grab_buttons(c, 0);
	if (!c->is_floating) {
		c->is_floating = (c->oldstate = trans != None || c->is_fixed);
	}
	if (c->is_floating) {
		XRaiseWindow(dpy, c->win);
	}
	attach(c);
	attach_stack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	set_client_state(c, NormalState);
	if (c->mon == first_monitor) {
		unfocus(first_monitor->selected_client, 0);
	}
	c->mon->selected_client = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	focus(NULL);
}

void mapping_notify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard) {
		grab_keys();
	}
}

void map_request(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;
	Client *i;
	if ((i = window_to_systray_icon(ev->window))) {
		send_event(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
		resize_bar_win(first_monitor);
		update_systray();
	}

	if (!XGetWindowAttributes(dpy, ev->window, &wa)) {
		return;
	}
	if (wa.override_redirect) {
		return;
	}
	if (!window_to_client(ev->window)) {
		manage(ev->window, &wa);
	}
}

void motion_notify(XEvent *e)
{
	XMotionEvent *ev = &e->xmotion;
	if (ev->window != root) { 
		return;
	}
}

void move_with_mouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *client;
	XEvent ev;
	Time last_time = 0;

	if (!(client = first_monitor->selected_client)) {
		return;
	}

        /* Don't move fullscreen windows with the mouse */
	if (client->is_fullscreen) {
		return;
	}
	restack(first_monitor);
	ocx = client->x;
	ocy = client->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess) {
		return;
	}
	if (!get_root_ptr(&x, &y)) {
		return;
	}
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - last_time) <= (1000 / 60))
				continue;
			last_time = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(first_monitor->wx - nx) < snap)
				nx = first_monitor->wx;
			else if (abs((first_monitor->wx + first_monitor->ww) - (nx + WIDTH(client))) < snap)
				nx = first_monitor->wx + first_monitor->ww - WIDTH(client);
			if (abs(first_monitor->wy - ny) < snap)
				ny = first_monitor->wy;
			else if (abs((first_monitor->wy + first_monitor->wh) - (ny + HEIGHT(client))) < snap)
				ny = first_monitor->wy + first_monitor->wh - HEIGHT(client);
			if (!client->is_floating && first_monitor->lt[first_monitor->selected_layout]->arrange
			&& (abs(nx - client->x) > snap || abs(ny - client->y) > snap))
				toggle_floating(NULL);
			if (!first_monitor->lt[first_monitor->selected_layout]->arrange || client->is_floating)
				resize(client, nx, ny, client->w, client->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
}

Client *next_tiled_client(Client *c)
{
	for (; c && (c->is_floating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void property_notify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((c = window_to_systray_icon(ev->window))) {
		if (ev->atom == XA_WM_NORMAL_HINTS) {
			update_size_hints(c);
			update_systray_icon_geometry(c, c->w, c->h);
		} else {
			update_systray_icon_state(c, ev);
		}
		resize_bar_win(first_monitor);
		update_systray();
	}
	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		update_status();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = window_to_client(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->is_floating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->is_floating = (window_to_client(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			update_size_hints(c);
			break;
		case XA_WM_HINTS:
			update_wm_hints(c);
			draw_bar(first_monitor);
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName])
			update_title(c);
		if (ev->atom == netatom[NetWMWindowType])
			update_window_type(c);
	}
}

void quit(const Arg *arg)
{
	running = false;
}

void remove_systray_icon(Client *i)
{
	Client **ii;

	if (!i)
		return;
	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
	if (ii)
		*ii = i->next;
	free(i);
}

void reset_layout(const Arg *arg)
{
	Arg default_master_factor = {.f = master_factor + 1};
	set_master_factor(&default_master_factor);
}

void resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (apply_size_hints(c, &x, &y, &w, &h, interact))
		resize_client(c, x, y, w, h);
}

void resize_bar_win(Monitor *m) 
{
	unsigned int window = m->ww;
	if (m == first_monitor)
		window -= get_systray_width();
	XMoveResizeWindow(dpy, m->bar_win, m->wx, m->by, window, bh);
}

void resize_client(Client *c, int x, int y, int w, int h)
{
	XWindowChanges window_changes;

	c->oldx = c->x; c->x = window_changes.x = x;
	c->oldy = c->y; c->y = window_changes.y = y;
	c->oldw = c->w; c->w = window_changes.width = w;
	c->oldh = c->h; c->h = window_changes.height = h;
	window_changes.border_width = c->bw;

	if ((next_tiled_client(c->mon->clients) == c) && !(next_tiled_client(c->next)))
		reset_layout(NULL);

	if (((next_tiled_client(c->mon->clients) == c && !next_tiled_client(c->next))) && (!c->is_fullscreen)
        && !c->is_floating && c->mon->lt[c->mon->selected_layout]->arrange) {
		c->w = window_changes.width += c->bw * 2;
		c->h = window_changes.height += c->bw * 2;
		window_changes.border_width = 0;
	}
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &window_changes);
	configure(c);
	XSync(dpy, False);
}

void resize_with_mouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	XEvent ev;
	Time last_time = 0;

	if (!(c = first_monitor->selected_client))
		return;
	if (c->is_fullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(first_monitor);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - last_time) <= (1000 / 60))
				continue;
			last_time = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= first_monitor->wx && c->mon->wx + nw <= first_monitor->wx + first_monitor->ww
			&& c->mon->wy + nh >= first_monitor->wy && c->mon->wy + nh <= first_monitor->wy + first_monitor->wh)
			{
				if (!c->is_floating && first_monitor->lt[first_monitor->selected_layout]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					toggle_floating(NULL);
			}
			if (!first_monitor->lt[first_monitor->selected_layout]->arrange || c->is_floating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void resize_request(XEvent *e)
{
	XResizeRequestEvent *event = &e->xresizerequest;
	Client *i;

	if ((i = window_to_systray_icon(event->window))) {
		update_systray_icon_geometry(i, event->width, event->height);
		resize_bar_win(first_monitor);
		update_systray();
	}
}

void restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	draw_bar(m);
	if (!m->selected_client)
		return;
	if (m->selected_client->is_floating || !m->lt[m->selected_layout]->arrange)
		XRaiseWindow(dpy, m->selected_client->win);
	if (m->lt[m->selected_layout]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->bar_win;
		for (c = m->stack; c; c = c->stack_next)
			if (!c->is_floating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void run(void)
{
	XEvent ev;
	/* Main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev)) {
		if (handler[ev.type]) {
                        /* Call handler */
			handler[ev.type](&ev);
		}
	}
}

void scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || get_state(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || get_state(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins) 
			XFree(wins);
	}
}

void set_client_state(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32, PropModeReplace, (unsigned char *)data, 2);
}

int send_event(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
	int n;
	Atom *protocols, mt;
	int exists = 0;
	XEvent ev;

	if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
		mt = wmatom[WMProtocols];
		if (XGetWMProtocols(dpy, w, &protocols, &n)) {
			while (!exists && n--)
				exists = protocols[n] == proto;
			XFree(protocols);
		}
	}
	else {
		exists = True;
		mt = proto;
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type = mt;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = d0;
		ev.xclient.data.l[1] = d1;
		ev.xclient.data.l[2] = d2;
		ev.xclient.data.l[3] = d3;
		ev.xclient.data.l[4] = d4;
		XSendEvent(dpy, w, False, mask, &ev);
	}
	return exists;
}

void set_focus(Client *c)
{
	if (!c->never_focus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &(c->win), 1);
	}
	send_event(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void set_fullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->is_fullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
		    PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->is_fullscreen = true;
		c->oldstate = c->is_floating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->is_floating = true;
		resize_client(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->is_fullscreen) {
                XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char*)0, 0);
		c->is_fullscreen = false;
		c->is_floating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resize_client(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

/* arg > 1.0 will set master_factor absolutely */
void set_master_factor(const Arg *arg)
{
	float f;

	if (!arg || !first_monitor->lt[first_monitor->selected_layout]->arrange) 
		return;
	f = arg->f < 1.0 ? arg->f + first_monitor->master_factor : arg->f - 1.0;
	
	if (f < 0.05 || f > 0.95) 
		return;
	first_monitor->master_factor = first_monitor->pertag->master_factors[first_monitor->pertag->current_tag] = f;
	arrange(first_monitor);
}

void setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	struct sigaction sa;
	/* Do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* Clean up any zombies (inherited from .xinitrc etc) immediately */
	while (waitpid(-1, NULL, WNOHANG) > 0);

	/* Init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 2;
	update_geometry();

	/* Init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);

	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	/* Init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);

	/* Init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);

	/* Init system tray */
	update_systray();

	/* Init bars */
	update_bars();
	update_status();

	/* Supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "ndwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);

	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);

	/* Select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grab_keys();
	focus(NULL);
}


void set_urgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->is_urgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win))) {
		return;
	}
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void shift_view_clients(const Arg *arg)
{
	Arg shifted;
	Client *client;
	unsigned int tagmask = 0;

	for (client = first_monitor->clients; client; client = client->next)
		tagmask = tagmask | client->tags;
	shifted.ui = first_monitor->tagset[first_monitor->seltags];
	if (arg->i > 0) { 
		/* Left shift */
		do {
			shifted.ui = (shifted.ui << arg->i)
			   | (shifted.ui >> (LENGTH(tags) - arg->i));
		} while (tagmask && !(shifted.ui & tagmask));
	} else {
		/* Right shift */
		do {
			shifted.ui = (shifted.ui >> (- arg->i)
			   | shifted.ui << (LENGTH(tags) + arg->i));
		} while (tagmask && !(shifted.ui & tagmask));
	}
	view(&shifted);
}

void showhide(Client *c)
{
	if (!c) return;
	if (ISVISIBLE(c)) { 
		/* Show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->selected_layout]->arrange || c->is_floating) && !c->is_fullscreen) {
			resize(c, c->x, c->y, c->w, c->h, 0);
		}
		showhide(c->stack_next);
	} else { 
		/* Hide clients bottom up */
		showhide(c->stack_next);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void spawn(const Arg *arg)
{
	struct sigaction sa;
	if (fork() == 0) {
		if (dpy) 
			close(ConnectionNumber(dpy));
		
		setsid();
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("ndwm: execvp '%s' failed:", ((char **)arg->v)[0]);
	}
}

void toggle_fullscreen(const Arg *arg)
{
    if (first_monitor->selected_client) {
	set_fullscreen(first_monitor->selected_client, !first_monitor->selected_client->is_fullscreen);
    }
}

void toggle_floating(const Arg *arg)
{
        if (!first_monitor->selected_client)
		return;
	if (first_monitor->selected_client->is_fullscreen) 
		return;
        first_monitor->selected_client->is_floating = !first_monitor->selected_client->is_floating || first_monitor->selected_client->is_fixed;
	if (first_monitor->selected_client->is_floating) 
		/* Restore last known float dimensions */
		resize(first_monitor->selected_client, first_monitor->selected_client->sfx, first_monitor->selected_client->sfy,
		       first_monitor->selected_client->sfw, first_monitor->selected_client->sfh, False);
	else { 
		/* Save last known float dimensions */
		first_monitor->selected_client->sfx = first_monitor->selected_client->x;
		first_monitor->selected_client->sfy = first_monitor->selected_client->y;
		first_monitor->selected_client->sfw = first_monitor->selected_client->w;
		first_monitor->selected_client->sfh = first_monitor->selected_client->h;
	}
	arrange(first_monitor);
}


void unfocus(Client *c, int setfocus)
{
	if (!c) {
	    return;
	}
	grab_buttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	detach(c);
	detach_stack(c);

	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		set_client_state(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	update_client_list();
	arrange(m);
}

void unmap_notify(XEvent *e)
{
	Client *client;
	XUnmapEvent *ev = &e->xunmap;

	if ((client = window_to_client(ev->window))) {
		if (ev->send_event) {
                    set_client_state(client, WithdrawnState);
		} else {
                    unmanage(client, 0);
		}
	} else if ((client = window_to_systray_icon(ev->window))) {
		/* KLUDGE! sometimes icons occasionally unmap their windows, but do
		 * _not_ destroy them. We map those windows back */
		XMapRaised(dpy, client->win);
		update_systray();
	}
}

void update_bars(void)
{
	unsigned int width;
	Monitor *mon;
	XSetWindowAttributes window_attrs = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"ndwm", "ndwm"};
	for (mon = first_monitor; mon; mon = mon->next) {
		if (mon->bar_win) {
			continue;
		}
		width = mon->ww;
		if (mon == first_monitor) {
			width -= get_systray_width();
		}
		mon->bar_win = XCreateWindow(dpy, root, mon->wx, mon->by, width, bh, 0, DefaultDepth(dpy, screen),
				CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &window_attrs);
		XDefineCursor(dpy, mon->bar_win, cursor[CurNormal]->cursor);
		if (mon == first_monitor) {
			XMapRaised(dpy, systray->win);
		}
		XMapRaised(dpy, mon->bar_win);
		XSetClassHint(dpy, mon->bar_win, &ch);
	}
}

void update_bar_pos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	m->wh -= bh;
	m->by = m->top_bar ? m->wy : m->wy + m->wh;
	m->wy = m->top_bar ? m->wy + bh : m->wy;
}

void update_client_list()
{
	Client *client;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (client = first_monitor->clients; client; client = client->next) {
		XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *) &(client->win), 1);
	}
}

bool update_geometry(void)
{
	bool dirty = false;

	 /* Default monitor setup */
	if (!first_monitor) {
		first_monitor = create_monitor();
	}
	if (first_monitor->mw != sw || first_monitor->mh != sh) {
		dirty = true;
		first_monitor->mw = first_monitor->ww = sw;
		first_monitor->mh = first_monitor->wh = sh;
		update_bar_pos(first_monitor);
	}
	return dirty;
}

void update_numlock_mask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < modmap->max_keypermod; j++) {
			if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dpy, XK_Num_Lock)) {
				numlockmask = (1 << i);
			}
		}
	}
	XFreeModifiermap(modmap);
}

void update_size_hints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize)) {
		/* Size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	}
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else {
		c->basew = c->baseh = 0;
	}
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else {
		c->incw = c->inch = 0;
	}
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else {
		c->maxw = c->maxh = 0;
	}
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else {
		c->minw = c->minh = 0;
	}
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else {
		c->maxa = c->mina = 0.0;
	}
	c->is_fixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void update_status(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext))) { 
		strcpy(stext, "neodwm");
	}
	draw_bar(first_monitor);
	update_systray();
}

void update_systray_icon_geometry(Client *client, int w, int h)
{
	if (!client) {
	    return;
	}
		
	client->h = bh;
	if (w == h) { 
		client->w = bh;
	} else if (h == bh) { 
		client->w = w;
	} else { 
		client->w = (int) ((float)bh * ((float)w / (float)h));
	}
	apply_size_hints(client, &(client->x), &(client->y), &(client->w), &(client->h), False);

	/* Force icons into the systray dimensions if they don't want to */ 
	if (client->h > bh) {
		if (client->w == client->h) client->w = bh;
		else client->w = (int) ((float)bh * ((float)client->w / (float)client->h));
		client->h = bh;
	}
	
}

void update_systray_icon_state(Client *i, XPropertyEvent *ev)
{
	long flags;
	int code = 0;

	if (!i || ev->atom != xatom[XembedInfo] || !(flags = getatomprop(i, xatom[XembedInfo]))) { 
		return;
	}

	if (flags & XEMBED_MAPPED && !i->tags) {
		i->tags = 1;
		code = XEMBED_WINDOW_ACTIVATE;
		XMapRaised(dpy, i->win);
		set_client_state(i, NormalState);
	} else if (!(flags & XEMBED_MAPPED) && i->tags) {
		i->tags = 0;
		code = XEMBED_WINDOW_DEACTIVATE;
		XUnmapWindow(dpy, i->win);
		set_client_state(i, WithdrawnState);
	} else {
	    return;
	}
	send_event(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,systray->win, XEMBED_EMBEDDED_VERSION);
}

void update_systray(void)
{
	XSetWindowAttributes window_attrs;
	XWindowChanges window_changes;
	Client *client;
	Monitor *m = first_monitor;
	unsigned int x = m->mx + m->mw;
	unsigned int w = 1;

	if (!systray) { 
		/* Init systray */
		if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
			die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
		systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
		window_attrs.event_mask        = ButtonPressMask | ExposureMask;
		window_attrs.override_redirect = True;
		window_attrs.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
		XSelectInput(dpy, systray->win, SubstructureNotifyMask);
		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
		XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &window_attrs);
		XMapRaised(dpy, systray->win);
		XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
		if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
			send_event(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
			XSync(dpy, False);
		} else {
			fprintf(stderr, "ndwm: unable to obtain system tray.\n");
			free(systray);
			systray = NULL;
			return;
		}
	}
	for (w = 0, client = systray->icons; client; client = client->next) {
		/* Make sure the background color stays the same */
		window_attrs.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
		XChangeWindowAttributes(dpy, client->win, CWBackPixel, &window_attrs);
		XMapRaised(dpy, client->win);
		w += systrayspacing;
		client->x = w;
		XMoveResizeWindow(dpy, client->win, client->x, 0, client->w, client->h);
		w += client->w;
		if (client->mon != m) client->mon = m;
	}
	w = w ? w + systrayspacing : 1;
	x -= w;
	XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
	window_changes.x = x; window_changes.y = m->by; window_changes.width = w; window_changes.height = bh;
	window_changes.stack_mode = Above; window_changes.sibling = m->bar_win;
	XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &window_changes);
	XMapWindow(dpy, systray->win);
	XMapSubwindows(dpy, systray->win);

	/* Redraw background */
	XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
	XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
	XSync(dpy, False);
}

void update_title(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name)) {
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	}
        /* Hack to mark broken clients */
	if (c->name[0] == '\0') {
		strcpy(c->name, stext);
	}
}

void update_window_type(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen]) { 
		set_fullscreen(c, 1);
	}
	if (wtype == netatom[NetWMWindowTypeDialog]) {
		c->is_floating = 1;
	}
}

void update_wm_hints(Client *c)
{
	XWMHints *wm_hints;

	if ((wm_hints = XGetWMHints(dpy, c->win))) {
		if (c == first_monitor->selected_client && wm_hints->flags & XUrgencyHint) {
			wm_hints->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wm_hints);
		} else {
			c->is_urgent = (wm_hints->flags & XUrgencyHint) ? true : false;
		}
		if (wm_hints->flags & InputHint) {
		    c->never_focus = !wm_hints->input;
		} else { 
		    c->never_focus = false;
		}
		XFree(wm_hints);
	}
}

void view(const Arg *arg)
{
	unsigned int i;
	unsigned int tmp_tag;

	if ((arg->ui & TAGMASK) == first_monitor->tagset[first_monitor->seltags]) {
		return;
	}
	
        /* Toggle sel tagset */
	first_monitor->seltags ^= 1; 
	if (arg->ui & TAGMASK) {
		first_monitor->tagset[first_monitor->seltags] = arg->ui & TAGMASK;
		first_monitor->pertag->previous_tag = first_monitor->pertag->current_tag;

		if (arg->ui == ~0) {
			first_monitor->pertag->current_tag = 0;
		} else {
			for (i = 0; !(arg->ui & 1 << i); i++);
			first_monitor->pertag->current_tag = i + 1;
		}
	} else {
		tmp_tag = first_monitor->pertag->previous_tag;
		first_monitor->pertag->previous_tag = first_monitor->pertag->current_tag;
		first_monitor->pertag->current_tag = tmp_tag;
	}

	first_monitor->master_factor = first_monitor->pertag->master_factors[first_monitor->pertag->current_tag];
	first_monitor->selected_layout = first_monitor->pertag->selected_layouts[first_monitor->pertag->current_tag];
	first_monitor->lt[first_monitor->selected_layout] = first_monitor->pertag->ltidxs[first_monitor->pertag->current_tag][first_monitor->selected_layout];
	first_monitor->lt[first_monitor->selected_layout^1] = first_monitor->pertag->ltidxs[first_monitor->pertag->current_tag][first_monitor->selected_layout^1];

	focus(NULL);
	arrange(first_monitor);
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

/* There's no way to check accesses to destroyed windows, thus those cases are ignored (especially on UnmapNotify's). 
 * Other types of errors call Xlibs default error handler, which may call exit. 
 */
int xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "ndwm: fatal error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* May call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

int xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("ndwm: another window manager is already running");
	return -1;
}

void tag(const Arg *arg)
{
	if (first_monitor->selected_client && arg->ui & TAGMASK) {
		first_monitor->selected_client->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(first_monitor);
	}
}

int main(void)
{
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
		fputs("warning: no locale support\n", stderr);
	}
	if (!(dpy = XOpenDisplay(NULL))) {
		die("ndwm: cannot open display");
	}
	check_another_wm_running();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1) {
		die("pledge");
	}
#endif
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}

void tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	Client *c;

	for (n = 0, c = next_tiled_client(m->clients); c; c = next_tiled_client(c->next), n++);
	if (n == 0) {
	    return;
	}

	if (n > 1) { 
	    mw = m->ww * m->master_factor;
	} else { 
	    mw = m->ww;
	}

	for (i = my = ty = 0, c = next_tiled_client(m->clients); c; c = next_tiled_client(c->next), i++)
		if (i < 1) {
			h = (m->wh - my) / (MIN(n, 1) - i);
			resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0);
			if (my + HEIGHT(c) < m->wh) my += HEIGHT(c);
		} else {
			h = (m->wh - ty) / (n - i);
			resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), 0);
			if (ty + HEIGHT(c) < m->wh) ty += HEIGHT(c);
		}
}

