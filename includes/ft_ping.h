#ifndef FT_PING_H
# define FT_PING_H

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>
# include <signal.h>
# include <sys/select.h>       /* select, fd_set */
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
# define INTERVAL_US	1000000     /* 1 second between two requests */

typedef struct s_ping
{
	char				*host;                      /* what the user typed */
	char				ip_str[INET_ADDRSTRLEN];    /* "142.250.x.x" */
	struct sockaddr_in	addr;                       /* destination for sendto() */
	int					sockfd;                     /* raw ICMP socket */
	unsigned short		id;                         /* identifies OUR packets */

	/* main loop */
	long long			interval_us;                /* time between two sends */
	long long			next_send_us;               /* when to send the next one */

	/* statistics (transmitted is also the next icmp_seq) */
	long				transmitted;
	long				received;
	double				rtt_min;                    /* all in ms */
	double				rtt_max;
	double				rtt_sum;
	double				rtt_sumsq;                  /* sum of rtt^2, for stddev */
}	t_ping;

/* set to 1 by the SIGINT handler, read by the main loop */
extern volatile sig_atomic_t	g_stop;

/* resolve.c */
int				resolve_host(t_ping *ping);

/* packet.c */
unsigned short	checksum(void *data, int len);
void			build_packet(t_ping *ping, char *buf, int seq);

/* ping.c */
void			send_ping(t_ping *ping);
void			recv_ping(t_ping *ping);
void			ping_loop(t_ping *ping);

/* time.c */
long long		now_us(void);
long long		tv_to_us(struct timeval tv);

/* stats.c */
void			update_stats(t_ping *ping, double rtt);
int				print_stats(t_ping *ping);

#endif
