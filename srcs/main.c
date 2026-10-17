#include "../includes/ft_ping.h"

volatile sig_atomic_t	g_stop = 0;

/*
** Ctrl+C: we only raise a flag. printf() and friends are not safe inside
** a signal handler, so the statistics are printed by main after the loop.
*/
static void	on_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

int	main(int argc, char **argv)
{
	t_ping				ping;
	struct sigaction	sa;
	int					ret;

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

	/*
	** 3. sigaction without SA_RESTART: Ctrl+C makes select() return -1
	**    with errno EINTR, so the loop sees g_stop right away.
	*/
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_sigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);

	/* 4. Send every second, print replies, until Ctrl+C */
	ping_loop(&ping);
	ret = print_stats(&ping);

	close(ping.sockfd);
	return (ret);
}
