#include "../includes/ft_ping.h"

/*
** Internet checksum (RFC 1071):
**  - add the data as a sequence of 16-bit words,
**  - fold the carries (bits above 16) back into the low 16 bits,
**  - return the one's complement (~).
** The receiver does the same sum over the whole packet (checksum included)
** and must get 0xFFFF; that is how corruption is detected.
*/

// to deepen
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
	if (len == 1)                       /* odd length: last lonely byte */
		sum += *(unsigned char *)p;
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ((unsigned short)~sum);
}

/*
** Packet layout (64 bytes):
**
**   0       1       2               4
**   +-------+-------+---------------+
**   | type  | code  |   checksum    |   <- struct icmphdr (8 bytes)
**   +-------+-------+---------------+
**   |  identifier   |   sequence    |
**   +---------------+---------------+
**   | struct timeval (send time)    |   <- data (56 bytes)
**   | ... filler bytes ...          |
**   +-------------------------------+
**
** Storing the send time inside the packet means the reply carries it back
** to us (the target echoes the data unchanged): RTT = now - that time.
*/


// RFC792 define ICMP header
// check obs ICMP file
// We fill "packet" with the data of the icmp header
// we use id to identify our ping and seq to differentiate the packets we send
// internet protocols often use big endian byte order as defined for ICMP in the RFC, and most modern cpu use little endian
// that s why the variables above 1 byte (id and sequence) need htons convertion (not the checksum for some magic shit)

void	build_packet(t_ping *ping, char *packet, int seq)
{
	struct icmphdr	*icmp;
	struct timeval	tv;
	int				i;

	memset(packet, 0, PACKET_LEN);
	icmp = (struct icmphdr *)packet;
	icmp->type = ICMP_ECHO;         /* 8 = echo request */
	icmp->code = 0;
	/* Network byte order (big endian) for multi-byte fields: htons */
	icmp->un.echo.id = htons(ping->id);
	icmp->un.echo.sequence = htons(seq);

	/* Data part: timestamp first, then a recognisable pattern */
	gettimeofday(&tv, NULL);
	memcpy(packet + ICMP_HDR_LEN, &tv, sizeof(tv));
	i = ICMP_HDR_LEN + sizeof(tv);
	// Filling the rest of data part with just an incremental number sequence ... :
	while (i < PACKET_LEN)
	{
		packet[i] = i;
		i++;
	}

	/* Checksum is computed LAST, with the checksum field still at 0 */
	icmp->checksum = 0;
	icmp->checksum = checksum(packet, PACKET_LEN);
}
