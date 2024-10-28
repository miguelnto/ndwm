# Customize according to your system

# Executable file
MAIN = ndwm

# Output directory for the executable file
BIN = bin

# Output directory for *.o files
OBJDIR = obj

# Source code directory
SRCDIR = src

# Install directory
INSTALLDIR = /usr/local/bin

# X11
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Freetype
FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = /usr/include/freetype2

# Includes and libs
INCS = -I${X11INC} -I${FREETYPEINC}
LDFLAGS = -L${X11LIB} -lX11 ${FREETYPELIBS}

# Flags
COPTIONS = -pedantic -Wall -Wextra -Wunused -Wunused-function -Wunused-local-typedefs -Wunused-macros -Os
CPPFLAGS = -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700L  
CFLAGS   = -std=c99 ${COPTIONS} ${INCS} ${CPPFLAGS}

# Compiler
CC = cc
