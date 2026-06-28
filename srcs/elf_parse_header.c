#include "ft_nm.h"

/*
	Lit l'e_ident pour déterminer la classe (32/64) et l'endianness.
	Puis extrait depuis le ELF header les champs nécessaires à l'analyse
	de la section header table : e_shoff, e_shentsize, e_shnum, e_shstrndx.
	Retourne 0 si succès, -1 si structure invalide ou non supportée.
*/
int	elf_parse_header(t_elf_ctx *ctx, const char *filename) {
	const unsigned char	*ident;
	const Elf64_Ehdr	*ehdr64;
	const Elf32_Ehdr	*ehdr32;

	ident = (const unsigned char *)ctx->mapped_data;
	// Classe : 32 ou 64 bits
	if (ident[EI_CLASS] == ELFCLASS64)
		ctx->is_64bit = 1;
	else if (ident[EI_CLASS] == ELFCLASS32)
		ctx->is_64bit = 0;
	else {
		ft_error(filename, "unsupported ELF class");
		return (-1);
	}
	// Endianness
	if (ident[EI_DATA] == ELFDATA2LSB)
		ctx->is_big_endian = 0;
	else if (ident[EI_DATA] == ELFDATA2MSB)
		ctx->is_big_endian = 1;
	else {
		ft_error(filename, "unsupported ELF data encoding");
		return (-1);
	}
	if (ctx->is_64bit) {
		if (!bounds_ok(ctx, 0, sizeof(Elf64_Ehdr))) {
			ft_error(filename, "file truncated");
			return (-1);
		}
		ehdr64 = (const Elf64_Ehdr *)ctx->mapped_data;
		ctx->shoff = (t_u64)ehdr64->e_shoff;
		ctx->shentsize = ehdr64->e_shentsize;
		ctx->shnum = ehdr64->e_shnum;
		ctx->shstrndx = ehdr64->e_shstrndx;
	}
	else {
		if (!bounds_ok(ctx, 0, sizeof(Elf32_Ehdr))) {
			ft_error(filename, "file truncated");
			return (-1);
		}
		ehdr32 = (const Elf32_Ehdr *)ctx->mapped_data;
		ctx->shoff = (t_u64)ehdr32->e_shoff;
		ctx->shentsize = ehdr32->e_shentsize;
		ctx->shnum = ehdr32->e_shnum;
		ctx->shstrndx = ehdr32->e_shstrndx;
	}
	// Valide que la section header table entière est dans le fichier
	if (ctx->shnum > 0 && ctx->shentsize > 0) {
		if (!bounds_ok(ctx, (size_t)ctx->shoff, (size_t)ctx->shnum * (size_t)ctx->shentsize)) {
			ft_error(filename, "section header table out of bounds");
			return (-1);
		}
	}
	return (0);
}
