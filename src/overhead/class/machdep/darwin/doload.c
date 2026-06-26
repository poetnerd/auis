/*
 * doload.c - dynamic loader using POSIX dlopen()/dlsym()
 *
 * Based on the Solaris (sun_sparc_51) doload which was the first
 * AUIS platform to use dlopen().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

char doload_extension[] = ".do";

int doload_trace = 0;

char *doload(int inFD, char *name, char **bp, long *lenP, char *path)
{
    char *dummy;
    void *handle;
    char *EntryPoint = NULL;
    char epname[1024];
    char *p;

    dummy = (char *) malloc(1);
    if (dummy == NULL)
        return NULL;
    *bp = dummy;
    *lenP = 1;

    handle = dlopen(path, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "doload: Error loading package \"%s\" - %s\n",
                path, dlerror());
        return NULL;
    }

    /* Strip suffix from name and append __GetClassInfo */
    (void) strcpy(epname, name);
    p = strrchr(epname, '.');
    if (p == NULL)
        p = epname + strlen(epname);
    (void) strcpy(p, "__GetClassInfo");

    EntryPoint = (char *) dlsym(handle, epname);
    if (EntryPoint == NULL) {
        /* try with leading underscore (Mach-O convention) */
        char epname2[1028];
        snprintf(epname2, sizeof(epname2), "_%s", epname);
        EntryPoint = (char *) dlsym(handle, epname2);
    }
    if (EntryPoint == NULL) {
        fprintf(stderr, "doload: Error finding entry point \"%s\" in \"%s\" - %s\n",
                epname, path, dlerror());
        return NULL;
    }

    if (doload_trace)
        fprintf(stderr, "doload: %s: entry = %p\n", name, EntryPoint);

    return EntryPoint;
}
