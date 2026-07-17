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
	tlscon.c -- Protocol-neutral TLS connection module.  TCP connect
		   + implicit TLS (SSL_connect; no STARTTLS in v1),
		   hostname verification on, 60-second read timeout.
		   See tlscon.h for the public entry points.
*/

#include <andrewos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <tlscon.h>

#define TLSCON_RCVTIMEO 60	/* seconds */
#define TLSCON_RBUFSIZE 4096

struct tlscon {
    int fd;
    SSL_CTX *ctx;
    SSL *ssl;
    char rbuf[TLSCON_RBUFSIZE];
    int rstart, rend;		/* unread bytes are rbuf[rstart..rend) */
};

static void tlscon_sslerr(errbuf, errlen, prefix)
    char *errbuf;
    int errlen;
    char *prefix;
{
    unsigned long e;
    char ebuf[256];

    e = ERR_get_error();
    if (e != 0) ERR_error_string_n(e, ebuf, sizeof(ebuf));
    else strcpy(ebuf, "TLS error");
    snprintf(errbuf, errlen, "%s: %s", prefix, ebuf);
}

int tlscon_Open(cp, host, port, errbuf, errlen)
    struct tlscon **cp;
    char *host;
    int port;
    char *errbuf;
    int errlen;
{
    struct addrinfo hints, *res, *ai;
    char portbuf[16];
    int gaierr, sock;
    struct timeval tv;
    struct tlscon *c;
    SSL_CTX *ctx;
    SSL *ssl;
    X509_VERIFY_PARAM *vpm;
    long verifyresult;

    if (errbuf != NULL && errlen > 0) errbuf[0] = '\0';

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    sprintf(portbuf, "%d", port);
    gaierr = getaddrinfo(host, portbuf, &hints, &res);
    if (gaierr != 0) {
	if (errbuf != NULL) snprintf(errbuf, errlen, "DNS lookup of \"%s\" failed: %s",
		host, gai_strerror(gaierr));
	return -1;
    }

    sock = -1;
    for (ai = res; ai != NULL; ai = ai->ai_next) {
	sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sock < 0) continue;
	if (connect(sock, ai->ai_addr, ai->ai_addrlen) == 0) break;
	close(sock);
	sock = -1;
    }
    if (sock < 0) {
	if (errbuf != NULL) snprintf(errbuf, errlen, "connect() to %s:%d failed: %s",
		host, port, strerror(errno));
	freeaddrinfo(res);
	return -1;
    }
    freeaddrinfo(res);

    tv.tv_sec = TLSCON_RCVTIMEO;
    tv.tv_usec = 0;
    (void) setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv));

    ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL) {
	if (errbuf != NULL) tlscon_sslerr(errbuf, errlen, "SSL_CTX_new failed");
	close(sock);
	return -1;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
	if (errbuf != NULL) tlscon_sslerr(errbuf, errlen, "can't load CA trust store");
	SSL_CTX_free(ctx);
	close(sock);
	return -1;
    }

    ssl = SSL_new(ctx);
    if (ssl == NULL) {
	if (errbuf != NULL) tlscon_sslerr(errbuf, errlen, "SSL_new failed");
	SSL_CTX_free(ctx);
	close(sock);
	return -1;
    }

    /* Hostname verification (RFC 6125) and SNI. */
    vpm = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set_hostflags(vpm, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    X509_VERIFY_PARAM_set1_host(vpm, host, 0);
    SSL_set_tlsext_host_name(ssl, host);

    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) != 1) {
	if (errbuf != NULL) tlscon_sslerr(errbuf, errlen, "TLS handshake failed");
	SSL_free(ssl);
	SSL_CTX_free(ctx);
	close(sock);
	return -1;
    }
    verifyresult = SSL_get_verify_result(ssl);
    if (verifyresult != X509_V_OK) {
	if (errbuf != NULL) snprintf(errbuf, errlen, "TLS certificate verify failed: %s",
		X509_verify_cert_error_string(verifyresult));
	SSL_shutdown(ssl);
	SSL_free(ssl);
	SSL_CTX_free(ctx);
	close(sock);
	return -1;
    }

    c = (struct tlscon *) malloc(sizeof(struct tlscon));
    if (c == NULL) {
	if (errbuf != NULL) strcpy(errbuf, "out of memory");
	SSL_shutdown(ssl);
	SSL_free(ssl);
	SSL_CTX_free(ctx);
	close(sock);
	return -1;
    }
    c->fd = sock;
    c->ctx = ctx;
    c->ssl = ssl;
    c->rstart = c->rend = 0;

    *cp = c;
    return 0;
}

/* Refill the internal buffer with at least one more byte.  Returns
   the number of bytes read (>0), 0 on clean EOF, -1 on error/timeout. */
static int tlscon_refill(c)
    struct tlscon *c;
{
    int n;

    if (c->rstart > 0 && c->rstart == c->rend) {
	/* buffer empty: reset to the front so a long run of small reads
	   doesn't walk off the end for no reason */
	c->rstart = c->rend = 0;
    } else if (c->rend == TLSCON_RBUFSIZE) {
	if (c->rstart == 0) return -1;	/* line longer than the buffer */
	memmove(c->rbuf, c->rbuf + c->rstart, c->rend - c->rstart);
	c->rend -= c->rstart;
	c->rstart = 0;
    }

    n = SSL_read(c->ssl, c->rbuf + c->rend, TLSCON_RBUFSIZE - c->rend);
    if (n <= 0) return -1;	/* timeout, error, or peer closed mid-reply */
    c->rend += n;
    return n;
}

int tlscon_ReadLine(c, buf, len)
    struct tlscon *c;
    char *buf;
    int len;
{
    int i, n, outlen;

    for (;;) {
	for (i = c->rstart; i < c->rend; i++) {
	    if (c->rbuf[i] == '\n') {
		outlen = i - c->rstart;
		if (outlen > 0 && c->rbuf[i-1] == '\r') outlen--;
		if (outlen >= len) return -1;	/* doesn't fit */
		memcpy(buf, c->rbuf + c->rstart, outlen);
		buf[outlen] = '\0';
		c->rstart = i + 1;
		return outlen;
	    }
	}
	n = tlscon_refill(c);
	if (n < 0) return -1;
    }
}

/* Milestone 2 addition: read exactly n raw bytes (IMAP literals).
   Shares the same internal buffer/refill machinery as tlscon_ReadLine;
   does not otherwise change that function's behavior. */
int tlscon_ReadBytes(c, buf, n)
    struct tlscon *c;
    char *buf;
    int n;
{
    int avail, tocopy, got;

    got = 0;
    while (got < n) {
	avail = c->rend - c->rstart;
	if (avail <= 0) {
	    if (tlscon_refill(c) < 0) return -1;
	    continue;
	}
	tocopy = avail;
	if (tocopy > n - got) tocopy = n - got;
	memcpy(buf + got, c->rbuf + c->rstart, tocopy);
	c->rstart += tocopy;
	got += tocopy;
    }
    return got;
}

int tlscon_Write(c, buf, len)
    struct tlscon *c;
    char *buf;
    int len;
{
    int off, n;

    off = 0;
    while (off < len) {
	n = SSL_write(c->ssl, buf + off, len - off);
	if (n <= 0) return -1;
	off += n;
    }
    return 0;
}

void tlscon_Close(c)
    struct tlscon *c;
{
    if (c == NULL) return;
    if (c->ssl != NULL) {
	SSL_shutdown(c->ssl);
	SSL_free(c->ssl);
    }
    if (c->ctx != NULL) SSL_CTX_free(c->ctx);
    if (c->fd >= 0) close(c->fd);
    free(c);
}
