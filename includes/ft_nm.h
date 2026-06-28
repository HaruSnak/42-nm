#ifndef FT_NM_H
# define FT_NM_H

# include <elf.h>
# include <errno.h>
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/stat.h>
# include <unistd.h>
# include <stdlib.h>
# include <string.h>

// ─── Aliases entiers non-ambiguïs ────────────────────────────

typedef unsigned char       t_u8;
typedef unsigned short      t_u16;
typedef unsigned int        t_u32;
typedef unsigned long long  t_u64;

// ─── Contexte ELF : une instance par fichier analysé ──────────

typedef struct s_elf_ctx
{
	void	*mapped_data;	// pointeur mmap
	size_t	 file_size;		// taille du fichier (octets)
	int		 is_64bit;		// 0 = ELF32, 1 = ELF64
	int		 is_big_endian;	// 0 = little-endian, 1 = big-endian
	// Champs extraits du ELF header
	t_u64	 shoff;			// offset de la section header table
	t_u16	 shentsize;		// taille d'une entrée section header
	t_u16	 shnum;			// nombre de section headers
	t_u16	 shstrndx;		// index de la section des noms de sections
}	t_elf_ctx;

// ─── Symbole parsé, prêt à être affiché ───────────────────────

typedef struct s_symbol
{
	const char	*name;			// pointeur dans la strtab mmappée
	size_t		 name_max_len;	// octets lisibles depuis name (borne sûre)
	t_u64		 value;			// adresse / valeur du symbole
	char		 type;			// lettre nm : T, D, B, U, t, d, b, w…
	int			 is_undef;		// 1 si SHN_UNDEF (pas d'adresse à afficher)
}	t_symbol;

// ─── Liste dynamique de symboles ──────────────────────────────

typedef struct s_symbol_list
{
	t_symbol	**array;	// tableau de pointeurs alloué avec malloc
	int			  count;
	int			  capacity;
}	t_symbol_list;

// ─── Vérification de borne sur la zone mmappée ───────────────

/*
	Retourne 1 si [offset, offset+size[ est entièrement dans le fichier.
	Évite les débordements entiers : on compare size avant de soustraire.
*/
static inline int	bounds_ok(const t_elf_ctx *ctx, size_t offset, size_t size)
{
	return (size <= ctx->file_size && offset <= ctx->file_size - size);
}

// ─── Prototypes — ft_utils.c ──────────────────────────────────

size_t	ft_strlen(const char *s);
int		ft_strcmp(const char *a, const char *b);
int		ft_nm_sort_cmp(const char *name_a, const char *name_b);
void	ft_putchar_fd(int fd, char c);
void	ft_putstr_fd(int fd, const char *s);
void	ft_putstr_bounded_fd(int fd, const char *s, size_t max_len);
void	ft_puthex_fd(int fd, t_u64 value, int width);
void	ft_error(const char *filename, const char *msg);
void	ft_perror_file(const char *filename);

// ─── Prototypes — elf_loader.c ────────────────────────────────

int		elf_load(const char *filename, t_elf_ctx *ctx);
void	elf_unload(t_elf_ctx *ctx);
int		elf_validate_magic(t_elf_ctx *ctx, const char *filename);

// ─── Prototypes — elf_parse_header.c ─────────────────────────

int		elf_parse_header(t_elf_ctx *ctx, const char *filename);

// ─── Prototypes — elf_parse_sections.c ───────────────────────

// Retourne 0 si .symtab trouvée, 1 si absente, -1 si erreur structure.
int		elf_find_symtab(t_elf_ctx *ctx,
			size_t *symtab_offset, size_t *symtab_size,
			size_t *strtab_offset, size_t *strtab_size);

// ─── Prototypes — symbol_extract.c ───────────────────────────

int		symbol_extract_all(t_elf_ctx *ctx,
			size_t symtab_off, size_t symtab_size,
			size_t strtab_off, size_t strtab_size,
			t_symbol_list *list);
void	symbol_list_free(t_symbol_list *list);

// ─── Prototypes — symbol_type.c ──────────────────────────────

char	symbol_get_type(t_u16 shndx, t_u8 st_info,
			t_u64 sh_flags, t_u32 sh_type);

// ─── Prototypes — symbol_sort.c ──────────────────────────────

void	symbol_sort(t_symbol_list *list);

// ─── Prototypes — symbol_print.c ─────────────────────────────

void	symbol_print_all(const t_symbol_list *list, int is_64bit);

#endif