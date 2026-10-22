#include "../includes/ft_ping.h"

unsigned short	checksum(void *data, int len)
{
	unsigned short	*p;
	unsigned int	sum;

	p = data;
	sum = 0;
	while (len > 1)
	{
		sum += *p++;
		len -= 2;
	}
	if (len == 1)
		sum += *(unsigned char *)p;
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ((unsigned short)~sum);
}

void	build_packet(t_ping *ping, char *packet, int seq)
{
	struct icmphdr	*icmp;
	struct timeval	tv;
	char			*filler;
	int				filler_len;
	int				i;

	memset(packet, 0, PACKET_LEN);
	icmp = (struct icmphdr *)packet;
	icmp->type = ICMP_ECHO;
	icmp->code = 0;

	icmp->un.echo.id = htons(ping->id);
	icmp->un.echo.sequence = htons(seq);

	gettimeofday(&tv, NULL);
	memcpy(packet + ICMP_HDR_LEN, &tv, sizeof(tv));

	filler = packet + ICMP_HDR_LEN + sizeof(tv);
	filler_len = PACKET_LEN - (ICMP_HDR_LEN + sizeof(tv));
	i = 0;
	while (i < filler_len)
	{
		filler[i] = i;
		++i;
	}

	icmp->checksum = 0;
	icmp->checksum = checksum(packet, PACKET_LEN);
}
