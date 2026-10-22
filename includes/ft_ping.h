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
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <arpa/inet.h>
# include <netdb.h>

# include <ctype.h>

# define PING_DATA_LEN	56
# define ICMP_HDR_LEN	8
# define PACKET_LEN		(ICMP_HDR_LEN + PING_DATA_LEN)

# define PING_INTERVAL_US	1000000LL

# define DUP_TABLE_SIZE	1024

typedef struct s_ping
{
	char				*host;
	char				canonname[NI_MAXHOST];
	char				ip_str[INET_ADDRSTRLEN];
	struct sockaddr_in	sockaddr;
	int					sockfd;
	unsigned short		id;

	long				transmitted;
	long				received;
	long				duplicates;
	double				rtt_min;
	double				rtt_max;
	double				rtt_sum;
	double				rtt_sumsq;
	unsigned char		dup_table[DUP_TABLE_SIZE];
	int				verbose;
}	t_ping;

extern volatile sig_atomic_t	g_stop;

int				resolve_host(t_ping *ping);

unsigned short	checksum(void *data, int len);
void			build_packet(t_ping *ping, char *buf, int seq);

int				send_ping(t_ping *ping);
void			recv_ping(t_ping *ping);

void			handle_icmp_error(t_ping *ping, char *buf, ssize_t n,
					int ip_hlen, const char *from_str);

long long		now_us(void);
int				ping_loop(t_ping *ping);

void			debug_print_packet(char *buf, ssize_t n);

void			print_stats(t_ping *ping);

void			check_input_options(int argc, char **argv, t_ping *ping);
void			get_check_host(int argc, char **argv, t_ping *ping);

#endif
