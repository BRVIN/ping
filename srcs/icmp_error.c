#include "../includes/ft_ping.h"

/*
** ICMP errors (Destination Unreachable, Time Exceeded, ...) are sent by a
** router or host that could not deliver one of our echo requests. They
** quote the beginning of the packet that failed, so we can tell whether
** it was ours:
**
**   +-------------+-----------+-------------------+-------------------+
**   | IP header   | ICMP error| quoted IP header  | quoted ICMP header|
**   | (ip_hlen)   | 8 bytes   | (our request's)   | 8 bytes: our id,  |
**   |             | type/code |                   | our seq           |
**   +-------------+-----------+-------------------+-------------------+
**
** Only the message line is printed, with or without -v (see Compromise.md);
** errors never stop the program and are not counted in the statistics.
*/

typedef struct s_icmp_code
{
	int			type;
	int			code;
	const char	*text;
}	t_icmp_code;

/* Codes 9 and 10 of Dest Unreachable are missing on purpose (as inetutils) */
static const t_icmp_code	g_codes[] = {
	{ICMP_DEST_UNREACH, ICMP_NET_UNREACH, "Destination Net Unreachable"},
	{ICMP_DEST_UNREACH, ICMP_HOST_UNREACH, "Destination Host Unreachable"},
	{ICMP_DEST_UNREACH, ICMP_PROT_UNREACH, "Destination Protocol Unreachable"},
	{ICMP_DEST_UNREACH, ICMP_PORT_UNREACH, "Destination Port Unreachable"},
	{ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED, "Fragmentation needed and DF set"},
	{ICMP_DEST_UNREACH, ICMP_SR_FAILED, "Source Route Failed"},
	{ICMP_DEST_UNREACH, ICMP_NET_UNKNOWN, "Network Unknown"},
	{ICMP_DEST_UNREACH, ICMP_HOST_UNKNOWN, "Host Unknown"},
	{ICMP_DEST_UNREACH, ICMP_HOST_ISOLATED, "Host Isolated"},
	{ICMP_DEST_UNREACH, ICMP_NET_UNR_TOS,
		"Destination Network Unreachable At This TOS"},
	{ICMP_DEST_UNREACH, ICMP_HOST_UNR_TOS,
		"Destination Host Unreachable At This TOS"},
	{ICMP_DEST_UNREACH, ICMP_PKT_FILTERED, "Packet Filtered"},
	{ICMP_DEST_UNREACH, ICMP_PREC_VIOLATION, "Precedence Violation"},
	{ICMP_DEST_UNREACH, ICMP_PREC_CUTOFF, "Precedence Cutoff"},
	{ICMP_REDIRECT, ICMP_REDIR_NET, "Redirect Network"},
	{ICMP_REDIRECT, ICMP_REDIR_HOST, "Redirect Host"},
	{ICMP_REDIRECT, ICMP_REDIR_NETTOS, "Redirect Type of Service and Network"},
	{ICMP_REDIRECT, ICMP_REDIR_HOSTTOS, "Redirect Type of Service and Host"},
	{ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, "Time to live exceeded"},
	{ICMP_TIME_EXCEEDED, ICMP_EXC_FRAGTIME, "Frag reassembly time exceeded"},
};

/*
** The error is ours if the quoted packet is an echo request with our id,
** sent to our target. Also checks that everything we read is inside n.
*/
// 
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      Internet Header + 64 bits of Original Data Datagram      |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// Remember: here we unwrap the IP/ICMP content copied into the real ICMP packet content Data
static int	is_from_us(t_ping *ping, char *buf, ssize_t n, int ip_hlen)
{
	struct iphdr	*orig;
	struct icmphdr	*orig_icmp;
	int				orig_hlen;

	// getting the copied ip header from "IH+64" ICMP content
	if (n < ip_hlen + ICMP_HDR_LEN + (ssize_t)sizeof(struct iphdr))
		return (0);
	orig = (struct iphdr *)(buf + ip_hlen + ICMP_HDR_LEN);
	orig_hlen = orig->ihl * 4;

	// getting the copied ICMP header ...
	if (n < ip_hlen + ICMP_HDR_LEN + orig_hlen + ICMP_HDR_LEN)
		return (0);
	orig_icmp = (struct icmphdr *)((char *)orig + orig_hlen);

	if (orig->daddr != ping->addr.sin_addr.s_addr
		|| orig->protocol != IPPROTO_ICMP
		|| orig_icmp->type != ICMP_ECHO
		|| ntohs(orig_icmp->un.echo.id) != ping->id) {
		return 0;
	}

	return 1;
}

static void	print_code_message(int type, int code, const char *prefix)
{
	size_t	i;
	size_t	g_codes_size;

	/* number of entries in g_codes, computed at compile time */
	g_codes_size = sizeof(g_codes) / sizeof(g_codes[0]);
	i = 0;
	while (i < g_codes_size)
	{
		if (g_codes[i].type == type && g_codes[i].code == code)
		{
			printf("%s\n", g_codes[i].text);
			return ;
		}
		i++;
	}
	printf("%s, Unknown Code: %d\n", prefix, code);
}

/*
** Called by recv_ping for the 5 ICMP error types only: Dest Unreachable,
** Source Quench, Redirect, Time Exceeded, Parameter Problem.
*/

// possible ICMP types entering are :
// [ICMP_DEST_UNREACH] [ICMP_SOURCE_QUENCH] [ICMP_REDIRECT] [ICMP_TIME_EXCEEDED] [ICMP_PARAMETERPROB]
// they all have the same header :
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |     Type      |     Code      |          Checksum             |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    Pointer    |                   unused                      |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      Internet Header + 64 bits of Original Data Datagram      |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// they all send " The internet header plus the first 64 bits of the original datagram's data"
// is_from_us checks it to see if we are the sender
//
void	handle_icmp_error(t_ping *ping, char *buf, ssize_t n, int ip_hlen, struct sockaddr_in *from)
{
	struct icmphdr	*icmp;
	char			from_str[INET_ADDRSTRLEN];

	// check "IH+64" data to see if we are the sender, ignore if we are not
	if (!is_from_us(ping, buf, n, ip_hlen))
		return ;
	icmp = (struct icmphdr *)(buf + ip_hlen);
	/* Numeric address only: no reverse DNS on returned packets (subject) */
	inet_ntop(AF_INET, &from->sin_addr, from_str, sizeof(from_str));
	printf("%ld bytes from %s: ", (long)(n - ip_hlen), from_str);
	if (icmp->type == ICMP_DEST_UNREACH)
		print_code_message(icmp->type, icmp->code, "Dest Unreachable");
	else if (icmp->type == ICMP_REDIRECT)
		print_code_message(icmp->type, icmp->code, "Redirect");
	else if (icmp->type == ICMP_TIME_EXCEEDED)
		print_code_message(icmp->type, icmp->code, "Time exceeded");
	else if (icmp->type == ICMP_SOURCE_QUENCH)
		printf("Source Quench\n");
	else if (icmp->type == ICMP_PARAMETERPROB)
		printf("Parameter problem: pointer = %u\n",
			((unsigned char *)icmp)[4]);
}
