/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/* htmlprevapp -- minimal standalone driver for htmlatk.c (Stage 3 of
   the HTML-mail-rendering project), the graphical counterpart to
   htmlatktest.test. Takes one argument, a raw HTML file (no MIME
   envelope), and displays it in a plain scrollable window via the
   exact same htmlpart_Parse()/htmlatk_Render() pipeline text822.c
   uses for real mail -- no mailbox, no MIME wrapping, no network
   fetch. <img src> values are resolved as plain local files relative
   to the HTML file's own directory (htmlprv.c's ResolveLocalImage),
   which is what lets a fixture with its own images/ folder (see
   revival/tests/bookrack.html) render its real pictures with zero
   network dependency. Invoke via "runapp htmlprv <file.html>". */
class htmlprevapp[htmlprv] : application[app] {
overrides:
    ParseArgs(int argc, char **argv) returns boolean;
    Start() returns boolean;
data:
    char *filename;
};
