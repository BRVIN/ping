#ifndef FT_PING_H
# define FT_PING_H

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>
# include <signal.h>
# include <sys/select.h>
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

/* All times are kept as long long microseconds */
# define PING_INTERVAL_US	1000000LL

/* Duplicate detection: one cell per seq, modulo DUP_TABLE_SIZE */
# define DUP_TABLE_SIZE	1024

typedef struct s_ping
{
	char				*host;                      /* what the user typed */
	char				canonname[NI_MAXHOST];      /* official name after aliases, for display */
	char				ip_str[INET_ADDRSTRLEN];    /* "142.250.x.x" */
	struct sockaddr_in	addr;                       /* destination for sendto() */
	int					sockfd;                     /* raw ICMP socket */
	unsigned short		id;                         /* identifies OUR packets */

	int					seq;                        /* seq of the next packet to send */
	long				transmitted;                /* packets sent successfully */
	long				received;                   /* echo replies (duplicates excluded) */
	long				duplicates;                 /* replies whose seq was already seen */
	double				rtt_min;                    /* in ms */
	double				rtt_max;
	double				rtt_sum;
	double				rtt_sumsq;                  /* sum of squares, for stddev */
	unsigned char		dup_table[DUP_TABLE_SIZE];
}	t_ping;

/* Set by the SIGINT handler, checked by the main loop */
extern volatile sig_atomic_t	g_stop;

/* resolve.c */
int				resolve_host(t_ping *ping);

/* packet.c */
unsigned short	checksum(void *data, int len);
void			build_packet(t_ping *ping, char *buf, int seq);

/* ping.c */
int				send_ping(t_ping *ping);
void			recv_ping(t_ping *ping);

/* icmp_error.c */
void			handle_icmp_error(t_ping *ping, char *buf, ssize_t n,
					int ip_hlen, struct sockaddr_in *from);

/* loop.c */
long long		now_us(void);
int				ping_loop(t_ping *ping);

/* stats.c */
void			print_stats(t_ping *ping);

#endif
