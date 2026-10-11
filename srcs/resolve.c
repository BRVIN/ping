#include "../includes/ft_ping.h"

/*
** Turns "google.com" or "8.8.8.8" into a struct sockaddr_in.
** getaddrinfo handles both cases (DNS lookup or plain numeric parse).
** We ask only for IPv4 (AF_INET) as the subject requires.
*/
int	resolve_host(t_ping *ping)
{
	struct addrinfo	hints;
	struct addrinfo	*res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;

	if (getaddrinfo(ping->host, NULL, &hints, &res) != 0)
	{
		fprintf(stderr, "ft_ping: unknown host\n");
		return (1);
	}
	/* Take the first result; it is a sockaddr_in since we asked AF_INET */
	memcpy(&ping->addr, res->ai_addr, sizeof(struct sockaddr_in));
	freeaddrinfo(res);

	/* Binary address -> printable string, for display */
	inet_ntop(AF_INET, &ping->addr.sin_addr, ping->ip_str, sizeof(ping->ip_str));
	return (0);
}
