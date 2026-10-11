#include "../includes/ft_ping.h"

int	main(int argc, char **argv)
{
	t_ping	ping;

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <host>\n", argv[0]);
		return (1);
	}
	memset(&ping, 0, sizeof(ping));
	ping.host = argv[1];
	/*
	** The ICMP "identifier" field lets us recognise our own replies.
	** It is 16 bits, so we keep only the low 16 bits of the pid.
	*/
	ping.id = getpid() & 0xFFFF;

	/* 1. hostname / IP string -> struct sockaddr_in */
	if (resolve_host(&ping) != 0)
		return (1);

	/*
	** 2. Raw socket: we write the ICMP header ourselves, the kernel adds
	**    the IP header. Needs root (or CAP_NET_RAW) -> run with sudo.
	*/
	ping.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ping.sockfd < 0)
	{
		perror("socket");
		return (1);
	}

	printf("PING %s (%s): %d data bytes\n", ping.host, ping.ip_str, PING_DATA_LEN);

	/* 3. One request, one reply */
	send_ping(&ping, 0);
	recv_ping(&ping);

	close(ping.sockfd);
	return (0);
}
