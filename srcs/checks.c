#include "../includes/ft_ping.h"

void check_input_options(int argc, char **argv, t_ping *ping) {
	char *str;

	for (int i = 1; i < argc; ++i)
	{
      		str = argv[i];

		if(str[0] == '-')
		{
			if (!str[1] || str[2] || (str[1] != '?' && str[1] != 'v') ) {
				fprintf(stderr, "ft_ping: invalid option -- '%s'\nTry 'ft_ping -?' for more information.\n", str + 1);
				exit(64);
			}

      			if (str[1] == '?') {
				printf("Usage: ft_ping [OPTION...] HOST ...\nSend ICMP ECHO_REQUEST packets to network hosts.\n\n-?   give this help list\n-v   verbose output\n");
				exit(0);
			}
			else if (str[1] == 'v') {
				ping->verbose = 1;
			}
			else
				exit(64);
		}
	}
	return;
}

void	get_check_host(int argc, char **argv, t_ping *ping)
{
	char *str = NULL;
	int i = 1;

	while (i < argc) {
		if(argv[i][0] && argv[i][0] != '-') {
			ping->host = argv[i];
			str = argv[i];
			break;
		}
		++i;
	}

	if (!str) {
		fprintf(stderr, "ft_ping: missing host operand\nTry 'ft_ping -?' for more information.\n");
		exit(64);
	}

	i = 0;

	while (str[i])
	{
		if(!isalnum((unsigned char)str[i]) && str[i] != '-' && str[i] != '.')
		{
			fprintf(stderr, "ft_ping: unknown host\n");
			exit(1);
		}
		++i;
	}

	return;
}
