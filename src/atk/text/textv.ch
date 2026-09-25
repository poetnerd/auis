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


 

#define textview_VERSION 1

enum textview_MovementUnits  {
    textview_MoveByPixels,
    textview_MoveByLines,
    textview_MoveByPseudoLines
};

enum textview_ScrollDirection  {
    textview_NoScroll,
    textview_ScrollForward,
    textview_ScrollBackward,
    textview_MultipleScroll
};

enum textview_LineRedrawType {
    textview_FullLineRedraw,
    textview_GetPosition,
    textview_GetCoordinate,
    textview_GetHeight,
    textview_MoveView,
    textview_PartialLineRedraw
};

struct linedesc  {
    short y;				/* last position drawn */
    short height;			/* height used during last draw */
    short textheight;                   /* textheight used during last draw */
    short xMax;				/* farthest printed */
    short nChars;			/* length of the line in chars */
    boolean containsView;		/* true if this line contains a view */
    struct mark *data;			/* the range of the text storing the data */
};

/* Shared with textv.c's own (formerly private) FINESCROLL -- textv.c
   #defines its local FINESCROLL as this constant, so there is exactly
   one place either has to change. Originally 7 (1988), widened to 14
   on 2026-09-24 for the elevator-drag fix below; that widening's own
   comment (textv.c) claimed "nothing outside this file reads
   FINESCROLL" and left this class's EncodePosition/DecodePosition
   macromethods (below) hardcoded to the old literal 7 -- a sibling
   site the grep missed because it's a *macro body*, not a read of the
   FINESCROLL symbol. Found live 2026-09-25: any textview subclass
   whose own scroll interface calls EncodePosition/DecodePosition to
   translate a getinfo()-returned (FINESCROLL-shifted) position back to
   a raw character offset -- messages' `captions` is the one that does
   this -- silently decoded with the wrong shift (>>7 undoing a <<14),
   leaving the decoded position ~2^7 too large. captions.c's GetInfo
   treats an out-of-range decode as "off the end" and collapses the
   scrollbar to a degenerate whole-document elevator. */
#define textview_FINESCROLL 14

/* Elevator-drag scrollbar-position correction (2026-09-24, see
   html-scroll-plan.md's Step 3): records the real pixel height of a
   line found -- via any code path, full paint or measurement-only --
   to contain an embedded view disproportionately taller than ordinary
   text (e.g. a many-thousand-pixel HTML table kept as one view so its
   own horizontal scroll stays in sync across rows). `self->lines[]`
   itself can't be used for this, since it's only a moving window of
   currently-rendered lines and forgets a line's real height once it
   scrolls out of view, but the scrollbar's `total`/`position()`
   encoding (see FINESCROLL above) needs to keep accounting for it
   regardless of what's currently on screen. `position` is the line's
   raw (pre-FINESCROLL-shift) character position; `extraWeight` is
   already expressed in the same units `position()` returns, ready to
   add/subtract directly.

   Deliberately NOT a `struct textview` data field: `textview` is
   subclassed in ~25+ places across the tree (`messages` among them,
   built as its own separately-linked `messages.do`), and subclassing
   here embeds the parent struct by value -- adding fields would shift
   every subclass's own field offsets and require recompiling all of
   them, or risk exactly the kind of silent ABI mismatch this project
   has hit before (see the ".do Silent Underlink" bug class). Instead,
   a small per-textview blob (`struct scrollweightlist`, textv.c) is
   stashed via the existing `dictionary_Insert`/`LookUp` mechanism
   (dict.ch) keyed by `self` -- the same mechanism this file already
   uses for viewref tracking (see LinkTree/FinalizeObject) -- so no
   struct layout changes anywhere. */
struct scrollweight {
    long position;
    long extraWeight;
    long height;	/* real pixel height, needed by DecodeWeight's
			   proportional rescale -- see its own comment
			   (textv.c, 2026-09-24) */
};

struct InsertStack {
    struct InsertStack *next;
    char *name;
};


#define textview_UNKNOWNVIEW 1
#define textview_REMOVEVIEW 0L

/* Menu mask bits. */
#define textview_NoMenus -1         /* Used as a flag indicating no menus are posted. Minor hack. */
#define textview_SelectionMenus 1   /* Menus to be posted when there is a selection region. */
#define textview_NoSelectionMenus 2 /* Menus to be posted only when there is no
                                     * selection. Handles the bogus case of the
                                     * paste item which should show up all the
                                     * time.
                                     */
#define textview_NotReadOnlyMenus 4 /* Menus to be posted for writable textviews. */

/* Key (as in third argument to menulist_ChainML) under which style menus are
 * chained to normal menulist.
 */
#define textview_StyleMenusKey 1

class textview[textv] : view {
overrides:
    WantUpdate(struct view *requestor);
    SetDataObject(struct dataobject *dataObject);
    FullUpdate(enum view_UpdateType type, long left, long top, long width, long height);
    Update();
    Hit (enum view_MouseAction action, long x, long y, long numberOfClicks) returns struct view *;
    ReceiveInputFocus();
    LoseInputFocus();
    GetApplicationLayer() returns struct view *;
    DeleteApplicationLayer(struct view *scrollbar);
    GetInterface(char *interfaceName) returns char *;
    Print(FILE *file, char *processor, char *finalFormat, boolean topLevel);
    WantNewSize(struct view *requestor);
    ObservedChanged (struct observable *changed, long value);
    LinkTree(struct view *parent);
    DesiredSize(long width, long height, enum view_DSpass pass, long *dWidth, long *dheight) returns enum view_DSattributes;
    GetOrigin(long width, long height, long *originX, long *originY);
    InitChildren();
    CanView(char *TypeName) returns boolean;
    WriteSelection(File *out) returns long;
    LoseSelectionOwnership();
    
methods:
    SetDotPosition(long newPosition);
    SetDotLength(long newLength);
    GetDotPosition() returns long;
    GetDotLength() returns long;
    GetTopPosition() returns long;
    SetTopPosition(long newTopPosition);
    SetBorder(long xBorder, long yBorder);
    SetEmbeddedBorder(long xBorder, long yBorder);
    CollapseDot() returns long;
    GetClickPosition(long position, long numberOfClicks, enum view_MouseAction action, long startLeft, long startRight, long *leftPos, long *rightPos);
    Visible(long pos) returns boolean;
    Locate(long x, long y, struct view **view) returns long;
    GetTextSize(long *width, long *height);
    MoveBack(long pos, long units, enum textview_MovementUnits type, long *distMoved, long *linesAdded) returns long;
    MoveForward(long pos, long units, enum textview_MovementUnits type, long *distMoved, long *linesAdded) returns long;
    FrameDot(long pos);
    FindLineNumber(long pos) returns long;
    LineRedraw(enum textview_LineRedrawType type, struct mark *currentLine, long x, long y, long xSize, long ySize, long search, boolean *cont,long *textheight, struct formattinginfo *info) returns long;
    LookCmd(int look);
    SetDefaultStyle(struct style *styelptr);
    GetDefaultStyle() returns struct style *;
    ViewMove(struct linedesc *lineStructure,long movement);
    GetStyleInformation(struct text_statevector *sv, long pos, long *length) returns struct environment *;
    GetEnclosedStyleInformation(long pos, long *length) returns struct environment *;
    ReleaseStyleInformation(struct environment *env);

    PrepareDeletion(long pos, long len);
    FinishDeletion();
    DeleteCharacters(long pos, long len);

    PrepareInsertion(boolean insertingNewLine);
    FinishInsertion();

    GetEnclosingEnvironment(long pos) returns struct environment *;
    GetInsertEnvironment(long pos) returns struct environment *;

    ClearInsertStack();
    PopInsertStack();
    AddStyleToInsertStack(char *styleName);

    PlainInsertEnvironment();
    UpInsertEnvironment();
    DownInsertEnvironment();
    LeftInsertEnvironment();
    RightInsertEnvironment();

    DoCopyRegion(long pos, long len, boolean append, boolean copyAsText);

    ToggleVIMode();	/* toggle input/command modes for vi editor FAS */
    ToggleEditor();	/* toggle between editors (vi/emacs) FAS */

    SetTopOffTop(long newTopPosition, long pixelsOffTop);

macromethods:
    GetEditor() (self->editor)
    GetVIMode() (self->viMode)
    EncodePosition(pos)  ((pos) << textview_FINESCROLL)
    DecodePosition(pos)  ((pos) >> textview_FINESCROLL)

classprocedures:
    FinalizeObject(struct textview *self);
    InitializeClass() returns boolean;
    InitializeObject(struct textview *self) returns boolean;
data:
    short aLines;				/* allocated lines */
    short nLines;				/* number of lines currently shown */
    struct linedesc *lines;			/* the view image */
    short bx;				/* the border size - when using application layer*/
    short by;
    short ebx;				/* the border size when embedded */
    short eby;
    boolean hasApplicationLayer;
    boolean hasInputFocus;			
    boolean zapRest;				/* true if hit file's eof in redisplay */
    boolean force;				/* should we force a full redraw? */
    struct mark *dot;
    struct mark *top;
    struct mark *frameDot;
    struct keystate *keystate;
    struct menulist *styleMenus, *menus;	/* style menus are only present when dot is non-zero length. */
    short clearFrom;				/* y last cleared from */
    short csxPos;				/* old cursor x position */
    short csyPos;				/* old cursor y posn (-1 -> never shown) */
    short cshPos;				/* old cursor line height */
    short csbPos;				/* old cursor linebelow */
    short cexPos;				/* old cursor x position */
    short ceyPos;				/* old cursor y posn (-1 -> never shown) */
    short cehPos;				/* old cursor line height */
    short cebPos;				/* old cursor linebelow */
    enum textview_ScrollDirection scroll;
    int scrollLine;				/* line to use for scrolling. */
    int scrollDist;				/* distance to scroll */
    boolean needUpdate;
    int movePosition;
    long lastStyleMenuVersion;			/* Version number of stylesheet from which menus were last posted. */
    struct mark *atMarker;
    struct style *defaultStyle;			/* style from preferences */
    struct viewref *currentViewreference; /* Used for creating new views on existing dataobjects */
    long displayLength;
    struct environment *displayEnvironment;
    long displayEnvironmentPosition;
    long tabWidth;
    boolean exposeStyles;
    boolean showColorStyles;
    long predrawnY,predrawnX; 
    struct mark	*predrawn;	/* text preceding the dot that may avoid redrawing */
    struct mark	*prepredrawn;	/* text preceeding predrawn that may avoid redrawing,
					 used if the predrawn mark is modified */

    int editor;					/* 0->emacs, 1->vi */
    int viMode;					/* 0->command, 1->input */
    struct keystate *emacsKeystate;
    struct keystate *viInputModeKeystate;
    struct keystate *viCommandModeKeystate;

    struct environment *insertEnvironment;
    struct mark *insertEnvMark;
    struct InsertStack *insertStack;

    /* The next two params are to get tabs looking correct. Good values are
     * Andy12 = Adobe14 -> Mul = 14, Div = 12     -njw.
     */
    int ScreenScaleMul;
    int ScreenScaleDiv;

    /* These three are used to keep track of insets that are partially
     * scrolled off the top of the view. -dba.
     */
    int pixelsShownOffTop;
    int pixelsReadyToBeOffTop;
    int pixelsComingOffTop;
    /* This parameter determines if we draw a line through format
     * notes when we display them on the screen. */
    int LineThruFormatNotes;
};

#define EMACS	0
#define VI	1

/* VI mode values  FAS */
#define	COMMAND	0
#define INPUT	1

