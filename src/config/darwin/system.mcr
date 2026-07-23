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

CC = cc -fwritable-strings
COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type
LEX = flex
RANLIB = ranlib

XBASEDIR = /opt/X11
XBINDIR = bin
XUTILDIR = /opt/homebrew/bin
XLIBDIR = /opt/X11/lib
XINCDIR = /opt/X11/include
XLIB = -L$(XLIBDIR) -lX11

#define HAVE_XFT 1
XFTLIB = -lXft
XFTINCDIR = $(XINCDIR)/freetype2
STD_DEFINES = -DHAVE_XFT

/* OpenSSL (Homebrew), used by overhead/mail/lib/tlscon.c for SMTP (and,
   from milestone 2, IMAP).  Not part of X11/Xft; kept as its own macro
   block so consumers can pull in just $(SSLLIB) without XLIB/XFTLIB. */
SSLBASEDIR = /opt/homebrew/opt/openssl@3
SSLINCDIR = $(SSLBASEDIR)/include
SSLLIBDIR = $(SSLBASEDIR)/lib
SSLLIB = -L$(SSLLIBDIR) -lssl -lcrypto

/* No native Andrew bitmap fonts on this system; map andy* to X11 scalable fonts */
#define ISO80_FONTS_ENV 1

DYN_LINK_LIB = -ldl
LEXLIB = -ll

/* macOS splits BIND resolver symbols (res_init/res_send/res_mkquery/
   dn_expand/dn_skipname, all renamed to res_9_* by <resolv.h> on this
   platform) out of libc into a separate library; older Andrew configs
   never needed this since those symbols lived in libc there. Needed by
   anything linking overhead/mail/lib/rsearch.c or valhost.c (e.g. AMS's
   ValidateMailHostName). */
RESOLVER_LIB = -lresolv

/* Get site-specific inclusions */
#include <site.mcr>
SYS_CONFDIR = darwin
