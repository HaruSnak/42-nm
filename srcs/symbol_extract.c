#include "ft_nm.h"

/*
	Retourne 1 si le symbole doit être ignoré dans la sortie nm :
	- STT_FILE  : nom du fichier source (métadonnée de débogage)
	- STT_SECTION : symbole de section (pas un vrai symbole)
	- Nom vide  : entrée nulle (index 0 de symtab) ou invalide
*/
static int	should_skip(t_u8 st_info, const char *name) {
	int	sym_type;

	sym_type = (st_info & 0xf);
	if (sym_type == STT_FILE || sym_type == STT_SECTION)
		return (1);
	if (!name || name[0] == '\0')
		return (1);
	return (0);
}

/*
	Agrandit la liste par doublement de capacité si nécessaire.
	Retourne 0 si succès, -1 si malloc échoue.
*/
static int	list_push(t_symbol_list *list, t_symbol *sym) {
	t_symbol	**new_array;
	int			  new_capacity;
	int			  i;

	if (list->count >= list->capacity) {
		new_capacity = (list->capacity == 0) ? 64 : list->capacity * 2;
		new_array = malloc(sizeof(t_symbol *) * (size_t)new_capacity);
		if (!new_array)
			return (-1);
		i = 0;
		while (i < list->count) {
			new_array[i] = list->array[i];
			i++;
		}
		free(list->array);
		list->array = new_array;
		list->capacity = new_capacity;
	}
	list->array[list->count++] = sym;
	return (0);
}

/*
	Récupère les sh_flags et sh_type de la section à l'index donné (64 bits).
	SHN_LORESERVE = 0xff00 : au-dessus, les indices sont des valeurs spéciales.

	L'adresse de l'entrée est calculée avec ctx->shentsize comme pas réel
	(et non sizeof(Elf64_Shdr)) : voir le commentaire de
	elf_shentsize_matches_reference() dans elf_parse_header.c pour la raison
	(indexer un tableau Elf64_Shdr* suppose à tort que shentsize ==
	sizeof(Elf64_Shdr), ce qui peut faire lire hors de la zone mmap'ée).
*/
static void	get_section_info_64(const t_elf_ctx *ctx, t_u16 shndx, t_u64 *sh_flags, t_u32 *sh_type) {
	const Elf64_Shdr	*section_header;

	*sh_flags = 0;
	*sh_type = 0;
	if (shndx >= SHN_LORESERVE || shndx >= ctx->shnum)
		return ;
	section_header = (const Elf64_Shdr *)((const char *)ctx->mapped_data
			+ ctx->shoff + (size_t)shndx * (size_t)ctx->shentsize);
	*sh_flags = section_header->sh_flags;
	*sh_type = section_header->sh_type;
}

// Idem pour 32 bits.
static void	get_section_info_32(const t_elf_ctx *ctx, t_u16 shndx, t_u64 *sh_flags, t_u32 *sh_type) {
	const Elf32_Shdr	*section_header;

	*sh_flags = 0;
	*sh_type = 0;
	if (shndx >= SHN_LORESERVE || shndx >= ctx->shnum)
		return ;
	section_header = (const Elf32_Shdr *)((const char *)ctx->mapped_data
			+ ctx->shoff + (size_t)shndx * (size_t)ctx->shentsize);
	*sh_flags = (t_u64)section_header->sh_flags;
	*sh_type = section_header->sh_type;
}

// Parcourt la symtab 64 bits et crée un t_symbol pour chaque entrée valide.
static int	extract_symbols_64(t_elf_ctx *ctx, size_t symtab_off, size_t symtab_size,
		size_t strtab_off, size_t strtab_size, t_symbol_list *list) {
	const Elf64_Sym	*sym;
	const char		*strtab;
	const char		*name;
	t_symbol		*symbol;
	t_u64			 sh_flags;
	t_u32			 sh_type;
	size_t			 sym_count;
	size_t			 i;

	sym_count = symtab_size / sizeof(Elf64_Sym);
	strtab = (const char *)ctx->mapped_data + strtab_off;
	i = 1;
	while (i < sym_count) {
		if (!bounds_ok(ctx, symtab_off + i * sizeof(Elf64_Sym),
				sizeof(Elf64_Sym)))
			break ;
		sym = (const Elf64_Sym *)((const char *)ctx->mapped_data
				+ symtab_off) + i;
		if ((size_t)sym->st_name >= strtab_size) {
			i++;
			continue ;
		}
		name = strtab + sym->st_name;
		if (should_skip(sym->st_info, name)) {
			i++;
			continue ;
		}
		get_section_info_64(ctx, sym->st_shndx, &sh_flags, &sh_type);
		symbol = malloc(sizeof(t_symbol));
		if (!symbol)
			return (-1);
		symbol->name = name;
		symbol->name_max_len = strtab_size - (size_t)sym->st_name;
		symbol->value = sym->st_value;
		symbol->is_undef = (sym->st_shndx == SHN_UNDEF);
		symbol->type = symbol_get_type(sym->st_shndx, sym->st_info,
				sh_flags, sh_type);
		if (list_push(list, symbol) < 0) {
			free(symbol);
			return (-1);
		}
		i++;
	}
	return (0);
}

// Parcourt la symtab 32 bits et crée un t_symbol pour chaque entrée valide.
static int	extract_symbols_32(t_elf_ctx *ctx, size_t symtab_off, size_t symtab_size,
		size_t strtab_off, size_t strtab_size, t_symbol_list *list) {
	const Elf32_Sym	*sym;
	const char		*strtab;
	const char		*name;
	t_symbol		*symbol;
	t_u64			 sh_flags;
	t_u32			 sh_type;
	size_t			 sym_count;
	size_t			 i;

	sym_count = symtab_size / sizeof(Elf32_Sym);
	strtab = (const char *)ctx->mapped_data + strtab_off;
	i = 1;
	while (i < sym_count) {
		if (!bounds_ok(ctx, symtab_off + i * sizeof(Elf32_Sym), sizeof(Elf32_Sym)))
			break ;
		sym = (const Elf32_Sym *)((const char *)ctx->mapped_data
				+ symtab_off) + i;
		if ((size_t)sym->st_name >= strtab_size) {
			i++;
			continue ;
		}
		name = strtab + sym->st_name;
		if (should_skip(sym->st_info, name)) {
			i++;
			continue ;
		}
		get_section_info_32(ctx, sym->st_shndx, &sh_flags, &sh_type);
		symbol = malloc(sizeof(t_symbol));
		if (!symbol)
			return (-1);
		symbol->name = name;
		symbol->name_max_len = strtab_size - (size_t)sym->st_name;
		symbol->value = (t_u64)sym->st_value;
		symbol->is_undef = (sym->st_shndx == SHN_UNDEF);
		symbol->type = symbol_get_type(sym->st_shndx, sym->st_info,
				sh_flags, sh_type);
		if (list_push(list, symbol) < 0) {
			free(symbol);
			return (-1);
		}
		i++;
	}
	return (0);
}

int	symbol_extract_all(t_elf_ctx *ctx, size_t symtab_off, size_t symtab_size,
		size_t strtab_off, size_t strtab_size, t_symbol_list *list) {
	list->array = NULL;
	list->count = 0;
	list->capacity = 0;
	if (ctx->is_64bit)
		return (extract_symbols_64(ctx, symtab_off, symtab_size,
				strtab_off, strtab_size, list));
	return (extract_symbols_32(ctx, symtab_off, symtab_size,
			strtab_off, strtab_size, list));
}

/*
	Libère chaque t_symbol alloué, puis le tableau de pointeurs.
	Les champs .name pointent dans la zone mmap : ne pas les libérer.
*/
void	symbol_list_free(t_symbol_list *list) {
	int	i;

	i = 0;
	while (i < list->count) {
		free(list->array[i]);
		i++;
	}
	free(list->array);
	list->array = NULL;
	list->count = 0;
	list->capacity = 0;
}
