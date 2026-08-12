#include "ft_nm.h"

// Comme le vrai nm, invoqué sans argument on lit "a.out" dans le répertoire
// courant (voir `man nm`), plutôt qu'un simple message d'usage.
# define FT_NM_DEFAULT_FILENAME "a.out"

/*
	Traite un seul fichier ELF : chargement, validation, parsing, extraction,
	tri et affichage. Le header de fichier (en mode multi) est géré par main.
	Retourne 0 si tout s'est bien passé, 1 sinon.
	Les symboles sont libérés AVANT munmap (les noms pointent dans la zone mmappée).
*/
static int	process_file(const char *filename)
{
	t_elf_ctx		ctx;
	t_symbol_list	list;
	size_t			symtab_off;
	size_t			symtab_size;
	size_t			strtab_off;
	size_t			strtab_size;
	int				ret;

	if (elf_load(filename, &ctx) < 0)
		return (1);
	if (elf_validate_magic(&ctx, filename) < 0) {
		elf_unload(&ctx);
		return (1);
	}
	if (elf_parse_header(&ctx, filename) < 0) {
		elf_unload(&ctx);
		return (1);
	}
	ret = elf_find_symtab(&ctx, &symtab_off, &symtab_size,
			&strtab_off, &strtab_size);
	if (ret != 0) {
		if (ret < 0)
			ft_error(filename, "invalid symbol table");
		else
			ft_error(filename, "no symbols");
		elf_unload(&ctx);
		return (1);
	}
	if (symbol_extract_all(&ctx, symtab_off, symtab_size, strtab_off, strtab_size, &list) < 0) {
		symbol_list_free(&list);
		elf_unload(&ctx);
		return (1);
	}
	if (list.count == 0) {
		symbol_list_free(&list);
		elf_unload(&ctx);
		ft_error(filename, "no symbols");
		return (1);
	}
	symbol_sort(&list);
	symbol_print_all(&list, ctx.is_64bit);
	symbol_list_free(&list);
	elf_unload(&ctx);
	return (0);
}

int	main(int argc, char **argv)
{
	int	exit_code;
	int	multi_file;
	int	file_index;

	if (argc < 2)
		return (process_file(FT_NM_DEFAULT_FILENAME));
	exit_code = 0;
	multi_file = (argc > 2);
	file_index = 1;
	while (file_index < argc) {
		if (multi_file) {
			/*
				GNU nm affiche une ligne vide avant chaque header de fichier,
				y compris le premier. Le header est toujours affiché même si
				le fichier n'a pas de symboles.
			*/
			ft_putchar_fd(1, '\n');
			ft_putstr_fd(1, argv[file_index]);
			ft_putstr_fd(1, ":\n");
		}
		exit_code |= process_file(argv[file_index]);
		file_index++;
	}
	return (exit_code);
}
