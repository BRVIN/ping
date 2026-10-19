NAME	= ft_ping

CC		= cc
CFLAGS	= -Wall -Wextra -Werror -Iincludes

SRCS	= srcs/main.c \
		  srcs/resolve.c \
		  srcs/packet.c \
		  srcs/ping.c \
		  srcs/icmp_error.c \
		  srcs/loop.c \
		  srcs/stats.c \
		  srcs/checks.c
OBJS	= $(SRCS:.c=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.c includes/ft_ping.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
