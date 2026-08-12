#include "ft_nm.h"

/*
	Fusionne deux plages triées et contiguës de `array`
	([range_start, range_middle] et [range_middle + 1, range_end]) en
	utilisant `merge_buffer` comme zone de travail, puis recopie le
	résultat trié dans `array`.
*/
static void	merge_sorted_ranges(t_symbol **array, t_symbol **merge_buffer,
		int range_start, int range_middle, int range_end) {
	int	left_index;
	int	right_index;
	int	out_index;

	left_index = range_start;
	right_index = range_middle + 1;
	out_index = range_start;
	while (left_index <= range_middle && right_index <= range_end) {
		if (ft_nm_sort_cmp(array[left_index]->name, array[right_index]->name) <= 0)
			merge_buffer[out_index++] = array[left_index++];
		else
			merge_buffer[out_index++] = array[right_index++];
	}
	while (left_index <= range_middle)
		merge_buffer[out_index++] = array[left_index++];
	while (right_index <= range_end)
		merge_buffer[out_index++] = array[right_index++];
	out_index = range_start;
	while (out_index <= range_end) {
		array[out_index] = merge_buffer[out_index];
		out_index++;
	}
}

/*
	Tri fusion récursif sur [range_start, range_end] (bornes incluses).
	O(n log n) dans tous les cas, contrairement à un tri par insertion qui
	dégénère en O(n^2) sur de gros binaires réels (ex : un noyau non
	strippé avec plusieurs centaines de milliers de symboles).
*/
static void	symbol_merge_sort(t_symbol **array, t_symbol **merge_buffer,
		int range_start, int range_end) {
	int	range_middle;

	if (range_start >= range_end)
		return ;
	range_middle = range_start + (range_end - range_start) / 2;
	symbol_merge_sort(array, merge_buffer, range_start, range_middle);
	symbol_merge_sort(array, merge_buffer, range_middle + 1, range_end);
	merge_sorted_ranges(array, merge_buffer, range_start, range_middle, range_end);
}

/*
	Trie list->array par nom de symbole, ordre identique à GNU nm
	(strcmp brut, voir ft_nm_sort_cmp).
	Si l'allocation du buffer de fusion échoue (mémoire exceptionnellement
	épuisée), la liste est laissée non triée plutôt que de faire planter
	le programme : nm doit continuer à afficher des symboles corrects,
	même dans un ordre dégradé, dans ce cas extrême.
*/
void	symbol_sort(t_symbol_list *list) {
	t_symbol	**merge_buffer;

	if (list->count < 2)
		return ;
	merge_buffer = malloc(sizeof(t_symbol *) * (size_t)list->count);
	if (!merge_buffer)
		return ;
	symbol_merge_sort(list->array, merge_buffer, 0, list->count - 1);
	free(merge_buffer);
}
