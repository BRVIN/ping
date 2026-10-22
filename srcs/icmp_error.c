#include "../includes/ft_ping.h"

typedef struct s_icmp_code
{
	int			type;
	int			code;
	const char	*text;
}	t_icmp_code;

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

static int	is_from_us(t_ping *ping, char *buf, ssize_t n, int ip_hlen)
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

static void	print_quoted_packet(struct iphdr *orig, int verbose)
{
	unsigned char	*bytes;
	struct icmphdr	*orig_icmp;
	char			src[INET_ADDRSTRLEN];
	char			dst[INET_ADDRSTRLEN];
	size_t			i;

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

	inet_ntop(AF_INET, &orig->saddr, src, sizeof(src));
	inet_ntop(AF_INET, &orig->daddr, dst, sizeof(dst));
	printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src\tDst\tData\n");
	printf(" %1x  %1x  %02x %04x %04x   %1x %04x  %02x  %02x %04x %s  %s \n",
		orig->version, orig->ihl, orig->tos, ntohs(orig->tot_len),
		ntohs(orig->id), (ntohs(orig->frag_off) & 0xe000) >> 13,
		ntohs(orig->frag_off) & 0x1fff, orig->ttl, orig->protocol,
		ntohs(orig->check), src, dst);

	orig_icmp = (struct icmphdr *)((char *)orig + orig->ihl * 4);
	printf("ICMP: type %u, code %u, size %u, id 0x%04x, seq 0x%04x\n",
		orig_icmp->type, orig_icmp->code,
		ntohs(orig->tot_len) - orig->ihl * 4,
		ntohs(orig_icmp->un.echo.id), ntohs(orig_icmp->un.echo.sequence));
}

void	handle_icmp_error(t_ping *ping, char *buf, ssize_t n, int ip_hlen, const char *from_str)
{
	struct icmphdr	*icmp;

	if (!is_from_us(ping, buf, n, ip_hlen))
		return ;
	icmp = (struct icmphdr *)(buf + ip_hlen);

	printf("%ld bytes from %s: ", (long)(n - ip_hlen), from_str);

	switch(icmp->type) {
		case ICMP_DEST_UNREACH:
			print_code_message(icmp->type, icmp->code, "Dest Unreachable");
			break;
		case ICMP_REDIRECT:
			print_code_message(icmp->type, icmp->code, "Redirect");
			break;
		case ICMP_TIME_EXCEEDED:
			print_code_message(icmp->type, icmp->code, "Time exceeded");
			break;
		case ICMP_SOURCE_QUENCH:
			printf("Source Quench\n");
			break;
		case ICMP_PARAMETERPROB:
			printf("Parameter problem: pointer = %u\n", ((unsigned char *)icmp)[4]);
			break;
	}

	if (ping->verbose || icmp->type == ICMP_SOURCE_QUENCH
		|| icmp->type == ICMP_PARAMETERPROB)
		print_quoted_packet((struct iphdr *)(buf + ip_hlen + ICMP_HDR_LEN), ping->verbose);
}
