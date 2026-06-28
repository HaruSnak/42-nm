#include "ft_nm.h"

/*
	Retourne la lettre uppercase si le symbole est global/weak,
	lowercase s'il est local. Entrée : base en minuscule.
*/
static char	uppercase_if_global(char base_lower, int binding) {
	if (binding == STB_GLOBAL || binding == STB_WEAK)
		return ((char)(base_lower - 32));
	return (base_lower);
}

/*
	Détermine la lettre nm pour un symbole, selon les règles de GNU nm :

	U / w / u   — symboles non définis (SHN_UNDEF)
	A / a       — symboles absolus (SHN_ABS)
	C / c       — symboles communs tentatives (SHN_COMMON)
	T / t       — section exécutable (.text)
	B / b       — section BSS (SHT_NOBITS, non initialisée)
	R / r       — section lecture seule (.rodata)
	D / d       — section données initialisées (.data)
	?           — inconnu

	Majuscule = global ou weak défini, minuscule = local.

	Paramètres :
	shndx    — st_shndx du symbole (peut être SHN_UNDEF, SHN_ABS, SHN_COMMON)
	st_info  — octet ELF qui encode binding (bits 7-4) et type (bits 3-0)
	sh_flags — sh_flags de la section du symbole (0 si section spéciale)
	sh_type  — sh_type de la section du symbole (0 si section spéciale)
*/
char	symbol_get_type(t_u16 shndx, t_u8 st_info, t_u64 sh_flags, t_u32 sh_type) {
	int	binding;
	int	sym_type;

	binding = (st_info >> 4);
	sym_type = (st_info & 0xf);
	// Weak : traité avant les vérifications de section
	if (binding == STB_WEAK) {
		if (shndx == SHN_UNDEF)
			return ('w');
		if (sym_type == STT_OBJECT)
			return ('V');
		return ('W');
	}
	// GNU unique (extension GCC)
	if (binding == STB_GNU_UNIQUE)
		return ('u');
	// Non défini
	if (shndx == SHN_UNDEF)
		return ('U');
	// Absolu (ex : constantes de débogage)
	if (shndx == SHN_ABS)
		return (uppercase_if_global('a', binding));
	// Common / tentative (déclarations C non initialisées dans certains cas)
	if (shndx == SHN_COMMON)
		return (uppercase_if_global('c', binding));
	// Section normale : classification par flags
	if (sh_flags & SHF_EXECINSTR)
		return (uppercase_if_global('t', binding));
	if (sh_type == SHT_NOBITS)
		return (uppercase_if_global('b', binding));
	if ((sh_flags & SHF_ALLOC) && !(sh_flags & SHF_WRITE))
		return (uppercase_if_global('r', binding));
	if (sh_flags & SHF_ALLOC)
		return (uppercase_if_global('d', binding));
	return ('?');
}
