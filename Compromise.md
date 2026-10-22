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

## 2. `-v` quoted packet: IP options are not printed

**What we did.** After the message line of an ICMP error about our request,
`print_quoted_packet()` (`srcs/icmp_error.c`) prints the quoted packet like inetutils: with
`-v` for every error type, and also without `-v` for Source Quench and Parameter Problem (in
that case without the `IP Hdr Dump:` lines):

```
92 bytes from 192.168.122.1: Time to live exceeded
IP Hdr Dump:
 4500 0054 be8f 4000 0101 6fb2 c0a8 7aaf 0808 0808 
Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src	Dst	Data
 4  5  00 0054 be8f   2 0000  01  01 6fb2 192.168.122.175  8.8.8.8 
ICMP: type 8, code 0, size 64, id 0x059d, seq 0x0000
```

**inetutils behavior.** `print_ip_header` (`ping_echo.c:301`) also prints the quoted IP options
in hex in the `Data` column, and guesses the byte order of the `Len` field.

**ft_ping behavior.** The `Data` column is always empty, and `Len` is always read in network
byte order.

**When it shows.** Never in practice: our requests carry no IP options (the kernel builds their
IP header), and the quoted header comes from the network, so it is always in network order.

**Why it's acceptable.** The output is identical for every packet ft_ping can send (checked
against inetutils with `ping -v --ttl=1 8.8.8.8` on the VM), and the code stays short.

---

## 3. Parameter Problem: print the pointer, not an "IP address"

**What we did.** For a Parameter Problem message, ft_ping prints the pointer as a number:

```
N bytes from X: Parameter problem: pointer = 20
```

**What RFC 792 says.** In the "Parameter Problem Message" section, the second 32-bit row of the
ICMP header is:

```
   |    Pointer    |                   unused                      |
```

- **Pointer** (byte 4, one byte): "identifies the octet where an error was detected". It is the
  number of the byte, counted from 0, in our original IP header that the router did not accept.
  For example 1 = TOS, 20 = first IP option.
- **unused** (bytes 5–7): always 0.

**inetutils behavior.** `print_parameterprob` (`ping_echo.c:382`) does:

```c
printf ("Parameter problem: IP address = %s\n", inet_ntoa (icmp->icmp_gwaddr));
```

`icmp_gwaddr` is the **Redirect** field, where bytes 4–7 really are a 4-byte gateway address.
For Parameter Problem, inetutils reads the 1-byte pointer and the 3 unused bytes as one IP
address. Pointer 20 is therefore printed as:

```
Parameter problem: IP address = 20.0.0.0
```

This has two mistakes:

- **Wrong label.** "IP address", but the value is a byte offset, not an address.
- **Wrong size.** 4 bytes are read instead of 1, so the 3 unused bytes show up as `.0.0.0`.

**ft_ping behavior.** We read only byte 4 and print it as a number, following the RFC.

**When it shows.** Only when a Parameter Problem about our own echo request arrives. This practically
never happens: it needs a malformed IP header (usually bad IP options), but our requests have no
options, and the kernel builds their IP header.

**Why it's acceptable.**

- **It follows the standard.** The RFC defines the field as a one-byte pointer, and we print it
  as one.
- **The output is clearer.** `pointer = 20` can be read directly. `IP address = 20.0.0.0` hides
  the value behind the wrong label.
- **The information is the same.** The pointer value appears in both outputs. We only fix how
  it is shown.
- **It almost never shows**, so in practice our output cannot be told apart from inetutils'.

---

## 4. `recvfrom` error: reported and ignored, never fatal

**What we did.** In `recv_ping` (`srcs/ping.c`), when `recvfrom` fails with anything other than
`EINTR` (retried) or `EAGAIN` / `EWOULDBLOCK` (nothing to read), ft_ping prints
`recvfrom: <strerror>` on stderr and returns to the loop. The packet is treated as lost.
`recv_ping` can therefore never fail, and `ping_loop` only stops on a `sendto` or `select`
failure.

**inetutils behavior.** `ping_recv()` (`libping.c`) returns -1 on a `recvfrom` error without
printing anything. `ping_run()` ignores that return value and keeps looping. The user never
learns that a read failed.

**ft_ping behavior.** Same loop behavior (keep pinging, statistics preserved), plus one error
line on stderr.

**When it shows.** Practically never. After `select` reported the socket readable, the only
realistic `recvfrom` failures on a raw ICMP socket are `ENOMEM` / `ENOBUFS` under kernel memory
pressure. The other documented errors (`EBADF`, `EFAULT`, `EINVAL`...) can only come from a bug
in our own code. There is no connection and no error queue, so no `ECONNREFUSED`-style errors.

**Why it's acceptable.**

- **Same robustness as inetutils.** A transient kernel error costs one reply, not the whole
  session: the run goes on and the statistics are still printed at the end.
- **Better visibility.** inetutils hides the error; we show it, like we already do for
  `packet too short` and `checksum mismatch`.
- **Consistent with the send side.** A `sendto` failure stays fatal (as in inetutils), because
  ping cannot do its job without sending. A single failed read does not prevent it.
- **Before this change** ft_ping exited with status 1 and no statistics on such an error. That
  was stricter than the reference, not better: it threw away everything measured so far.
