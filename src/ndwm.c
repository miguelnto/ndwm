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
#include "types/arg.h"
#include "types/button.h"
#include "types/key.h"
#include "types/client.h"
#include "systray.h"
#include "monitor.h"
#include "xerror.h"

/* MACROS */
#define BUTTONMASK                  (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)             (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define MOUSEMASK                   (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                    ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)                   ((X)->h + 2 * (X)->bw)
#define TAGMASK                     ((1 << TAGS_LEN) - 1)
#define TEXTW(X)                    (drw_fontset_getwidth(drw, (X)) + lrpad)
#define ISVISIBLE(C)                ((C->tags & first_monitor->tagset[first_monitor->seltags]))
#define SYSTEM_TRAY_REQUEST_DOCK    0

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON          10
#define XEMBED_MAPPED               1
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2
#define XEMBED_EMBEDDED_VERSION     0

enum { CurNormal, CurResize, CurMove, CurLast }; /* Cursor */
enum { SchemeNorm, SchemeSel }; /* Color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* Default atoms */
enum { ClkTagBar, ClkClientWin, ClkRootWin }; /* Clicks */


/* Init and deinit functions, following the Zig memory management pattern. */
static Systray *systray_init(Monitor *m, XSetWindowAttributes *window_attrs);
static Monitor *monitor_init(void);
static Client *client_init(void);
static void monitor_deinit(Display *dpy, Monitor *m);
static void systray_deinit(Display *display, Systray *systray);

static void arrange(Monitor *m);
static void configure(Display *dpy, Client *c);
static void draw_bar(Monitor *m);
static Client *next_tiled_client(Client *c);
static void restack(Monitor *m);
static void scan(void);
static bool send_event(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void showhide(Client *c);
static Client *window_to_client(Window w);
static void make_master(const Arg *arg);
static void move_client_next(const Arg *arg);
static void rotate_clients(const Arg *arg);

/* Setup function */
static void setup(void);

/* Other systray-related functions */
static void remove_systray_icon(Systray *systray, Client *c);
static unsigned int get_systray_width(Systray *systray);
static Client *window_to_systray_icon(Systray *systray, Window w);

/* Focus and manage functions */
static void focus(Display *dpy, Monitor *mon, Window root, Client *c);
static void unfocus(Client *c, bool set_focus);
static void manage(Window w, const XWindowAttributes *wa);
static void unmanage(Client *c, bool destroyed);

/* Grab functions */
static void grab_buttons(Client *c, bool focused);
static void grab_keys(void);

/* Apply functions */
static void apply_rules(Display *dpy, Monitor *mon, Client *c);
static bool apply_size_hints(const Monitor *mon, Client *c, int *x, int *y, int *w, int *h, bool interact);

/* Get functions */
static int get_root_ptr(int *x, int *y);
static long get_state(Window w);
static Atom get_atom_prop(Client *c, Atom prop);
static bool get_text_prop(Window w, Atom atom, char *text, unsigned int size);

/* Set functions */
static void set_focus(Display *dpy, Client *c);
static void set_urgent(Client *c, bool urg);
static void set_client_state(Client *c, long state);
static void set_fullscreen(Display *dpy, Monitor *mon, Client *c, bool fullscreen);

/* Resize functions */
static void resize(Client *c, int x, int y, int w, int h, bool interact);
static void resize_bar_win(Display *display, Monitor *m, Systray *systray);
static void resize_client(Display *dpy, Monitor *mon, Client *c, int x, int y, int w, int h);

/* Cleanup functions */
static void cleanup(void);

/* Attach and detach functions */
static void attach(Monitor *mon, Client *c);
static void attach_stack(Monitor *mon, Client *c);
static void detach(Monitor *mon, Client *c);
static void detach_stack(Monitor *mon, Client *c);

/* Update functons */
static void update_bar(Display *dpy, Window root, Monitor *mon);
static void update_client_list(void);
static bool update_geometry(void);
static void update_numlock_mask(unsigned int *numlockmask);
static void update_size_hints(Client *c);
static void update_status(void);
static void update_systray(Display *dpy, Monitor *m);
static void update_systray_icon_geometry(const Monitor *mon, Client *client, int w, int h);
static void update_systray_icon_state(Client *i, const XPropertyEvent *ev);
static void update_title(Client *c);
static void update_window_type(Client *c);
static void update_wm_hints(Display *dpy, const Monitor *mon, Client *c);
static void update_bar_pos(Monitor *m);

/* Key commands */
static void focus_previous(const Arg *arg);
static void focus_next(const Arg *arg);
static void toggle_fullscreen(const Arg *arg);
static void quit(const Arg *arg);
static void view(const Arg *arg);
static void spawn(const Arg *arg);
static void toggle_floating(const Arg *arg);
static void go_to_left_tag(const Arg *arg);
static void go_to_right_tag(const Arg *arg);
static void move_client_to_right_tag(const Arg *arg);
static void move_client_to_left_tag(const Arg *arg);
static void destroy_client(const Arg *arg);
static void resize_with_mouse(const Arg *arg);
static void increase_master_width(const Arg *arg);
static void decrease_master_width(const Arg *arg);
static void tag(const Arg *arg);
static void move_with_mouse(const Arg *arg);

/* XEvent handlers */
static void button_press(XEvent *e);
static void client_message(XEvent *e);
static void configure_request(XEvent *e);
static void configure_notify(XEvent *e);
static void destroy_notify(XEvent *e);
static void enter_notify(XEvent *e);
static void expose(XEvent *e);
static void focus_in(XEvent *e);
static void key_press(XEvent *e);
static void mapping_notify(XEvent *e);
static void map_request(XEvent *e);
static void motion_notify(XEvent *e);
static void property_notify(XEvent *e);
static void resize_request(XEvent *e);
static void unmap_notify(XEvent *e);

/* XError utils */
static void check_another_wm_running(Display *dpy);
static int xerror(Display *dpy, XErrorEvent *ee);

/* Tile function */
static void tile(Monitor *m);

/* Handler */
static void (*handler[LASTEvent]) (XEvent *) = {
    [ButtonPress] = button_press,
    [ClientMessage] = client_message,
    [ConfigureRequest] = configure_request,
    [ConfigureNotify] = configure_notify,
    [DestroyNotify] = destroy_notify,
    [EnterNotify] = enter_notify,
    [Expose] = expose,
    [FocusIn] = focus_in,
    [KeyPress] = key_press,
    [MappingNotify] = mapping_notify,
    [MapRequest] = map_request,
    [MotionNotify] = motion_notify,
    [PropertyNotify] = property_notify,
    [ResizeRequest] = resize_request,
    [UnmapNotify] = unmap_notify
};

/* wmatom, netatom, xatom */
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];

/* Global variables */
static char stext[256];
static int screen;
static int screen_width, sh;           /* X display screen width and height */
static int lrpad;            /* Sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static bool running = true;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Window root, wmcheckwin;
static Systray *systray = NULL;
static Monitor *first_monitor = NULL;

/* Configuration, allows nested code to access above variables */
#include "config.h"

Systray *systray_init(Monitor *m, XSetWindowAttributes *window_attrs)
{
    Systray *new_systray = ecalloc(1, sizeof(Systray));
    new_systray->win = XCreateSimpleWindow(dpy, root, m->mx + m->mw, m->by, 1, m->bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
    window_attrs->event_mask        = ButtonPressMask | ExposureMask;
    window_attrs->override_redirect = True;
    window_attrs->background_pixel  = scheme[SchemeNorm][ColBg].pixel;
    XSelectInput(dpy, new_systray->win, SubstructureNotifyMask);
    XChangeProperty(dpy, new_systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
    XChangeWindowAttributes(dpy, new_systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, window_attrs);
    XMapRaised(dpy, new_systray->win);
    XSetSelectionOwner(dpy, netatom[NetSystemTray], new_systray->win, CurrentTime);
    return new_systray;
}

void rotate_clients(const Arg *arg)
{
    Client *c = next_tiled_client(first_monitor->clients);

    if (!first_monitor->selected_client) {
        return;
    }
    for (; c && next_tiled_client(c->next); c = next_tiled_client(c->next));
    if (c) {
        detach(first_monitor, c);
        attach(first_monitor, c);
        detach_stack(first_monitor, c);
        attach_stack(first_monitor, c);
        arrange(first_monitor);
        focus(dpy, first_monitor, root, c);
    }
}

void move_client_next(const Arg *arg)
{
    Client *selected_client = first_monitor->selected_client;
    if (!selected_client) {
        return;
    }

    Client *c = next_tiled_client(selected_client->next);

    if (c) {
        detach(first_monitor, selected_client);
        selected_client->next = c->next;
        c->next = selected_client;
    } else {
        detach(first_monitor, selected_client);
        attach(first_monitor, selected_client);
    }
    focus(dpy, first_monitor, root, selected_client);
    arrange(first_monitor);
}

void button_press(XEvent *e)
{
    unsigned int i;
    Arg arg = {0};
    XButtonPressedEvent *ev = &e->xbutton;
    unsigned int click = ClkRootWin;
    Client *client = window_to_client(ev->window);

    /* Focus monitor if necessary */
    unfocus(first_monitor->selected_client, true);
    focus(dpy, first_monitor, root, NULL);
    if (ev->window == first_monitor->bar_win) {
        unsigned int x = 0;
        i = x;
        do {
            x += TEXTW(tags[i]);
        } while (ev->x >= x && ++i < TAGS_LEN);
        if (i < TAGS_LEN) {
            click = ClkTagBar;
            arg.ui = 1 << i;
        } 
    } else if (client) {
        focus(dpy, first_monitor, root, client);
        restack(first_monitor);
        XAllowEvents(dpy, ReplayPointer, CurrentTime);
        click = ClkClientWin;
    }
    for (i = 0; i < LENGTH(buttons); i++) {
        if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
        && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state)) {
            buttons[i].func(&arg);
        }
    }
}

void attach_stack(Monitor *mon, Client *c)
{
    c->stack_next = mon->stack;
    mon->stack = c;
}

void attach(Monitor *mon, Client *c)
{
    c->next = mon->clients;
    mon->clients = c;
}

Client *window_to_systray_icon(Systray *systray, Window w) 
{
    Client *client = systray->icons;

    if (!w) { 
        return client;
    }
    for (; client && client->win != w; client = client->next);
    return client;
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

void update_bar_pos(Monitor *m)
{
    m->wy = m->my;
    m->wh = m->mh;
    m->wh -= m->bh;
    m->by = m->top_bar ? m->wy : m->wy + m->wh;
    m->wy = m->top_bar ? m->wy + m->bh : m->wy;
}

void remove_systray_icon(Systray *systray, Client *c)
{
    Client **icon = &systray->icons;

    for (; *icon && *icon != c; icon = &(*icon)->next);
    *icon = c->next;
    free(c);
}

unsigned int get_systray_width(Systray *systray)
{
    unsigned int systray_width = 0;
    for (Client *icon = systray->icons; icon; systray_width += icon->w + systrayspacing, icon = icon->next);
    return (systray_width != 0) ? systray_width + systrayspacing : 1;
}

void apply_rules(Display *dpy, Monitor *mon, Client *c)
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
    c->tags = (c->tags & TAGMASK) ? c->tags & TAGMASK : mon->tagset[mon->seltags];
}

bool apply_size_hints(const Monitor *mon, Client *c, int *x, int *y, int *w, int *h, bool interact)
{
    /* Set minimum possible */
    *w = MAX(1, *w);
    *h = MAX(1, *h);
    if (interact) {
        if (*x > screen_width) {
            *x = screen_width - WIDTH(c);
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
        if (*x >= mon->wx + mon->ww) {
            *x = mon->wx + mon->ww - WIDTH(c);
        }
        if (*y >= mon->wy + mon->wh) {
            *y = mon->wy + mon->wh - HEIGHT(c);
        }
        if (*x + *w + 2 * c->bw <= mon->wx) {
            *x = mon->wx;
        }
        if (*y + *h + 2 * c->bw <= mon->wy) {
            *y = mon->wy;
        }
    }
    if (*h < mon->bh) {
        *h = mon->bh;
    }
    if (*w < mon->bh) {
        *w = mon->bh;
    }
    if (c->is_floating) {
        /* See last two sentences in ICCCM 4.1.2.3 */
        bool baseismin = c->basew == c->minw && c->baseh == c->minh;
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
    showhide(m->stack);
    tile(m);
    restack(m);
}

void systray_deinit(Display *display, Systray *systray)
{
    XUnmapWindow(display, systray->win);
    XDestroyWindow(display, systray->win);
    free(systray);
}

void cleanup(void)
{
    Arg a = {.ui = 0};
    size_t i;

    view(&a);
    unmanage(first_monitor->stack, false);
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    
    monitor_deinit(dpy, first_monitor);
    systray_deinit(dpy, systray);
    
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

void monitor_deinit(Display *dpy, Monitor *m)
{
    XUnmapWindow(dpy, m->bar_win);
    XDestroyWindow(dpy, m->bar_win);
    free(m);
}

Client *client_init(void) {
    Client *new_client = (Client *)ecalloc(1, sizeof(Client));
    return new_client;
}

void client_message(XEvent *e)
{
    XWindowAttributes wa;
    XSetWindowAttributes swa;
    XClientMessageEvent *cme = &e->xclient;
    Client *c = window_to_client(cme->window);

    if (cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
        /* Add systray icons */
        if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
            c = client_init();
            if (!(c->win = cme->data.l[2])) {
                free(c);
                return;
            }
            c->next = systray->icons;
            systray->icons = c;
            if (!XGetWindowAttributes(dpy, c->win, &wa)) {
                /* Use sane defaults */
                wa.width = first_monitor->bh;
                wa.height = first_monitor->bh;
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
            update_systray_icon_geometry(first_monitor, c, wa.width, wa.height);
            XAddToSaveSet(dpy, c->win);
            XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
            XReparentWindow(dpy, c->win, systray->win, 0, 0);
            /* Use parents background color */
            swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
            XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
            send_event(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
            /* TODO: Do we need to send these events? */
            send_event(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
            send_event(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
            send_event(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
            XSync(dpy, False);
            resize_bar_win(dpy, first_monitor, systray);
            update_systray(dpy, first_monitor);
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
            set_fullscreen(dpy, first_monitor, c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->is_fullscreen)));
        }
    } else if (cme->message_type == netatom[NetActiveWindow]) {
        unsigned int i = 0;
        for (; i < TAGS_LEN && !((1 << i) & c->tags); i++);
        if (i < TAGS_LEN) {
            const Arg a = {.ui = 1 << i};
            view(&a);
            focus(dpy, first_monitor, root,  c);
            restack(first_monitor);
        }
    }
}

void configure(Display *dpy, Client *c)
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
    const XConfigureEvent *ev = &e->xconfigure;

    /* TODO: update_geometry handling needs to be simplified */
    if (ev->window == root) {
        bool dirty = (screen_width != ev->width || sh != ev->height);
        screen_width = ev->width;
        sh = ev->height;
        if (update_geometry() || dirty) {
            drw_resize(drw, screen_width, first_monitor->bh);
            update_bar(dpy, root, first_monitor);
            for (Client *client = first_monitor->clients; client; client = client->next) {
                if (client->is_fullscreen) {
                    resize_client(dpy, first_monitor, client, first_monitor->mx, first_monitor->my, first_monitor->mw, first_monitor->mh);
                }
            }
            resize_bar_win(dpy, first_monitor, systray);
            focus(dpy, first_monitor, root,  NULL);
            arrange(first_monitor);
        }
    }
}

void configure_request(XEvent *e)
{
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    Client *c = window_to_client(ev->window);

    if (c) {
        if (ev->value_mask & CWBorderWidth) {
            c->bw = ev->border_width;
        } else if (c->is_floating) {
            if (ev->value_mask & CWX) {
                c->oldx = c->x;
                c->x = first_monitor->mx + ev->x;
            }
            if (ev->value_mask & CWY) {
                c->oldy = c->y;
                c->y = first_monitor->my + ev->y;
            }
            if (ev->value_mask & CWWidth) {
                c->oldw = c->w;
                c->w = ev->width;
            }
            if (ev->value_mask & CWHeight) {
                c->oldh = c->h;
                c->h = ev->height;
            }
            if ((c->x + c->w) > first_monitor->mx + first_monitor->mw && c->is_floating) {
                c->x = first_monitor->mx + (first_monitor->mw / 2 - WIDTH(c) / 2); /* Center in x direction */
            }
            if ((c->y + c->h) > first_monitor->my + first_monitor->mh && c->is_floating) {
                c->y = first_monitor->my + (first_monitor->mh / 2 - HEIGHT(c) / 2); /* Center in y direction */
            }
            if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight))) {
                configure(dpy, c);
            }
            if (ISVISIBLE(c)) {
                XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
            }
        } else {
            configure(dpy, c);
        }
    } else {
        XWindowChanges wc;
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

Monitor *monitor_init(void)
{
    Monitor *new_monitor = ecalloc(1, sizeof(Monitor));

    new_monitor->tagset[0] = new_monitor->tagset[1] = 1;
    new_monitor->master_factor = master_factor;
    new_monitor->top_bar = top_bar;
    new_monitor->bh = drw->fonts->h + 2;
    new_monitor->pertag = ecalloc(1, sizeof(Pertag));
    new_monitor->pertag->current_tag = new_monitor->pertag->previous_tag = 1;

    for (unsigned int i = 0; i <= TAGS_LEN; i++) {
        new_monitor->pertag->master_factors[i] = new_monitor->master_factor;
    }
    return new_monitor;
}

void destroy_notify(XEvent *e)
{
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    Client *client = window_to_client(ev->window);

    if (client) {
        unmanage(client, true);
    } else if ((client = window_to_systray_icon(systray, ev->window))) {
        remove_systray_icon(systray, client);
        resize_bar_win(dpy, first_monitor, systray);
        update_systray(dpy, first_monitor);
    }
}

void detach(Monitor *mon, Client *c)
{
    Client **tc;

    for (tc = &mon->clients; *tc && *tc != c; tc = &(*tc)->next);
    *tc = c->next;
}

void detach_stack(Monitor *mon, Client *c)
{
    Client **tc, *t;

    for (tc = &mon->stack; *tc && *tc != c; tc = &(*tc)->stack_next);
    *tc = c->stack_next;

    if (c == mon->selected_client) {
        for (t = mon->stack; t && !ISVISIBLE(t); t = t->stack_next);
        mon->selected_client = t;
    }
}

void draw_bar(Monitor *m)
{
    int boxs = drw->fonts->h / 9;
    int boxw = drw->fonts->h / 6 + 2;
    unsigned int occ = 0, urg = 0;

    int stw = get_systray_width(systray);
    /* Draw status first so it can be overdrawn by tags later */
    drw->scheme = scheme[SchemeNorm];
    int sw = TEXTW(stext) - lrpad / 2 + 2; /* 2px right padding */
    drw_text(drw, m->ww - sw - stw, 0, sw, m->bh, lrpad / 2 - 2, stext, 0);

    resize_bar_win(dpy, m, systray);
    for (Client *client = m->clients; client; client = client->next) {
        occ |= client->tags;
        if (client->is_urgent) {
            urg |= client->tags;
        }
    }
    int x = 0;
    int w;
    for (unsigned int i = 0; i < TAGS_LEN; i++) {
        w = TEXTW(tags[i]);
        drw->scheme = scheme[(m->tagset[m->seltags] & 1 << i) ? SchemeSel : SchemeNorm];
        drw_text(drw, x, 0, w, m->bh, lrpad / 2, tags[i], urg & 1 << i);
        if (occ & 1 << i) {
            drw_rect(drw, x + boxs, boxs, boxw, boxw,
                first_monitor->selected_client && first_monitor->selected_client->tags & 1 << i,urg & 1 << i);
        }
        x += w;
    }
    w = 0;
    drw->scheme = scheme[SchemeNorm];

    if ((w = m->ww - sw - stw - x) > m->bh) {
        if (m->selected_client && show_title) {
            drw->scheme = scheme[SchemeSel];
            drw_text(drw, x, 0, w, m->bh, lrpad / 2, m->selected_client->name, 0);
            if (m->selected_client->is_floating) {
                drw_rect(drw, x + boxs, boxs, boxw, boxw, m->selected_client->is_fixed, 0);
            }
        } else {
            drw->scheme = scheme[SchemeNorm];
            drw_rect(drw, x, 0, w, m->bh, 1, 1);
        }
    }
    drw_map(drw, m->bar_win, 0, 0, m->ww - stw, m->bh);
}

void enter_notify(XEvent *e)
{
    XCrossingEvent *ev = &e->xcrossing;

    if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root) {
        return;
    }
    Client *client = window_to_client(ev->window);
    if (!client || client == first_monitor->selected_client) {
        return;
    }
    focus(dpy, first_monitor, root,client);
}

void expose(XEvent *e)
{
    if (e->xexpose.count == 0) {
        draw_bar(first_monitor);
        update_systray(dpy, first_monitor);
    }
}

void focus(Display *dpy, Monitor *mon, Window root, Client *c)
{
    if (!c || !ISVISIBLE(c)) {
        for (c = mon->stack; c && !ISVISIBLE(c); c = c->stack_next);
    }
    if (mon->selected_client && mon->selected_client != c) {
        unfocus(mon->selected_client, false);
    }
    if (c) {
        if (c->is_urgent) {
            set_urgent(c, false);
        }
        detach_stack(mon, c);
        attach_stack(mon, c);
        grab_buttons(c, true);
        XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
        set_focus(dpy, c);
    } else {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
    mon->selected_client = c;
    draw_bar(mon);
}

void focus_in(XEvent *e)
{
    if (first_monitor->selected_client && e->xfocus.window != first_monitor->selected_client->win) {
        set_focus(dpy, first_monitor->selected_client);
    }
}

void focus_next(const Arg *arg) 
{
    if (!first_monitor->selected_client) {
        return;
    }
    Client *c = first_monitor->selected_client->next;
    for (; c && !ISVISIBLE(c); c = c->next);
    if (!c) {
        for (c = first_monitor->clients; c && !ISVISIBLE(c); c = c->next);
    }
    if (c) {
        focus(dpy, first_monitor, root,  c);
        restack(first_monitor);
    }
}

void make_master(const Arg *arg)
{
    Client *c = first_monitor->selected_client;

    if (!c || c->is_floating || c->is_fullscreen || c == next_tiled_client(first_monitor->clients))  {
        return;
    }
    detach(first_monitor, c);
    attach(first_monitor, c);
    focus(dpy, first_monitor, root, c);
    arrange(first_monitor);
}

void focus_previous(const Arg *arg)
{
    if (!first_monitor->selected_client) {
        return;
    }
    Client *c = NULL;
    Client *i = first_monitor->clients;
    for (; i != first_monitor->selected_client; i = i->next) {
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
    if (c) {
        focus(dpy, first_monitor, root,  c);
        restack(first_monitor);
    }
}

Atom get_atom_prop(Client *c, Atom prop)
{
    int di;
    unsigned long dl;
    unsigned char *p = NULL;
    Atom da, atom = None;
    if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM, &da, &di, &dl, &dl, &p) == Success && p) {
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


bool get_text_prop(Window w, Atom atom, char *text, unsigned int size)
{
    char **list = NULL;
    XTextProperty name;

    if (!text || size == 0) {
        return false;
    }
    text[0] = '\0';
    if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems) {
        return false;
    }
    if (name.encoding == XA_STRING) {
        strncpy(text, (char *)name.value, size - 1);
    } else {
        int n;
        if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
            strncpy(text, *list, size - 1);
            XFreeStringList(list);
        }
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return true;
}

void grab_buttons(Client *c, bool focused)
{
    update_numlock_mask(&numlockmask);
    {
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        if (!focused) {
            XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
        }
        for (unsigned int i = 0; i < LENGTH(buttons); i++) {
            if (buttons[i].click == ClkClientWin) {
                for (unsigned int j = 0; j < LENGTH(modifiers); j++) {
                    XGrabButton(dpy, buttons[i].button,
                        buttons[i].mask | modifiers[j], c->win, False, BUTTONMASK,
                        GrabModeAsync, GrabModeSync, None, None);
                }
            }
        }
    }
}

void grab_keys(void)
{
    update_numlock_mask(&numlockmask);
    {
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        KeyCode code;

        XUngrabKey(dpy, AnyKey, AnyModifier, root);
        for (unsigned int i = 0; i < LENGTH(keys); i++) {
            if ((code = XKeysymToKeycode(dpy, keys[i].key_symbol))) {
                for (unsigned int j = 0; j < LENGTH(modifiers); j++) {
                    XGrabKey(dpy, code, keys[i].mod | modifiers[j], root, True, GrabModeAsync, GrabModeAsync);
                }
            }
        }
    }
}

void key_press(XEvent *e)
{
    XKeyEvent *ev = &e->xkey;
    KeySym keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
    for (unsigned int i = 0; i < LENGTH(keys); i++) {
        if (keysym == keys[i].key_symbol
        && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
        && keys[i].func) {
            keys[i].func(&(keys[i].arg));
        }
    }
}

void destroy_client(const Arg *arg)
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

void manage(Window w, const XWindowAttributes *wa)
{
    Client *t = NULL;
    Window trans = None;
    XWindowChanges wc;

    Client *c = client_init();
    c->win = w;
    /* Geometry */
    c->x = c->oldx = wa->x;
    c->y = c->oldy = wa->y;
    c->w = c->oldw = wa->width;
    c->h = c->oldh = wa->height;
    c->oldbw = wa->border_width;

    update_title(c);
    if (XGetTransientForHint(dpy, w, &trans) && (t = window_to_client(trans))) {
        c->tags = t->tags;
    } else {
        apply_rules(dpy, first_monitor, c);
    }

    if (c->x + WIDTH(c) > first_monitor->wx + first_monitor->ww) {
        c->x = first_monitor->wx + first_monitor->ww - WIDTH(c);
    }
    if (c->y + HEIGHT(c) > first_monitor->wy + first_monitor->wh) {
        c->y = first_monitor->wy + first_monitor->wh - HEIGHT(c);
    }
    c->x = MAX(c->x, first_monitor->wx);
    c->y = MAX(c->y, first_monitor->wy);
    c->bw = borderpx;

    wc.border_width = c->bw;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
    configure(dpy, c); /* Propagates border_width, if size doesn't change */
    update_window_type(c);
    update_size_hints(c);
    update_wm_hints(dpy, first_monitor, c);
    c->x = first_monitor->mx + (first_monitor->mw - WIDTH(c)) / 2;
    c->y = first_monitor->my + (first_monitor->mh - HEIGHT(c)) / 2;
    c->sfx = c->x;
    c->sfy = c->y;
    c->sfw = c->w;
    c->sfh = c->h;
    XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
    grab_buttons(c, false);
    if (!c->is_floating) {
        c->is_floating = (c->oldstate = trans != None || c->is_fixed);
    }
    if (c->is_floating) {
        XRaiseWindow(dpy, c->win);
    }
    attach(first_monitor, c);
    attach_stack(first_monitor, c);
    XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *) &(c->win), 1);
    XMoveResizeWindow(dpy, c->win, c->x + 2 * screen_width, c->y, c->w, c->h); /* Some windows require this */
    set_client_state(c, NormalState);
    unfocus(first_monitor->selected_client, false);
    first_monitor->selected_client = c;
    arrange(first_monitor);
    XMapWindow(dpy, c->win);
    focus(dpy, first_monitor, root, NULL);
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
    Client *i = window_to_systray_icon(systray, ev->window);
    if (i) {
        send_event(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
        resize_bar_win(dpy, first_monitor, systray);
        update_systray(dpy, first_monitor);
    }

    if (!XGetWindowAttributes(dpy, ev->window, &wa)) {
        return;
    }
    if (!wa.override_redirect && !window_to_client(ev->window)) {
        manage(ev->window, &wa);
    }
}

/* TODO: I should do something about this */
void motion_notify(XEvent *e)
{
    return;
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
    if (client->is_fullscreen) {
        return;
    }
    restack(first_monitor);
    ocx = client->x;
    ocy = client->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess) {
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
            if ((ev.xmotion.time - last_time) <= (1000 / 60)) {
                continue;
            }
            last_time = ev.xmotion.time;

            nx = ocx + (ev.xmotion.x - x);
            ny = ocy + (ev.xmotion.y - y);
            if (!client->is_floating) {
                toggle_floating(NULL);
            }
            if (client->is_floating) {
                resize(client, nx, ny, client->w, client->h, true);
            }
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

    if ((c = window_to_systray_icon(systray, ev->window))) {
        if (ev->atom == XA_WM_NORMAL_HINTS) {
            update_size_hints(c);
            update_systray_icon_geometry(first_monitor, c, c->w, c->h);
        } else {
            update_systray_icon_state(c, ev);
        }
        resize_bar_win(dpy, first_monitor, systray);
        update_systray(dpy, first_monitor);
    }
    if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
        update_status();

    } else if (ev->state == PropertyDelete) {
        return; 
    } else if ((c = window_to_client(ev->window))) {
        switch(ev->atom) {
        default: break;
        case XA_WM_TRANSIENT_FOR:
            if (!c->is_floating && (XGetTransientForHint(dpy, c->win, &trans)) &&
                (c->is_floating = (window_to_client(trans)) != NULL)) {
                arrange(first_monitor);
            }
            break;
        case XA_WM_NORMAL_HINTS:
            update_size_hints(c);
            break;
        case XA_WM_HINTS:
            update_wm_hints(dpy, first_monitor, c);
            draw_bar(first_monitor);
            break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
            update_title(c);
                if (c == first_monitor->selected_client && show_title) {
                draw_bar(first_monitor);
                }
        }
        if (ev->atom == netatom[NetWMWindowType]) {
            update_window_type(c);
        }
    }
}

void quit(const Arg *arg)
{
    running = false;
}

void resize(Client *c, int x, int y, int w, int h, bool interact)
{
    if (apply_size_hints(first_monitor, c, &x, &y, &w, &h, interact)) {
        resize_client(dpy, first_monitor, c, x, y, w, h);
    }
}

void resize_bar_win(Display *display, Monitor *m, Systray *systray)
{
    XMoveResizeWindow(display, m->bar_win, m->wx, m->by, m->ww - get_systray_width(systray), m->bh);
}

void resize_client(Display *dpy, Monitor *mon, Client *c, int x, int y, int w, int h)
{
    XWindowChanges window_changes;

    c->oldx = c->x; c->x = window_changes.x = x;
    c->oldy = c->y; c->y = window_changes.y = y;
    c->oldw = c->w; c->w = window_changes.width = w;
    c->oldh = c->h; c->h = window_changes.height = h;
    window_changes.border_width = c->bw;

    if ((next_tiled_client(mon->clients) == c) && !(next_tiled_client(c->next))) {
        mon->master_factor = master_factor;
    }

    if (((next_tiled_client(mon->clients) == c && !next_tiled_client(c->next))) && (!c->is_fullscreen) && !c->is_floating) {
        c->w = window_changes.width += c->bw * 2;
        c->h = window_changes.height += c->bw * 2;
        window_changes.border_width = 0;
    }
    XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &window_changes);
    configure(dpy, c);
    XSync(dpy, False);
}

void resize_with_mouse(const Arg *arg)
{
    int ocx, ocy, nw, nh;
    Client *c;
    XEvent ev;
    Time last_time = 0;

    if (!(c = first_monitor->selected_client)) {
        return;
    }
    if (c->is_fullscreen) { 
        return;
    }
    restack(first_monitor);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess) {
        return;
    }
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
            if ((ev.xmotion.time - last_time) <= (1000 / 60)) {
                continue;
            }
            last_time = ev.xmotion.time;

            nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
            nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
            if (first_monitor->wx + nw >= first_monitor->wx && first_monitor->wx + nw <= first_monitor->wx + first_monitor->ww
            && first_monitor->wy + nh >= first_monitor->wy && first_monitor->wy + nh <= first_monitor->wy + first_monitor->wh) {
                if (!c->is_floating) {
                    toggle_floating(NULL);
                }
            }
            if (c->is_floating) {
                resize(c, c->x, c->y, nw, nh, true);
            }
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
    Client *i = window_to_systray_icon(systray, event->window);

    if (i) {
        update_systray_icon_geometry(first_monitor, i, event->width, event->height);
        resize_bar_win(dpy, first_monitor, systray);
        update_systray(dpy, first_monitor);
    }
}

void restack(Monitor *m)
{
    XEvent ev;
    XWindowChanges wc;

    draw_bar(m);
    if (!m->selected_client) {
        return;
    }
    if (m->selected_client->is_floating) {
        XRaiseWindow(dpy, m->selected_client->win);
    }
    wc.stack_mode = Below;
    wc.sibling = m->bar_win;
    for (Client *c = m->stack; c; c = c->stack_next) {
        if (!c->is_floating && ISVISIBLE(c)) {
            XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
            wc.sibling = c->win;
        }
    }
    XSync(dpy, False);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void scan(void)
{
    unsigned int num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;

    if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
        unsigned int i;
        for (i = 0; i < num; i++) {
            if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1)) {
                continue;
            }
            if (wa.map_state == IsViewable || get_state(wins[i]) == IconicState) {
                manage(wins[i], &wa);
            }
        }
        for (i = 0; i < num; i++) { 
            /* Now the transients */
            if (!XGetWindowAttributes(dpy, wins[i], &wa)) {
                continue;
            }
            if (XGetTransientForHint(dpy, wins[i], &d1) && (wa.map_state == IsViewable || get_state(wins[i]) == IconicState)) {
                manage(wins[i], &wa);
            }
        }
        if (wins) { 
            XFree(wins);
        }
    }
}

void set_client_state(Client *c, long state)
{
    long data[] = { state, None };

    XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32, PropModeReplace, (unsigned char *)data, 2);
}

bool send_event(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
    Atom mt;
    int exists = 0;

    if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
        mt = wmatom[WMProtocols];
        int n;
        Atom *protocols;
        if (XGetWMProtocols(dpy, w, &protocols, &n)) {
            while (!exists && n--) {
                exists = protocols[n] == proto;
            }
            XFree(protocols);
        }
    } else {
        exists = True;
        mt = proto;
    }
    if (exists) {
        XEvent ev;
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

void set_focus(Display *dpy, Client *c)
{
    if (!c->never_focus) {
        XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
        XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &(c->win), 1);
    }
    send_event(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void set_fullscreen(Display *dpy, Monitor *mon, Client *c, bool fullscreen)
{
    if (fullscreen && !c->is_fullscreen) {
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
        c->is_fullscreen = true;
        c->oldstate = c->is_floating;
        c->oldbw = c->bw;
        c->bw = 0;
        c->is_floating = true;
        resize_client(dpy, mon, c, mon->mx, mon->my, mon->mw, mon->mh);
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
        resize_client(dpy, mon, c, c->x, c->y, c->w, c->h);
        arrange(mon);
    }
}

void increase_master_width(const Arg *arg)
{
    float f = first_monitor->master_factor + 0.02;
    if (f > 0.95) {
        return;
    }
    first_monitor->master_factor = first_monitor->pertag->master_factors[first_monitor->pertag->current_tag] = f;
    arrange(first_monitor);
}

void decrease_master_width(const Arg *arg)
{
    float f = first_monitor->master_factor - 0.02;
    if (f < 0.05) {
        return;
    }
    first_monitor->master_factor = first_monitor->pertag->master_factors[first_monitor->pertag->current_tag] = f;
    arrange(first_monitor);
}

void setup(void)
{
    XSetWindowAttributes wa;

    struct sigaction sa;
    /* Do not transform children into zombies when they terminate */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, NULL);

    /* Clean up any zombies (inherited from .xinitrc, etc) immediately */
    while (waitpid(-1, NULL, WNOHANG) > 0);

    /* Init screen */
    screen = DefaultScreen(dpy);
    screen_width = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    drw = drw_create(dpy, screen, root, screen_width, sh);

    if (!drw_fontset_create(drw, fonts, LENGTH(fonts))) {
        die("no fonts could be loaded.");
    }
    lrpad = drw->fonts->h;
    update_geometry();

    /* Init atoms */
    Atom utf8string = XInternAtom(dpy, "UTF8_STRING", False);
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
    for (unsigned int i = 0; i < LENGTH(colors); i++) {
        scheme[i] = drw_scm_create(drw, colors[i], 3);
    }

    /* Init system tray */
    update_systray(dpy, first_monitor);

    /* Init bars */
    update_bar(dpy, root, first_monitor);
    update_status();

    /* Supporting window for NetWMCheck */
    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &wmcheckwin, 1);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *) "ndwm", 4);
    XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &wmcheckwin, 1);

    /* EWMH support per view */
    XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *) netatom, NetLast);
    XDeleteProperty(dpy, root, netatom[NetClientList]);

    /* Select events */
    wa.cursor = cursor[CurNormal]->cursor;
    wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
        |ButtonPressMask|PointerMotionMask|EnterWindowMask
        |LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
    XSelectInput(dpy, root, wa.event_mask);
    grab_keys();
    focus(dpy, first_monitor, root, NULL);
}

void set_urgent(Client *c, bool urg)
{
    XWMHints *wmh =  XGetWMHints(dpy, c->win);

    c->is_urgent = urg;
    if (!wmh) {
        return;
    }
    wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
    XSetWMHints(dpy, c->win, wmh);
    XFree(wmh);
}

void go_to_left_tag(const Arg *arg)
{
    Arg shifted;
    shifted.ui = first_monitor->tagset[first_monitor->seltags];
    shifted.ui = (shifted.ui >> 1 | shifted.ui << (TAGS_LEN - 1));
    view(&shifted);
}

void go_to_right_tag(const Arg *arg) 
{
    Arg shifted;
    shifted.ui = first_monitor->tagset[first_monitor->seltags];
    shifted.ui = (shifted.ui << 1) | (shifted.ui >> (TAGS_LEN - 1));
    view(&shifted);
}

void move_client_to_right_tag(const Arg *arg)
{
    Arg shifted;
    shifted.ui = first_monitor->tagset[first_monitor->seltags];
    shifted.ui = (shifted.ui << 1) | (shifted.ui >> (TAGS_LEN - 1));
    tag(&shifted);
}

void move_client_to_left_tag(const Arg *arg)
{
    Arg shifted;
    shifted.ui = first_monitor->tagset[first_monitor->seltags];
    shifted.ui = (shifted.ui >> 1 | shifted.ui << (TAGS_LEN - 1));
    tag(&shifted);
}

void showhide(Client *c)
{
    if (!c) {
        return;
    }
    if (ISVISIBLE(c)) { 
        /* Show clients top down */
        XMoveWindow(dpy, c->win, c->x, c->y);
        if (c->is_floating && !c->is_fullscreen) {
            resize(c, c->x, c->y, c->w, c->h, false);
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
        if (dpy) { 
            close(ConnectionNumber(dpy));
        }
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
        set_fullscreen(dpy, first_monitor, first_monitor->selected_client, !first_monitor->selected_client->is_fullscreen);
    }
}

void toggle_floating(const Arg *arg)
{
    if (!first_monitor->selected_client || first_monitor->selected_client->is_fullscreen) {
        return;
    }

    first_monitor->selected_client->is_floating = !first_monitor->selected_client->is_floating || first_monitor->selected_client->is_fixed;
    if (first_monitor->selected_client->is_floating) {
        /* Restore last known float dimensions */
        resize(first_monitor->selected_client, first_monitor->selected_client->sfx, first_monitor->selected_client->sfy,
               first_monitor->selected_client->sfw, first_monitor->selected_client->sfh, False);
    } else { 
        /* Save last known float dimensions */
        first_monitor->selected_client->sfx = first_monitor->selected_client->x;
        first_monitor->selected_client->sfy = first_monitor->selected_client->y;
        first_monitor->selected_client->sfw = first_monitor->selected_client->w;
        first_monitor->selected_client->sfh = first_monitor->selected_client->h;
    }
    arrange(first_monitor);
}

void unfocus(Client *c, bool set_focus)
{
    if (!c) {
        return;
    }
    grab_buttons(c, false);
    XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
    if (set_focus) {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
}

void unmanage(Client *c, bool destroyed)
{
    XWindowChanges wc;

    detach(first_monitor, c);
    detach_stack(first_monitor, c);

    if (!destroyed) {
        wc.border_width = c->oldbw;
        XGrabServer(dpy); /* Avoid race conditions */
        XSetErrorHandler(xerrordummy);
        XSelectInput(dpy, c->win, NoEventMask);
        XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* Restore border */
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        set_client_state(c, WithdrawnState);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
    free(c);
    focus(dpy, first_monitor, root, NULL);
    update_client_list();
    arrange(first_monitor);
}

void unmap_notify(XEvent *e)
{
    Client *client;
    XUnmapEvent *ev = &e->xunmap;

    if ((client = window_to_client(ev->window))) {
        if (ev->send_event) {
            set_client_state(client, WithdrawnState);
        } else {
            unmanage(client, false);
        }
    } else if ((client = window_to_systray_icon(systray, ev->window))) {
        /* Sometimes icons occasionally unmap their windows, but do not destroy them. We map those windows back */
        XMapRaised(dpy, client->win);
        update_systray(dpy, first_monitor);
    }
}

void update_bar(Display *dpy, Window root, Monitor *mon)
{
    if (mon->bar_win) {
        return;
    }
    XSetWindowAttributes window_attrs = {
        .override_redirect = True,
        .background_pixmap = ParentRelative,
        .event_mask = ButtonPressMask|ExposureMask
    };
    XClassHint ch = {"ndwm", "ndwm"};

    mon->bar_win = XCreateWindow(dpy, root, mon->wx, mon->by, mon->ww - get_systray_width(systray), mon->bh, 0, 
            DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
            CWOverrideRedirect|CWBackPixmap|CWEventMask, &window_attrs);
    XDefineCursor(dpy, mon->bar_win, cursor[CurNormal]->cursor);
    XMapRaised(dpy, systray->win);
    XMapRaised(dpy, mon->bar_win);
    XSetClassHint(dpy, mon->bar_win, &ch);
}

void update_client_list()
{
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    for (Client *client = first_monitor->clients; client; client = client->next) {
        XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *) &(client->win), 1);
    }
}

bool update_geometry(void)
{
    bool dirty = false;

     /* Default monitor setup */
    if (!first_monitor) {
        first_monitor = monitor_init();
    }
    if (first_monitor->mw != screen_width || first_monitor->mh != sh) {
        dirty = true;
        first_monitor->mw = first_monitor->ww = screen_width;
        first_monitor->mh = first_monitor->wh = sh;
        update_bar_pos(first_monitor);
    }
    return dirty;
}

void update_numlock_mask(unsigned int *numlockmask)
{
    *numlockmask = 0;
    XModifierKeymap *modmap = XGetModifierMapping(dpy);
    for (unsigned int i = 0; i < 8; i++) {
        for (unsigned int j = 0; j < modmap->max_keypermod; j++) {
            if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dpy, XK_Num_Lock)) {
                *numlockmask = (1 << i);
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
    if (!get_text_prop(root, XA_WM_NAME, stext, sizeof(stext))) { 
        strcpy(stext, "ndwm");
    }
    draw_bar(first_monitor);
    update_systray(dpy, first_monitor);
}

void update_systray_icon_geometry(const Monitor *mon, Client *client, int w, int h)
{
    if (!client) {
        return;
    }
    
    client->h = mon->bh;
    if (w == h) { 
        client->w = mon->bh;
    } else if (h == mon->bh) { 
        client->w = w;
    } else { 
        client->w = (int) ((float)mon->bh * ((float)w / (float)h));
    }
    apply_size_hints(mon, client, &(client->x), &(client->y), &(client->w), &(client->h), False);

    /* Force icons into the systray dimensions if they don't want to */ 
    if (client->h > mon->bh) {
        if (client->w == client->h) { 
            client->w = mon->bh;
        } else { 
            client->w = (int) ((float)mon->bh * ((float)client->w / (float)client->h));
        }
        client->h = mon->bh;
    }
}

void update_systray_icon_state(Client *i, const XPropertyEvent *ev)
{
    long flags;
    int code = XEMBED_WINDOW_ACTIVATE;

    if (!i || ev->atom != xatom[XembedInfo] || !(flags = get_atom_prop(i, xatom[XembedInfo]))) { 
        return;
    }
    if (flags & XEMBED_MAPPED && !i->tags) {
        i->tags = 1;
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

void update_systray(Display *dpy, Monitor *m)
{
    XSetWindowAttributes window_attrs;
    XWindowChanges window_changes;
    unsigned int x = m->mx + m->mw;

    if (!systray) {
        systray = systray_init(m, &window_attrs);
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
    unsigned int w = 0;
    for (Client *client = systray->icons; client; client = client->next) {
        /* Make sure the background color stays the same */
        window_attrs.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
        XChangeWindowAttributes(dpy, client->win, CWBackPixel, &window_attrs);
        XMapRaised(dpy, client->win);
        w += systrayspacing;
        client->x = w;
        XMoveResizeWindow(dpy, client->win, client->x, 0, client->w, client->h);
        w += client->w;
    }
    w = w ? w + systrayspacing : 1;
    x -= w;
    XMoveResizeWindow(dpy, systray->win, x, m->by, w, m->bh);
    window_changes.x = x; window_changes.y = m->by; window_changes.width = w; window_changes.height = m->bh;
    window_changes.stack_mode = Above; window_changes.sibling = m->bar_win;
    XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &window_changes);
    XMapWindow(dpy, systray->win);
    XMapSubwindows(dpy, systray->win);

    /* Redraw background */
    XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
    XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, m->bh);
    XSync(dpy, False);
}

void update_title(Client *c)
{
    if (!get_text_prop(c->win, netatom[NetWMName], c->name, sizeof c->name)) {
        get_text_prop(c->win, XA_WM_NAME, c->name, sizeof c->name);
    }
    /* Hack to mark broken clients */
    if (c->name[0] == '\0') {
        strcpy(c->name, stext);
    }
}

void update_window_type(Client *c)
{
    if (get_atom_prop(c, netatom[NetWMState]) == netatom[NetWMFullscreen]) { 
        set_fullscreen(dpy, first_monitor, c, true);
    }
    if (get_atom_prop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog]) {
        c->is_floating = true;
    }
}

void update_wm_hints(Display *dpy, const Monitor *mon, Client *c)
{
    XWMHints *wm_hints = XGetWMHints(dpy, c->win);

    if (wm_hints) {
        if (c == mon->selected_client && wm_hints->flags & XUrgencyHint) {
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
    if ((arg->ui & TAGMASK) == first_monitor->tagset[first_monitor->seltags]) {
        return;
    }
        /* Toggle sel tagset */
    first_monitor->seltags ^= 1; 
    if (arg->ui & TAGMASK) {
        first_monitor->tagset[first_monitor->seltags] = arg->ui & TAGMASK;
        first_monitor->pertag->previous_tag = first_monitor->pertag->current_tag;

        if (arg->ui == 0) {
            first_monitor->pertag->current_tag = 0;
        } else {
            unsigned int i = 0;
            for (; !(arg->ui & 1 << i); i++);
            first_monitor->pertag->current_tag = i + 1;
        }
    } else {
        unsigned int tmp_tag = first_monitor->pertag->previous_tag;
        first_monitor->pertag->previous_tag = first_monitor->pertag->current_tag;
        first_monitor->pertag->current_tag = tmp_tag;
    }

    first_monitor->master_factor = first_monitor->pertag->master_factors[first_monitor->pertag->current_tag];
    focus(dpy, first_monitor, root,  NULL);
    arrange(first_monitor);
}

void check_another_wm_running(Display *dpy)
{
    xerrorxlib = XSetErrorHandler(xerrorstart);

    /* This causes an error if some other window manager is running */
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XSync(dpy, False);
}

/* There's no way to check accesses to destroyed windows, thus those cases are ignored (especially on UnmapNotify's). 
 * Other types of errors call Xlibs default error handler, which may call exit. */
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
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable)) {
        return 0;
    }
    fprintf(stderr, "ndwm: fatal error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
    return xerrorxlib(dpy, ee); 
}

void tag(const Arg *arg)
{
    if (first_monitor->selected_client && arg->ui & TAGMASK) {
        first_monitor->selected_client->tags = arg->ui & TAGMASK;
        focus(dpy, first_monitor, root, NULL);
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
    check_another_wm_running(dpy);
    setup();
    scan();
    XEvent ev;
    /* Main event loop */
    XSync(dpy, False);
    while (running && !XNextEvent(dpy, &ev)) {
        if (handler[ev.type]) {
            /* Call handler */
            handler[ev.type](&ev);
        }
    }
    cleanup();
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}

void tile(Monitor *m)
{
    unsigned int i, n, h, mw, my, ty;
    Client *c = next_tiled_client(m->clients);

    for (n = 0; c; c = next_tiled_client(c->next), n++);
    if (n == 0) {
        return;
    }

    if (n > 1) { 
        mw = m->ww * m->master_factor;
    } else { 
        mw = m->ww;
    }

    for (i = my = ty = 0, c = next_tiled_client(m->clients); c; c = next_tiled_client(c->next), i++) {
        if (i < 1) {
            h = (m->wh - my) / (MIN(n, 1) - i);
            resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), false);
            if (my + HEIGHT(c) < m->wh) my += HEIGHT(c);
        } else {
            h = (m->wh - ty) / (n - i);
            resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw),false);
            if (ty + HEIGHT(c) < m->wh) ty += HEIGHT(c);
        }
    }
}

