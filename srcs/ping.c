#include "../includes/ft_ping.h"

/*
** Sends one echo request. Its icmp_seq is the number of packets already
** transmitted (0, 1, 2 ...). Like inetutils, a failed sendto() is fatal.
*/
void	send_ping(t_ping *ping)
{
	char	packet[PACKET_LEN];

	build_packet(ping, packet, ping->transmitted);
	/* The kernel builds the IP header around our ICMP packet */
	if (sendto(ping->sockfd, packet, PACKET_LEN, 0,
			(struct sockaddr *)&ping->addr, sizeof(ping->addr)) < 0)
	{
		fprintf(stderr, "ft_ping: sending packet: %s\n", strerror(errno));
		close(ping->sockfd);
		exit(1);
	}
	ping->transmitted++;
}

/*
** Reads ONE packet. select() told us one is waiting, so recvfrom() won't
** block. A raw ICMP socket receives EVERY ICMP packet arriving on the
** machine (other ping processes, errors, our own request on loopback...),
** so most of the time we just ignore what we read and return: the main
** loop will call us again when the next packet arrives.
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
	struct sockaddr_in	from;
	socklen_t			fromlen;
	ssize_t				n;
	struct iphdr		*ip;
	struct icmphdr		*icmp;
	int					ip_hlen;
	struct timeval		sent;
	long long			now;
	double				rtt;
	char				from_str[INET_ADDRSTRLEN];

	fromlen = sizeof(from);
	n = recvfrom(ping->sockfd, buf, sizeof(buf), 0,
			(struct sockaddr *)&from, &fromlen);
	if (n < 0)
		return ;
	now = now_us();

	ip = (struct iphdr *)buf;
	ip_hlen = ip->ihl * 4;
	icmp = (struct icmphdr *)(buf + ip_hlen);

	/* Not an echo reply, or not for us (another process): skip it */
	if (icmp->type != ICMP_ECHOREPLY || ntohs(icmp->un.echo.id) != ping->id)
		return ;

	/* Our timestamp came back inside the data */
	memcpy(&sent, buf + ip_hlen + ICMP_HDR_LEN, sizeof(sent));
	rtt = (now - tv_to_us(sent)) / 1000.0;
	update_stats(ping, rtt);

	/* inet_ntop writes in OUR buffer (inet_ntoa uses a static one) */
	inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str));
	/* n - ip_hlen = ICMP size (64), what real ping prints */
	printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
		(long)(n - ip_hlen), from_str,
		ntohs(icmp->un.echo.sequence), ip->ttl, rtt);
}

/*
** How long select() may sleep: until the next send is due.
** Later this is the one place to add the -w deadline and the -c linger.
*/
static long long	compute_timeout(t_ping *ping)
{
	long long	wait;

	wait = ping->next_send_us - now_us();
	if (wait < 0)
		wait = 0;
	return (wait);
}

/*
** The main loop. We wait for whichever comes first:
**   - a packet on the socket  -> recv_ping() reads it
**   - the next send time      -> send_ping() sends the next request
**   - Ctrl+C                  -> select() fails with EINTR, g_stop is 1
**
**   time --->
**   send 0          send 1          send 2
**     |----wait------|----wait-------|
**          ^ reply 0       ^ reply 1      (replies arrive whenever)
**
** select() modifies both the fd_set and the timeval, so both are
** rebuilt at every turn.
*/
void	ping_loop(t_ping *ping)
{
	fd_set			readfds;
	struct timeval	tv;
	long long		wait;
	int				ret;

	ping->interval_us = INTERVAL_US;
	send_ping(ping);                    /* the first one goes immediately */
	ping->next_send_us = now_us() + ping->interval_us;
	while (!g_stop)
	{
		wait = compute_timeout(ping);
		tv.tv_sec = wait / 1000000;
		tv.tv_usec = wait % 1000000;
		FD_ZERO(&readfds);
		FD_SET(ping->sockfd, &readfds);
		ret = select(ping->sockfd + 1, &readfds, NULL, NULL, &tv);
		if (ret < 0)
		{
			if (errno == EINTR)         /* a signal: loop checks g_stop */
				continue ;
			perror("select");
			break ;
		}
		if (ret > 0)
			recv_ping(ping);
		/* relative schedule: next = time of this send + interval */
		if (now_us() >= ping->next_send_us)
		{
			send_ping(ping);
			ping->next_send_us = now_us() + ping->interval_us;
		}
	}
}
