#include "../includes/ft_ping.h"

/*
** Duplicate table: one cell per sequence number (modulo DUP_TABLE_SIZE).
**   0 = no reply received yet for this seq
**   1 = a reply was already received
** The cell is reset to 0 when that seq is sent and set to 1 on its first
** reply, so a second reply with the same seq finds it already at 1.
*/
static void	dup_clear(t_ping *ping, int seq)
{
	ping->dup_table[seq % DUP_TABLE_SIZE] = 0;
}

/* Returns 1 if seq was already received, else marks it and returns 0 */
static int	dup_test_and_set(t_ping *ping, int seq)
{
	if (ping->dup_table[seq % DUP_TABLE_SIZE] == 1)
		return (1);
	ping->dup_table[seq % DUP_TABLE_SIZE] = 1;
	return (0);
}

/*
** Sends the echo request number ping->transmitted (its seq on 16 bits,
** like inetutils which uses ping_num_xmit as the sequence number).
** A sendto failure is fatal, like inetutils ("sending packet: ...").
*/
int	send_ping(t_ping *ping)
{
	char	packet[PACKET_LEN];

	build_packet(ping, packet, ping->transmitted & 0xFFFF);
	dup_clear(ping, ping->transmitted & 0xFFFF);
	/* The kernel builds the IP header around our ICMP packet */
	if (sendto(ping->sockfd, packet, PACKET_LEN, 0,
			(struct sockaddr *)&ping->sockaddr, sizeof(ping->sockaddr)) < 0)
	{
		fprintf(stderr, "ft_ping: sending packet: %s\n", strerror(errno));
		return (-1);
	}
	ping->transmitted++;
	return (0);
}

/* Adds one RTT (in ms) to min / max / sum / sum of squares */
static void	update_rtt(t_ping *ping, double rtt)
{
	if (ping->received + ping->duplicates == 1 || rtt < ping->rtt_min)
		ping->rtt_min = rtt;
	if (rtt > ping->rtt_max)
		ping->rtt_max = rtt;
	ping->rtt_sum += rtt;
	ping->rtt_sumsq += rtt * rtt;
}

/*
** A raw ICMP socket receives EVERY ICMP packet arriving on the machine
** (other ping processes, errors, ...), so we filter to keep only ours.
** Called by the main loop when select() says the socket is readable:
** handles exactly ONE packet.
**
** What recvfrom() gives us on a raw socket:
**
**   +----------------------+------------+----------------+
**   | IP header (20+ bytes)| ICMP hdr 8 | data 56        |
**   +----------------------+------------+----------------+
**
** The IP header length is variable: ihl = number of 32-bit words,
** so its size in bytes is ihl * 4.
*/

//recv_ping
//
//  ip->ihl * 4 : Internet Header Length is the length of the internet header in 32 bit word, so we multiply * 4 to get byte size
//
//  recvfrom([notre fd] [message retour] [<- taille] [Flag] [addresse sender] [<- taille] )
//  recvfrom([notre fd]          [message retour]  [<- taille]        [Flag]               [addresse sender]            [<- taille]          )
//  recvfrom([(int)ping->sockfd] [(char[1024])buf] [(int)sizeof(buf)] [(int)MSG_DONTWAIT]  [(struct sockaddr *)&from]   [(socklen_t)fromlen] )
//  recvfrrom here put packet response into buf, and address of sender into from, we know a message should be available thanks to select().
//
// checksum :
// Checksum over the whole ICMP part (checksum field included): a valid
// packet sums to 0. Like inetutils, a mismatch is reported but the reply is still processed and counted.
//
//icmp multiple type checks :
// Requests (our own ones, seen on loopback): skip them.
// Timestamp / address mask replies answer requests we never send.
// not errors, someone else ping us or stuff like this
//
void	recv_ping(t_ping *ping)
{
	char				buf[1024];
	char				from_str[INET_ADDRSTRLEN];
	struct sockaddr_in	from;
	socklen_t			fromlen;
	ssize_t				n;
	struct iphdr		*ip;
	struct icmphdr		*icmp;
	int					ip_hlen;
	struct timeval		sent;
	long long			now;
	double				rtt;
	int					seq;
	int					dup;

	// 1. receive packet content into buf
	fromlen = sizeof(from);
	n = recvfrom(ping->sockfd, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *)&from, &fromlen);
	if (n < 0)
		return ;
	now = now_us();
	//debug_print_packet(buf, n);

	//2. get ip content (for good structure and clarity)
	ip = (struct iphdr *)buf;
	ip_hlen = ip->ihl * 4;
	/* Too short to contain the IP header + the ICMP header: same message as inetutils */
	if (n < ip_hlen + ICMP_HDR_LEN) {
		inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str));
		fprintf(stderr, "packet too short (%ld bytes) from %s\n", (long)n, from_str);
		return ;
	}
	icmp = (struct icmphdr *)(buf + ip_hlen);

	//3. ICMP content check now
	/* ICMP errors (RFC 792): they quote the packet that failed, maybe ours */
	if (icmp->type == ICMP_DEST_UNREACH || icmp->type == ICMP_SOURCE_QUENCH
		|| icmp->type == ICMP_REDIRECT || icmp->type == ICMP_TIME_EXCEEDED
		|| icmp->type == ICMP_PARAMETERPROB)
	{
		handle_icmp_error(ping, buf, n, ip_hlen, &from);
		return ;
	}
	/* Requests, other replies, unknown types: not for us */
	if (icmp->type != ICMP_ECHOREPLY)
		return ;
	/* Echo reply for another process: skip it */
	if (ntohs(icmp->un.echo.id) != ping->id)
		return ;

	if (checksum(icmp, n - ip_hlen) != 0)
	{
		inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str));
		fprintf(stderr, "checksum mismatch from %s\n", from_str);
	}
	if (n < ip_hlen + ICMP_HDR_LEN + (ssize_t)sizeof(sent))
	{
		fprintf(stderr, "ft_ping error: packet to short too hold ICMP Data\n");
		return ;
	}

	//3. retrieve our send timestamp and calcul the difference with now 
	memcpy(&sent, buf + ip_hlen + ICMP_HDR_LEN, sizeof(sent));
	rtt = (now - ((long long)sent.tv_sec * 1000000LL + sent.tv_usec)) / 1000.0;

	//4. Packet logic: update packet structures and stats
	seq = ntohs(icmp->un.echo.sequence);
	dup = dup_test_and_set(ping, seq);
	if (dup)
		ping->duplicates++;
	else
		ping->received++;
	update_rtt(ping, rtt);

	//5. Just display
	/* n - ip_hlen = ICMP size (64), what real ping prints */
	inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str));
	printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms%s\n",
		(long)(n - ip_hlen), from_str, seq, ip->ttl, rtt,
		dup ? " (DUP!)" : "");
}
