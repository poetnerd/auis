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
	netrc.c -- Minimal ~/.netrc parser: machine/login/password
		   stanzas, plus "default".  Refuses group- or
		   world-readable files.  See netrc.h for the one
		   entry point, netrc_Lookup().

	This intentionally does not support "macdef" (macro definition)
	stanzas; a stray macdef in a netrc used only for mail credentials
	is not expected, and if present its body is simply skipped as a
	sequence of unrecognized tokens (safe as long as the macro body
	doesn't itself contain the words "machine", "default", "login",
	or "password").
*/

#include <andrewos.h>
#include <stdio.h>
#include <sys/stat.h>
#ifndef MAXPATHLEN
#include <sys/param.h>
#endif /* MAXPATHLEN */

#include <netrc.h>

#define NRC_TOKMAX 512

int netrc_Lookup(path, machine, login, loginlen, passwd, passwdlen)
    char *path, *machine, *login, *passwd;
    int loginlen, passwdlen;
{
    FILE *fp;
    struct stat st;
    char tok[NRC_TOKMAX];
    char curLogin[NRC_TOKMAX], curPasswd[NRC_TOKMAX];
    char defLogin[NRC_TOKMAX], defPasswd[NRC_TOKMAX];
    int inTarget, inDefault, haveDefault, matched;

    if (stat(path, &st) < 0) return NETRC_NOENT;
    if (st.st_mode & (S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH))
	return NETRC_BADPERM;

    fp = fopen(path, "r");
    if (fp == NULL) return NETRC_NOENT;

    curLogin[0] = curPasswd[0] = '\0';
    defLogin[0] = defPasswd[0] = '\0';
    inTarget = inDefault = haveDefault = matched = 0;

    while (fscanf(fp, "%511s", tok) == 1) {
	if (strcmp(tok, "machine") == 0) {
	    if (fscanf(fp, "%511s", tok) != 1) break;
	    curLogin[0] = curPasswd[0] = '\0';
	    inTarget = (strcmp(tok, machine) == 0);
	    inDefault = 0;
	} else if (strcmp(tok, "default") == 0) {
	    curLogin[0] = curPasswd[0] = '\0';
	    inTarget = 0;
	    inDefault = 1;
	} else if (strcmp(tok, "login") == 0) {
	    if (fscanf(fp, "%511s", tok) != 1) break;
	    strncpy(curLogin, tok, sizeof(curLogin)-1);
	    curLogin[sizeof(curLogin)-1] = '\0';
	} else if (strcmp(tok, "password") == 0 || strcmp(tok, "account") == 0) {
	    if (fscanf(fp, "%511s", tok) != 1) break;
	    if (strcmp(tok, "password") == 0) {
		/* only "password" (not "account") is what we want */
	    }
	    strncpy(curPasswd, tok, sizeof(curPasswd)-1);
	    curPasswd[sizeof(curPasswd)-1] = '\0';
	    if (inTarget && curLogin[0] != '\0') {
		strncpy(login, curLogin, loginlen-1); login[loginlen-1] = '\0';
		strncpy(passwd, curPasswd, passwdlen-1); passwd[passwdlen-1] = '\0';
		matched = 1;
		break;
	    } else if (inDefault && !haveDefault) {
		strncpy(defLogin, curLogin, sizeof(defLogin)-1); defLogin[sizeof(defLogin)-1] = '\0';
		strncpy(defPasswd, curPasswd, sizeof(defPasswd)-1); defPasswd[sizeof(defPasswd)-1] = '\0';
		haveDefault = 1;
	    }
	}
	/* Any other token (e.g. a macdef body word) is silently skipped. */
    }
    fclose(fp);

    if (!matched && haveDefault) {
	strncpy(login, defLogin, loginlen-1); login[loginlen-1] = '\0';
	strncpy(passwd, defPasswd, passwdlen-1); passwd[passwdlen-1] = '\0';
	matched = 1;
    }

    return (matched ? NETRC_OK : NETRC_NOMATCH);
}
