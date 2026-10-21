#include "../includes/ft_ping.h"

int	main(int argc, char **argv)
{
	t_ping	ping;
	int		one;

	//setvbuf: to have output always print directly, it's like doing fflush(stdout) after every print
	setvbuf(stdout, NULL, _IOLBF, 0);

	if (argc == 1) {
		fprintf(stderr, "ft_ping: missing host operand\nTry 'ft_ping -?' for more information.\n");
		return (64);
	}

	memset(&ping, 0, sizeof(ping));

	//check permission first ?
	check_input_options(argc, argv, &ping);
	get_check_host(argc, argv, &ping);

	// ICMP "identifier" to recognise our replies (ping replies go for any app listening)
	ping.id = getpid() & 0xFFFF;

	/* 1. hostname / IP string -> struct sockaddr_in */
	if (resolve_host(&ping) != 0)
		return (1);

	/*
	** 2. Raw socket: we write the ICMP header ourselves, the kernel adds
	**    the IP header. Needs root (or CAP_NET_RAW) -> run with sudo.
	*/
	//check// should I put it before resolve_host
	ping.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ping.sockfd < 0)
	{
		/* Not root (and no CAP_NET_RAW): same message as inetutils */
		if (errno == EPERM || errno == EACCES)
			fprintf(stderr, "ft_ping: Lacking privilege for icmp socket.\n");
		else
			fprintf(stderr, "ft_ping: %s\n", strerror(errno));
		return (1);
	}
	/* Like inetutils: allow sending to a broadcast address (no effect otherwise) */
	one = 1;
	//check//secure
	setsockopt(ping.sockfd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));

	printf("PING %s (%s): %d data bytes", ping.canonname, ping.ip_str, PING_DATA_LEN);
	if (ping.verbose)
		printf(", id 0x%04x = %u", ping.id, ping.id);
	printf("\n");

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
