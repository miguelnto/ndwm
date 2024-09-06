/* Appearance */
#include <X11/Xutil.h>

static const unsigned int borderpx    = 1;        /* Border pixel of windows */
static const bool top_bar             = true;        /* Default: bar is displayed on the top */
static const char *fonts[]            = { "Terminus (TTF):style=Medium:size=9",  /* Stable: JetBrains Mono:size=12 ; Japanese: IPAGothic:style=Regular */
					"FontAwesome:style=Regular:size=9:antialias=true:autohint=true",
};

static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_black[]       = "#000000";
static const char col_gray5[]       = "#9f9ea8";
static const char col_gray6[]       = "#1d2021";
static const char col_cyan[]        = "#005577";
static const char col_cyan2[]       = "#007777";
static const char col_cyan3[]       = "#006677";
static const char col_pink1[]       = "#E0115F";
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray6, NULL },
	[SchemeSel]  = { col_gray4, col_pink1, col_pink1 },
};

/* Tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* Layout */
static const float master_factor = 0.52; /* factor of master area size [0.05..0.95] */

static const Layout layouts[] = {
	{ tile },    /* First entry is default */
	{ NULL },    /* No layout function means floating behavior */
};

#define MODKEY Mod4Mask

#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
        { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \


#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

static const char *termcmd[]  = { "st", NULL };

#include <X11/XF86keysym.h>

static const Key keys[] = {

	/* Application hotkeys */
	{ MODKEY,                       XK_d,      spawn,          SHCMD("dmenu_run") },
	{ MODKEY,                       XK_Return, spawn,          { .v = termcmd } },
	{ MODKEY,                       XK_w,      spawn,          SHCMD("pcmanfm") },
	
	/* Media keys */
	{ 0,                       XF86XK_AudioMute,             spawn,          SHCMD("amixer sset Master toggle; pkill -RTMIN+10 sblocks") },
	{ 0,                       XF86XK_AudioRaiseVolume,      spawn,          SHCMD("amixer set Master 2%+; pkill -RTMIN+10 sblocks") },
	{ 0,                       XF86XK_AudioLowerVolume,      spawn,          SHCMD("amixer set Master 2%-; pkill -RTMIN+10 sblocks") },
	{ 0,                       XF86XK_MonBrightnessUp,       spawn,          SHCMD("brightnessctl set +10%; pkill -RTMIN+9 sblocks") },
	{ 0,                       XF86XK_MonBrightnessDown,     spawn,          SHCMD("brightnessctl set 10%-; pkill -RTMIN+9 sblocks") },

	/* TAGKEYS                      XK_number             number*/
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)

	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_j,      focus_stack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focus_stack,     {.i = -1 } },
	{ MODKEY,                       XK_h,      set_master_factor,       {.f = -0.02} },
	{ MODKEY,                       XK_l,      set_master_factor,       {.f = +0.02} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY,                       XK_Right,      shift_view_clients,     {.i = +1 } },
	{ MODKEY,                       XK_Left,      shift_view_clients,     {.i = -1 } },
	{ MODKEY,                       XK_q,      kill_client,     {0} },
	{ MODKEY,                       XK_space,      toggle_floating, {0} },
	{ MODKEY,                       XK_f,      toggle_fullscreen,  {0} },
	{ MODKEY|ShiftMask,             XK_r,      quit,           {0} },
};

/* Button3 is Right Click, Button1 is Left Click, Button2 is Middle Click */
/* Click can be ClkTagBar, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkClientWin,         MODKEY,         Button1,        move_with_mouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        toggle_floating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resize_with_mouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
};

