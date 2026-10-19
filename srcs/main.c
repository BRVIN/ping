#include "../includes/ft_ping.h"

// any argv beginning with "-" is considered an option, then it can be only :
// [-v] without additional character
// [-?] without any additional character
void check_input_options(int argc, char **argv, t_ping *ping) {

	char *str;

	for (int i = 1; i < argc; ++i)
	{
      		str = argv[i];

		if(str[0] == '-')
		{
			if (!str[1] || str[2] || (str[1] != '?' && str[1] != 'v') ) {
				fprintf(stderr, "ft_ping: invalid option -- '%s'\nTry '-?' for more information.\n", str + 1);	
				exit(64);
			}

      			if (str[1] == '?') {
				printf("Usage: ping [OPTION...] HOST ...\nSend ICMP ECHO_REQUEST packets to network hosts.\n\n-?   give this help list\n-v   verbose output\n");
				exit(0);
			}
			else if (str[1] == 'v') {
				//printf("DEBUG verbose enable\n");
				ping->verbose = 1;
			}
			else //should nt happen
				exit(64);
		}
	}
	return;
}

// extract host and little format check
// we ignore '' arguments
// any argument that's not considered as an option will be the one treated as the potential host, any following potential host will be ignored
// no potential host -> exit(1)
// then potential host format check regarding regarding ipv4 characters & domain name possible characters, if the potential host contain something other then alphanumeric or '-' or '.' then exit(1)
void	get_check_host(int argc, char **argv, t_ping *ping)
{
	char *str = NULL;
	int i = 1;

	while (i < argc) {
		//if not empty string and not potential option (then taken as the only potential host)
		if(argv[i][0] && argv[i][0] != '-') {
			ping->host = argv[i];
			str = argv[i];
			break;
		}
		++i;
	}

	// no potential host found
	if (!str) {
		fprintf(stderr, "ft_ping: missing host operand\nTry 'ping -?' for more information.\n");
		exit(64);
	}

	i = 0;

	//checking format of potential host, has to be ipv4 or domain name character only
	while (str[i])
	{
		if(!isalnum(str[i]) && str[i] != '-' && str[i] != '.')
		{
			fprintf(stderr, "ft_ping: unknown host.\n");
			exit(1);
		}
		++i;
	}

	return;
}

int	main(int argc, char **argv)
{
	t_ping	ping;

	//setvbuf: to have output always print directly, it's like doing fflush(stdout) after every print
	setvbuf(stdout, NULL, _IOLBF, 0);

	if (argc == 1) {
		fprintf(stderr, "ft_ping: missing host operand\nTry 'ping -?' for more information.\n");
		return (64);
	}

	memset(&ping, 0, sizeof(ping));

	//check permission first ?
	check_input_options(argc, argv, &ping);
	get_check_host(argc, argv, &ping);

	// ICMP "identifier" to recognise our replies (ping replies go for any app listening)
	ping.id = getpid() & 0xFFFF;

	/* 1. hostname / IP string -> struct sockaddr_in */
	if (resolve_host(&ping) != 0)
		return (1);

	/*
	** 2. Raw socket: we write the ICMP header ourselves, the kernel adds
	**    the IP header. Needs root (or CAP_NET_RAW) -> run with sudo.
	*/
	ping.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ping.sockfd < 0)
	{
		perror("socket");
		return (1);
	}

	printf("PING %s (%s): %d data bytes\n", ping.canonname, ping.ip_str, PING_DATA_LEN);

	/* 3. Send / receive until Ctrl+C (a send failure stops without stats) */
	if (ping_loop(&ping) < 0)
	{
		close(ping.sockfd);
		return (1);
	}
	print_stats(&ping);
	close(ping.sockfd);

	/* Like inetutils: success only if at least one reply came back */
	if (ping.received == 0)
		return (1);
	return (0);
}
