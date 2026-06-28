#include "ft_nm.h"

/*
	Ouvre le fichier, récupère sa taille avec fstat, puis mappe
	son contenu en lecture seule avec mmap.
	Le fd est fermé immédiatement après mmap (le mapping reste valide).
	Retourne 0 si succès, -1 si erreur.
*/
int	elf_load(const char *filename, t_elf_ctx *ctx) {
	int			fd;
	struct stat	st;

	ctx->mapped_data = MAP_FAILED;
	ctx->file_size = 0;
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		ft_perror_file(filename);
		return (-1);
	}
	if (fstat(fd, &st) < 0) {
		ft_perror_file(filename);
		close(fd);
		return (-1);
	}
	if (st.st_size == 0) {
		ft_error(filename, "file is empty");
		close(fd);
		return (-1);
	}
	ctx->file_size = (size_t)st.st_size;
	ctx->mapped_data = mmap(NULL, ctx->file_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (ctx->mapped_data == MAP_FAILED) {
		ft_perror_file(filename);
		return (-1);
	}
	return (0);
}


// Libère le mapping mmap si actif.
void	elf_unload(t_elf_ctx *ctx) {
	if (ctx->mapped_data != MAP_FAILED && ctx->mapped_data != NULL)
		munmap(ctx->mapped_data, ctx->file_size);
	ctx->mapped_data = NULL;
	ctx->file_size = 0;
}

/*
	Vérifie les 4 octets magiques ELF : 0x7f 'E' 'L' 'F'.
	Retourne 0 si valide, -1 sinon.
*/
int	elf_validate_magic(t_elf_ctx *ctx, const char *filename) {
	const unsigned char	*ident;

	if (ctx->file_size < EI_NIDENT) {
		ft_error(filename, "file format not recognized");
		return (-1);
	}
	ident = (const unsigned char *)ctx->mapped_data;
	if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1
		|| ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3) {
		ft_error(filename, "file format not recognized");
		return (-1);
	}
	return (0);
}
