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
** Like inetutils, errors are printed with or without -v, never stop the
** program and are not counted in the statistics.
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
static int	is_our_error(t_ping *ping, char *buf, ssize_t n, int ip_hlen)
{
	struct iphdr	*orig;
	struct icmphdr	*orig_icmp;
	int				orig_hlen;

	if (n < ip_hlen + ICMP_HDR_LEN + (ssize_t)sizeof(struct iphdr))
		return (0);
	orig = (struct iphdr *)(buf + ip_hlen + ICMP_HDR_LEN);
	orig_hlen = orig->ihl * 4;
	if (n < ip_hlen + ICMP_HDR_LEN + orig_hlen + ICMP_HDR_LEN)
		return (0);
	orig_icmp = (struct icmphdr *)((char *)orig + orig_hlen);
	return (orig->daddr == ping->addr.sin_addr.s_addr
		&& orig->protocol == IPPROTO_ICMP
		&& orig_icmp->type == ICMP_ECHO
		&& ntohs(orig_icmp->un.echo.id) == ping->id);
}

static void	print_code_message(int type, int code, const char *prefix)
{
	size_t	i;

	i = 0;
	while (i < sizeof(g_codes) / sizeof(g_codes[0]))
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

/* -v only: the 20 raw bytes of the quoted IP header, grouped two by two */
static void	print_ip_dump(struct iphdr *orig)
{
	unsigned char	*bytes;
	size_t			i;

	bytes = (unsigned char *)orig;
	printf("IP Hdr Dump:\n ");
	i = 0;
	while (i < sizeof(struct iphdr))
	{
		printf("%02x", bytes[i]);
		if (i % 2)
			printf(" ");
		i++;
	}
	printf("\n");
}

/*
** Decoded view of the quoted IP header, then of our original request.
** With -v, the raw "IP Hdr Dump" comes first.
*/
static void	print_quoted_packet(struct iphdr *orig, int verbose)
{
	struct icmphdr	*orig_icmp;
	char			src[INET_ADDRSTRLEN];
	char			dst[INET_ADDRSTRLEN];
	unsigned char	*opt;
	int				orig_hlen;
	unsigned short	len;
	int				i;

	orig_hlen = orig->ihl * 4;
	orig_icmp = (struct icmphdr *)((char *)orig + orig_hlen);
	inet_ntop(AF_INET, &orig->saddr, src, sizeof(src));
	inet_ntop(AF_INET, &orig->daddr, dst, sizeof(dst));
	/* inetutils' byte order guess for the length field */
	len = orig->tot_len;
	if (len > 0x2000)
		len = ntohs(len);
	if (verbose)
		print_ip_dump(orig);
	printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src\tDst\tData\n");
	printf(" %1x  %1x  %02x %04x %04x   %1x %04x  %02x  %02x %04x %s  %s ",
		orig->version, orig->ihl, orig->tos, len, ntohs(orig->id),
		(ntohs(orig->frag_off) & 0xe000) >> 13, ntohs(orig->frag_off) & 0x1fff,
		orig->ttl, orig->protocol, ntohs(orig->check), src, dst);
	/* IP options, if the quoted header has some */
	opt = (unsigned char *)orig + sizeof(struct iphdr);
	i = sizeof(struct iphdr);
	while (i++ < orig_hlen)
		printf("%02x", *opt++);
	printf("\n");
	printf("ICMP: type %u, code %u, size %u, id 0x%04x, seq 0x%04x\n",
		orig_icmp->type, orig_icmp->code, ntohs(orig->tot_len) - orig_hlen,
		ntohs(orig_icmp->un.echo.id), ntohs(orig_icmp->un.echo.sequence));
}

/* Message line, then (-v only) the decoded packet that failed */
static void	print_icmp(struct icmphdr *icmp, struct iphdr *orig,
				const char *prefix, int verbose)
{
	print_code_message(icmp->type, icmp->code, prefix);
	if (verbose)
		print_quoted_packet(orig, verbose);
}

/* Called by recv_ping for every ICMP type that is not a reply or a request */
void	handle_icmp_error(t_ping *ping, char *buf, ssize_t n, int ip_hlen, struct sockaddr_in *from)
{
	struct icmphdr	*icmp;
	struct iphdr	*orig;
	char			from_str[INET_ADDRSTRLEN];
	char			gw_str[INET_ADDRSTRLEN];

	if (!is_our_error(ping, buf, n, ip_hlen))
		return ;
	icmp = (struct icmphdr *)(buf + ip_hlen);
	orig = (struct iphdr *)(buf + ip_hlen + ICMP_HDR_LEN);
	/* Numeric address only: no reverse DNS on returned packets (subject) */
	inet_ntop(AF_INET, &from->sin_addr, from_str, sizeof(from_str));
	printf("%ld bytes from %s: ", (long)(n - ip_hlen), from_str);
	if (icmp->type == ICMP_DEST_UNREACH)
		print_icmp(icmp, orig, "Dest Unreachable", ping->verbose);
	else if (icmp->type == ICMP_REDIRECT)
		print_icmp(icmp, orig, "Redirect", ping->verbose);
	else if (icmp->type == ICMP_TIME_EXCEEDED)
		print_icmp(icmp, orig, "Time exceeded", ping->verbose);
	else if (icmp->type == ICMP_SOURCE_QUENCH)
	{
		printf("Source Quench\n");
		print_quoted_packet(orig, ping->verbose);
	}
	else if (icmp->type == ICMP_PARAMETERPROB)
	{
		inet_ntop(AF_INET, &icmp->un.gateway, gw_str, sizeof(gw_str));
		printf("Parameter problem: IP address = %s\n", gw_str);
		print_quoted_packet(orig, ping->verbose);
	}
	else if (icmp->type == 9)
		printf("Router Advertisement\n");
	else if (icmp->type == 10)
		printf("Router Discovery\n");
	else if (icmp->type == ICMP_INFO_REQUEST)
		printf("Information Request\n");
	else if (icmp->type == ICMP_INFO_REPLY)
		printf("Information Reply\n");
	else
		printf("Bad ICMP type: %d\n", icmp->type);
}
