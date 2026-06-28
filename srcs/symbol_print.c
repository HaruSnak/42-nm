#include "ft_nm.h"

/*
	Affiche l'en-tête d'adresse d'un symbole.
	Si is_undef : on affiche des espaces (symbole sans adresse résolue).
	Sinon : adresse en hex, paddée sur 16 chiffres (64 bits) ou 8 (32 bits).
*/
static void	print_address(const t_symbol *sym, int is_64bit) {
	int	addr_width;
	int	i;

	addr_width = is_64bit ? 16 : 8;
	if (sym->is_undef) {
		i = 0;
		while (i < addr_width) {
			ft_putchar_fd(1, ' ');
			i++;
		}
	}
	else
		ft_puthex_fd(1, sym->value, addr_width);
}

/*
	Affiche tous les symboles de la liste au format nm :
	<adresse_ou_espaces> <lettre_type> <nom>\n
*/
void	symbol_print_all(const t_symbol_list *list, int is_64bit) {
	const t_symbol	*sym;
	int				 i;

	i = 0;
	while (i < list->count) {
		sym = list->array[i];
		print_address(sym, is_64bit);
		ft_putchar_fd(1, ' ');
		ft_putchar_fd(1, sym->type);
		ft_putchar_fd(1, ' ');
		ft_putstr_bounded_fd(1, sym->name, sym->name_max_len);
		ft_putchar_fd(1, '\n');
		i++;
	}
}
