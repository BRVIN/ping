#include "../includes/ft_ping.h"

static void	dup_clear(t_ping *ping, int seq)
{
	ping->dup_table[seq % DUP_TABLE_SIZE] = 0;
}

static int	dup_test_and_set(t_ping *ping, int seq)
{
	if (ping->dup_table[seq % DUP_TABLE_SIZE] == 1)
		return (1);
	ping->dup_table[seq % DUP_TABLE_SIZE] = 1;
	return (0);
}

int	send_ping(t_ping *ping)
{
	char	packet[PACKET_LEN];

	build_packet(ping, packet, ping->transmitted & 0xFFFF);
	dup_clear(ping, ping->transmitted & 0xFFFF);

	while(1){
		const ssize_t n = sendto(ping->sockfd, packet, PACKET_LEN, 0, (struct sockaddr *)&ping->sockaddr, sizeof(ping->sockaddr));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "ft_ping - sendto: %s\n", strerror(errno));
			return (-1);
		}

		if (n != PACKET_LEN) {
			fprintf(stderr, "ft_ping - sendto: Packet not fully sent\n");
			return (0);
		}
		break;
	}
	ping->transmitted++;
	return (0);
}

static void	update_rtt(t_ping *ping, double rtt)
{
	if (ping->received + ping->duplicates == 1 || rtt < ping->rtt_min)
		ping->rtt_min = rtt;
	if (rtt > ping->rtt_max)
		ping->rtt_max = rtt;
	ping->rtt_sum += rtt;
	ping->rtt_sumsq += rtt * rtt;
}

void	recv_ping(t_ping *ping)
{
	char buf[1024];
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);

	ssize_t n;

	while (1) {
		n = recvfrom(ping->sockfd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&from, &fromlen);
		if (n < 0) {
			if (errno == EINTR)
				continue;

			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return ;

			fprintf(stderr, "ft_ping - recvfrom: %s\n", strerror(errno));
			return ;
		}
		break;
	}

	const long long now = now_us();

	const struct iphdr* ip = (struct iphdr *)buf;
	const int ip_hlen = ip->ihl * 4;

	char from_str[INET_ADDRSTRLEN] = "unknown";

	(void)inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str));

	if (n < ip_hlen + ICMP_HDR_LEN) {
		fprintf(stderr, "ft_ping: packet too short (%ld bytes) from %s\n", (long)n, from_str);
		return ;
	}
	struct icmphdr* icmp = (struct icmphdr *)(buf + ip_hlen);

	if (icmp->type == ICMP_DEST_UNREACH || icmp->type == ICMP_SOURCE_QUENCH ||
	    icmp->type == ICMP_REDIRECT     || icmp->type == ICMP_TIME_EXCEEDED ||
	    icmp->type == ICMP_PARAMETERPROB) {
		handle_icmp_error(ping, buf, n, ip_hlen, from_str);
		return ;
	}

	if (icmp->type != ICMP_ECHOREPLY)
		return ;

	if (ntohs(icmp->un.echo.id) != ping->id)
		return ;

	if (checksum(icmp, n - ip_hlen) != 0)
		fprintf(stderr, "ft_ping: checksum mismatch from %s\n", from_str);

	struct timeval		sent;

	if (n < ip_hlen + ICMP_HDR_LEN + (ssize_t)sizeof(sent)) {
		fprintf(stderr, "ft_ping: packet too short to hold ICMP Data\n");
		return ;
	}

	memcpy(&sent, buf + ip_hlen + ICMP_HDR_LEN, sizeof(sent));
	double rtt = (now - ((long long)sent.tv_sec * 1000000LL + sent.tv_usec)) / 1000.0;

	int seq = ntohs(icmp->un.echo.sequence);
	int dup = dup_test_and_set(ping, seq);
	if (dup)
		ping->duplicates++;
	else
		ping->received++;
	update_rtt(ping, rtt);

	printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms%s\n",
		(long)(n - ip_hlen), from_str, seq, ip->ttl, rtt,
		dup ? " (DUP!)" : "");
	return ;
}
