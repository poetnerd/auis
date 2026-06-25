#ifndef SYSTEM_H
#define SYSTEM_H

#include <allsys.h>

#define OPSYSNAME       "darwin"
#define sys_darwin      1
#define SYS_NAME        "darwin"

#undef  SY_B43
#define SY_B43  1

#define HAVE_SHARED_LIBRARIES 1
#define HAS_GETTIMEOFDAY 1

#ifndef In_Imake

#undef FILE_NEEDS_FLUSH
#define FILE_NEEDS_FLUSH(f) (1)

#undef SIGSET_TYPE
#define SIGSET_TYPE sigset_t

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>

#define SYSV_STRINGS
#include <string.h>

#include <fcntl.h>
#include <sys/file.h>

#include <dirent.h>
#define DIRENT_TYPE struct dirent
#define DIRENT_NAMELEN(d) (strlen((d)->d_name))
#define NEWPGRP() setpgrp(0, 0)

#include <time.h>

#define OSI_HAS_SYMLINKS 1

#define osi_readlink(PATH,BUF,SIZE) readlink((PATH),(BUF),(SIZE))

#define osi_ExclusiveLockNoBlock(fid)   flock((fid), LOCK_EX | LOCK_NB)
#define osi_UnLock(fid)                 flock((fid), LOCK_UN)
#define osi_O_READLOCK                  O_RDONLY
#define osi_F_READLOCK                  "r"

#define osi_vfork()                     fork()

#define osi_setjmp  _setjmp
#define osi_longjmp _longjmp

struct osi_Times {unsigned long int Secs; unsigned long int USecs;};
#define osi_GetSecs() time((long int *) 0)
#define osi_SetZone() tzset()
#define osi_ZoneNames tzname
#define osi_SecondsWest timezone
#define osi_IsEverDaylight daylight

/* macOS provides setlinebuf() natively */

#define HAS_SYSEXITS 1

#define getwd(pathname) getcwd(pathname, MAXPATHLEN)

#endif /* !In_Imake */

#ifndef FLEX_ENV
#define FLEX_ENV 1
#endif

#ifndef POSIX_ENV
#define POSIX_ENV       1
#endif

#define ANSI_COMPILER 1

#include <site.h>

#endif  /* SYSTEM_H */
