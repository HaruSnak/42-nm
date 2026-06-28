#include "ft_nm.h"

// ─── String helpers ───────────────────────────────────────────

size_t	ft_strlen(const char *s) {
	size_t	len;

	len = 0;
	while (s[len])
		len++;
	return (len);
}

int	ft_strcmp(const char *a, const char *b) {
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return ((unsigned char)*a - (unsigned char)*b);
}

/*
	Comparaison utilisée par le tri.

	GNU nm (binutils nm.c, fonction non_numeric_forward) trie avec strcmp
	brut — pas de démanglage, pas d'ignorance des underscores, sensible
	à la casse. On reproduit ce comportement avec notre ft_strcmp.

	Conséquence : '_' (ASCII 95) < lettres minuscules (a=97), donc les
	symboles "__xxx" apparaissent avant les symboles commençant par des
	lettres, ce qui correspond à la sortie nm en locale C (LC_ALL=C).
*/
int	ft_nm_sort_cmp(const char *name_a, const char *name_b) {
	return (ft_strcmp(name_a, name_b));
}

// ─── Sortie caractère / string ───────────────────────────────

void	ft_putchar_fd(int fd, char c) {
	write(fd, &c, 1);
}

void	ft_putstr_fd(int fd, const char *s) {
	if (!s)
		return ;
	write(fd, s, ft_strlen(s));
}

/*
	Affiche jusqu'à max_len octets ou jusqu'au premier '\0'.
	Protège contre les strtab non terminées par '\0'.
*/
void	ft_putstr_bounded_fd(int fd, const char *s, size_t max_len) {
	size_t	len;

	len = 0;
	while (len < max_len && s[len] != '\0')
		len++;
	write(fd, s, len);
}

/*
	Affiche `value` en hexadécimal sur exactement `width` chiffres
	(padding de zéros à gauche).
*/
void	ft_puthex_fd(int fd, t_u64 value, int width) {
	static const char	hex_digits[] = "0123456789abcdef";
	char				buf[16];
	int					i;

	i = width;
	while (i > 0) {
		buf[--i] = hex_digits[value & 0xf];
		value >>= 4;
	}
	write(fd, buf, (size_t)width);
}

// ─── Messages d'erreur ────────────────────────────────────────

void	ft_error(const char *filename, const char *msg) {
	ft_putstr_fd(2, "ft_nm: ");
	ft_putstr_fd(2, filename);
	ft_putstr_fd(2, ": ");
	ft_putstr_fd(2, msg);
	ft_putchar_fd(2, '\n');
}

/*
	Equivalent de perror("ft_nm: filename") sans pouvoir utiliser sprintf.
	Construit le préfixe dans un buffer statique de taille raisonnable.
*/
void	ft_perror_file(const char *filename) {
	char		buf[4096];
	size_t		i;
	size_t		j;
	const char	*prefix;
	const char	*err_msg;

	prefix = "ft_nm: ";
	i = 0;
	j = 0;
	while (prefix[j] && i < sizeof(buf) - 1)
		buf[i++] = prefix[j++];
	j = 0;
	while (filename[j] && i < sizeof(buf) - 1)
		buf[i++] = filename[j++];
	buf[i] = '\0';
	err_msg = strerror(errno);
	ft_putstr_fd(2, buf);
	ft_putstr_fd(2, ": ");
	ft_putstr_fd(2, err_msg);
	ft_putchar_fd(2, '\n');
}
