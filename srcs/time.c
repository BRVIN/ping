#include "../includes/ft_ping.h"

/*
** All the loop arithmetic is done on plain numbers (microseconds since
** the epoch) instead of struct timeval: "a - b" instead of normalising
** tv_usec by hand. A long long holds this for ~292000 years.
**
** Only the timestamp written INSIDE the packet stays a struct timeval.
*/

long long	tv_to_us(struct timeval tv)
{
	return ((long long)tv.tv_sec * 1000000 + tv.tv_usec);
}

long long	now_us(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return (tv_to_us(tv));
}
