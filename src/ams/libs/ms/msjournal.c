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

/*
	msjournal.c -- per-folder change journal for IMAP writeback
		      (capture side only; nothing here talks to IMAP or
		      replays anything -- that is a later milestone).

	Two entry points, called from four existing K&R store functions
	(MS_AlterSnapshot, MS_PurgeDeletedMessages, MS_CloneMessage,
	AppendFileToMSDirInternal, all in this directory):

	    void MSJournal_Record(char *dir, char *fmt, ...)
	    void MSJournal_Suppress(int on)

	MSJournal_Record appends one formatted text line to a file named
	".MS_Journal" inside the folder directory `dir`, but ONLY if that
	directory is a mirrored folder -- recognized by the presence of
	".MS_IMAPSync" (written by imapsync, see imap_sync.c), stat'd once
	per distinct directory per process and cached thereafter. A folder
	with no ".MS_IMAPSync" never journals: for every local-only mail
	folder this function costs one cache lookup and returns, with no
	side effect whatsoever -- the store's on-disk behavior for
	non-mirrored mail is unchanged.

	MSJournal_Suppress(1) turns MSJournal_Record into a pure no-op
	(skipped before the mirrored-folder check, so no stat traffic
	either) for the calling process. imapsync calls this once at
	startup so that its own mirror writes (through
	MS_AppendFileToFolderWithId and its own MS_AlterSnapshot flag
	calls) never re-journal what it just pulled from the server.
	Nothing else calls it.

	Journal grammar (version-tagged so a later format can coexist with
	old, un-replayed entries): one record per line, fields separated by
	single spaces, no embedded whitespace in any field --

	    J1 flags <id> <or-hex> <and-hex>
	    J1 purge <id>
	    J1 append <id>

	  J1        -- format version tag, always the first field.
	  <id>      -- the message's 18-character AMS id (already the
	               store's own id charset, so never contains a space).
	  <or-hex>/<and-hex> (flags only) -- AMS_ATTRIBUTESIZE (21) bytes
	               each, lowercase hex (42 characters, no separator),
	               of an OR-mask and an AND-mask such that
	                   new_attributes = (old_attributes | or-mask) & and-mask
	               for every one of MS_AlterSnapshot's five Code
	               variants (see the encoding note in altsnap.c's hook
	               -- REPLACE* variants set or-mask = and-mask = the new
	               attribute bytes, which forces new_attributes to equal
	               the new bytes regardless of old_attributes). This two-
	               mask form is exactly what replay needs to recompute
	               the server-side flag delta for the three server-
	               mapped attributes (UNSEEN/DELETED/REPLIEDTO) without
	               having to know which of the five Codes produced it;
	               the other 165 attribute bits are still journaled
	               (both masks cover the whole attribute field) but are
	               local-only and ignored at replay.
	  purge     -- <id> was unlinked by MS_PurgeDeletedMessages, or
	               marked deleted by MS_CloneMessage's COPYDEL/APPENDDEL
	               out of a mirrored source (that case is recorded as a
	               purge immediately, not deferred to the later real
	               purge -- see clonemsg.c's hook).
	  append    -- <id> is new in this (mirrored) folder via a native
	               file append (MS_AppendFileToFolder) or as the dest
	               side of MS_CloneMessage. Its id is the store's
	               existing id, native to the source folder -- NOT a
	               synthesized IMAP-shaped id -- so replay cannot use it
	               as a UID; that is a replay-time concern (see the
	               plan's §2A note that the local native-id copy gets
	               replaced by an APPEND + suppressed re-purge, out of
	               scope here).

	Language level: ANSI C (C89 prototypes); scanf family unused.
*/

#ifndef NORCSID
#define NORCSID
#endif

#include <andrewos.h>
#include <ms.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MSJOURNAL_MARKER  ".MS_IMAPSync"
#define MSJOURNAL_FILE    ".MS_Journal"
#define MSJOURNAL_MAXCACHE 64
#define MSJOURNAL_LINEMAX  512

struct msjournal_cacheent {
    char dir[1 + MAXPATHLEN];
    int mirrored;
};

static struct msjournal_cacheent msjournal_cache[MSJOURNAL_MAXCACHE];
static int msjournal_cachecount = 0;
static int msjournal_suppressed = 0;

static int msjournal_is_mirrored(const char *dir);

void
MSJournal_Suppress(int on)
{
    msjournal_suppressed = on;
}

void
MSJournal_Record(const char *dir, const char *fmt, ...)
{
    va_list ap;
    char line[MSJOURNAL_LINEMAX];
    char path[MAXPATHLEN + 32];
    int len, fd;

    if (msjournal_suppressed) return;
    if (dir == NULL || dir[0] == '\0') return;
    if (!msjournal_is_mirrored(dir)) return;

    va_start(ap, fmt);
    len = vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    if (len < 0) return;			/* formatting failed; skip silently */
    if ((size_t) len >= sizeof(line) - 1) {
	len = sizeof(line) - 2;		/* truncated; still record what fit */
    }
    line[len] = '\n';
    line[len + 1] = '\0';

    sprintf(path, "%s/%s", dir, MSJOURNAL_FILE);
    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if (fd < 0) return;		/* best-effort: a lost journal record must
				   never break the mutation it describes */
    (void) writeall(fd, line, len + 1);
    (void) close(fd);
}

static int
msjournal_is_mirrored(const char *dir)
{
    int i;
    char path[MAXPATHLEN + 32];
    struct stat st;
    int mirrored;

    for (i = 0; i < msjournal_cachecount; ++i) {
	if (strcmp(msjournal_cache[i].dir, dir) == 0) {
	    return msjournal_cache[i].mirrored;
	}
    }

    sprintf(path, "%s/%s", dir, MSJOURNAL_MARKER);
    mirrored = (stat(path, &st) == 0);

    if (msjournal_cachecount < MSJOURNAL_MAXCACHE
	&& strlen(dir) < sizeof(msjournal_cache[0].dir)) {
	strcpy(msjournal_cache[msjournal_cachecount].dir, dir);
	msjournal_cache[msjournal_cachecount].mirrored = mirrored;
	++msjournal_cachecount;
    }
    return mirrored;
}
