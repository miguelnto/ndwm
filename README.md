# ndwm

<img align="left" style="width:256px" src="doc/logo.png" width="256px">

**NDWM is a fully-featured and simple dynamic window manager for X11.**

The purpose of NDWM is to be a simple yet powerful window manager with sensible defaults and the essential features you would expect in a window manager. The user should also be able to easily modify the window manager's behavior. Configuration is done by hacking into the source code.

NDWM uses libx11 to communicate with the X server.

![Screenshot](doc/ndwm.png)

## Dependencies

- C99 Compiler
- GNU Make
- libx11
- libxft

## Installation

First, make sure you have all the dependencies **installed.**

Enter the following command to build and install (as root):

```sh
make clean install
```

By default, the program is installed under `/usr/local/bin`.

## Configuration

You should configure **ndwm** by manualy editing the file `config.h` to match your preferences, then recompile the program.


## Usage

Put "**ndwm**" in your `.xinitrc` file or other startup script to start the window manager. It's also recommended to use **ndwm** with a status bar, such as **sblocks.** If you choose to do so, your `.xinitrc` file should look something like this:

```
sblocks &
exec ndwm
```

## Features

- [x] System tray support
- [x] Fullscreen mode on windows
- [x] Configurable keybindings
- [x] Sane defaults
- [x] Configurable behaviour
- [x] Configurable color scheme

## TODO

- [ ] Color scheme configuration needs to be simplified.
- [ ] Configuration should be done in a .toml file.

## Behavior

By default there are 9 tags you can navigate to. The window model is **master-stack**. A new window will always be the **master** window, while the existing ones will be pushed upon a *stack** on the right side of the screen. 


    +------+----------------------------------+--------+
    | tags | title                            | status |
    +------+---------------------+------------+--------+
    |                            |                     |
    |                            |                     |
    |                            |                     |
    |                            |                     |
    |          master            |        stack        |
    |                            |                     |
    |                            |                     |
    |                            |                     |
    |                            |                     |
    +----------------------------+---------------------+

You can move around, change the master window, rotate the position of the windows, etc. Windows are **tiled**, but you can make them **float** by pressing `MODKEY + Left Mouse Button`, then drag the window.

## Default keybindings

By default, MODKEY is the Super button (or Windows button).

| Keybinding  | Action |
| ------------- | ------------- |
| MODKEY + d | Open dmenu  |
| MODKEY + Enter  | Open terminal (st)  |
| AudioMute  | Mute audio volume  |
| AudioRaiseVolume  | Raise volume  |
| AudioLowerVolume  | Lower audio volume  |
| MonBrightnessUp  | Increase the monitor brightness  |
| MonBrightnessDown  | Decrease the monitor brightness  |
| MODKEY + [1-9] | Go to tag [1-9]  |
| MODKEY + k | Focus the previous window in the stack  |
| MODKEY + j  | Focus the next window in the stack  |
| MODKEY + f  | Toggle fullscreen mode  |
| MODKEY + q  | Close window |
| MODKEY + Space  | Toggle floating mode |
| MODKEY + Left  | Go to the left tag |
| MODKEY + Right  | Go to the right tag |
| MODKEY + Shift + Left  | Move window to left tag |
| MODKEY + Shift + Right  | Move window to right tag |
| MODKEY + v  | Move the client to the next position in the stack  |
| MODKEY + r  | Rotate clients |
| MODKEY + l  | Increase the width of the master window |
| MODKEY + h  | Decrease the width of the master window |
| MODKEY + z  | Turn the focused into the master window  |
| MODKEY + Shift + r  | Quit ndwm |

