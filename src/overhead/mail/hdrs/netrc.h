/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	$Disclaimer:
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice, this permission notice, and the following
 * disclaimer appear in supporting documentation, and that the names of
 * IBM, Carnegie Mellon University, and other copyright holders, not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * IBM, CARNEGIE MELLON UNIVERSITY, AND THE OTHER COPYRIGHT HOLDERS
 * DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT
 * SHALL IBM, CARNEGIE MELLON UNIVERSITY, OR ANY OTHER COPYRIGHT HOLDER
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *  $
*/

/*
	netrc.h -- Include file for using the netrc.c ~/.netrc lookup routine.

	Parses standard machine/login/password stanzas (and a "default"
	stanza) out of a netrc-format file, refusing to use files that are
	group- or world-readable.  Shared by the SMTP dropoff module
	(milestone 1) and the IMAP module (milestone 2).
*/

/* Return codes from netrc_Lookup() */

#define NETRC_OK	0	/* Found a matching (or default) entry */
#define NETRC_NOENT	(-1)	/* File doesn't exist or can't be opened */
#define NETRC_BADPERM	(-2)	/* File is group- or world-readable; refused */
#define NETRC_NOMATCH	(-3)	/* File is fine, but no matching/default entry */

/* int netrc_Lookup(path, machine, login, loginlen, passwd, passwdlen)
       char *path, *machine, *login, *passwd;
       int loginlen, passwdlen;

   Looks up "machine" in the netrc file named by "path" (no tilde
   expansion -- callers pass an already-expanded path).  On NETRC_OK,
   "login" and "passwd" are filled in (null-terminated, truncated to
   loginlen/passwdlen if necessary).  On any other return, their
   contents are undefined and the caller should not use them. */

extern int netrc_Lookup();
