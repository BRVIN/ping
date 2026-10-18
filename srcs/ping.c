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
** Sends the echo request number ping->seq.
** A sendto failure is fatal, like inetutils ("sending packet: ...").
*/
int	send_ping(t_ping *ping)
{
	char	packet[PACKET_LEN];

	build_packet(ping, packet, ping->seq);
	dup_clear(ping, ping->seq & 0xFFFF);
	/* The kernel builds the IP header around our ICMP packet */
	if (sendto(ping->sockfd, packet, PACKET_LEN, 0,
			(struct sockaddr *)&ping->addr, sizeof(ping->addr)) < 0)
	{
		fprintf(stderr, "ft_ping: sending packet: %s\n", strerror(errno));
		return (-1);
	}
	ping->seq++;
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

	/* MSG_DONTWAIT: never block here, the main loop does the waiting */
	fromlen = sizeof(from);
	n = recvfrom(ping->sockfd, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *)&from, &fromlen);
	if (n < 0)
		return ;
	now = now_us();

	ip = (struct iphdr *)buf;
	ip_hlen = ip->ihl * 4;
	/* Too short to contain the IP header + the ICMP header: ignore */
	if (n < ip_hlen + ICMP_HDR_LEN)
		return ;
	icmp = (struct icmphdr *)(buf + ip_hlen);

	/* Not an echo reply, or not for us (another process): skip it */
	if (icmp->type != ICMP_ECHOREPLY || ntohs(icmp->un.echo.id) != ping->id)
		return ;
	if (n < ip_hlen + ICMP_HDR_LEN + (ssize_t)sizeof(sent))
		return ;

	/* Our timestamp came back inside the data */
	memcpy(&sent, buf + ip_hlen + ICMP_HDR_LEN, sizeof(sent));
	rtt = (now - ((long long)sent.tv_sec * 1000000LL + sent.tv_usec))
		/ 1000.0;

	seq = ntohs(icmp->un.echo.sequence);
	dup = dup_test_and_set(ping, seq);
	if (dup)
		ping->duplicates++;
	else
		ping->received++;
	update_rtt(ping, rtt);

	/* n - ip_hlen = ICMP size (64), what real ping prints */
	inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str));
	printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms%s\n",
		(long)(n - ip_hlen), from_str, seq, ip->ttl, rtt,
		dup ? " (DUP!)" : "");
}
