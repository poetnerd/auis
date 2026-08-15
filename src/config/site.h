/* Site-specific configuration for AUIS revival on macOS/Darwin */

/* EDIT THIS before your first build if your checkout isn't at this
 * exact path. This is compiled into every binary as the fallback
 * ANDREWDIR (where the tree looks for its own build/etc, build/fonts,
 * etc. at runtime) whenever the ANDREWDIR environment variable isn't
 * set. Point it at wherever YOUR checkout's build/ directory will be.
 * See quickstart.md's "Site configuration" section. */
#undef DEFAULT_ANDREWDIR_ENV
#define DEFAULT_ANDREWDIR_ENV /Users/wdc/src/AUIS/html/build

#define AMS_ENV 1
#define MK_CUI
#define CONTRIB_ENV 1
#define MK_ZIP
#define MK_CALC
#undef ANDREW_MALLOC_ENV
