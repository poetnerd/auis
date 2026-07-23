/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	imaptest.c -- Gate 2 standalone driver for imap_prot.c.  Not part
		     of libmail.a; built only via the .test suffix rule
		     (TestingOnlyTestingRule in this directory's Imakefile):

			make imaptest.test

	Usage:
	    imaptest.test capability  <host> <port>
	    imaptest.test login       <host> <port> <netrcpath> <machine>
	    imaptest.test list        <host> <port> <netrcpath> <machine>
	    imaptest.test examine     <host> <port> <netrcpath> <machine> <mailbox>
	    imaptest.test searchall   <host> <port> <netrcpath> <machine> <mailbox>
	    imaptest.test searchfetch <host> <port> <netrcpath> <machine> <mailbox> <subjectmarker> <bodyoutfile>
	    imaptest.test reconnect   <host> <port> <netrcpath> <machine> <mailbox>
	    imaptest.test create      <host> <port> <netrcpath> <machine> <mailbox>
	    imaptest.test storeflags  <host> <port> <netrcpath> <machine> <mailbox> <uid> <add|rem> <flag> [flag...]
	    imaptest.test expunge     <host> <port> <netrcpath> <machine> <mailbox> <uid>
	    imaptest.test append      <host> <port> <netrcpath> <machine> <mailbox> <bodyfile> [flags]
	    imaptest.test fetchflags  <host> <port> <netrcpath> <machine> <mailbox> <uid>
	    imaptest.test canned

	Every subcommand (except "canned", which is fully offline) opens
	its own fresh connection, does its thing, and prints "KEY: value"
	lines to stdout for revival/tests/imap-protocol-tests to parse;
	protocol trace (if IMAP_TRACE is set in the environment) goes to
	stderr via imap_prot.c itself. netrcpath is used as given (already
	tilde-expanded by the caller) -- this driver does no path
	expansion of its own.

	Read-only through "reconnect": only EXAMINE and BODY.PEEK[] ever
	reach the wire. The four writeback-milestone subcommands
	(create/storeflags/expunge/append) are destructive by design --
	CREATE/STORE/EXPUNGE/APPEND reach the wire -- and each one
	refuses (GUARD-REFUSED: yes, nonzero exit, no further action) to
	run against any mailbox but "Revival/WritebackTest" (see
	guard_mailbox() below). Never call them with any other mailbox
	argument, including INBOX.

	ANSI C (C89 prototypes) throughout, per the milestone's language
	shift, which explicitly covers "any new test drivers" alongside
	imap_prot.c/.h -- unlike smtptest.c (milestone 1, still K&R).
*/

#ifndef NORCSID
#define NORCSID
#endif

#include <andrewos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netrc.h>
#include <imap_prot.h>

static const char *rcname(int rc)
{
    switch (rc) {
    case IMAP_OK:		return "IMAP_OK";
    case IMAP_NO:		return "IMAP_NO";
    case IMAP_BAD:		return "IMAP_BAD";
    case IMAP_DEAD:		return "IMAP_DEAD";
    case IMAP_UIDCHANGED:	return "IMAP_UIDCHANGED";
    default:			return "???";
    }
}

static int do_open(struct imapconn **connp, const char *host, int port)
{
    char errbuf[512];
    int rc;

    rc = imap_Open(connp, host, port, errbuf, sizeof(errbuf));
    if (rc != IMAP_OK) {
        printf("OPEN-RC: %d (%s)\n", rc, rcname(rc));
        printf("OPEN-ERR: %s\n", errbuf);
        return -1;
    }
    printf("OPEN-RC: %d (%s)\n", rc, rcname(rc));
    return 0;
}

static int do_login(struct imapconn *conn, const char *netrcpath, const char *machine)
{
    char login[256], passwd[256];
    int nrc, rc;

    nrc = netrc_Lookup((char *) netrcpath, (char *) machine, login, sizeof(login), passwd, sizeof(passwd));
    if (nrc != NETRC_OK) {
        printf("NETRC-RC: %d\n", nrc);
        return -1;
    }
    printf("NETRC-RC: %d\n", nrc);

    rc = imap_Login(conn, login, passwd);
    memset(passwd, 0, sizeof(passwd));
    printf("LOGIN-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) printf("LOGIN-ERR: %s\n", imap_ErrMsg(conn));
    return (rc == IMAP_OK) ? 0 : -1;
}

/* ---- capability ---- */

static int cmd_capability(int argc, char **argv)
{
    struct imapconn *conn;
    int port;

    if (argc != 4) { fprintf(stderr, "usage: capability <host> <port>\n"); return 2; }
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    printf("CAPABLE-IMAP4REV1: %s\n", imap_Capable(conn, "IMAP4REV1") ? "yes" : "no");
    imap_Close(conn);
    return 0;
}

/* ---- login ---- */

static int cmd_login(int argc, char **argv)
{
    struct imapconn *conn;
    int port;

    if (argc != 6) { fprintf(stderr, "usage: login <host> <port> <netrcpath> <machine>\n"); return 2; }
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }
    imap_Close(conn);
    return 0;
}

/* ---- list ---- */

static int list_cb(const char *name, const char *delim, const char *flags, void *rock)
{
    (void) delim;
    (void) flags;
    (void) rock;
    printf("LISTED: %s\n", name);
    return 0;
}

static int cmd_list(int argc, char **argv)
{
    struct imapconn *conn;
    int port, rc;

    if (argc != 6) { fprintf(stderr, "usage: list <host> <port> <netrcpath> <machine>\n"); return 2; }
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }

    rc = imap_List(conn, "", "*", list_cb, (void *) NULL);
    printf("LIST-RC: %d (%s)\n", rc, rcname(rc));
    imap_Close(conn);
    return (rc == IMAP_OK) ? 0 : 1;
}

/* ---- examine ---- */

static int cmd_examine(int argc, char **argv)
{
    struct imapconn *conn;
    struct imap_mboxinfo info;
    int port, rc;

    if (argc != 7) { fprintf(stderr, "usage: examine <host> <port> <netrcpath> <machine> <mailbox>\n"); return 2; }
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }

    rc = imap_Examine(conn, argv[6], &info);
    printf("EXAMINE-RC: %d (%s)\n", rc, rcname(rc));
    if (rc == IMAP_OK) {
        printf("EXISTS: %ld\n", info.exists);
        printf("UIDVALIDITY: %lu\n", info.uidvalidity);
        printf("UIDNEXT: %lu\n", info.uidnext);
        printf("HIGHESTMODSEQ: %llu\n", info.highestmodseq);
    }
    imap_Close(conn);
    return (rc == IMAP_OK) ? 0 : 1;
}

/* ---- searchall ---- */

static int cmd_searchall(int argc, char **argv)
{
    struct imapconn *conn;
    struct imap_mboxinfo info;
    unsigned long *uids;
    long count;
    int port, rc;

    if (argc != 7) { fprintf(stderr, "usage: searchall <host> <port> <netrcpath> <machine> <mailbox>\n"); return 2; }
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }

    rc = imap_Examine(conn, argv[6], &info);
    printf("EXAMINE-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }
    printf("EXAMINE-EXISTS: %ld\n", info.exists);

    rc = imap_UidSearch(conn, "ALL", &uids, &count);
    printf("SEARCH-RC: %d (%s)\n", rc, rcname(rc));
    if (rc == IMAP_OK) {
        printf("SEARCH-COUNT: %ld\n", count);
        if (uids != NULL) free(uids);
    }

    /* Prove the connection survived the (potentially huge) response --
       exactly Gate 1's searchall_probe.c evidence, now via the module. */
    if (rc == IMAP_OK) {
        struct imap_mboxinfo info2;
        int rc2 = imap_Examine(conn, argv[6], &info2);
        printf("POST-SEARCH-EXAMINE-RC: %d (%s)\n", rc2, rcname(rc2));
    }

    imap_Close(conn);
    return (rc == IMAP_OK) ? 0 : 1;
}

/* ---- searchfetch (subject search + fetch meta + fetch body) ---- */

struct meta_rock {
    unsigned long uid;
    char subject[512];
    int got;
};

static int meta_cb(unsigned long uid, const char *flags, const char *internaldate,
                    const struct imap_envelope *env, void *rockp)
{
    struct meta_rock *r = (struct meta_rock *) rockp;

    (void) flags;
    (void) internaldate;

    if (!r->got) {
        r->uid = uid;
        if (env->subject != NULL) {
            strncpy(r->subject, env->subject, sizeof(r->subject) - 1);
            r->subject[sizeof(r->subject) - 1] = '\0';
        } else {
            r->subject[0] = '\0';
        }
        r->got = 1;
    }
    return 0;
}

static int cmd_searchfetch(int argc, char **argv)
{
    struct imapconn *conn;
    struct imap_mboxinfo info;
    unsigned long *uids;
    long count;
    int port, rc;
    char criteria[600];
    struct meta_rock rock;
    FILE *outf;
    long bodysize;

    if (argc != 9) {
        fprintf(stderr, "usage: searchfetch <host> <port> <netrcpath> <machine> <mailbox> <subjectmarker> <bodyoutfile>\n");
        return 2;
    }
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }

    rc = imap_Examine(conn, argv[6], &info);
    printf("EXAMINE-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }

    snprintf(criteria, sizeof(criteria), "SUBJECT \"%s\"", argv[7]);
    rc = imap_UidSearch(conn, criteria, &uids, &count);
    printf("SEARCH-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }
    printf("SEARCH-COUNT: %ld\n", count);
    if (count < 1) { if (uids != NULL) free(uids); imap_Close(conn); return 1; }

    rock.got = 0;
    rock.subject[0] = '\0';
    {
        char uidset[32];
        snprintf(uidset, sizeof(uidset), "%lu", uids[0]);
        rc = imap_UidFetchMeta(conn, uidset, meta_cb, &rock);
    }
    free(uids);
    printf("FETCHMETA-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK || !rock.got) { imap_Close(conn); return 1; }
    printf("FOUND-UID: %lu\n", rock.uid);
    printf("ENVELOPE-SUBJECT: %s\n", rock.subject);

    outf = fopen(argv[8], "wb");
    if (outf == NULL) { fprintf(stderr, "cannot open %s for write\n", argv[8]); imap_Close(conn); return 1; }
    rc = imap_UidFetchBody(conn, rock.uid, outf, &bodysize);
    fclose(outf);
    printf("FETCHBODY-RC: %d (%s)\n", rc, rcname(rc));
    if (rc == IMAP_OK) printf("BODY-SIZE: %ld\n", bodysize);

    imap_Close(conn);
    return (rc == IMAP_OK) ? 0 : 1;
}

/* ---- reconnect drill ---- */

static int cmd_reconnect(int argc, char **argv)
{
    struct imapconn *conn;
    struct imap_mboxinfo info, info2;
    int port, rc;
    unsigned long orig_uidvalidity;
    char login[256], passwd[256];
    int nrc;

    if (argc != 7) { fprintf(stderr, "usage: reconnect <host> <port> <netrcpath> <machine> <mailbox>\n"); return 2; }
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;

    nrc = netrc_Lookup(argv[4], argv[5], login, sizeof(login), passwd, sizeof(passwd));
    if (nrc != NETRC_OK) { printf("NETRC-RC: %d\n", nrc); imap_Close(conn); return 1; }
    printf("NETRC-RC: %d\n", nrc);

    rc = imap_Login(conn, login, passwd);
    printf("LOGIN-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { memset(passwd, 0, sizeof(passwd)); imap_Close(conn); return 1; }

    rc = imap_Examine(conn, argv[6], &info);
    printf("EXAMINE-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { memset(passwd, 0, sizeof(passwd)); imap_Close(conn); return 1; }
    orig_uidvalidity = info.uidvalidity;
    printf("ORIG-UIDVALIDITY: %lu\n", orig_uidvalidity);

    rc = imap_TestForceClose(conn);
    printf("FORCECLOSE-RC: %d (%s)\n", rc, rcname(rc));

    rc = imap_Examine(conn, argv[6], &info2);
    printf("POSTCLOSE-EXAMINE-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_DEAD) {
        printf("UNEXPECTED: post-forceclose EXAMINE did not report IMAP_DEAD\n");
        memset(passwd, 0, sizeof(passwd));
        imap_Close(conn);
        return 1;
    }

    rc = imap_Reopen(conn, login, passwd);
    memset(passwd, 0, sizeof(passwd));
    printf("REOPEN-RC: %d (%s)\n", rc, rcname(rc));
    if (rc == IMAP_OK) {
        printf("REOPEN-UIDVALIDITY-MATCH: yes\n");
    } else if (rc == IMAP_UIDCHANGED) {
        printf("REOPEN-UIDVALIDITY-MATCH: no (IMAP_UIDCHANGED)\n");
    }

    imap_Close(conn);
    return (rc == IMAP_OK) ? 0 : 1;
}

/* ---- writeback milestone: write entry points (create/storeflags/
   expunge/append). Every one of these refuses to run against any
   mailbox but WRITE_MAILBOX_GUARD -- this driver is the thing that
   actually issues CREATE/STORE/EXPUNGE/APPEND on the wire, so the
   guard belongs here, not only in the Python test driver that calls
   it (revival/tests/imap-sync-tests has its own copy of this same
   assertion, independently, on the theory that a real safety rule
   should not have exactly one enforcement point). ---- */

#define WRITE_MAILBOX_GUARD "Revival/WritebackTest"

static int guard_mailbox(const char *mailbox)
{
    if (strcmp(mailbox, WRITE_MAILBOX_GUARD) != 0) {
        printf("GUARD-REFUSED: yes (mailbox \"%s\" != \"%s\")\n", mailbox, WRITE_MAILBOX_GUARD);
        return -1;
    }
    printf("GUARD-REFUSED: no\n");
    return 0;
}

/* ---- create ---- */

static int cmd_create(int argc, char **argv)
{
    struct imapconn *conn;
    int port, rc;

    if (argc != 7) { fprintf(stderr, "usage: create <host> <port> <netrcpath> <machine> <mailbox>\n"); return 2; }
    if (guard_mailbox(argv[6]) < 0) return 1;
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }

    rc = imap_Create(conn, argv[6]);
    printf("CREATE-RC: %d (%s)\n", rc, rcname(rc));
    imap_Close(conn);
    /* IMAP_NO (already exists) is an acceptable outcome for a
       "provision if absent" caller -- only exit nonzero on a harder
       failure. */
    return (rc == IMAP_OK || rc == IMAP_NO) ? 0 : 1;
}

/* ---- shared FLAGS-verify fetch callback (storeflags/expunge) ---- */

struct flags_rock {
    char flags[256];
    int got;
};

static int flags_verify_cb(unsigned long uid, const char *flags, const char *internaldate,
                            const struct imap_envelope *env, void *rockp)
{
    struct flags_rock *r = (struct flags_rock *) rockp;

    (void) uid;
    (void) internaldate;
    (void) env;
    strncpy(r->flags, flags != NULL ? flags : "", sizeof(r->flags) - 1);
    r->flags[sizeof(r->flags) - 1] = '\0';
    r->got = 1;
    return 0;
}

/* ---- storeflags ---- */

static int cmd_storeflags(int argc, char **argv)
{
    struct imapconn *conn;
    struct imap_mboxinfo info;
    int port, rc, add;
    struct flags_rock frock;
    char flagsbuf[256];
    int i;

    if (argc < 9) {
        fprintf(stderr, "usage: storeflags <host> <port> <netrcpath> <machine> <mailbox> <uid> <add|rem> <flag> [flag...]\n");
        return 2;
    }
    if (guard_mailbox(argv[6]) < 0) return 1;
    port = atoi(argv[3]);
    if (strcmp(argv[8], "add") == 0) add = 1;
    else if (strcmp(argv[8], "rem") == 0) add = 0;
    else { fprintf(stderr, "seventh arg must be \"add\" or \"rem\"\n"); return 2; }

    flagsbuf[0] = '\0';
    for (i = 9; i < argc; ++i) {
        if (i > 9) strncat(flagsbuf, " ", sizeof(flagsbuf) - strlen(flagsbuf) - 1);
        strncat(flagsbuf, argv[i], sizeof(flagsbuf) - strlen(flagsbuf) - 1);
    }

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }

    rc = imap_Select(conn, argv[6], &info);
    printf("SELECT-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }

    rc = imap_UidStoreFlags(conn, argv[7], add, flagsbuf);
    printf("STOREFLAGS-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }

    frock.got = 0;
    frock.flags[0] = '\0';
    rc = imap_UidFetchMeta(conn, argv[7], flags_verify_cb, &frock);
    printf("VERIFY-FETCHMETA-RC: %d (%s)\n", rc, rcname(rc));
    printf("VERIFY-FLAGS: %s\n", frock.flags);

    imap_Close(conn);
    return (rc == IMAP_OK) ? 0 : 1;
}

/* ---- fetchflags (read-only verification helper; no guard needed --
   EXAMINE + FETCH only, never STORE/EXPUNGE/APPEND/CREATE) ---- */

static int cmd_fetchflags(int argc, char **argv)
{
    struct imapconn *conn;
    struct imap_mboxinfo info;
    int port, rc;
    struct flags_rock frock;

    if (argc != 8) { fprintf(stderr, "usage: fetchflags <host> <port> <netrcpath> <machine> <mailbox> <uid>\n"); return 2; }
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }

    rc = imap_Examine(conn, argv[6], &info);
    printf("EXAMINE-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }

    frock.got = 0;
    frock.flags[0] = '\0';
    rc = imap_UidFetchMeta(conn, argv[7], flags_verify_cb, &frock);
    printf("FETCHMETA-RC: %d (%s)\n", rc, rcname(rc));
    printf("FOUND: %s\n", frock.got ? "yes" : "no");
    printf("FLAGS: %s\n", frock.flags);

    imap_Close(conn);
    return (rc == IMAP_OK) ? 0 : 1;
}

/* ---- expunge ---- */

static int cmd_expunge(int argc, char **argv)
{
    struct imapconn *conn;
    struct imap_mboxinfo info;
    int port, rc;
    struct flags_rock frock;

    if (argc != 8) { fprintf(stderr, "usage: expunge <host> <port> <netrcpath> <machine> <mailbox> <uid>\n"); return 2; }
    if (guard_mailbox(argv[6]) < 0) return 1;
    port = atoi(argv[3]);

    if (do_open(&conn, argv[2], port) < 0) return 1;
    if (do_login(conn, argv[4], argv[5]) < 0) { imap_Close(conn); return 1; }

    rc = imap_Select(conn, argv[6], &info);
    printf("SELECT-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }
    printf("UIDPLUS: %s\n", imap_Capable(conn, "UIDPLUS") ? "yes" : "no");

    rc = imap_UidStoreFlags(conn, argv[7], 1, "\\Deleted");
    printf("STOREFLAGS-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }

    rc = imap_UidExpunge(conn, argv[7]);
    printf("EXPUNGE-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }

    /* Verify: a FETCH of the now-expunged uid should come back with no
       matching FETCH line at all (haveuid stays 0 in the callback). */
    frock.got = 0;
    frock.flags[0] = '\0';
    rc = imap_UidFetchMeta(conn, argv[7], flags_verify_cb, &frock);
    printf("POSTEXPUNGE-FETCHMETA-RC: %d (%s)\n", rc, rcname(rc));
    printf("POSTEXPUNGE-STILL-PRESENT: %s\n", frock.got ? "yes" : "no");

    imap_Close(conn);
    return (rc == IMAP_OK) ? 0 : 1;
}

/* ---- append ---- */

static int cmd_append(int argc, char **argv)
{
    struct imapconn *conn;
    struct imap_mboxinfo info;
    int port, rc;
    FILE *bodyf;
    long bodysize;
    unsigned long out_uidvalidity, out_uid;
    const char *flags;

    if (argc < 8) {
        fprintf(stderr, "usage: append <host> <port> <netrcpath> <machine> <mailbox> <bodyfile> [flags]\n");
        return 2;
    }
    if (guard_mailbox(argv[6]) < 0) return 1;
    port = atoi(argv[3]);
    flags = (argc > 8) ? argv[8] : "";

    bodyf = fopen(argv[7], "rb");
    if (bodyf == NULL) { fprintf(stderr, "cannot open %s for read\n", argv[7]); return 1; }
    if (fseek(bodyf, 0, SEEK_END) != 0) { fclose(bodyf); return 1; }
    bodysize = ftell(bodyf);
    rewind(bodyf);

    if (do_open(&conn, argv[2], port) < 0) { fclose(bodyf); return 1; }
    if (do_login(conn, argv[4], argv[5]) < 0) { fclose(bodyf); imap_Close(conn); return 1; }

    rc = imap_Append(conn, argv[6], flags, NULL, bodyf, bodysize, &out_uidvalidity, &out_uid);
    fclose(bodyf);
    printf("APPEND-RC: %d (%s)\n", rc, rcname(rc));
    if (rc != IMAP_OK) { imap_Close(conn); return 1; }
    printf("APPEND-UIDVALIDITY: %lu\n", out_uidvalidity);
    printf("APPEND-UID: %lu\n", out_uid);

    rc = imap_Examine(conn, argv[6], &info);
    printf("VERIFY-EXAMINE-RC: %d (%s)\n", rc, rcname(rc));
    if (rc == IMAP_OK) printf("VERIFY-EXISTS: %ld\n", info.exists);

    imap_Close(conn);
    return 0;
}

/* ---- canned (offline, no network) ---- */

static int cmd_canned(int argc, char **argv)
{
    /* A single ENVELOPE response with the subject sent as a literal
       (rather than a quoted string) -- exactly the milestone 2
       spike's known stub. Byte-for-byte wire format: ENVELOPE's own
       opening "(" first (this function's contract, matching how
       imap_UidFetchMeta calls imap_parse_envelope after consuming
       that token itself), then date (quoted), subject as "{22}\r\n"
       followed by exactly 22 raw bytes, then from/sender/replyto/to/
       cc/bcc address-lists (a single address in "from"/"to", NIL for
       the rest), then inreplyto/messageid (NIL), then the closing
       ")". */
    static const char subject_literal[] = "STAGE-3-LITERAL-TEST!";	/* 22 bytes */
    char canned[1024];
    int n;
    struct imap_envelope env;
    int rc;
    int ok;

    (void) argc;
    (void) argv;

    n = snprintf(canned, sizeof(canned),
        "(\"Wed, 16 Jul 2026 12:00:00 -0400\" {%d}\r\n%s "
        "((\"A Sender\" NIL \"sender\" \"example.com\")) "
        "((\"A Sender\" NIL \"sender\" \"example.com\")) "
        "NIL "
        "((\"A Recipient\" NIL \"recipient\" \"example.org\")) "
        "NIL NIL NIL NIL)\r\n",
        (int) (sizeof(subject_literal) - 1), subject_literal);

    printf("CANNED-BYTES: %d\n", n);

    rc = imap_TestParseEnvelope(canned, (size_t) n, &env);
    printf("PARSE-RC: %d\n", rc);
    if (rc != 0) return 1;

    printf("PARSED-SUBJECT: %s\n", env.subject != NULL ? env.subject : "(null)");
    printf("PARSED-FROM: %s\n", env.from != NULL ? env.from : "(null)");
    printf("PARSED-TO: %s\n", env.to != NULL ? env.to : "(null)");
    printf("PARSED-CC: %s\n", env.cc != NULL ? env.cc : "(null)");

    ok = (env.subject != NULL && strcmp(env.subject, subject_literal) == 0);
    printf("SUBJECT-MATCH: %s\n", ok ? "yes" : "no");

    imap_FreeEnvelope(&env);
    return ok ? 0 : 1;
}

/* ---- dispatch ---- */

int main(int argc, char **argv)
{
    setvbuf(stdout, (char *) NULL, _IOLBF, 0);

    if (argc < 2) {
        fprintf(stderr, "usage: %s <capability|login|list|examine|searchall|searchfetch|reconnect|"
                        "create|storeflags|expunge|append|fetchflags|canned> ...\n", argv[0]);
        exit(2);
    }

    if (strcmp(argv[1], "capability") == 0) exit(cmd_capability(argc, argv));
    if (strcmp(argv[1], "login") == 0) exit(cmd_login(argc, argv));
    if (strcmp(argv[1], "list") == 0) exit(cmd_list(argc, argv));
    if (strcmp(argv[1], "examine") == 0) exit(cmd_examine(argc, argv));
    if (strcmp(argv[1], "searchall") == 0) exit(cmd_searchall(argc, argv));
    if (strcmp(argv[1], "searchfetch") == 0) exit(cmd_searchfetch(argc, argv));
    if (strcmp(argv[1], "reconnect") == 0) exit(cmd_reconnect(argc, argv));
    if (strcmp(argv[1], "create") == 0) exit(cmd_create(argc, argv));
    if (strcmp(argv[1], "storeflags") == 0) exit(cmd_storeflags(argc, argv));
    if (strcmp(argv[1], "expunge") == 0) exit(cmd_expunge(argc, argv));
    if (strcmp(argv[1], "append") == 0) exit(cmd_append(argc, argv));
    if (strcmp(argv[1], "fetchflags") == 0) exit(cmd_fetchflags(argc, argv));
    if (strcmp(argv[1], "canned") == 0) exit(cmd_canned(argc, argv));

    fprintf(stderr, "%s: unknown subcommand \"%s\"\n", argv[0], argv[1]);
    exit(2);
    return 2;
}
