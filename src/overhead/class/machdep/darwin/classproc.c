/*
 * classproc.c - Darwin/dlopen stub for class_RoutineStruct
 *
 * On platforms using the old a.out dynamic loader, class_RoutineStruct
 * is a table of ClassEntry<N> trampolines that redirect class procedure
 * calls through the executable's symbol table. With dlopen-based loading
 * on Darwin, each loaded .do provides its own method table directly via
 * __GetClassInfo, so these trampolines are unused. This file provides a
 * null definition to satisfy the linker.
 */

#include <class.h>

struct basicobject_methods class_RoutineStruct = { 0 };
