#include "../includes/ft_ping.h"

int	main(int argc, char **argv)
{
	t_ping	ping;

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <host>\n", argv[0]);
		return (1);
	}
	//setvbuf: to have output always print directly, it's like doing fflush(stdout) after every print
	setvbuf(stdout, NULL, _IOLBF, 0);
	memset(&ping, 0, sizeof(ping));
	ping.host = argv[1];

	// ICMP "identifier" to recognise our replies (ping replies go for any app listening)
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

	/* 3. Send / receive until Ctrl+C (a send failure stops without stats) */
	if (ping_loop(&ping) < 0)
	{
		close(ping.sockfd);
		return (1);
	}
	print_stats(&ping);
	close(ping.sockfd);

	/* Like inetutils: success only if at least one reply came back */
	if (ping.received == 0)
		return (1);
	return (0);
}
