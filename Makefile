NAME	= ft_ping

CC		= cc
CFLAGS	= -Wall -Wextra -Werror -Wpedantic -std=gnu99 -Iincludes -O1

SRCS	= srcs/main.c \
		  srcs/resolve.c \
		  srcs/packet.c \
		  srcs/ping.c \
		  srcs/icmp_error.c \
		  srcs/loop.c \
		  srcs/stats.c \
		  srcs/checks.c \
		  srcs/debug.c
OBJS	= $(SRCS:.c=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(OBJS) -o $(NAME)

%.o: %.c includes/ft_ping.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -vf $(OBJS)

fclean: clean
	@rm -vf $(NAME)

re: fclean all

.PHONY: all clean fclean re
