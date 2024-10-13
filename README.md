# ndwm

![ndwm](doc/ndwm.png)

NDWM is a dynamic window manager for X11, based on DWM.

## Requirements

In order to build this project, you need:

- A C99 Compiler
- GNU Make
- libx11
- libfreetype2

## Build

You can build this project by running `make`:

```sh
make
```

The executable will be located at the `bin` directory.

## Install

Enter the following command to build and install (if necessary, run it as root):

```sh
make clean install
```

By default, the program is installed in `/usr/local/bin`.

## Usage

Put **ndwm** in your `.xinitrc` file or other startup script to start the window manager. It's also recommended to use **ndwm** with a status bar, such as **sblocks.** If you choose to do so, your `.xinitrc` file should look something like this:

```
sblocks &
exec ndwm
```

## Features

- [x] System tray support
- [x] Fullscreen mode on windows
- [x] Configurable key bindings
- [x] Sane defaults
- [x] Configurable behaviour
- [x] Configurable color scheme

## TODO

- [ ] Color scheme configuration needs to be simplified.
- [ ] Configuration should be done in a proper configuration file such as a .toml file.
- [ ] There's still a lot of nonsensical code to fix.

## Default configuration

This section is under development.

## Configuration

This section is under development.

## Screenshots

![Screenshot](doc/print.png)
