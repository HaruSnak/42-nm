#include "ft_nm.h"

/*
	Tri par insertion : stable, lisible, suffisant pour des tables
	de symboles (typiquement < 10 000 entrées).
	Ordre : identique à GNU nm — underscores ignorés en tête,
	insensible à la casse, tiebreak par strcmp brut.
*/
void	symbol_sort(t_symbol_list *list) {
	t_symbol	*key;
	int			 i;
	int			 j;

	i = 1;
	while (i < list->count) {
		key = list->array[i];
		j = i - 1;
		while (j >= 0 && ft_nm_sort_cmp(list->array[j]->name, key->name) > 0) {
			list->array[j + 1] = list->array[j];
			j--;
		}
		list->array[j + 1] = key;
		i++;
	}
}
