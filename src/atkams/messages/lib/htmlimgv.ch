/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/* The view for an htmlimage (htmlimg.ch): an imagev that draws its
   image at a percentage of the width it is offered, with the height in
   proportion.  DesiredSize only does arithmetic, since layout probes it
   many times at many widths; FullUpdate resamples the pixels, by area
   averaging to the exact size, once for each width actually drawn, and
   hands the copy to imagev as its "scaled" image, which imagev already
   draws in place of the original.  Everything else is imagev's. */
class htmlimgview[htmlimgv]: imagev {
    overrides:
      DesiredSize(long width, long height, enum view_DSpass pass, long *dWidth, long *dheight) returns enum view_DSattributes;
      FullUpdate(enum view_UpdateType type, long left, long top, long width, long height);
    classprocedures:
      InitializeObject(struct htmlimgview *self) returns boolean;
    data:
      long drawnw;	/* width of the resampled copy in scaled, 0 if none */
};
