#include "../includes/ft_ping.h"


void	send_ping(t_ping *ping, int seq)
{
	char	packet[PACKET_LEN];

	build_packet(ping, packet, seq);
	/* The kernel builds the IP header around our ICMP packet */
	if (sendto(ping->sockfd, packet, PACKET_LEN, 0,
			(struct sockaddr *)&ping->addr, sizeof(ping->addr)) < 0)
		perror("sendto");
}

/*
** A raw ICMP socket receives EVERY ICMP packet arriving on the machine
** (other ping processes, errors, ...), so we loop until we find ours.
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
void	recv_ping(t_ping *ping)
{
	char				buf[1024];
	struct sockaddr_in	from;
	socklen_t			fromlen;
	ssize_t				n;
	struct iphdr		*ip;
	struct icmphdr		*icmp;
	int					ip_hlen;
	struct timeval		sent;
	struct timeval		now;
	double				rtt;

	while (1)
	{
		fromlen = sizeof(from);
		n = recvfrom(ping->sockfd, buf, sizeof(buf), 0,
				(struct sockaddr *)&from, &fromlen);
		if (n < 0)
		{
			perror("recvfrom");
			return ;
		}
		gettimeofday(&now, NULL);

		ip = (struct iphdr *)buf;
		ip_hlen = ip->ihl * 4;
		icmp = (struct icmphdr *)(buf + ip_hlen);

		/* Not an echo reply, or not for us (another process): skip it */
		if (icmp->type != ICMP_ECHOREPLY || ntohs(icmp->un.echo.id) != ping->id)
			continue ;

		/* Our timestamp came back inside the data */
		memcpy(&sent, buf + ip_hlen + ICMP_HDR_LEN, sizeof(sent));
		rtt = (now.tv_sec - sent.tv_sec) * 1000.0
			+ (now.tv_usec - sent.tv_usec) / 1000.0;

		/* n - ip_hlen = ICMP size (64), what real ping prints */
		printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
			(long)(n - ip_hlen), inet_ntoa(from.sin_addr),
			ntohs(icmp->un.echo.sequence), ip->ttl, rtt);
		return ;
	}
}
