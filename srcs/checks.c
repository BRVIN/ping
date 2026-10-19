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
				printf("Usage: ft_ping [OPTION...] HOST ...\nSend ICMP ECHO_REQUEST packets to network hosts.\n\n-?   give this help list\n-v   verbose output\n");
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
// no potential host -> exit
// then potential host format check regarding regarding ipv4 characters & domain name possible characters, if the potential host contain something other then alphanumeric or '-' or '.' then exit
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
		fprintf(stderr, "ft_ping: missing host operand\nTry 'ft_ping -?' for more information.\n");
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

