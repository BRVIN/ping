#include "../includes/ft_ping.h"

#define DUMP_WIDTH	16

static void	print_rows(unsigned char *pkt, int start, int len)
{
	int	row;
	int	i;

	row = start;
	while (row < start + len)
	{
		printf("|  %04x   ", row);
		i = 0;
		while (i < DUMP_WIDTH)
		{
			if (row + i < start + len)
				printf("%02x ", pkt[row + i]);
			else
				printf("   ");
			if (i == 7)
				printf(" ");
			i++;
		}
		printf(" |");
		i = 0;
		while (i < DUMP_WIDTH && row + i < start + len)
		{
			printf("%c", isprint(pkt[row + i]) ? pkt[row + i] : '.');
			i++;
		}
		printf("|\n");
		row += DUMP_WIDTH;
	}
}

static void	print_section(const char *title, int start, int len)
{
	printf("|\n+-- %s ---- bytes %d..%d (%d)\n", title, start,
		start + len - 1, len);
}

static const char	*icmp_type_name(int type)
{
	if (type == ICMP_ECHOREPLY)
		return ("Echo Reply");
	if (type == ICMP_DEST_UNREACH)
		return ("Destination Unreachable");
	if (type == ICMP_SOURCE_QUENCH)
		return ("Source Quench");
	if (type == ICMP_REDIRECT)
		return ("Redirect");
	if (type == ICMP_ECHO)
		return ("Echo Request");
	if (type == ICMP_TIME_EXCEEDED)
		return ("Time Exceeded");
	if (type == ICMP_PARAMETERPROB)
		return ("Parameter Problem");
	return ("Other");
}

static void	print_ip_fields(struct iphdr *ip)
{
	char	src[INET_ADDRSTRLEN];
	char	dst[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, &ip->saddr, src, sizeof(src));
	inet_ntop(AF_INET, &ip->daddr, dst, sizeof(dst));
	printf("|    version %u   ihl %u (%u bytes)   tos 0x%02x   total len %u\n",
		ip->version, ip->ihl, ip->ihl * 4, ip->tos, ntohs(ip->tot_len));
	printf("|    id 0x%04x   flags 0x%x   frag offset %u   ttl %u"
		"   protocol %u%s\n", ntohs(ip->id),
		(ntohs(ip->frag_off) & 0xe000) >> 13, ntohs(ip->frag_off) & 0x1fff,
		ip->ttl, ip->protocol, ip->protocol == IPPROTO_ICMP ? " (ICMP)" : "");
	printf("|    checksum 0x%04x   src %s   dst %s\n",
		ntohs(ip->check), src, dst);
}

static void	print_icmp_fields(struct icmphdr *icmp)
{
	printf("|    type %u (%s)   code %u   checksum 0x%04x\n",
		icmp->type, icmp_type_name(icmp->type), icmp->code,
		ntohs(icmp->checksum));
	if (icmp->type == ICMP_ECHOREPLY || icmp->type == ICMP_ECHO)
		printf("|    id 0x%04x (%u)   sequence %u\n",
			ntohs(icmp->un.echo.id), ntohs(icmp->un.echo.id),
			ntohs(icmp->un.echo.sequence));
	else
		printf("|    rest of header 0x%08x\n", ntohl(icmp->un.gateway));
}

static void	print_data_fields(struct icmphdr *icmp, unsigned char *data,
				int len)
{
	struct timeval	tv;

	if ((icmp->type == ICMP_ECHOREPLY || icmp->type == ICMP_ECHO)
		&& len >= (int)sizeof(tv))
	{
		memcpy(&tv, data, sizeof(tv));
		printf("|    timestamp %ld.%06ld (first %zu bytes), then filler\n",
			(long)tv.tv_sec, (long)tv.tv_usec, sizeof(tv));
	}
	else if (icmp->type != ICMP_ECHOREPLY && icmp->type != ICMP_ECHO)
		printf("|    quoted original packet (IP header + ICMP header)\n");
}

void	debug_print_packet(char *buf, ssize_t n)
{
	unsigned char	*pkt;
	struct iphdr	*ip;
	int				ip_hlen;

	pkt = (unsigned char *)buf;
	printf("\n+==== RECEIVED PACKET: %ld bytes "
		"===========================================\n", (long)n);
	if (n < (ssize_t)sizeof(struct iphdr))
	{
		print_section("TRUNCATED (no full IP header)", 0, n);
		print_rows(pkt, 0, n);
		printf("+===========================================================\n");
		return ;
	}
	ip = (struct iphdr *)buf;
	ip_hlen = ip->ihl * 4;
	if (ip_hlen > n)
		ip_hlen = n;
	print_section("IP HEADER", 0, ip_hlen);
	print_rows(pkt, 0, ip_hlen);
	print_ip_fields(ip);
	if (n >= ip_hlen + ICMP_HDR_LEN)
	{
		print_section("ICMP HEADER", ip_hlen, ICMP_HDR_LEN);
		print_rows(pkt, ip_hlen, ICMP_HDR_LEN);
		print_icmp_fields((struct icmphdr *)(buf + ip_hlen));
		if (n > ip_hlen + ICMP_HDR_LEN)
		{
			print_section("ICMP DATA", ip_hlen + ICMP_HDR_LEN,
				n - ip_hlen - ICMP_HDR_LEN);
			print_rows(pkt, ip_hlen + ICMP_HDR_LEN,
				n - ip_hlen - ICMP_HDR_LEN);
			print_data_fields((struct icmphdr *)(buf + ip_hlen),
				pkt + ip_hlen + ICMP_HDR_LEN, n - ip_hlen - ICMP_HDR_LEN);
		}
	}
	else if (n > ip_hlen)
	{
		print_section("TRUNCATED ICMP", ip_hlen, n - ip_hlen);
		print_rows(pkt, ip_hlen, n - ip_hlen);
	}
	printf("+===========================================================\n\n");
}
