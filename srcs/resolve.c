#include "../includes/ft_ping.h"

int	resolve_host(t_ping *ping)
{
	struct addrinfo	hints;
	struct addrinfo	*res;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;

	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;

	hints.ai_flags = AI_CANONNAME;

	const int ret = getaddrinfo(ping->host, NULL, &hints, &res);
	if (ret != 0)
	{
		fprintf(stderr, "ft_ping - getaddrinfo: %s\n", gai_strerror(ret));
		fprintf(stderr, "(unknown host)\n");
		return (1);
	}

	memcpy(&ping->sockaddr, res->ai_addr, sizeof(struct sockaddr_in));

	if (res->ai_canonname)
		snprintf(ping->canonname, sizeof(ping->canonname), "%s", res->ai_canonname);
	else
		snprintf(ping->canonname, sizeof(ping->canonname), "%s", ping->host);

	freeaddrinfo(res);

	if (!inet_ntop(AF_INET, &ping->sockaddr.sin_addr, ping->ip_str, sizeof(ping->ip_str))) {
		fprintf(stderr, "ft_ping - inet_ntop: %s\n", strerror(errno));
		return (1);
	}

	return (0);
}
