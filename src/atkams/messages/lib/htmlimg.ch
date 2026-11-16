/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/* An HTML mail image whose width is a percentage of its container
   (<img width="100%">, or CSS width: N%).  The pixels are the decoded
   image's own, at natural size; htmlimgview scales them to N% of the
   width the layout gives it, as a browser does.  The renderer can't
   size these at build time, since it doesn't know a cell's width until
   the window is laid out.  Kohl's assembles its hero picture from
   double-resolution slices this way. */
class htmlimage[htmlimg]: image {
    overrides:
      ViewName() returns char *;
    classprocedures:
      InitializeObject(struct htmlimage *self) returns boolean;
    macromethods:
      SetPercent(p) ((self)->pct = (p))
      GetPercent() ((self)->pct)
      SetMaxWidth(w) ((self)->maxwidth = (w))
      GetMaxWidth() ((self)->maxwidth)
    data:
      long pct;		/* width as a percentage of the container */
      long maxwidth;	/* CSS max-width in pixels, 0 for none */
};
