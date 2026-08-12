#include "ft_nm.h"

/*
	Calcule l'adresse de l'entrée `section_index` de la section header
	table 64 bits.

	Important : on n'indexe PAS un tableau `Elf64_Shdr *` (ce qui figerait
	le pas à sizeof(Elf64_Shdr)). On avance de ctx->shentsize octets par
	entrée, la seule valeur dont on sache — après elf_parse_header — qu'elle
	correspond au pas réel utilisé par le fichier et qu'elle est assez
	grande pour contenir une Elf64_Shdr complète.
*/
static const Elf64_Shdr	*section_header_at_index_64(const t_elf_ctx *ctx,
		t_u16 section_index) {
	const char	*section_header_table_base;

	section_header_table_base = (const char *)ctx->mapped_data + ctx->shoff;
	return ((const Elf64_Shdr *)(section_header_table_base
			+ (size_t)section_index * (size_t)ctx->shentsize));
}

// Idem pour les fichiers ELF 32 bits.
static const Elf32_Shdr	*section_header_at_index_32(const t_elf_ctx *ctx,
		t_u16 section_index) {
	const char	*section_header_table_base;

	section_header_table_base = (const char *)ctx->mapped_data + ctx->shoff;
	return ((const Elf32_Shdr *)(section_header_table_base
			+ (size_t)section_index * (size_t)ctx->shentsize));
}

/*
 Parcourt la section header table 64 bits pour trouver la première
 section de type SHT_SYMTAB. Le champ sh_link pointe vers la strtab
 associée. Retourne 0 si trouvée, 1 si absente, -1 si erreur.
*/
static int	find_symtab_64(t_elf_ctx *ctx, size_t *symtab_off, size_t *symtab_size, size_t *strtab_off, size_t *strtab_size) {
	const Elf64_Shdr	*current_shdr;
	const Elf64_Shdr	*strtab_shdr;
	t_u16				 section_index;

	section_index = 0;
	while (section_index < ctx->shnum) {
		current_shdr = section_header_at_index_64(ctx, section_index);
		if (current_shdr->sh_type == SHT_SYMTAB) {
			if (!bounds_ok(ctx, (size_t)current_shdr->sh_offset, (size_t)current_shdr->sh_size))
				return (-1);
			*symtab_off = (size_t)current_shdr->sh_offset;
			*symtab_size = (size_t)current_shdr->sh_size;
			/* sh_link = index de la strtab associée */
			if (current_shdr->sh_link >= ctx->shnum)
				return (-1);
			strtab_shdr = section_header_at_index_64(ctx, current_shdr->sh_link);
			if (!bounds_ok(ctx, (size_t)strtab_shdr->sh_offset,
					(size_t)strtab_shdr->sh_size))
				return (-1);
			*strtab_off = (size_t)strtab_shdr->sh_offset;
			*strtab_size = (size_t)strtab_shdr->sh_size;
			return (0);
		}
		section_index++;
	}
	return (1);
}


// Idem pour les fichiers ELF 32 bits.
static int	find_symtab_32(t_elf_ctx *ctx, size_t *symtab_off, size_t *symtab_size, size_t *strtab_off, size_t *strtab_size) {
	const Elf32_Shdr	*current_shdr;
	const Elf32_Shdr	*strtab_shdr;
	t_u16				 section_index;

	section_index = 0;
	while (section_index < ctx->shnum) {
		current_shdr = section_header_at_index_32(ctx, section_index);
		if (current_shdr->sh_type == SHT_SYMTAB) {
			if (!bounds_ok(ctx, (size_t)current_shdr->sh_offset, (size_t)current_shdr->sh_size))
				return (-1);
			*symtab_off = (size_t)current_shdr->sh_offset;
			*symtab_size = (size_t)current_shdr->sh_size;
			if (current_shdr->sh_link >= ctx->shnum)
				return (-1);
			strtab_shdr = section_header_at_index_32(ctx, current_shdr->sh_link);
			if (!bounds_ok(ctx, (size_t)strtab_shdr->sh_offset,
					(size_t)strtab_shdr->sh_size))
				return (-1);
			*strtab_off = (size_t)strtab_shdr->sh_offset;
			*strtab_size = (size_t)strtab_shdr->sh_size;
			return (0);
		}
		section_index++;
	}
	return (1);
}

int	elf_find_symtab(t_elf_ctx *ctx, size_t *symtab_off, size_t *symtab_size, size_t *strtab_off, size_t *strtab_size) {
	if (ctx->shnum == 0 || ctx->shentsize == 0)
		return (1);
	if (ctx->is_64bit)
		return (find_symtab_64(ctx, symtab_off, symtab_size, strtab_off, strtab_size));
	return (find_symtab_32(ctx, symtab_off, symtab_size, strtab_off, strtab_size));
}
