#ifndef FT_PING_H
# define FT_PING_H

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <netinet/in.h>
# include <netinet/ip.h>       /* struct iphdr   : the IPv4 header */
# include <netinet/ip_icmp.h>  /* struct icmphdr : the ICMP header, ICMP_ECHO... */
# include <arpa/inet.h>        /* inet_ntop, htons, ntohs */
# include <netdb.h>            /* getaddrinfo */

/*
** inetutils ping sends 56 bytes of data by default.
** 56 data + 8 ICMP header = 64 bytes ("64 bytes from ...").
*/
# define PING_DATA_LEN	56
# define ICMP_HDR_LEN	8
# define PACKET_LEN		(ICMP_HDR_LEN + PING_DATA_LEN)

typedef struct s_ping
{
	char				*host;                      /* what the user typed */
	char				ip_str[INET_ADDRSTRLEN];    /* "142.250.x.x" */
	struct sockaddr_in	addr;                       /* destination for sendto() */
	int					sockfd;                     /* raw ICMP socket */
	unsigned short		id;                         /* identifies OUR packets */
}	t_ping;

/* resolve.c */
int				resolve_host(t_ping *ping);

/* packet.c */
unsigned short	checksum(void *data, int len);
void			build_packet(t_ping *ping, char *buf, int seq);

/* ping.c */
void			send_ping(t_ping *ping, int seq);
void			recv_ping(t_ping *ping);

#endif
