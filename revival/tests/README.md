# revival/tests

Committed, repeatable test scripts for the AUIS revival (as opposed to
`revival/tools`, which holds one-off developer utilities).

## smtp-protocol-tests

`smtp-protocol-tests [RECIPIENT]` -- the milestone 1 SMTP dropoff
module's Gate A protocol suite (see `revival/doc/smtp-send-prompt.md`),
packaged as a durable regression test. It builds `smtptest.test` and
`dropoff.test` from `src/overhead/mail/lib`, generates its whole
message/PROFILES/netrc corpus fresh into a temp directory on each run,
and exercises the seven Gate A cases: simple send, dot-stuffing,
LF-only input, two recipients, a bad recipient, a bad password, and
the legacy sendmail path when `smtphost` is unset.

Run with a recipient address to exercise the five live-send cases
against the real `smtp.fastmail.com` (mail goes only to that address);
run with no argument to run just the two deterministic, offline cases
(bad password, `smtphost` unset). Never touches `~/preferences` or the
real `~/.netrc`'s contents; never prints a password.

```
revival/tests/smtp-protocol-tests wdc@fastmail.com
```

For the real end-to-end acceptance counterpart -- driving the actual
`cui` UI through mail composition and submission, rather than the SMTP
module directly -- see `revival/tools/smtp-send-test`.
