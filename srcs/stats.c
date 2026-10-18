#include "../includes/ft_ping.h"

#define SQRT_PRECISION	0.0005

static double	ft_abs(double x)
{
	if (x < 0)
		return (-x);
	return (x);
}

/*
** Square root by Newton's method: x <- (x + a / x) / 2 converges to sqrt(a).
** Same precision rule as inetutils, so the stddev column matches:
** values below SQRT_PRECISION give 0, and iteration stops once a step
** changes x by no more than SQRT_PRECISION. (Also avoids linking libm.)
*/
static double	ft_sqrt(double a)
{
	double	x;
	double	next;

	if (a < SQRT_PRECISION)
		return (0.0);
	x = a / 2;
	next = (x + a / x) / 2;
	while (ft_abs(next - x) > SQRT_PRECISION)
	{
		x = next;
		next = (x + a / x) / 2;
	}
	return (next);
}

/*
** --- host ping statistics ---
** N packets transmitted, M packets received, [+D duplicates, ]L% packet loss
** round-trip min/avg/max/stddev = a/b/c/d ms      (only if replies came back)
**
** Loss is an integer division (truncated, like inetutils).
** Duplicates are not "received", but their RTT counts in the averages.
*/
void	print_stats(t_ping *ping)
{
	long	total;
	double	avg;
	double	variance;

	printf("--- %s ping statistics ---\n", ping->canonname);
	printf("%ld packets transmitted, %ld packets received, ",
		ping->transmitted, ping->received);
	if (ping->duplicates)
		printf("+%ld duplicates, ", ping->duplicates);
	if (ping->transmitted)
	{
		if (ping->received > ping->transmitted)
			printf("-- somebody is printing forged packets!");
		else
			printf("%ld%% packet loss", (ping->transmitted - ping->received)
				* 100 / ping->transmitted);
	}
	printf("\n");
	if (ping->received == 0)
		return ;
	total = ping->received + ping->duplicates;
	avg = ping->rtt_sum / total;
	variance = ping->rtt_sumsq / total - avg * avg;
	printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
		ping->rtt_min, avg, ping->rtt_max, ft_sqrt(variance));
}
