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

#ifndef NORCSID
#define NORCSID
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/overhead/util/lib/RCS/profile.c,v 2.16 1992/12/15 21:10:49 rr2b R6tape $";
#endif


 

#include <fdplumb.h>
#include <stdio.h>
#include <errno.h>
#include <andrewos.h>
extern char *AndrewDir();		/* sys/types.h */
extern char *getenv();
#include <sys/stat.h>
#include <sys/param.h>	/* For MAXPATHLEN */
#include <pwd.h>
#include <ctype.h>
#include <util.h>

#define DEFAULTPROFILES "~/preferences:~/.preferences:~/.Xdefaults"
#define GLOBALPROFILE AndrewDir("/lib/global.prf")

static struct configurelist *profileHead = NULL;
static struct configurelist *GloprofileHead = NULL;

static  int inited = 0;  /* Used to be static local to openprofile -- nsb */
static char *profileFileName = NULL;
static char *firstProfileFileName = NULL;
static int profileLoadTransientErrno = 0;

/* open a list of profile files, with an environment variable that can override
 * the default list.  "savefname" flag added 12/13/91 by cn0h
 */
static struct configurelist *
openprofile(filename, defaultname, savefname)
    char *filename;
    char *defaultname;
    int savefname;
{
    char *pl=(char *) getenv(filename);
    char *home=(char *) gethome(NULL);
    char *sep;
    int homelen=(home==NULL ? 0 : strlen(home));
    struct configurelist *cl;
    char tmpFileName[MAXPATHLEN];

    if(pl==NULL || *pl=='\0')
        pl=defaultname;

    do{
	char *name;
	int namelen;

	sep=(char *)index(pl,':');

	name = pl;

	if(sep!=NULL){
	    namelen = sep - pl;
	    pl=sep+1;
	}
	else
	    namelen=strlen(name);

	if(*name=='~' && (name++,namelen--,homelen>0)){
	    strcpy(tmpFileName,home);
	    strncat(tmpFileName,name, namelen);
	    tmpFileName[namelen + homelen] = '\0';
	}
	else {
	    strncpy(tmpFileName,name, namelen);
	    tmpFileName[namelen] = '\0';
	}

	if (savefname && firstProfileFileName == NULL)  {
	    firstProfileFileName = (char *) malloc(strlen(tmpFileName) + 1);
	    strcpy(firstProfileFileName, tmpFileName);
	}

	errno = 0;
	if ((cl = (struct configurelist *) ReadConfigureFile(tmpFileName)) != NULL)  {
	    if (savefname) {
		if (profileFileName != NULL)  {
		    free(profileFileName);
		}
		profileFileName = (char *) malloc(strlen(tmpFileName) + 1);
		strcpy(profileFileName, tmpFileName);
	    }
	    return cl;
	}
	/* ENOENT (no such file) is a definitive answer; anything else
	   (EMFILE, ENOMEM mid-parse, ...) may be transient and must not
	   be cached for the life of the process. */
	if (errno != 0 && errno != ENOENT)
	    profileLoadTransientErrno = errno;

    } while(sep!=NULL);

    return NULL;

}

/* Load both profile lists once.  A load that failed for a possibly
   transient reason is NOT latched: inited stays 0 so the next
   getprofile call retries, instead of silently returning NULL for
   every preference for the life of the process. */
static void
initprofiles()
{
    if (inited)
	return;
    profileLoadTransientErrno = 0;
    profileHead = openprofile("PROFILES", DEFAULTPROFILES, 1);
    GloprofileHead = openprofile("GLOBALPROFILES", GLOBALPROFILE, 0);
    if (profileHead == NULL && profileLoadTransientErrno != 0) {
	if (GloprofileHead != NULL) {
	    FreeConfigureList(GloprofileHead);
	    GloprofileHead = NULL;
	}
	fprintf(stderr, "<warning:profile>Preferences load failed (errno %d); will retry on next access.\n", profileLoadTransientErrno);
	return;
    }
    inited = 1;
}

char *GetProfileFileName()
{
    initprofiles();

    return profileFileName;
}

char *GetFirstProfileFileName()
{
    initprofiles();

    return firstProfileFileName;
}

refreshprofile() {  /* Force rereading */
    if (profileHead != NULL)  {
	FreeConfigureList(profileHead);
	profileHead = NULL;
    }
    if (profileFileName != NULL)  {
	free(profileFileName);
	profileFileName = NULL;
    }
    inited = 0;
}

char *getprofile (var)
char *var; {
    char *retval;
    initprofiles();
#ifdef GLOBALPREFERENCE
/* check for exact match in users profile */
    if((retval = (char *) GetConfig(profileHead, var, 0)) != NULL)
	return retval;
/* check for exact match in global profile */
    if((retval = (char *) GetConfig(GloprofileHead, var, 0)) != NULL)
	return retval;
#endif
/* check for unexact match in users profile */
    if((retval = (char *) GetConfig(profileHead, var, 1)) != NULL)
	return retval;
/* check for unexact match in global profile */
    return (char *) GetConfig(GloprofileHead, var, 1) ;
}

getprofileswitch (var, DefaultValue)
char   *var; {
    char   *val;
    static struct keys {
	char   *name;
	int     value;
    }                   keys[] = {
	                    "true", 1,
	                    "false", 0,
	                    "on", 1,
	                    "off", 0,
	                    "yes", 1,
	                    "no", 0,
	                    "1", 1,
	                    "0", 0,
	                    0, 0
    };
    register struct keys   *p;
    if (var && (val = getprofile (var))) {
	for (p = keys; p -> name; p++)
	    if (FOLDEDEQ(p->name, val))
		return p -> value;
    }
    return DefaultValue;
}

getprofileint (var, DefaultValue)
char   *var; {
    register char  *val;
    register    n = 0;
    register    neg = 0;

    if (var == 0 || (val = getprofile(var)) == 0)  {
	return DefaultValue;
    }
    while (*val) {
	if (*val == '-')
	    neg = ~neg;
	else
	    if (*val != ' ' && *val != '\t')
		if ('0' <= *val && *val <= '9')
		    n = n * 10 + *val - '0';
		else  {
		    return DefaultValue;
		}
	val++;
    }
    return neg ? -n : n;
}

profileentryexists(var, usedefault)
    char *var;
    int usedefault;
{

    initprofiles();

    return (var != NULL && ( (GetConfig(profileHead, var, usedefault) != NULL) || 
			     (GetConfig(GloprofileHead, var, usedefault) != NULL)));
}
