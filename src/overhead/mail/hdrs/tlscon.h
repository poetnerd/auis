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
	tlscon.h -- Include file for the protocol-neutral TLS connection
		   module (tlscon.c).  A tlscon is a TCP connection with
		   implicit TLS (SSL_connect immediately after connect;
		   no STARTTLS state machine in v1) and line-buffered
		   read on top.  Used by the SMTP dropoff module
		   (milestone 1); IMAP (milestone 2) reuses it.  This
		   module knows nothing about SMTP or IMAP.
*/

struct tlscon;		/* opaque */

/* int tlscon_Open(cp, host, port, errbuf, errlen)
       struct tlscon **cp;
       char *host;
       int port;
       char *errbuf;
       int errlen;

   Connects to host:port, performs the TLS handshake (implicit TLS,
   hostname verification on), and sets a 60-second read timeout on the
   underlying socket.  On success, *cp is set to a new tlscon and 0 is
   returned.  On failure, a printable reason is placed in errbuf (up to
   errlen bytes) and -1 is returned; *cp is not touched. */
extern int tlscon_Open();

/* int tlscon_ReadLine(c, buf, len)
       struct tlscon *c;
       char *buf;
       int len;

   Reads one CRLF-terminated line (CRLF stripped), null-terminated into
   buf (up to len-1 bytes plus the null).  Returns the line length, or
   -1 on error, timeout, or a line that doesn't fit in the buffer. */
extern int tlscon_ReadLine();

/* int tlscon_ReadBytes(c, buf, n)
       struct tlscon *c;
       char *buf;
       int n;

   Reads exactly n raw bytes with no line/CRLF interpretation -- for
   protocols with byte-counted literals (e.g. IMAP's "{n}" syntax).
   buf is NOT null-terminated and may contain embedded CRLF/NUL.
   Returns n on success, -1 on error/timeout (in which case some bytes
   may have been consumed from the stream; the connection should be
   treated as unusable). Added for milestone 2 (IMAP); milestone 1's
   tlscon_ReadLine/_Write/_Open/_Close are unchanged. */
extern int tlscon_ReadBytes();

/* int tlscon_Write(c, buf, len)
       struct tlscon *c;
       char *buf;
       int len;

   Writes all len bytes of buf.  Returns 0 on success, -1 on error. */
extern int tlscon_Write();

/* void tlscon_Close(c)
       struct tlscon *c;

   Shuts the TLS session down gracefully, closes the socket, and frees
   the tlscon.  c must not be used afterward. */
extern void tlscon_Close();
