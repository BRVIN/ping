#include "../includes/ft_ping.h"

/* Called for every valid reply, rtt in ms */
void	update_stats(t_ping *ping, double rtt)
{
	if (ping->received == 0 || rtt < ping->rtt_min)
		ping->rtt_min = rtt;
	if (ping->received == 0 || rtt > ping->rtt_max)
		ping->rtt_max = rtt;
	ping->rtt_sum += rtt;
	ping->rtt_sumsq += rtt * rtt;
	ping->received++;
}

/*
** Square root by Newton's method: x = (x + a / x) / 2 converges to sqrt(a).
** We stop when two steps differ by less than prec. Like inetutils, a value
** smaller than prec gives 0 (that's why stddev can be printed as 0.000).
** Writing our own avoids linking with -lm.
*/
static double	ft_sqrt(double a, double prec)
{
	double	x;
	double	prev;

	if (a < prec)
		return (0);
	x = a / 2;
	do
	{
		prev = x;
		x = (prev + a / prev) / 2;
	} while (x - prev > prec || prev - x > prec);
	return (x);
}

/*
** --- host ping statistics ---
** N packets transmitted, M packets received, P% packet loss
** round-trip min/avg/max/stddev = a/b/c/d ms
**
** Returns the exit code: 0 if we got at least one reply, 1 otherwise.
*/
int	print_stats(t_ping *ping)
{
	double	avg;
	double	variance;

	printf("--- %s ping statistics ---\n", ping->host);
	printf("%ld packets transmitted, %ld packets received, ",
		ping->transmitted, ping->received);
	/* integer division: the percentage is truncated (2/3 lost -> 66%) */
	if (ping->transmitted > 0)
		printf("%ld%% packet loss\n",
			(ping->transmitted - ping->received) * 100 / ping->transmitted);
	else
		printf("0%% packet loss\n");
	if (ping->received > 0)
	{
		avg = ping->rtt_sum / ping->received;
		/* variance = mean of squares - square of mean */
		variance = ping->rtt_sumsq / ping->received - avg * avg;
		printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
			ping->rtt_min, avg, ping->rtt_max, ft_sqrt(variance, 0.0005));
		return (0);
	}
	return (1);
}
