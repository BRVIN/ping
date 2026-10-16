#include "../includes/ft_ping.h"

/*
** Turns "google.com" or "8.8.8.8" into a struct sockaddr_in.
** getaddrinfo handles both cases (DNS lookup or plain numeric parse).
** We ask only for IPv4 (AF_INET) as the subject requires.
*/

//MYNOTE
//
//resolve_host
// take the user input (ping->host) and fill 2 data :
// 1: ping->addr [struct sockaddr_in] (with getaddrinfo function)
// -> ping->addr.sin_addr [resulting IP binary form (4 raw bytes)]  (filled by getaddrinfo function)
// 2: ping->ip_str [resulting ip readable form] (with inet_ntop function) 

// getaddrinfo   (ping->host) & (&hints) -> (&res)
// getaddrinfo: take user input address (ping->host) and "hints" to check options and fill res with output
// If user input from ping->host is not yet ipv4, it does a real hostname resolution (DNS and stuff) (skipped if already ipv4)
// 
// It fill res->ai_addr which is a sockaddr_in because we asked for IPV4 (ai_family = AF_INET)
// it put the ip retrieved ip in res->ai_addr.sin_addr , containing the ip under non readable binary form (4 bytes of the litteral ip value)

// inet_ntop    (&ping->addr.sin_addr) -> (ping->ip_str)
// dump format conversion
// it take the ip 4bbytes binary format :  &ping->addr.sin_addr that was filled by getaddrinfo, and put it in readable form


int	resolve_host(t_ping *ping)
{
	struct addrinfo	hints;
	struct addrinfo	*res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;

	// getaddrinfo: take user input written address (ping->host) and "hints" to check options and fill res with output
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
