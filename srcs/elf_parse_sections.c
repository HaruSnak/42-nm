#include "ft_nm.h"

/*
 Parcourt la section header table 64 bits pour trouver la première
 section de type SHT_SYMTAB. Le champ sh_link pointe vers la strtab
 associée. Retourne 0 si trouvée, 1 si absente, -1 si erreur.
*/
static int	find_symtab_64(t_elf_ctx *ctx, size_t *symtab_off, size_t *symtab_size, size_t *strtab_off, size_t *strtab_size) {
	const Elf64_Shdr	*shdr_table;
	const Elf64_Shdr	*shdr;
	const Elf64_Shdr	*strtab_shdr;
	t_u16				 i;

	shdr_table = (const Elf64_Shdr *)((const char *)ctx->mapped_data
			+ ctx->shoff);
	i = 0;
	while (i < ctx->shnum) {
		shdr = &shdr_table[i];
		if (shdr->sh_type == SHT_SYMTAB) {
			if (!bounds_ok(ctx, (size_t)shdr->sh_offset, (size_t)shdr->sh_size))
				return (-1);
			*symtab_off = (size_t)shdr->sh_offset;
			*symtab_size = (size_t)shdr->sh_size;
			/* sh_link = index de la strtab associée */
			if (shdr->sh_link >= ctx->shnum)
				return (-1);
			strtab_shdr = &shdr_table[shdr->sh_link];
			if (!bounds_ok(ctx, (size_t)strtab_shdr->sh_offset,
					(size_t)strtab_shdr->sh_size))
				return (-1);
			*strtab_off = (size_t)strtab_shdr->sh_offset;
			*strtab_size = (size_t)strtab_shdr->sh_size;
			return (0);
		}
		i++;
	}
	return (1);
}


// Idem pour les fichiers ELF 32 bits.
static int	find_symtab_32(t_elf_ctx *ctx, size_t *symtab_off, size_t *symtab_size, size_t *strtab_off, size_t *strtab_size) {
	const Elf32_Shdr	*shdr_table;
	const Elf32_Shdr	*shdr;
	const Elf32_Shdr	*strtab_shdr;
	t_u16				 i;

	shdr_table = (const Elf32_Shdr *)((const char *)ctx->mapped_data
			+ ctx->shoff);
	i = 0;
	while (i < ctx->shnum) {
		shdr = &shdr_table[i];
		if (shdr->sh_type == SHT_SYMTAB) {
			if (!bounds_ok(ctx, (size_t)shdr->sh_offset, (size_t)shdr->sh_size))
				return (-1);
			*symtab_off = (size_t)shdr->sh_offset;
			*symtab_size = (size_t)shdr->sh_size;
			if (shdr->sh_link >= ctx->shnum)
				return (-1);
			strtab_shdr = &shdr_table[shdr->sh_link];
			if (!bounds_ok(ctx, (size_t)strtab_shdr->sh_offset,
					(size_t)strtab_shdr->sh_size))
				return (-1);
			*strtab_off = (size_t)strtab_shdr->sh_offset;
			*strtab_size = (size_t)strtab_shdr->sh_size;
			return (0);
		}
		i++;
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
