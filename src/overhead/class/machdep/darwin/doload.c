/*
 * doload.c - dynamic loader for AUIS class system on Darwin/macOS
 *
 * Uses POSIX dlopen()/dlsym() to load .do shared libraries.
 * Based on the SunOS 4.1/S5R4 dlopen approach from sun_sparc_51/doload.c.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

/* doload_extension is a macro in doload.h; define our globals without it */
int doload_trace = 0;

/*
 * doload: Load a dynamic object (.do file) and return a pointer to its
 * GetClassInfo entry point, or NULL on error.
 *
 * inFD  - open fd for the .do file (unused; we use path instead)
 * name  - class name as passed by doindex (may include .do suffix)
 * bp    - set to a fake base address for doindex bookkeeping
 * lenP  - set to a fake text length
 * path  - full pathname of the .do file
 */
char *doload(inFD, name, bp, lenP, path)
int inFD;
char *name;
char **bp;
long *lenP;
char *path;
{
    void *handle;
    char *EntryPoint = NULL;
    char epname[1024];
    char *p;
    char *dummy;

    /* Provide fake bp/len so doindex can call doload_free later */
    dummy = (char *) malloc(1);
    if (dummy == NULL)
        return NULL;
    *bp = dummy;
    *lenP = 1;

    handle = dlopen(path, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "doload: cannot load \"%s\": %s\n", path, dlerror());
        return NULL;
    }

    /* Build entry point name: strip suffix, append __GetClassInfo */
    strncpy(epname, name, sizeof(epname) - 16);
    epname[sizeof(epname) - 16] = '\0';
    p = strrchr(epname, '.');
    if (p == NULL)
        p = epname + strlen(epname);
    strcpy(p, "__GetClassInfo");

    EntryPoint = (char *) dlsym(handle, epname);
    if (EntryPoint == NULL) {
        fprintf(stderr, "doload: cannot find \"%s\" in \"%s\": %s\n",
                epname, path, dlerror());
        dlclose(handle);
        return NULL;
    }

    if (doload_trace)
        printf("doload: %s entry = %p\n", name, (void *)EntryPoint);

    return EntryPoint;
}
