/*
 * doload.h - environment for dynamic loader (dlopen version)
 *
 * This replaces the original platform-specific doload with
 * standard POSIX dlopen()/dlsym()/dlclose().
 */

#ifndef _DOLOAD_H_
#define _DOLOAD_H_

#include <dlfcn.h>

extern int doload_trace;

#define doload_extension ".so"

extern void *(*doload())();

#endif /* _DOLOAD_H_ */
