#include "../includes/ft_ping.h"

int	main(int argc, char **argv)
{
	t_ping	ping;
	int		one;

	setvbuf(stdout, NULL, _IOLBF, 0);

	if (argc == 1) {
		fprintf(stderr, "ft_ping: missing host operand\nTry 'ft_ping -?' for more information.\n");
		return (64);
	}

	memset(&ping, 0, sizeof(ping));

	check_input_options(argc, argv, &ping);
	get_check_host(argc, argv, &ping);

	ping.id = getpid() & 0xFFFF;

	if (resolve_host(&ping) != 0)
		return (1);

	ping.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ping.sockfd < 0)
	{
		fprintf(stderr, "socket: %s\n", strerror(errno));
		if (errno == EPERM || errno == EACCES)
			fprintf(stderr, "(Lacking privilege for icmp socket)\n");
		return (1);
	}

	one = 1;

	if (setsockopt(ping.sockfd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) != 0) {
		fprintf(stderr, "setsockopt: %s\n", strerror(errno));
		return (64);
	}

	printf("PING %s (%s): %d data bytes", ping.canonname, ping.ip_str, PING_DATA_LEN);
	if (ping.verbose)
		printf(", id 0x%04x = %u", ping.id, ping.id);
	printf("\n");

	if (ping_loop(&ping) < 0)
	{
		close(ping.sockfd);
		return (1);
	}
	print_stats(&ping);
	close(ping.sockfd);

	if (ping.received == 0)
		return (1);

	return (0);
}
