/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	smtptest.c -- Gate A standalone driver for smtp_dropoff().  Not
		     part of libmail.a; built only via the .test suffix
		     rule (TestingOnlyTestingRule in this directory's
		     Imakefile):

			make smtptest.test

	Usage:
	    smtptest.test <msgfile> <to1> [<to2> ...]

	Configuration (smtphost/smtpport/smtpnetrc) comes from the normal
	getprofile() preference machinery, i.e. from whatever profile
	file(s) the PROFILES environment variable points to for this
	process -- set that to a scratch profile before running so the
	real ~/preferences is never touched.  See revival/doc/
	smtp-send-prompt.md and the milestone 1 Gate A report for the
	exact scratch profiles used.
*/

#ifndef NORCSID
#define NORCSID
#endif

#include <andrewos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dropoff.h>

extern int errno;

extern char Dropoff_ErrMsg[];
extern int smtp_dropoff();

static char *rcname(rc)
    int rc;
{
    switch (rc) {
    case D_OK:		return "D_OK";
    case D_OK_WARN:	return "D_OK_WARN";
    case D_LOCALQ:	return "D_LOCALQ";
    case D_CANT_QUEUE:	return "D_CANT_QUEUE";
    case D_BAD_PARMS:	return "D_BAD_PARMS";
    case D_TEMP_FAIL:	return "D_TEMP_FAIL";
    case D_BAD_MESGFILE: return "D_BAD_MESGFILE";
    case D_OSERR:	return "D_OSERR";
    default:		return "???";
    }
}

main(argc, argv)
    int argc;
    char **argv;
{
    int f, rc, i, ntos;
    char *tolist[16];

    if (argc < 3) {
	fprintf(stderr, "usage: %s <msgfile> <to1> [<to2> ...]\n", argv[0]);
	exit(2);
    }

    f = open(argv[1], O_RDONLY, 0);
    if (f < 0) {
	fprintf(stderr, "Can't open \"%s\": %s\n", argv[1], strerror(errno));
	exit(2);
    }

    ntos = 0;
    for (i = 2; i < argc && ntos < 15; i++) tolist[ntos++] = argv[i];
    tolist[ntos] = NULL;

    rc = smtp_dropoff(f, tolist, (char *) NULL);
    printf("smtp_dropoff() returns %d (%s): %s\n", rc, rcname(rc), Dropoff_ErrMsg);
    exit(rc == D_OK ? 0 : 1);
}
