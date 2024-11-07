#include <X11/X.h>
#include <X11/Xutil.h>

/* USER CONFIGURATION */
/* ------------------------------------------------------------------------------------------ */

static const unsigned int borderpx = 3;        /* Border pixel of windows */

/* TODO: Color scheme configuration needs to be simplified. */
static const char bar_background_color[] = "#1d2021"; 
static const char bar_foreground_color[] = "#bbbbbb"; 
static const char selected_tag_foreground_color[] = "#eeeeee";
static const char selected_tag_background_color[] = "#fe347e";
static const char unfocused_client_border_color[] = "#572649";
static const char focused_client_border_color[] = "#fe347e";

static const char *fonts[] = { "Terminus (TTF):style=Medium:size=9", 
                    "FontAwesome:style=Regular:size=9:antialias=true:autohint=true",
};

static const bool top_bar             = true;
static const bool show_title          = true;
static const unsigned int systrayspacing = 2;

/* TODO: There are some variables that should be user-defined, namely:
 * gap_size 
 * bar_height 
 * systray_width 
 * resize_step
*/

static const float master_factor = 0.52; /* Factor of master width size [0.05..0.95] */

#define MODKEY Mod4Mask

/* USER CONFIGURATION */
/* ------------------------------------------------------------------------------------------ */

static const char *colors[][3] = {
    [SchemeNorm] = { bar_foreground_color, bar_background_color, unfocused_client_border_color },
    [SchemeSel]  = { selected_tag_foreground_color, selected_tag_background_color, focused_client_border_color },
};

#define TAGKEYS(KEY,TAG) \
    { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
    { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \

#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

#include <X11/XF86keysym.h>

static const char *termcmd[]  = { "st", NULL };

static const Key keys[] = {

    /* Application hotkeys */
    { MODKEY,                       XK_d,      spawn,          SHCMD("dmenu_run") },
    { MODKEY,                       XK_Return, spawn,          { .v = termcmd } },
    
    /* Media keys */
    { 0,                       XF86XK_AudioMute,             spawn,          SHCMD("amixer sset Master toggle; pkill -RTMIN+10 sblocks") },
    { 0,                       XF86XK_AudioRaiseVolume,      spawn,          SHCMD("amixer set Master 2%+; pkill -RTMIN+10 sblocks") },
    { 0,                       XF86XK_AudioLowerVolume,      spawn,          SHCMD("amixer set Master 2%-; pkill -RTMIN+10 sblocks") },
    { 0,                       XF86XK_MonBrightnessUp,       spawn,          SHCMD("brightnessctl set +10%") },
    { 0,                       XF86XK_MonBrightnessDown,     spawn,          SHCMD("brightnessctl set 10%-") },

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

    /* modifier                     key        function                    argument */
    { MODKEY,                       XK_k,      focus_previous,                 {0} },
    { MODKEY,                       XK_j,      focus_next,                     {0} },
    { MODKEY,                       XK_f,      toggle_fullscreen,              {0} },
    { MODKEY,                       XK_q,      destroy_client,                 {0} },
    { MODKEY,                       XK_space,  toggle_floating,                {0} },
    { MODKEY,                       XK_Left,   go_to_left_tag,                 {0} },
    { MODKEY,                       XK_Right,  go_to_right_tag,                {0} },
    { MODKEY|ShiftMask,             XK_Right,  move_client_to_right_tag,       {0} },
    { MODKEY|ShiftMask,             XK_Left,   move_client_to_left_tag,        {0} },
    { MODKEY,                       XK_v,      move_client_next,               {0} },
    { MODKEY,                       XK_r,      rotate_clients,                 {0} },
    { MODKEY,                       XK_l,      increase_master_width,          {0} },
    { MODKEY,                       XK_h,      decrease_master_width,          {0} },
    { MODKEY,                       XK_z,      make_master,                    {0} },
    { MODKEY|ShiftMask,             XK_r,      quit,                           {0} },
};

/* Button3 is Right Click, Button1 is Left Click, Button2 is Middle Click.
* Click can be ClkTagBar, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
        /* click                event mask      button          function          */
    { ClkClientWin,         MODKEY,         Button1,        move_with_mouse   },
    { ClkClientWin,         MODKEY,         Button2,        toggle_floating   },
    { ClkClientWin,         MODKEY,         Button3,        resize_with_mouse },
    { ClkTagBar,            0,              Button1,        view              },
};

