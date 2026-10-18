#include "../includes/ft_ping.h"

volatile sig_atomic_t	g_stop = 0;

/*
** A signal handler may interrupt the program anywhere, so it only sets a
** flag: printing the statistics happens later, in the normal code path.
*/
static void	on_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

/* Current time as one integer: microseconds since 1970 */
long long	now_us(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return ((long long)tv.tv_sec * 1000000LL + tv.tv_usec);
}

/*
** Main loop: wait for "a packet arrived" OR "time to send", whichever
** comes first, with select().
**
**   send packet 0, schedule the next send 1 s later
**   while no Ctrl+C:
**       select(socket readable?, timeout = time left until next send)
**         EINTR      -> a signal (Ctrl+C) woke us up: re-check g_stop
**         readable   -> handle ONE packet
**       if the send time is reached (checked on every wake-up, so a stream
**       of incoming packets can never delay the sends) -> send, reschedule
**
** Returns -1 on a fatal error (the caller exits without statistics).
*/
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

		/* select wants a struct timeval: convert from microseconds */
		timeout.tv_sec = wait / 1000000;
		timeout.tv_usec = wait % 1000000;

		/* select modifies the set, so it is rebuilt before every call */
		FD_ZERO(&readfds);
		FD_SET(ping->sockfd, &readfds);

		ret = select(ping->sockfd + 1, &readfds, NULL, NULL, &timeout);

		if (ret < 0)
		{
			//signal caught :
			if (errno == EINTR)
				continue ;
			perror("ft_ping: select");
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
