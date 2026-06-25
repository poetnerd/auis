/*
 * doload.c - dynamic loader using POSIX dlopen()/dlsym()
 *
 * Replaces the original platform-specific dynamic loader that
 * parsed a.out/ELF object files manually. With dlopen(), the
 * OS handles all relocation and symbol resolution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <andrewos.h>

int doload_trace = 0;

/*
 * doload - load a dynamic object and return its entry point
 *
 * Parameters:
 *   fd       - file descriptor (unused with dlopen, kept for interface compat)
 *   name     - class name (used to construct entry point symbol)
 *   base     - returns the handle (cast to char*) for later dlclose()
 *   len      - returns 0 (no meaningful text length with dlopen)
 *   path     - full path to the .so file
 *
 * Returns:
 *   Function pointer to the class's __GetClassInfo entry point,
 *   or NULL on failure.
 */
void *(*doload(int fd, char *name, char **base, long *len, char *path))()
{
    void *handle;
    void *(*entry)();
    char symbol[256];

    /* dlopen uses the path, not the fd */
    handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL) {
        if (doload_trace)
            fprintf(stderr, "doload: dlopen(%s) failed: %s\n", path, dlerror());
        return NULL;
    }

    /* construct the entry point symbol name: classname__GetClassInfo */
    snprintf(symbol, sizeof(symbol), "%s__GetClassInfo", name);

    entry = (void *(*)()) dlsym(handle, symbol);
    if (entry == NULL) {
        /* try with leading underscore (macOS Mach-O convention) */
        char symbol2[260];
        snprintf(symbol2, sizeof(symbol2), "_%s", symbol);
        entry = (void *(*)()) dlsym(handle, symbol2);
    }

    if (entry == NULL) {
        if (doload_trace)
            fprintf(stderr, "doload: dlsym(%s) failed: %s\n", symbol, dlerror());
        dlclose(handle);
        return NULL;
    }

    *base = (char *) handle;
    *len = 0;

    if (doload_trace)
        fprintf(stderr, "doload: loaded %s from %s\n", name, path);

    return entry;
}
