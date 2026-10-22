#include "../includes/ft_ping.h"

volatile sig_atomic_t	g_stop = 0;

static void	on_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

long long	now_us(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return ((long long)tv.tv_sec * 1000000LL + tv.tv_usec);
}

int	ping_loop(t_ping *ping)
{
	struct sigaction	sa;
	fd_set				readfds;
	struct timeval		timeout;
	long long			next_send;
	long long			wait;
	int					ret;

	memset(&sa, 0, sizeof(sa));

	sa.sa_handler = on_sigint;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	if (send_ping(ping) < 0)
		return (-1);

	next_send = now_us() + PING_INTERVAL_US;

	while (!g_stop)
	{
		wait = next_send - now_us();
		if (wait < 0)
			wait = 0;

		timeout.tv_sec = wait / 1000000;
		timeout.tv_usec = wait % 1000000;

		FD_ZERO(&readfds);
		FD_SET(ping->sockfd, &readfds);

		ret = select(ping->sockfd + 1, &readfds, NULL, NULL, &timeout);
		if (ret < 0)
		{
			if (errno == EINTR)
				continue ;

			fprintf(stderr, "select: %s\n", strerror(errno));

			return (-1);
		}

		if (ret > 0)
			recv_ping(ping);

		if (now_us() >= next_send)
		{
			if (send_ping(ping) < 0)
				return (-1);

			next_send = now_us() + PING_INTERVAL_US;
		}
	}
	return (0);
}
