# ping from GNU inetutils-2.0

Reference notes for the ft_ping project. This document describes the **behaviour and design** of
the ping shipped with GNU inetutils 2.0 (the reference named by the subject), based on reading
`official_ping_inetutils/` and `man_ping_inetutils_v2`.

> The subject forbids using the sources of a standard ping. These notes describe observable
> behaviour (formats, messages, timing, exit codes) — nothing here should be copied as code.

---

## 1. Overview

- Descends from the 4.3BSD ping (1980s). Its output format is therefore close to the BSD/macOS
  ping, and quite different from the Linux `iputils` ping most distributions ship today.
- Identify it with `ping -V` → `ping (GNU inetutils) 2.0`.
  (This machine has `iputils 20240905` instead; on Debian the reference is package `inetutils-ping`.)
- IPv4 only in `ping`; IPv6 is a separate program (`ping6`).

| Source file | Role |
|---|---|
| `ping.c` | Option parsing (argp), `main`, the main loop (`ping_run`), sending (`send_echo`), short statistics (`ping_finish`) |
| `ping_echo.c` | ECHO mode: header line, reply lines, ICMP error printing, `-v` dumps, round-trip statistics |
| `libping.c` | Socket creation, destination resolution, packet transmission, reception and filtering |
| `ping_common.c/.h` | Helpers: number parsing, data buffer filling, pattern decoding, time subtraction, square root, reverse DNS helpers, duplicate bitmap |
| `ping_timestamp.c`, `ping_address.c`, `ping_router.c` | Other request types (`--timestamp`, `--address`/`--mask`, router discovery) — not needed |
| `ping6.c` | IPv6 version — not needed |

---

## 2. Command line

Parsed with **argp** (GNU's argument parser): short and long options, options may appear before
or after the host, `--` ends options.

### Options

| Option | Meaning | ft_ping |
|---|---|---|
| `-v`, `--verbose` | Verbose output (see §7.4) | **mandatory** |
| `-?`, `--help` | Print help list | **mandatory** |
| `--usage` | Short usage message | not a bonus |
| `-V`, `--version` | Version | not a bonus |
| `-c`, `--count N` | Stop after N packets | bonus |
| `-i`, `--interval N` | Seconds between packets (decimal allowed; < 0.2 s refused for non-root) | bonus |
| `-w`, `--timeout N` | Stop after N seconds | bonus |
| `-W`, `--linger N` | Seconds to wait for replies after the last packet (default 10) | bonus |
| `-s`, `--size N` | Data bytes (default 56) | bonus |
| `-p`, `--pattern HEX` | Up to 16 filler bytes, in hex | bonus |
| `-l`, `--preload N` | Send N packets at once before normal mode | bonus |
| `-f`, `--flood` | Flood: as fast as replies / 100 per second, prints `.` and `\b` | bonus |
| `-n`, `--numeric` | No reverse DNS | bonus |
| `-q`, `--quiet` | Only header and summary | bonus |
| `-r`, `--ignore-routing` | `SO_DONTROUTE` | bonus |
| `-d`, `--debug` | `SO_DEBUG` | bonus |
| `-T`, `--tos N` | IP Type Of Service (0–255) | bonus |
| `--ttl N` | IP TTL (1–255) | bonus (and useful to test `-v`) |
| `-R`, `--route` | IP Record Route option | bonus |
| `--ip-timestamp tsonly\|tsaddr` | IP timestamp option | bonus |
| `-t`, `--type TYPE`, `--echo`, `--timestamp`, `--address`, `--mask` | Request type | — |

Note: in inetutils **`-t` is the packet type**, not the TTL (iputils uses `-t` for TTL).

### Messages and exit codes

| Situation | Output (stderr) | Exit |
|---|---|---|
| No host | `ping: missing host operand`<br>`Try 'ping --help' or 'ping --usage' for more information.` | 64 |
| Unknown option | `ping: invalid option -- 'x'` + the same "Try" line | 64 |
| Bad numeric value | `ping: invalid value (`'`x' near `y')`, `option value too small: x`, `option value too big: x` | 1 |
| Host not resolvable | `ping: unknown host` | 1 |
| No privilege | `ping: Lacking privilege for icmp socket.` | 1 |
| `sendto` fails | `ping: sending packet: <strerror>` (program stops) | 1 |
| Finished, ≥ 1 reply | statistics | **0** |
| Finished, no reply | statistics | **1** |

Numbers are parsed with `strtoul(..., 0)`, so `0x10` and `010` (octal) are accepted.

---

## 3. Startup

1. Parse options.
2. Open the socket: **raw ICMP socket**. If refused (not root), fall back to an unprivileged
   `SOCK_DGRAM` ICMP socket ("ping socket"); in that mode the kernel chooses the identifier, so
   id checks are disabled.
3. Enable `SO_BROADCAST` (allows pinging a broadcast address).
4. Drop root privileges (`setuid(getuid())`) — relevant when installed setuid-root.
5. Force **line buffering** on stdout (each line appears immediately even through a pipe).
6. Apply `--ttl` / `--tos` with `setsockopt` (a failure here only prints a warning).
7. Prepare the filler buffer.
8. For **each host** given on the command line, in order: resolve it, ping it, print statistics.

### Resolution

- `getaddrinfo` with `AF_INET` and the **`AI_CANONNAME`** flag (plus IDN flags if available).
- The name displayed in the header and the statistics is the **canonical name**: the string
  itself for an IP address, the final name after CNAME aliases for a DNS name.
- The first address returned is used.
- Reverse DNS is never done for the target, only (optionally) for senders of ICMP errors (§7.3).

---

## 4. Packet sent

```
 ICMP header (8 bytes)                       data (56 bytes by default)
+------+------+----------+--------+--------+----------------------+---------------------+
| type | code | checksum |   id   |  seq   | struct timeval (16)  | filler 00 01 .. 27  |
|  8   |  0   |          |        |        | send time            | (40 bytes)          |
+------+------+----------+--------+--------+----------------------+---------------------+
```

- **id** = `getpid() & 0xFFFF`, in network byte order.
- **seq** = number of packets successfully sent so far: **starts at 0**, network byte order.
- **Timestamp**: `gettimeofday` at send time, written raw (host byte order — only we read it back).
  Present only if data size ≥ `sizeof(struct timeval)` (16 on 64-bit). Otherwise there is no
  `time=` and no round-trip statistics. (The man page says "eight bytes" — that was true for
  32-bit systems.)
- **Filler**: a buffer `00 01 02 …` is prepared once; after the timestamp the bytes copied are
  `00 01 02 … 27` (counting from 0 again, not from the offset). With `-p`, the pattern repeats.
- Total ICMP size: 8 + 56 = 64; IP packet: 84.

---

## 5. Main loop

Single-threaded, driven by **`select()`**:

```
send first packet immediately, remember "last send time"
while not stopped:
    timeout = (last send + interval) - now        (clamped at 0)
    select(socket readable?, timeout)
      error EINTR   -> loop again (Ctrl+C lands here)
      other error   -> fatal "select failed"
      readable      -> receive + process one packet
                       (stop if -w elapsed, or -c replies reached)
      timeout       -> if more packets to send: send next one, last = now
                       else if already lingering: stop
                       else start lingering: interval = linger (-W, default 10 s)
print statistics
```

- **SIGINT** handler only sets a `volatile` flag; the loop notices it after `select` returns
  `EINTR`. Statistics are printed by the normal code path, not from the signal handler.
- Receiving a packet does **not** shift the send schedule: one packet per interval, regardless of
  replies.
- Without `-c`, it never stops by itself; lost packets are simply never answered (no
  "timeout" message).
- With `-c N`: after the Nth send it waits up to `linger` seconds for remaining replies.

---

## 6. Reception and filtering

Each readable event reads **one** datagram (IP header + ICMP) with `recvfrom`, then:

| Packet | Treatment |
|---|---|
| Too short to hold IP + ICMP headers | stderr `packet too short (N bytes) from <ip>`, ignored |
| Echo reply, id ≠ ours | ignored silently |
| Echo reply, bad ICMP checksum | stderr `checksum mismatch from <ip>`, **still counted and printed** |
| Echo reply, seq already received | counted as **duplicate**, printed with `(DUP!)` |
| Echo reply, new seq | counted as received, printed |
| Echo request (our own on loopback), timestamp/address requests | ignored |
| Any other type (errors…) | kept **only if** the original datagram quoted inside it was ours: original destination = our target, protocol ICMP, type ECHO, id = ours. Otherwise ignored |

### Duplicate detection

A bitmap of 128 bytes = 1024 bits, indexed by `seq % 1024`:
- bit **cleared** when that seq is sent,
- on reply: bit already set → duplicate; else set it.

Duplicates are **not** counted as received, but their RTT **is** included in min/avg/max/stddev.

---

## 7. Output formats

`<…>` are values; everything else, including spaces, is literal.

### 7.1 Header

```
PING <canonname> (<ip>): <datalen> data bytes
PING <canonname> (<ip>): <datalen> data bytes, id 0x<id 4 hex> = <id decimal>     (with -v)
```

### 7.2 Echo reply

```
<n> bytes from <ip>: icmp_seq=<seq> ttl=<ttl> time=<rtt %.3f> ms
<n> bytes from <ip>: icmp_seq=<seq> ttl=<ttl> time=<rtt %.3f> ms (DUP!)
```
- `<n>` = received length − IP header length (64 by default).
- `<ip>` printed numerically (no reverse DNS on replies).
- `ttl` from the reply's IP header.
- ` time=…` omitted when data < 16 bytes.
- IP options in the reply (record route, timestamps) are printed after, on extra lines.
- With `-f`: prints `\b` instead of the line; with `-q`: nothing.

### 7.3 ICMP errors (with or without `-v`)

```
<n> bytes from <sender>: <message>
```
- `<n>` = received length − outer IP header length.
- `<sender>` = `hostname (ip)` via reverse DNS, or just `ip` with `-n` or when lookup fails.
  **The subject excludes this reverse DNS → print the IP only.**

Messages by type/code:

| Type | Code | Message |
|---|---|---|
| 3 Dest Unreachable | 0 | `Destination Net Unreachable` |
| | 1 | `Destination Host Unreachable` |
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
| 5 Redirect | 0–3 | `Redirect Network` / `Redirect Host` / `Redirect Type of Service and Network` / `Redirect Type of Service and Host` |
| | other | `Redirect, Unknown Code: <code>` |
| 11 Time Exceeded | 0 | `Time to live exceeded` |
| | 1 | `Frag reassembly time exceeded` |
| | other | `Time exceeded, Unknown Code: <code>` |
| 4 | | `Source Quench` + header table (always) |
| 12 | | `Parameter problem: IP address = <addr>` + header table (always) |
| 0, 8, 9, 10, 13–18 | | their name (`Echo Reply`, `Echo Request`, `Router Advertisement`, …) |
| unknown | | `Bad ICMP type: <type>` |

### 7.4 What `-v` changes

1. Header line gets `, id 0x…. = …`.
2. For **Dest Unreachable / Redirect / Time Exceeded**, after the message line, a dump of the
   **original** packet quoted in the error:

```
IP Hdr Dump:
 4500 0054 1a2b 4000 0101 c3d4 c0a8 0102 0808 0808 
Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src	Dst	Data
 4  5  00 0054 1a2b   2 0000  01  01 c3d4 192.168.1.2  8.8.8.8 
ICMP: type 8, code 0, size 64, id 0x1234, seq 0x0000
```
   - Dump: the 20 base header bytes, two by two, **each group followed by a space** (trailing
     space), line starts with one space.
   - Title line: fixed spaces, then **tabs** before `Dst` and `Data`.
   - Value line: ` %1x  %1x  %02x %04x %04x   %1x %04x  %02x  %02x %04x %s  %s ` then any IP
     option bytes in hex. Flg = top 3 bits of the fragment field, off = low 13 bits. Total length
     uses a heuristic to guess the byte order.
   - Last line: `ICMP: type T, code C, size S` (+ `, id 0x%04x, seq 0x%04x` for echo types).
     For a quoted TCP/UDP packet: `TCP: from port A, to port B (decimal)` / `UDP: …`.
3. In theory `-v` also shows ICMP packets not related to us, but the reception filter (§6)
   already drops them before printing — so in practice `-v` does **not** show foreign packets.
4. Normal echo reply lines are unchanged.

### 7.5 Statistics

Printed at the end (Ctrl+C, `-c`, `-w`):

```
--- <canonname> ping statistics ---
<xmit> packets transmitted, <recv> packets received, [+<dup> duplicates, ]<loss>% packet loss
round-trip min/avg/max/stddev = <min>/<avg>/<max>/<stddev> ms
```
- No blank line before `---`. In a terminal you see `^C--- … ping statistics ---` because the
  terminal echoes `^C` on the same line.
- `loss` = `(xmit − recv) × 100 / xmit`, **integer division (truncated)**: 1 lost of 3 → `33%`.
- If `recv > xmit`: `-- somebody is printing forged packets!` instead of the loss.
- If `xmit == 0`, the loss part is omitted.
- The `round-trip` line appears only if `recv > 0` and timing is enabled.
- `n = recv + dup`; `avg = sum / n`; `stddev = sqrt(sumsq / n − avg²)`. The square root is a
  home-made Newton iteration with precision 0.0005 (no libm), returning 0 for values < 0.0005.
- All values `%.3f`.

---

## 8. Particularities and quirks

- BSD-style output (`: 56 data bytes`, `round-trip`, `stddev`), seq starting at **0**.
- **A failed `sendto` is fatal** (e.g. network down) — unlike iputils which keeps going.
- ICMP errors for our packets are always shown; `-v` only adds detail.
- Errors are matched by the **quoted original header** (destination + id), so errors caused by
  another ping process are ignored.
- Packets with a bad checksum are reported **and** counted.
- Duplicates inflate the RTT statistics but not the received count.
- Displayed host name = canonical name from DNS, not necessarily what was typed.
- Several hosts can be given; they are pinged one after the other.
- No message for lost packets during the run; losses show only in the final summary.
- The man page's "first eight bytes for the timestamp" is outdated: it is `sizeof(struct timeval)`.
- Packet loss percentage is truncated, not rounded.

---

## 9. Comparison with other ping implementations

| | **inetutils 2.0** (reference) | **iputils** (modern Linux, e.g. 20240905) | **FreeBSD / macOS** | **BusyBox** |
|---|---|---|---|---|
| Header | `PING h (ip): 56 data bytes` | `PING h (ip) 56(84) bytes of data.` | `PING h (ip): 56 data bytes` | `PING h (ip): 56 data bytes` |
| First seq | 0 | **1** | 0 | 0 |
| Reply line | `64 bytes from ip: icmp_seq=0 ttl=64 time=0.040 ms` | `64 bytes from name (ip): icmp_seq=1 ttl=64 time=0.040 ms` (name when target given by name) | like inetutils | `64 bytes from ip: seq=0 ttl=64 time=0.040 ms` |
| Reverse DNS on replies | no | yes by default (`-n` disables) | no | no |
| ICMP error line | `92 bytes from ip: Time to live exceeded` | `From ip icmp_seq=1 Time to live exceeded` | `92 bytes from ip: Time to live exceeded` | similar to BSD |
| Lost packet during run | silent | silent (`-O` prints `no answer yet`) | macOS: `Request timeout for icmp_seq N` | silent |
| Stats line 1 | `N packets transmitted, M packets received, X% packet loss` | `N packets transmitted, M received, [+E errors, ]X% packet loss, time Tms` | macOS: `…, 0.0% packet loss` (one decimal) | `N packets transmitted, M packets received, X% packet loss` |
| Stats line 2 | `round-trip min/avg/max/stddev = …` | `rtt min/avg/max/mdev = …` | `round-trip min/avg/max/stddev = …` | `round-trip min/avg/max = …` |
| Blank line before stats | no | yes | yes | yes |
| TTL option | `--ttl N` (`-t` = type) | `-t N` | `-m N` (FreeBSD) / `-m` (macOS) | `-t N` |
| Deadline / wait | `-w` total, `-W` linger | `-w` deadline, `-W` per-reply timeout | `-t` timeout, `-W` wait | `-w`, `-W` |
| Unprivileged use | fallback to ping socket | ping socket (`net.ipv4.ping_group_range`) or file capability `cap_net_raw` | setuid / ping socket | setuid or ping socket |
| IPv6 | separate `ping6` | same binary (`-6`, auto) | `ping6` / same binary (newer FreeBSD) | `ping6` / `-6` |
| Exit codes | 0 replies, 1 none or error, 64 usage | 0 replies, 1 none, 2 other errors (incl. usage, unknown host) | 0 replies, 2 none, sysexits for errors | 0 replies, 1 none |
| Unknown host message | `ping: unknown host` | `ping: <host>: Name or service not known` | `ping: cannot resolve <host>: Unknown host` | `ping: bad address '<host>'` |

iputils lines above were verified on this machine (`ping 127.0.0.1`, `ping -t1 8.8.8.8`).
FreeBSD/macOS and BusyBox columns are from general knowledge — check before relying on them.

---

## 10. Checklist for ft_ping

- [ ] Options anywhere on the command line; `-v`, `-?` (and `--help`); argp-like messages; exit 64 on usage errors.
- [ ] `getaddrinfo(AF_INET, AI_CANONNAME)`; show canonical name; `ft_ping: unknown host`, exit 1.
- [ ] Raw socket; clear message if not root.
- [ ] Header line (+ `, id 0x%04x = %u` with `-v`).
- [ ] Packet: id = pid & 0xFFFF, seq from 0, timeval + filler `00 01 … 27`, checksum.
- [ ] `select()` loop, one send per second, SIGINT flag, `EINTR` handled.
- [ ] `sendto` failure → message + stop.
- [ ] Length checks, id filter, checksum check (report but count), duplicate bitmap + `(DUP!)`.
- [ ] ICMP errors matched via quoted header; message table; numeric sender (no reverse DNS).
- [ ] `-v` dump of the quoted IP/ICMP header with exact spacing and tabs.
- [ ] Statistics exactly as §7.5 (truncated loss, duplicates, stddev without libm).
- [ ] Exit 0 if any reply, 1 otherwise.
- [ ] `--ttl` early, to test Time Exceeded handling.
