/*
 * classproc.c - Darwin/dlopen version
 *
 * Provides class_RoutineStruct without the ClassEntry trampolines.
 * With dlopen(), classes are loaded eagerly and their vtables are
 * populated immediately — no lazy-loading stubs needed.
 */

#include <class.h>

struct basicobject_methods class_RoutineStruct = { NULL };
