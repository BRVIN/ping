# ping from GNU inetutils-2.0

Reference notes for the ft_ping project: **behaviour, output formats, design and quirks** of the
ping shipped with GNU inetutils 2.0 (the reference named by the subject).

Sources of information:
- reading `official_ping_inetutils/` and `man_ping_inetutils_v2`;
- **tests run on a Debian VM with the real inetutils 2.0 ping** (`ssh bully@192.168.122.175`,
  kernel 5.10, `/usr/bin/ping` installed **setuid root**, run as a normal user).
  Everything marked ✅ was observed there.

> The subject forbids using the sources of a standard ping. This file describes observable
> behaviour and design ideas only — nothing in it is meant to be copied as code.

In the transcripts below, `<TAB>` marks a real tab character, `␣` a significant trailing space,
and `\b` a backspace character.

---

## 1. Overview

- Descends from the 4.3BSD ping → output is BSD-style (close to macOS/FreeBSD), very different
  from the Linux `iputils` ping that most distributions ship today.
- `ping -V` ✅:
  ```
  ping (GNU inetutils) 2.0
  Copyright (C) 2021 Free Software Foundation, Inc.
  License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.

  Written by Sergey Poznyakoff.
  ```
  (`-V` wins over everything: `ping -V 127.0.0.1` only prints this.)
- `ping` is IPv4 only; IPv6 is a separate program (`ping6`).

| Source file | Role |
|---|---|
| `ping.c` | Option parsing (argp), `main`, main loop, sending, first part of the statistics |
| `ping_echo.c` | ECHO mode: header line, reply lines, ICMP error lines, `-v` dumps, IP options, round-trip statistics |
| `libping.c` | Socket creation, resolution, transmission, reception and filtering |
| `ping_common.c/.h` | Number parsing, data buffer, pattern decoding, time subtraction, square root, reverse DNS helpers, duplicate bitmap |
| `ping_timestamp.c`, `ping_address.c`, `ping_router.c`, `ping6.c` | Other request types / IPv6 — irrelevant for ft_ping |

---

## 2. Command line

Parsed with **GNU argp** (getopt_long underneath):
- options and host can be mixed in any order; `--` ends options ✅ (`ping -c1 -- -v` → host `-v` → `unknown host`);
- short options can be grouped: `-vc2` ✅;
- long options accept `--opt value` and `--opt=value`, and **unambiguous abbreviations**: `--verb`, `--tt 1` ✅;
- a repeated option: the last one wins (`-c 1 -c 2` → 2 packets) ✅;
- several hosts are accepted and pinged **one after the other**, each with its own header and statistics ✅.

### 2.1 Options

| Option | Meaning | Validation (✅) | ft_ping |
|---|---|---|---|
| `-v`, `--verbose` | Verbose (§7.5) | — | **mandatory** |
| `-?`, `--help` | Help list (§2.3) | `--help=x` → `option '--help' doesn't allow an argument`, exit 64 | **mandatory** |
| `--usage` | Short usage (§2.3) | | not a bonus |
| `-V`, `--version` | Version | | not a bonus |
| `-c`, `--count N` | Stop after N packets (§5.3) | `strtoul` base 0: `0x2` OK; `-1` wraps to a huge number → runs forever; `0` = unlimited | bonus |
| `-i`, `--interval N` | Seconds between sends, decimals OK | non-root and < 0.2 → `option value too small: 0.1` | bonus |
| `-w`, `--timeout N` | Stop after N seconds (§5.3) | 0 → `option value too small: 0` | bonus |
| `-W`, `--linger N` | Wait after the last send (default 10 s) | | bonus |
| `-s`, `--size N` | Data bytes (default 56) | 0 … **65399**; 65400 → `option value too big: 65400` | bonus |
| `-p`, `--pattern HEX` | Up to 16 filler bytes | `zz` → `error in pattern near zz` | bonus |
| `-l`, `--preload N` | N extra packets sent at once at start | "root only" in help, **not enforced** for a user on the setuid install ✅ | bonus |
| `-f`, `--flood` | Flood (§7.7) | same: not enforced ✅ | bonus |
| `-n`, `--numeric` | No reverse DNS (only affects error/option lines) | | bonus |
| `-q`, `--quiet` | Only header + statistics | | bonus |
| `-r`, `--ignore-routing` | `SO_DONTROUTE` | non-local target → `ping: sending packet: Network is unreachable`, exit 1 ✅ | bonus |
| `-d`, `--debug` | `SO_DEBUG` | | bonus |
| `-T`, `--tos N` | IP TOS | > 255 → `option value too big: 300` | bonus |
| `--ttl N` | IP TTL | 0 → `too small`, 256 → `too big`, `abc` → `invalid value` | bonus (needed to test errors) |
| `-R`, `--route` | Record Route IP option (§7.6) | | bonus |
| `--ip-timestamp tsonly\|tsaddr` | IP Timestamp option (§7.6) | other → `unsupported timestamp type: foo` | bonus |
| `-t`, `--type TYPE`, `--echo`, `--timestamp`, `--address`, `--mask` | Request type | `-t foo` → `unsupported packet type: foo` | — |

⚠️ In inetutils **`-t` is the packet type**, not the TTL (iputils uses `-t` for TTL).

### 2.2 Errors and exit codes ✅

All error messages go to **stderr**, prefixed by `ping: `.

| Situation | Message | Exit |
|---|---|---|
| No host | `ping: missing host operand`<br>`Try 'ping --help' or 'ping --usage' for more information.` | **64** |
| Unknown option | `ping: invalid option -- 'x'` + same "Try" line | **64** |
| Option argument error (argp) | e.g. `ping: option '--help' doesn't allow an argument` + "Try" line | **64** |
| Bad option value | `ping: invalid value (`'`abc' near `abc')`, `ping: option value too small: N`, `ping: option value too big: N` | 1 |
| Unresolvable host (incl. `256.1.1.1`) | `ping: unknown host` | 1 |
| No privilege (raw and datagram sockets refused) | `ping: Lacking privilege for icmp socket.` | 1 |
| `sendto` fails | `ping: sending packet: <strerror>` — **program stops**, no statistics | 1 |
| End, ≥ 1 echo reply | statistics | **0** |
| End, no echo reply (even if errors were received) | statistics | **1** |

Ctrl+C is a normal end: statistics then exit 0/1 as above ✅.

### 2.3 `-?` / `--help` and `--usage` output ✅

```
Usage: ping [OPTION...] HOST ...
Send ICMP ECHO_REQUEST packets to network hosts.

 Options controlling ICMP request types:
      --address              send ICMP_ADDRESS packets (root only)
      --echo                 send ICMP_ECHO packets (default)
      --mask                 same as --address
      --timestamp            send ICMP_TIMESTAMP packets
  -t, --type=TYPE            send TYPE packets

 Options valid for all request types:

  -c, --count=NUMBER         stop after sending NUMBER packets
  -d, --debug                set the SO_DEBUG option
  -i, --interval=NUMBER      wait NUMBER seconds between sending each packet
  -n, --numeric              do not resolve host addresses
  -r, --ignore-routing       send directly to a host on an attached network
      --ttl=N                specify N as time-to-live
  -T, --tos=NUM              set type of service (TOS) to NUM
  -v, --verbose              verbose output
  -w, --timeout=N            stop after N seconds
  -W, --linger=N             number of seconds to wait for response

 Options valid for --echo requests:

  -f, --flood                flood ping (root only)
      --ip-timestamp=FLAG    IP timestamp of type FLAG, which is one of
                             "tsonly" and "tsaddr"
  -l, --preload=NUMBER       send NUMBER packets as fast as possible before
                             falling into normal mode of behavior (root only)
  -p, --pattern=PATTERN      fill ICMP packet with given pattern (hex)
  -q, --quiet                quiet output
  -R, --route                record route
  -s, --size=NUMBER          send NUMBER data octets

  -?, --help                 give this help list
      --usage                give a short usage message
  -V, --version              print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Options marked with (root only) are available only to superuser.

Report bugs to <bug-inetutils@gnu.org>.
```
Exit 0. Note the layout produced by argp: group titles start with one space, options are sorted
inside a group, a blank line after some group titles but not the first one.

`--usage`:
```
Usage: ping [-dnrvfqR?V] [-t TYPE] [-c NUMBER] [-i NUMBER] [-T NUM] [-w N]
            [-W N] [-l NUMBER] [-p PATTERN] [-s NUMBER] [--address] [--echo]
            [--mask] [--timestamp] [--type=TYPE] [--count=NUMBER] [--debug]
            [--interval=NUMBER] [--numeric] [--ignore-routing] [--ttl=N]
            [--tos=NUM] [--verbose] [--timeout=N] [--linger=N] [--flood]
            [--ip-timestamp=FLAG] [--preload=NUMBER] [--pattern=PATTERN]
            [--quiet] [--route] [--size=NUMBER] [--help] [--usage] [--version]
            HOST ...
```

For ft_ping, the help should list only the options actually implemented, in the same style.

---

## 3. Startup

1. Parse options (errors stop here, before any socket).
2. Socket: **raw ICMP socket**. If refused (EPERM/EACCES), fall back to an unprivileged ICMP
   `SOCK_DGRAM` "ping socket" (kernel chooses the id → id check disabled). If refused too →
   `Lacking privilege…`. On the test VM the binary is setuid root and ping sockets are disabled
   (`ping_group_range = 1 0`), so the raw socket is used.
3. `SO_BROADCAST` enabled (broadcast targets allowed — though Linux hosts ignore broadcast
   echo requests by default: `ping 192.168.122.255` gets no reply ✅).
4. Root privileges dropped (`setuid(getuid())`).
5. stdout forced to **line buffering**.
6. `--ttl` / `--tos` applied with `setsockopt` (failure = warning only).
7. Filler buffer prepared.
8. For each host: resolve, print header, loop, print statistics.

### Resolution ✅

- `getaddrinfo(AF_INET, AI_CANONNAME)`; first address used.
- The **name printed in the header and the statistics is the canonical name**:
  - `ping 127.0.0.1` → `PING 127.0.0.1 (127.0.0.1)`
  - `ping localhost` → `PING localhost (127.0.0.1)`
  - `ping www.github.com` → `PING github.com (140.82.121.3)` / `--- github.com ping statistics ---` (CNAME followed)
  - `ping 127.1` → `PING 127.1 (127.0.0.1)` (short numeric forms accepted)
  - `ping 0` → `PING 0 (0.0.0.0)`, replies come `from 127.0.0.1`
  - `ping 256.1.1.1` → `ping: unknown host`
- No reverse DNS for the target. Reverse DNS **is** used for senders of ICMP errors and IP option
  addresses (§7.3, §7.6) — the subject excludes this for ft_ping.

---

## 4. Packet sent

```
 ICMP header (8 bytes)                       data (56 bytes by default)
+------+------+----------+--------+--------+----------------------+---------------------+
| type | code | checksum |   id   |  seq   | struct timeval (16)  | filler 00 01 .. 27  |
|  8   |  0   |          |        |        | send time            | (40 bytes)          |
+------+------+----------+--------+--------+----------------------+---------------------+
```

- **id** = `getpid() & 0xFFFF`, network byte order (`-v` shows it, e.g. `id 0x0dde = 3550` ✅).
- **seq** = number of packets successfully sent so far → **starts at 0** ✅.
- **Timestamp**: `gettimeofday` at send time, raw host format (only we read it back). Written only
  if data size ≥ `sizeof(struct timeval)` = **16** on 64-bit: `-s 15` → no `time=` and no
  round-trip line; `-s 16` → timing ✅. (The man page's "eight bytes" dates from 32-bit systems.)
- **Filler**: after the timestamp, bytes `00 01 02 … 27` (counting from 0, not from the offset).
  With `-p`, the given pattern repeated.
- Sizes: ICMP = 8 + data (64 by default), IP = 84. Max data = 65399 (65535 − 60 max IP header −
  76 ICMP margin) ✅.

---

## 5. Main loop

### 5.1 Structure

Single process, single thread, driven by **`select()`**:

```
send packet 0 immediately; last = now
while not interrupted:
    wait = (last + interval) - now   (clamped at 0)
    select(socket readable, wait)
      fails with EINTR  -> loop (Ctrl+C lands here, flag is now set)
      fails otherwise   -> fatal "select failed"
      readable          -> read + process one packet
                           stop if -w elapsed, or if -c answers reached
      timed out         -> if more to send: send, (-f: print "."), stop if -w elapsed
                           elif already lingering: stop
                           else: start lingering (wait = -W seconds, default 10)
                           last = now
print statistics
```

- **SIGINT** handler only sets a `volatile` flag; statistics are printed by the normal code path.
- Receiving does **not** shift the schedule: one send per interval whatever happens.
- No message for unanswered packets during the run.

### 5.2 Timing observations ✅

| Command | Duration | Result |
|---|---|---|
| `ping 127.0.0.1` + SIGINT at 6 s | 6 s | 6 transmitted (seq 0–5) |
| `ping -c 1 10.255.255.1` (black hole) | **11 s** | 1 s interval + 10 s linger |
| `ping -c 1 -W 2 10.255.255.1` | 3 s | 1 s + 2 s linger |
| `ping -w 3 10.255.255.1` | 3 s | **4** transmitted (sends at 0,1,2,3) |
| `ping -c 0 -w 2 127.0.0.1` | 2 s | **3 transmitted, 2 received, 33% loss** — stops right after the 3rd send, before its reply |
| `ping -c 2 --ttl 1 8.8.8.8` | 1 s | ICMP errors count as "answers" for `-c` |
| `ping -c 1 -l 3 127.0.0.1` | ~0 s | 4 transmitted, 1 received, 75% loss — stops at first answer |
| `ping -c 2 192.168.122.250` (no such LAN host) | 3 s | `Destination Host Unreachable` from **our own address**, arrives in bursts after ARP gives up (~3 s) |

### 5.3 End conditions
- Ctrl+C (SIGINT);
- `-c N`: after N "answers" (echo replies **or** relevant ICMP errors), or after N sends + linger;
- `-w N`: N seconds after start (checked after each send/receive);
- fatal error (`sendto`, `select`).

---

## 6. Reception and filtering

Each readable event → one `recvfrom` (IP header + ICMP), then:

| Packet | Treatment |
|---|---|
| Too short for IP + ICMP headers | stderr `packet too short (N bytes) from <ip>`, ignored |
| Echo reply, id ≠ ours | ignored silently |
| Echo reply, bad ICMP checksum | stderr `checksum mismatch from <ip>`, **still counted and printed** |
| Echo reply, seq already received | duplicate: printed with ` (DUP!)`, counted separately |
| Echo reply, new seq | received |
| Echo request (our own on loopback), timestamp/address requests | ignored |
| Any other ICMP type | handled **only if** the original datagram quoted inside it is ours: original destination = target, protocol ICMP, type ECHO, id = ours. Else ignored |

**Duplicates**: 1024-bit table indexed by `seq % 1024`; bit cleared when that seq is sent, set
on its first reply. Duplicates are not counted as received, but their RTT enters the statistics.

---

## 7. Output formats

`<…>` = value; everything else is literal.

### 7.1 Header ✅
```
PING <canonname> (<ip>): <datalen> data bytes
PING <canonname> (<ip>): <datalen> data bytes, id 0x<id %04x> = <id %u>        (-v)
```

### 7.2 Echo reply ✅
```
64 bytes from 127.0.0.1: icmp_seq=0 ttl=64 time=0.031 ms
64 bytes from 8.8.8.8: icmp_seq=0 ttl=115 time=6.291 ms
12 bytes from 127.0.0.1: icmp_seq=0 ttl=64                                     (-s 4: no time)
<n> bytes from <ip>: icmp_seq=<seq> ttl=<ttl> time=<rtt> ms (DUP!)             (duplicate)
```
- `<n>` = received length − IP header length (= 8 + data size).
- `<ip>` always numeric (no reverse DNS on replies, even without `-n`).
- `time` with 3 decimals; `-v` changes nothing on these lines ✅.

### 7.3 ICMP error lines ✅ (printed with or without `-v`)
```
92 bytes from _gateway (192.168.122.1): Time to live exceeded                  (--ttl 1)
92 bytes from 192.168.144.1: Time to live exceeded                             (--ttl 2, no reverse name)
92 bytes from bullsHost (192.168.122.175): Destination Host Unreachable        (unknown LAN host)
92 bytes from 192.168.122.1: Time to live exceeded                             (-n)
```
- `<n>` = received length − outer IP header = 8 (ICMP error header) + 20 (quoted IP header) + 64
  (quoted ICMP packet as Linux quotes it entirely) = **92**.
- Sender = `name (ip)` by reverse DNS, or `ip` with `-n` / when no name.
  **ft_ping: always print the IP** (subject: no DNS resolution on returned packets).
- Errors do **not** count as received; inetutils prints no error counter in the statistics.

Messages by type/code:

| Type | Code | Message |
|---|---|---|
| 3 Dest Unreachable | 0 | `Destination Net Unreachable` |
| | 1 | `Destination Host Unreachable` ✅ |
| | 2 | `Destination Protocol Unreachable` |
| | 3 | `Destination Port Unreachable` |
| | 4 | `Fragmentation needed and DF set` |
| | 5 | `Source Route Failed` |
| | 6 | `Network Unknown` |
| | 7 | `Host Unknown` |
| | 8 | `Host Isolated` |
| | 11 | `Destination Network Unreachable At This TOS` |
| | 12 | `Destination Host Unreachable At This TOS` |
| | 13 | `Packet Filtered` |
| | 14 | `Precedence Violation` |
| | 15 | `Precedence Cutoff` |
| | other | `Dest Unreachable, Unknown Code: <code>` |
| 5 Redirect | 0 / 1 / 2 / 3 | `Redirect Network` / `Redirect Host` / `Redirect Type of Service and Network` / `Redirect Type of Service and Host` |
| | other | `Redirect, Unknown Code: <code>` |
| 11 Time Exceeded | 0 | `Time to live exceeded` ✅ |
| | 1 | `Frag reassembly time exceeded` |
| | other | `Time exceeded, Unknown Code: <code>` |
| 4 | | `Source Quench`, then the header table (§7.4) **always** |
| 12 | | `Parameter problem: IP address = <addr>`, then the header table **always** |
| 0, 8, 9, 10, 13–18 | | their name (`Echo Reply`, `Echo Request`, `Router Advertisement`, `Router Discovery`, `Timestamp`, `Timestamp Reply`, `Information Request`, `Information Reply`, `Address Mask Request`, `Address Mask Reply`) |
| unknown | | `Bad ICMP type: <type>` |

### 7.4 `-v` dump of the quoted packet ✅

For Dest Unreachable / Redirect / Time Exceeded, `-v` adds 4 lines after the message, describing
**our original packet** as quoted in the error:

```
92 bytes from _gateway (192.168.122.1): Time to live exceeded
IP Hdr Dump:
 4500 0054 1b10 4000 0101 1332 c0a8 7aaf 0808 0808␣
Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src<TAB>Dst<TAB>Data
 4  5  00 0054 1b10   2 0000  01  01 1332 192.168.122.175  8.8.8.8␣
ICMP: type 8, code 0, size 64, id 0x0e66, seq 0x0000
```

Line by line:
1. `IP Hdr Dump:`
2. One space, then the 20 bytes of the quoted IP header in hex, by pairs, **each pair followed by
   a space** (so a trailing space).
3. Column titles: literal text with **tabs** between `Src`, `Dst` and `Data`.
4. Decoded values, format
   `" %1x  %1x  %02x %04x %04x   %1x %04x  %02x  %02x %04x %s  %s "` + IP option bytes in hex:
   version, IHL, TOS, total length, id, flags (3 top bits of the fragment field → `2` = DF),
   fragment offset, TTL (`01` = it left with TTL 1… then died), protocol (`01` = ICMP),
   checksum, source, destination. All hex except addresses; trailing space.
5. `ICMP: type 8, code 0, size 64, id 0x%04x, seq 0x%04x` — our original echo request
   (`size` = quoted total length − quoted IP header). For a quoted TCP/UDP packet the line is
   `TCP: from port A, to port B (decimal)` / `UDP: …` instead.

Same layout for `Destination Host Unreachable` (TTL there is `40` = 64, never decremented since
the packet never left our machine) ✅.

### 7.5 What `-v` changes — summary
1. Header gets `, id 0x…. = …` ✅.
2. Error lines get the 4-line dump (§7.4) ✅.
3. With `--ip-timestamp`, raw timestamps get ` = 0x…` (§7.6) ✅.
4. Nothing else: reply lines and statistics identical ✅. ICMP traffic unrelated to our packets
   is **not** shown even with `-v` (the reception filter drops it first).

### 7.6 IP options (bonus `-R`, `--ip-timestamp`) ✅
Printed after the reply line (before its newline), tab-separated, with reverse DNS:
```
64 bytes from 127.0.0.1: icmp_seq=0 ttl=64 time=0.021 ms
RR: <TAB>localhost (127.0.0.1)
<TAB>localhost (127.0.0.1)
<TAB>localhost (127.0.0.1)
<TAB>localhost (127.0.0.1)
```
```
64 bytes from 127.0.0.1: icmp_seq=0 ttl=64 time=0.112 ms
TS:<TAB>localhost (127.0.0.1)<TAB>61404360 ms = 0x03a8f4c8        (tsaddr, -v adds "= 0x…")
<TAB>localhost (127.0.0.1)<TAB>61404360 ms = 0x03a8f4c8
...
                                                                  (an empty line follows the TS block)
```
Timestamps = milliseconds since midnight UTC.

### 7.7 Flood `-f` ✅
No reply lines: `.` per send (except the very first), `\b` per reply. `ping -c 3 -f 127.0.0.1`
writes `\b.\b.\b` then the statistics on the same line (`\b--- 127.0.0.1 ping statistics ---`).

### 7.8 Quiet `-q` ✅
Only the header line and the statistics.

### 7.9 Statistics ✅
```
--- 8.8.8.8 ping statistics ---
4 packets transmitted, 4 packets received, 0% packet loss
round-trip min/avg/max/stddev = 4.906/5.922/7.554/1.096 ms
```
```
--- 8.8.8.8 ping statistics ---
2 packets transmitted, 0 packets received, 100% packet loss
```
- **No blank line before `---`** ✅. In an interactive terminal, Ctrl+C shows as
  `^C--- … ping statistics ---` (the terminal echoes `^C`, ping prints no newline).
- Line 2: `<xmit> packets transmitted, <recv> packets received, ` + `+<dup> duplicates, ` if any
  + `<loss>% packet loss`.
  - `loss = (xmit − recv) × 100 / xmit`, **integer division → truncated**: 1/3 lost → `33%` ✅,
    3/4 → `75%` ✅.
  - `recv > xmit` → `-- somebody is printing forged packets!` instead of the loss.
  - `xmit == 0` → loss part omitted.
- Line 3 only if at least one reply **and** timing enabled (data ≥ 16) ✅.
  - n = recv + duplicates; avg = Σt / n; **population** stddev = √(Σt²/n − avg²) ✅
    (verified: 6.291, 7.554, 4.936, 4.906 → 1.096).
  - ⚠️ **stddev quirk** ✅: the square root is a home-made Newton iteration with precision
    0.0005 that returns **0 when the variance is < 0.0005 ms²** (stddev < ≈ 0.022 ms). So on
    loopback stddev is almost always `0.000`, and when not, it can be off by a few thousandths
    (`0.017`/`0.064` → printed `0.024`, exact value 0.0235). Exact matching of this column
    requires reproducing the same iteration (start at x = a/2, x ← (x + a/x)/2 until the change
    ≤ 0.0005; 0 if a < 0.0005).
  - All values `%.3f`.

---

## 8. Particularities and quirks (summary)

1. BSD-style output (`: 56 data bytes`, `round-trip … stddev`), seq starts at **0**.
2. Canonical name (after CNAME) shown in header and statistics.
3. **A failed `sendto` is fatal**, no statistics printed.
4. Relevant ICMP errors are always printed; `-v` only adds the id and the dumps.
5. Errors matched through the quoted original header (destination + id): another process's
   errors are ignored.
6. Errors count as answers for `-c` but not as received; exit code 1 if only errors.
7. No "Request timeout" lines; losses visible only in statistics.
8. Bad-checksum replies are reported **and** counted.
9. Duplicates excluded from "received" but included in RTT statistics.
10. Loss % truncated; stddev 0 below ≈ 0.022 ms and slightly approximate otherwise.
11. `-w N` sends N+1 packets and may stop before the last reply (artificial loss).
12. `-c -1` → wraps → infinite; numbers accept hex/octal (`0x10`, `010`).
13. Reverse DNS on error senders and IP options (not on replies).
14. "root only" options (`-f`, `-l`) not enforced on a setuid install; `-i < 0.2` refused for users.
15. Several hosts pinged sequentially.

---

## 9. Comparison with other ping implementations

| | **inetutils 2.0** (reference) ✅ | **iputils** (modern Linux, 20240905) ✅ | **FreeBSD / macOS** | **BusyBox** |
|---|---|---|---|---|
| Header | `PING h (ip): 56 data bytes` | `PING h (ip) 56(84) bytes of data.` | `PING h (ip): 56 data bytes` | `PING h (ip): 56 data bytes` |
| First seq | 0 | **1** | 0 | 0 |
| Reply line | `64 bytes from ip: icmp_seq=0 ttl=64 time=0.040 ms` | `64 bytes from ip: icmp_seq=1 ttl=64 time=0.040 ms` (`name (ip)` when target given by name) | like inetutils | `64 bytes from ip: seq=0 ttl=64 time=0.040 ms` |
| Reverse DNS | error senders only | replies by default | no | no |
| TTL error line | `92 bytes from name (ip): Time to live exceeded` | `From ip icmp_seq=1 Time to live exceeded` | `92 bytes from ip: Time to live exceeded` | similar to BSD |
| Lost packet during run | silent | silent (`-O`: `no answer yet`) | macOS: `Request timeout for icmp_seq N` | silent |
| Stats line 1 | `N packets transmitted, M packets received, X% packet loss` | `N packets transmitted, M received, +E errors, X% packet loss, time Tms` | macOS: `…, 0.0% packet loss` (1 decimal) | `N packets transmitted, M packets received, X% packet loss` |
| Stats line 2 | `round-trip min/avg/max/stddev = …` | `rtt min/avg/max/mdev = …` | `round-trip min/avg/max/stddev = …` | `round-trip min/avg/max = …` |
| Blank line before stats | no | yes | yes | yes |
| TTL option | `--ttl N` (`-t` = type) | `-t N` | `-m N` | `-t N` |
| Deadline / wait | `-w` total, `-W` linger | `-w` deadline, `-W` per-reply timeout | `-t` timeout, `-W` wait | `-w`, `-W` |
| Unprivileged use | setuid, or ping-socket fallback | ping socket or `cap_net_raw` | setuid / ping socket | setuid or ping socket |
| IPv6 | separate `ping6` | same binary | `ping6` / same binary | `ping6` / `-6` |
| Exit codes | 0 reply, 1 none / error, 64 usage | 0 reply, 1 none, 2 error (incl. usage, unknown host) | 0 reply, 2 none, sysexits | 0 reply, 1 none |
| Unknown host | `ping: unknown host` | `ping: <host>: Name or service not known` | `ping: cannot resolve <host>: Unknown host` | `ping: bad address '<host>'` |
| No host | `ping: missing host operand` + Try line | `ping: usage error: Destination address required` | usage text | usage text |

✅ columns verified (inetutils on the VM, iputils on the dev machine); FreeBSD/macOS and BusyBox
from general knowledge.

---

## 10. Checklist for ft_ping (mandatory part)

- [ ] Options anywhere, grouped short options, `--`; `-v`, `-?`/`--help`; argp-style messages; exit 64 on usage errors.
- [ ] Help text in the argp layout, listing only implemented options.
- [ ] `getaddrinfo(AF_INET, AI_CANONNAME)`; canonical name in header/stats; `ft_ping: unknown host`, exit 1.
- [ ] Raw socket; clear message and exit 1 if not permitted.
- [ ] Header line (+ `, id 0x%04x = %u` with `-v`).
- [ ] Packet: id = pid & 0xFFFF, seq from 0, timeval + filler `00 01 … 27`, checksum.
- [ ] `select()` loop, one send per second, SIGINT flag, `EINTR`.
- [ ] `sendto` failure → `ft_ping: sending packet: <strerror>`, exit 1.
- [ ] Length checks, id filter, checksum check (report but count), duplicate bitmap + `(DUP!)`.
- [ ] ICMP errors matched via quoted header; message table; sender printed as IP only.
- [ ] `-v` 4-line dump with exact spacing, tabs and trailing spaces.
- [ ] Statistics exactly as §7.9 (truncated loss, duplicates, same stddev algorithm).
- [ ] Exit 0 if any reply, else 1.
- [ ] `--ttl` early, to test Time Exceeded / `-v`.
