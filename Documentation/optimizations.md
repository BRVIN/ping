# Optimizations and low-level tricks

This file lists the optimizations and compact techniques **used in ft_ping** (`srcs/`). Most of them
also appear in the reference ping (GNU inetutils 2.0, `official_ping_inetutils/`). Each one gets the same
sections:

- **In simple words**: a plain-language explanation first;
- **Where**: the file or function that uses it;
- **Idea**: how it works, with a small example;
- **Why**: what it saves;
- **Limit**: its cost or trade-off;
- **Plain version**: the simple alternative;
- **ft_ping**: how we use it.

> The inetutils code is described here, not copied (see the subject's rule about standard ping sources).

---

## 1. Ring indexing with modulo

**In simple words.** Think of a clock face with 1024 positions. Packet numbers keep growing, but every number is placed on the clock: after position 1023 you come back to 0. So packet 13 and packet 1037 land on the same position, at different times. Before a position is used again, we erase it, so old information never confuses the new packet.

**Where**: inetutils duplicate table (`seq % 1024`); ft_ping `seq % DUP_TABLE_SIZE`.

**Idea.** The table is smaller than the range of possible seqs (65536), so slots are **reused in a
circle**: seq 13, 1037, 2061… all use slot 13. To make reuse safe, a slot is **reset when its seq is
sent** (`dup_clear`). The old seq that used it is long finished by then.

Because 65536 is a multiple of 1024, the circle stays consistent when the 16-bit seq wraps from
65535 back to 0.

**Why.** Fixed, small memory, whatever the number of packets sent.

**Limit.** A reply arriving more than 1024 packets late (about 17 minutes at 1/s) would be judged
wrongly. In practice that never happens.

**Plain version.** `DUP_TABLE_SIZE 65536`: one slot per possible seq (64 KB) and no sharing.

**ft_ping.** Used (1024 slots).

---

## 2. Carrying the send time inside the packet

**In simple words.** Instead of writing down "I sent packet 5 at 10:00:00.123" in a notebook, we **write the time on the packet itself**. The other machine sends the packet back untouched, so when it returns we just read the time written on it and compare it with the current time. No notebook to keep.

**Where**: both. inetutils `send_echo` and `print_echo`; ft_ping `build_packet` and `recv_ping`.

**Idea.** The send time (`struct timeval`) is written at the start of the ICMP data. The target must
return the data unchanged, so the reply brings our timestamp back, and RTT = now − that timestamp.

**Why.** **No table of send times** to keep, look up or clean, and no problem matching a late reply
with its request. The packet carries its own state.

**Limit.** Only works if the data part has room for the timestamp: at least 16 bytes on 64-bit.
With `-s` below 16, inetutils simply stops printing `time=`. The timestamp is in host byte order,
which is fine because only our own machine reads it.

**Plain version.** An array `sent_time[seq]` filled when sending and read on reply.

**ft_ping.** Used.

---

## 3. Internet checksum: 32-bit accumulator, carries folded at the end

**In simple words.** The checksum is a quick total of all the packet's numbers, used to detect damage. The rule says that whatever "overflows" past 16 bits must be added back at the bottom. Instead of handling that overflow after every single addition, we **add everything in a bigger box** (32 bits) where overflow has room to pile up, and handle it all **once at the end**. Same result, less work.

**Where**: both. ft_ping `checksum()` in `srcs/packet.c`, following RFC 1071.

**Idea.** The checksum is a one's-complement sum of 16-bit words. In true one's-complement addition,
every carry out of bit 15 must be added back in at bit 0. Instead of doing that after each addition:

1. add all the 16-bit words into a **32-bit** variable, where the carries pile up in the upper 16 bits;
2. at the end, **fold** them back in: `sum = (sum & 0xFFFF) + (sum >> 16)`, repeated until nothing
   is left above bit 15;
3. invert with `~`.

Example: `0xFFFF + 0x0002 = 0x10001`, fold → `0x0001 + 0x0001 = 0x0002`. That matches one's-complement
addition.

**Why.** One simple addition per word, with no carry test inside the loop. RFC 1071 describes this
technique.

**Bonus property: byte-order independence.** One's-complement addition gives byte-swapped results on
byte-swapped inputs. Summing the raw memory and storing the result as-is gives the correct bytes on
**any** CPU, which is why the checksum **needs no `htons`**.

**Limit.** A 32-bit accumulator only overflows for packets above ~128 KB, far over the IP limit.

**Plain version.** Add the carry back after each addition (slower and longer, same result).

**ft_ping.** Used.

---

## 4. Running sums for the statistics

**In simple words.** To compute the average of your grades, you don't need to remember every grade: keeping the **total** and the **count** is enough. Same here: instead of storing every round-trip time, we only keep a few running numbers (smallest, largest, total, total of squares) and update them at each reply. At the end, they are enough to compute min, average, max and deviation.

**Where**: both. inetutils `struct ping_stat` (`tmin`, `tmax`, `tsum`, `tsumsq`); ft_ping `rtt_min`,
`rtt_max`, `rtt_sum`, `rtt_sumsq` in `t_ping`.

**Idea.** The program never stores the list of RTTs. It only updates 4 numbers per reply:
- min and max: compare and replace;
- sum: `sum += rtt`;
- sum of squares: `sumsq += rtt * rtt`.

At the end, with n replies:
- `avg = sum / n`
- `variance = sumsq / n − avg²` (mean of the squares minus the square of the mean)
- `stddev = √variance`

**Why.** **Constant memory** (4 doubles) even after millions of packets, with no allocation and no second pass.

**Limit.** The formula `E[x²] − E[x]²` subtracts two nearly equal big numbers when the values are close
together, so it loses precision. It can even give a tiny negative variance, which is why the square
root returns 0 for negatives. For RTTs in milliseconds this is harmless.

**Plain version.** Store every RTT in a growing array, then compute mean and deviation in two passes.

**ft_ping.** Used (required to match inetutils' numbers).

---

## 5. Home-made square root (Newton's method)

**In simple words.** To find √9, guess a number, then repeatedly replace the guess by the average of "guess" and "9 ÷ guess". Each step gets much closer to 3. We stop when the guess barely changes. Writing this small loop ourselves avoids pulling in the math library, and reproduces exactly the slightly-rounded results of the reference ping.

**Where**: both. inetutils `nsqrt`; ft_ping `ft_sqrt` in `srcs/stats.c`.

**Idea.** Newton's iteration `x ← (x + a / x) / 2` converges very fast to √a. Start at `x = a / 2` and
stop when a step changes x by no more than 0.0005. Below 0.0005, return 0.

Example, √4: 2 → (2 + 2) / 2 = 2, stop. √9: 4.5 → 3.25 → 3.0096 → 3.00002 → stop.

**Why.** Avoids linking the math library (`-lm`) for one call.

**Limit.** The precision cutoff is **visible**. inetutils prints `stddev 0.000` whenever the variance is
below 0.0005, and values off by a few thousandths otherwise (e.g. `0.024` instead of `0.0235`). ft_ping
uses the same rule **on purpose**, to print the same numbers as the reference.

**Plain version.** `sqrt()` from `<math.h>` with `-lm`. It is exact, but the output then differs from inetutils.

**ft_ping.** Used.

---

## 6. No memory allocation in the loop

**In simple words.** Asking the system for memory (`malloc`) takes time and can fail. ft_ping never
asks: every box it needs (packet to send, buffer to receive, duplicate table) is reserved in advance,
either inside the function that uses it or inside the `t_ping` struct, and reused for the whole run.

**Where**: `send_ping` (`char packet[PACKET_LEN]`), `recv_ping` (`char buf[1024]`), `dup_table[]`
inside `t_ping`.

**Idea.** Local arrays live on the **stack**: they are created automatically when the function starts
and disappear when it returns, at no cost. Fixed-size tables are simply fields of `t_ping`. The send /
receive loop therefore never calls `malloc` or `free`.

**Why.** Nothing to allocate means nothing that can fail, nothing to free, and no possible leak or
double free (the subject insists the program must never crash).

**Limit.** Sizes must be known in advance. If the `-s` bonus (variable packet size up to ~65 KB) is
added, the buffers must grow to the maximum size, or be allocated once at startup (that is what
inetutils does with `_ping_setbuf`).

**Plain version.** `malloc` a buffer at each send and receive, then `free` it.

**ft_ping.** Used: no `malloc` at all.
---

## 7. Reading headers in place (struct overlay, "zero-copy parsing")

**In simple words.** A received packet is just a row of bytes. Instead of copying those bytes into separate variables one by one, we lay a **transparent template** (a C struct) over them: the template says "byte 8 is the TTL, bytes 12–15 are the source address…", so `ip->ttl` reads the right byte directly in the buffer.

**Where**: both. inetutils casts to `struct ip *` / `icmphdr_t *`; ft_ping `ip = (struct iphdr *)buf;
icmp = (struct icmphdr *)(buf + ip_hlen);`.

**Idea.** The received bytes are not copied or decoded field by field. A pointer of the header's struct
type is pointed **at the right offset of the buffer**, and the compiler then reads `ip->ttl` or
`icmp->type` straight from the raw bytes.

**Why.** No copy and no manual offset arithmetic; field names make the code readable.

**Limit.**
- The struct must match the wire layout exactly. System headers guarantee this.
- Multi-byte fields still need `ntohs` / `ntohl`.
- **Alignment.** Some fields can sit at addresses the CPU doesn't like. That's why the timestamp is
  read with `memcpy` into a local variable instead of through a cast (`ip_hlen + 8` isn't always a
  multiple of 8). Both inetutils and ft_ping do this.

**Plain version.** Read each field byte by byte, e.g. `ttl = buf[8]; id = (buf[4] << 8) | buf[5];`.

**ft_ping.** Used.

---

## 8. Single-thread event loop with a computed timeout

**In simple words.** The program needs to do two things: send every second, and react when a reply arrives. Rather than constantly asking "anything yet? anything yet?" (which wastes the CPU), it tells the system: **"wake me up when a packet arrives, or in X milliseconds, whichever comes first"**, and sleeps until then.

**Where**: both. inetutils `ping_run`; ft_ping `ping_loop` in `srcs/loop.c`.

**Idea.** One `select()` call waits for whichever comes first: a packet, or the next send time. The
timeout is exactly the time left until the next send. The process sleeps in the kernel the rest of the time.

**Why.**
- **No busy-waiting**: zero CPU while waiting.
- **No threads**, so no locks around the counters.
- Sending stays on schedule however replies arrive.

**Limit.** The timeout has to be recomputed on every turn. A Ctrl+C landing just before `select` can
delay the exit by one interval (`pselect` would close that gap).

**Plain version.** A loop that keeps calling non-blocking `recvfrom` and checking the clock (burns
100% CPU), or two threads.

**ft_ping.** Used.

---

## 9. Time as one integer (microseconds)

**In simple words.** A time like "12 seconds and 300 000 microseconds" is awkward to subtract, like subtracting "2 h 10 min − 1 h 50 min" (you have to borrow). Converting everything into **one single number of microseconds** turns every time calculation into a simple subtraction or comparison.

**Where**: ft_ping only (`now_us()`, `long long` everywhere). inetutils keeps `struct timeval` and
subtracts with `tvsub`.

**Idea.** `tv_sec * 1000000 + tv_usec` gives one 64-bit integer. Differences and comparisons become
plain `-` and `>=`, and the value is converted back to a `timeval` only for `select`.

**Why.** Removes the "borrow one second when microseconds go negative" logic and its bugs.

**Limit.** None in practice: a `long long` holds microseconds for about 290 000 years.

**ft_ping.** Used.

---

## 10. Line buffering of stdout

**In simple words.** When output goes to a file or a pipe, C normally collects text and writes it in big chunks, so lines can appear late or in a strange order. Line buffering says: **write out each line as soon as it is finished**.

**Where**: both. `setvbuf(stdout, NULL, _IOLBF, 0)`.

**Idea.** When stdout is a pipe or a file, the C library normally holds output in a 4 KB buffer and
writes it all at once. Forcing **line buffering** flushes at every `\n`.

**Why.** Each reply line shows up immediately (`ft_ping host | tee log`), and stdout/stderr messages
stay in order. This is about correctness rather than speed: it actually trades a little speed for
immediacy.

**Limit.** Must be called **before the first output** (we had a bug where it came after the header line).

**ft_ping.** Used, first line of `main`.

---

## 11. Integer arithmetic for the loss percentage

**In simple words.** The loss percentage is computed with whole numbers only, multiplying before dividing, so decimals are simply cut off (66.7% becomes 66%). It is simple, needs no decimal numbers, and gives exactly the same output as the reference ping.

**Where**: both. `(transmitted − received) * 100 / transmitted`.

**Idea.** Multiply first, then divide in integers. The result is truncated: 1 lost out of 3 → `33%`.

**Why.** No floating point, and it is the exact inetutils output (rounding would print 33 as well, but
2/3 would give 67 instead of inetutils' 66).

**Limit.** Multiplying first could overflow only with absurd packet counts.

**ft_ping.** Used, deliberately truncated to match.

---

## 12. Non-blocking read after `select`

**In simple words.** Even after the system says "a packet is waiting", we read it with a "**don't wait**" flag. If, by bad luck, the packet is no longer there, the read gives up immediately instead of freezing the whole program.

**Where**: ft_ping `recvfrom(..., MSG_DONTWAIT, ...)` in `recv_ping`.

**Idea.** `select` says "readable", but in rare cases the packet is gone by the time we read it. With
`MSG_DONTWAIT`, `recvfrom` then returns immediately with `EAGAIN` instead of blocking the whole loop.

**Why.** Guarantees the loop can never get stuck in a read, so sends and Ctrl+C are never delayed.

**Limit.** None. inetutils doesn't do it, but nothing is lost by it.

**ft_ping.** Used.

---

## 13. Table-driven message lookup

**In simple words.** Instead of a long chain of `if`/`switch` saying "if type 3 code 1, print *Destination Host Unreachable*; if type 3 code 2, …", we write all the messages once in a **table** (type, code, text) and a tiny loop looks up the right line. Adding or fixing a message means editing one line of data, not code.

**Where**: both. ft_ping `icmp_error.c`: table `g_codes` and `print_code_message`.

**Idea.** Each row is `{type, code, text}`. The loop walks the rows and prints the text of the first
one matching the received type and code. No match → the fallback
`<prefix>, Unknown Code: <code>` (e.g. `Dest Unreachable, Unknown Code: 9`).

**Why.** Messages are data, separated from logic: easy to read, easy to compare with the inetutils
list, and a missing entry naturally falls into the "unknown" case.

**Limit.** Linear search, but on 20 rows, once per error packet: negligible. A table indexed directly
by code would be faster but needs holes for missing codes (9 and 10 are absent on purpose).

**Plain version.** A `switch` on type, then a `switch` on code with one `printf` per case.

**ft_ping.** Used for Dest Unreachable / Redirect / Time Exceeded codes. The other types (only a
handful) are an `if` chain in `handle_icmp_error`.

---

## Summary

| # | Technique | Where in ft_ping | Also in inetutils |
|---|---|---|---|
| 1 | Ring indexing with `%` (duplicate table) | `ping.c` `dup_clear`, `dup_test_and_set` | ✔ |
| 2 | Timestamp inside the packet | `packet.c` `build_packet`, `ping.c` `recv_ping` | ✔ |
| 3 | Checksum with deferred carry fold | `packet.c` `checksum` | ✔ |
| 4 | Running sums for stats | `t_ping` fields, `ping.c` `update_rtt` | ✔ |
| 5 | Newton square root | `stats.c` `ft_sqrt` | ✔ |
| 6 | No allocation in the loop (no malloc at all) | stack buffers, table in `t_ping` | ✔ (allocated once) |
| 7 | Struct overlay on raw bytes | `ping.c` `recv_ping` | ✔ |
| 8 | `select` event loop | `loop.c` `ping_loop` | ✔ |
| 9 | Time as `long long` µs | `loop.c` `now_us` | ✘ |
| 10 | Line buffering | `main.c` | ✔ |
| 11 | Integer loss % | `stats.c` `print_stats` | ✔ |
| 12 | `MSG_DONTWAIT` after `select` | `ping.c` `recv_ping` | ✘ |
| 13 | Table-driven message lookup | `icmp_error.c` `print_code_message` | ✔ |
