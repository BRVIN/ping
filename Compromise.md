# Compromises

This file lists the places where ft_ping **knowingly behaves differently** from the reference
(GNU inetutils-2.0 ping), what the difference is, and why we accepted it.

Each entry gives:

- **What we did**: the change in our code;
- **inetutils behavior**: what the reference does;
- **ft_ping behavior**: what we do now;
- **When it shows**: in which cases the two outputs differ;
- **Why it's acceptable**: why we chose to keep the difference.

---

## 1. Whitelist of ICMP error types before `handle_icmp_error`

**What we did.** In `recv_ping` (`srcs/ping.c`), only the 5 real ICMP error types are passed to
`handle_icmp_error` (`srcs/icmp_error.c`):

| Type | Name |
|---|---|
| 3 | Destination Unreachable |
| 4 | Source Quench |
| 5 | Redirect |
| 11 | Time Exceeded |
| 12 | Parameter Problem |

Echo replies (type 0) continue to the normal reply code. **Every other type is dropped silently.**

Before, the code worked the other way round. It dropped 5 types (8, 13, 14, 17, 18), kept echo
replies, and sent *everything else* to `handle_icmp_error`. Because only the 5 types above can
reach `handle_icmp_error` now, these branches were removed from it:

- type 9: `Router Advertisement`
- type 10: `Router Discovery`
- type 15: `Information Request`
- type 16: `Information Reply`
- any other type: `Bad ICMP type: N`

**inetutils behavior.** `ping_recv()` (`libping.c`) uses a `switch` with a `default:` branch.
Every type that is not a reply or a request goes through `my_echo_reply()`, which checks for a
quoted echo request with our target and our id. If that check passes, the packet is printed:
the real errors as above, and 9 / 10 / 15 / 16 / unknown types with the messages listed above.

**ft_ping behavior.** Types 9, 10, 15, 16 and unknown types are never printed.

**When it shows.** Practically never. A packet is printed only if it contains a **quoted copy of
our own echo request**: a quoted IP header to our target, protocol ICMP, then an echo request
header with our id. That is at least `ip_hlen + 8 + 20 + 8` bytes.

- **Types 3, 4, 5, 11, 12** quote the packet that failed (RFC 792 requires it). They are still
  handled, with exactly the same output as before.
- **Types 10, 15 and 16** are 8-byte messages with no data. They always failed the length check
  in `is_from_us`, so they were **never** printed, even before the change. Their branches
  could not run.
- **Type 9 and unknown types** have no quoted packet. They could only be printed if their data
  happened to look exactly like our quoted echo request. In practice that takes a crafted
  packet.

So the outputs differ only for a hand-crafted type 9 / unknown-type packet built to imitate our
request. For such a packet, inetutils prints `Router Advertisement` or `Bad ICMP type: N`, and
ft_ping prints nothing.

**Why it's acceptable.**

- **No visible change** for any real network traffic. Every legitimate packet produces the same
  output as inetutils.
- **The code is easier to read.** `recv_ping` now says explicitly which packets are errors,
  instead of "everything that is not something else".
- **`handle_icmp_error` is simpler.** It only handles packets that can actually reach it, so the
  branches that could never run are gone.
- **It fits the subject.** We do a minimal implementation of inetutils ping, not a byte-for-byte
  copy of every corner case. Dropping malformed or crafted packets silently is reasonable for
  a ping.

---

## 2. Minimal `-v`: only the id

**What we did.** With `-v`, ft_ping only adds the ICMP identifier to the first line:

```
PING host (1.2.3.4): 56 data bytes, id 0x1234 = 4660
```

Nothing else changes with `-v`. In `srcs/icmp_error.c`, `print_ip_dump()` and `print_icmp()`
were removed (`print_quoted_packet()` was removed later, see entry 3).

**inetutils behavior.** In `ping_echo.c`, `-v` (`OPT_VERBOSE`) also changes how ICMP errors
about our packets are printed:

- **Dest Unreachable, Redirect, Time Exceeded** (line 377): after the message line, it prints
  the quoted packet, as a decoded table of the IP header followed by an
  `ICMP: type …, code …, size …, id …, seq …` line.
- **All error types** (line 309): before that table, it prints the raw bytes of the quoted IP
  header (`IP Hdr Dump:`).

Without `-v`, inetutils prints only the message line for these three types. For Source Quench
and Parameter Problem, it prints the message line and then the decoded table, but no
`IP Hdr Dump:`.

**ft_ping behavior.** Errors are printed the same way with or without `-v`: for Dest
Unreachable / Redirect / Time Exceeded, `N bytes from X: <message>` only, which is exactly
inetutils' output **without** `-v`. (Source Quench and Parameter Problem: see entry 3.)

**When it shows.** Only when **both** of these are true:

- `-v` is given;
- an ICMP error about one of our packets arrives (unreachable host, TTL expired, …).

Echo replies, the `PING` line, statistics and every output without `-v` are identical to
inetutils.

**Why it's acceptable.**

- **The useful information is still printed.** The error message line, which tells what went
  wrong and which machine reported it, appears with or without `-v`. The extra `-v` output is a
  low-level dump of our own request, which we already know.
- **It fits the subject.** We do a minimal implementation of ping, and the subject asks for
  `-v` without describing these dumps. Keeping the id display still makes `-v` visibly do
  something, like inetutils.
- **Output without `-v` is unchanged** by this entry, so the default behavior still matches the
  reference (apart from entry 3).
- **The code is simpler.** The hex dump and the verbose-only branches are gone.

---

## 3. No quoted packet for Source Quench and Parameter Problem

**What we did.** For these two error types, ft_ping prints only the message line:

```
N bytes from X: Source Quench
N bytes from X: Parameter problem: IP address = 20.0.0.0
```

`print_quoted_packet()` was removed from `srcs/icmp_error.c`. With this entry and entry 2, every
ICMP error now prints exactly one line, with or without `-v`.

**inetutils behavior.** In its ICMP type table (`ping_echo.c:392`), Source Quench calls
`print_ip_data` directly, and `print_parameterprob` (`ping_echo.c:382`) calls it too. Neither
checks `-v`. So for these two types, inetutils always prints the decoded quoted packet after the
message, even without `-v`:

```
Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src	Dst	Data
 4  5  00 0054 1234   2 0000  40  01 abcd 192.168.1.2  8.8.8.8
ICMP: type 8, code 0, size 64, id 0x1234, seq 0x0003
```

RFC 792 does not ask for this. It describes the messages, not what a ping program should print.
It is a display choice of inetutils.

**ft_ping behavior.** The message line only. The `Parameter problem: IP address = …` line is
kept exactly as in inetutils. inetutils reads bytes 4–7 of the ICMP header as an IP address,
although RFC 792 defines byte 4 as a one-byte pointer. So pointer 20 shows as `20.0.0.0`.

**When it shows.** Only if one of these two errors about our own echo request arrives. That is
the only case where our output is shorter than inetutils', with or without `-v`.

- **Source Quench** is obsolete. RFC 6633 (2012) deprecated it: routers must not send it any
  more, hosts must ignore it, and Linux does not generate it.
- **Parameter Problem** needs a malformed IP header (usually bad IP options). Our requests have
  no options, and the kernel builds their IP header, so a router has nothing to complain about.

**Why it's acceptable.**

- **These cases practically never happen.** One message is deprecated, and the other cannot be
  triggered by the packets ft_ping sends.
- **The useful information is still printed.** The message line says what happened and which
  machine reported it.
- **The code is simpler.** About 35 lines of formatting code (table header, byte-order guess,
  IP options dump) are gone, and `handle_icmp_error` no longer needs the quoted header pointer.
- **It fits the subject.** We do a minimal implementation of ping, and the subject does not ask
  for packet dumps.
