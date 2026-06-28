NAME = ft_nm

CC = gcc
CFLAGS = -Wall -Wextra -Werror

SRCS =	srcs/main.c				\
		srcs/ft_utils.c			\
		srcs/elf_loader.c		\
		srcs/elf_parse_header.c	\
		srcs/elf_parse_sections.c	\
		srcs/symbol_extract.c	\
		srcs/symbol_type.c		\
		srcs/symbol_sort.c		\
		srcs/symbol_print.c

OBJS = $(SRCS:.c=.o)

INCLUDES = -I includes/

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re:
	$(MAKE) fclean
	$(MAKE) all

.PHONY: all clean fclean re
