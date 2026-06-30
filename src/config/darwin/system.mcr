/* Platform macros for macOS/Darwin */

#define In_Imake 1
/* The next two lines need to be kept in sync */
#include <darwin/system.h>
        SYSTEM_H_FILE = darwin/system.h
#undef In_Imake

        SYS_IDENT = darwin
        SYS_OS_ARCH = darwin

/* Get parent inclusions */
#include <allsys.mcr>

CC = cc
COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type
LEX = flex
RANLIB = ranlib

XBASEDIR = /opt/X11
XUTILDIR = /opt/homebrew/bin
XLIBDIR = /opt/X11/lib
XINCDIR = /opt/X11/include
XLIB = -L$(XLIBDIR) -lX11

DYN_LINK_LIB = -ldl
LEXLIB = -ll

/* Get site-specific inclusions */
#include <site.mcr>
SYS_CONFDIR = darwin
