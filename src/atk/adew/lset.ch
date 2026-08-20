/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	$Disclaimer: 
*Permission to use, copy, modify, and distribute this software and its 
*documentation for any purpose is hereby granted without fee, 
*provided that the above copyright notice appear in all copies and that 
*both that copyright notice, this permission notice, and the following 
*disclaimer appear in supporting documentation, and that the names of 
*IBM, Carnegie Mellon University, and other copyright holders, not be 
*used in advertising or publicity pertaining to distribution of the software 
*without specific, written prior permission.
*
*IBM, CARNEGIE MELLON UNIVERSITY, AND THE OTHER COPYRIGHT HOLDERS 
*DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING 
*ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT 
*SHALL IBM, CARNEGIE MELLON UNIVERSITY, OR ANY OTHER COPYRIGHT HOLDER 
*BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY 
*DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
*WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS 
*ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
*OF THIS SOFTWARE.
* $
*/


 

#define STRING 1
#define LONG 2
#define FLOAT 3

class lset : dataobject[dataobj] {
overrides:
    Read (FILE *file, long id) returns long;
    Write (FILE *file, long writeid, int level) returns long;
    GetModified() returns long;
classprocedures:
    InitializeClass() returns boolean;
    InitializeObject(struct lset *self) returns boolean;
methods:
    InsertObject (char *name,char *viewname);
    registername(char *rf) returns char *;
data:
	int type;
	int pct;
	/* When set, the view built for this split should not draw its
	   lpair divider bar (see lpair.ch's barvisible/SetBarVisible).
	   Added 2026-08-19 for htmlatk.c's HTML table rendering: a real
	   browser never shows a resize bar between table cells, so every
	   BuildLsetChain-generated split (htmlatk.c) sets this. Persisted
	   as of the on-disk \V 2 format (lset__Read/Write); defaults to 0
	   (bar shown, the original/pre-2026-08-19 behavior) for \V 1 files
	   and every other lset caller in the tree. */
	int nobar;
	/* When set, the side-by-side view built for this split centers
	   its shorter child vertically within the shared row height
	   (lpair_VCENTER, lpair.ch) instead of pinning it to the top --
	   real browsers' default table-cell valign. Added 2026-08-20
	   alongside nobar, same htmlatk.c motivation and same \V 2->3
	   persisted-format bump (lset__Read/Write); defaults to 0
	   (top-pinned, the original behavior) for \V <3 files and every
	   other lset caller in the tree. */
	int vcenter;
	char dataname[32];
	char viewname[32];
	char refname[64];
    struct dataobject *dobj;
    struct dataobject *left,*right;
    int application;
    struct text *pdoc;
    int revision;
};
