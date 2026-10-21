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
** A message line is printed, then the quoted packet: with -v, or always
** for Source Quench and Parameter Problem (like inetutils). Errors never
** stop the program and are not counted in the statistics.
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

	if (orig->daddr != ping->sockaddr.sin_addr.s_addr
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
** Shows the request the error is about, as inetutils does. The first two
** lines (raw bytes) only with -v. orig is the quoted IP header, already
** validated by is_from_us.
**
** IP Hdr Dump:
**  4500 0054 3f2e 4000 0101 b1c2 c0a8 7aaf 0808 0808
** Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src	Dst	Data
**  4  5  00 0054 3f2e   2 0000  01  01 b1c2 192.168.122.175  8.8.8.8
** ICMP: type 8, code 0, size 64, id 0x1a2b, seq 0x0000
**
** The Data column (IP options) stays empty: our requests have none.
*/
static void	print_quoted_packet(struct iphdr *orig, int verbose)
{
	unsigned char	*bytes;
	struct icmphdr	*orig_icmp;
	char			src[INET_ADDRSTRLEN];
	char			dst[INET_ADDRSTRLEN];
	size_t			i;

	// 1. -v only: raw bytes of the quoted IP header, grouped two by two
	if (verbose)
	{
		bytes = (unsigned char *)orig;
		printf("IP Hdr Dump:\n ");
		i = 0;
		while (i < sizeof(struct iphdr))
		{
			printf("%02x%s", bytes[i], (i % 2) ? " " : "");
			i++;
		}
		printf("\n");
	}

	// 2. the same header, decoded field by field
	inet_ntop(AF_INET, &orig->saddr, src, sizeof(src));
	inet_ntop(AF_INET, &orig->daddr, dst, sizeof(dst));
	printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src\tDst\tData\n");
	printf(" %1x  %1x  %02x %04x %04x   %1x %04x  %02x  %02x %04x %s  %s \n",
		orig->version, orig->ihl, orig->tos, ntohs(orig->tot_len),
		ntohs(orig->id), (ntohs(orig->frag_off) & 0xe000) >> 13,
		ntohs(orig->frag_off) & 0x1fff, orig->ttl, orig->protocol,
		ntohs(orig->check), src, dst);

	// 3. the quoted ICMP header: always our echo request (is_from_us)
	orig_icmp = (struct icmphdr *)((char *)orig + orig->ihl * 4);
	printf("ICMP: type %u, code %u, size %u, id 0x%04x, seq 0x%04x\n",
		orig_icmp->type, orig_icmp->code,
		ntohs(orig->tot_len) - orig->ihl * 4,
		ntohs(orig_icmp->un.echo.id), ntohs(orig_icmp->un.echo.sequence));
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
	/* Like inetutils: quoted packet always for these two, else only with -v */
	if (ping->verbose || icmp->type == ICMP_SOURCE_QUENCH
		|| icmp->type == ICMP_PARAMETERPROB)
		print_quoted_packet((struct iphdr *)(buf + ip_hlen + ICMP_HDR_LEN),
			ping->verbose);
}
