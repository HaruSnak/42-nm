<div align="center">

# ft_nm
### Unix symbol table inspector — ELF 32/64-bit reimplementation

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![License][license-shield]][license-url]

</div>

---

## 🇬🇧 English

<details>
<summary><b>📖 Click to expand/collapse English version</b></summary>

### 📖 About
**ft_nm** is a 42 School project that reimplements the Unix `nm` command in C, without any options. It reads ELF (Executable and Linkable Format) binaries and prints their symbol table in the same format as the system `nm`, including symbol type, address, and name.

The program works exclusively through low-level system calls: files are loaded into memory using `mmap`, parsed entirely within the mapped region, and every single byte access is bounds-checked to prevent any out-of-bounds read. No standard I/O functions (`printf`, `sprintf`, `puts`) are used — all output goes through `write(2)`.

---

### 🧠 Skills Learned
- **ELF Binary Format**: Deep understanding of ELF headers, section headers, symbol tables and string tables for both 32-bit and 64-bit architectures.
- **Memory Mapping**: Using `mmap`/`munmap` to map a file into process address space and navigate it safely.
- **Bounds Checking**: Systematic validation of every offset and size before any dereference, protecting against malformed or crafted binaries.
- **Low-level C**: Restricting the entire program to a minimal set of syscalls and libc functions as imposed by the subject.
- **Symbol Classification**: Reproducing GNU nm's type-letter logic (T/D/B/R/U/W/V/…) from ELF section flags and symbol metadata.

---

### 🗂️ ELF File Layout

An ELF file is structured as follows. `ft_nm` navigates this layout entirely inside the `mmap`'d region:

```
┌──────────────────────────────────────┐  offset 0
│           ELF Header                 │  (Elf64_Ehdr / Elf32_Ehdr)
│  magic · class · endianness          │
│  e_shoff · e_shentsize · e_shnum     │
│  e_shstrndx                          │
├──────────────────────────────────────┤
│         Program Header Table         │  (segments — ignored by nm)
│              ...                     │
├──────────────────────────────────────┤
│            .text  section            │  executable code
├──────────────────────────────────────┤
│            .data  section            │  initialised variables
├──────────────────────────────────────┤
│           .rodata section            │  read-only constants
├──────────────────────────────────────┤
│            .bss   section            │  uninitialised variables (no data)
├──────────────────────────────────────┤
│           .symtab section            │  ← array of Elf64_Sym entries
│   st_name · st_info · st_shndx      │
│   st_value · st_size                 │
├──────────────────────────────────────┤
│           .strtab section            │  ← null-terminated symbol names
│   "\0func_global_a\0main\0..."       │    st_name = byte offset into here
├──────────────────────────────────────┤
│       Section Header Table           │  offset = e_shoff
│   [0] NULL · [1] .text · [2] .data  │
│   [...] SHT_SYMTAB → sh_link →      │
│          SHT_STRTAB                  │
└──────────────────────────────────────┘
```

> `ft_nm` never follows program headers. It goes directly to the section header table via `e_shoff`, finds `SHT_SYMTAB`, then reaches `.strtab` through `sh_link`.

---

### ⚙️ Processing Pipeline

```
 ┌─────────┐    ┌──────────┐    ┌────────────────┐    ┌────────────────┐
 │  open() │───▶│ fstat()  │───▶│   mmap()       │───▶│ validate_magic │
 │         │    │ get size │    │ PROT_READ      │    │ 0x7f E L F     │
 └─────────┘    └──────────┘    │ MAP_PRIVATE    │    └───────┬────────┘
                                 │ close fd after │            │
                                 └────────────────┘            ▼
 ┌──────────────┐    ┌──────────────┐    ┌───────────────────────────────┐
 │ symbol_sort  │◀───│ symbol_      │◀───│       parse_header            │
 │ (strcmp,     │    │ extract_all  │    │ class(32/64) · endianness     │
 │  LC_ALL=C)   │    │ malloc per   │    │ shoff · shentsize · shnum     │
 └──────┬───────┘    │ t_symbol     │    └──────────────┬────────────────┘
        │            └──────────────┘                   │
        ▼                    ▲                          ▼
 ┌──────────────┐            │              ┌────────────────────────┐
 │ symbol_print │            └──────────────│    find_symtab         │
 │ write(2) only│                           │ walk SHT → SHT_SYMTAB  │
 └──────┬───────┘                           │ sh_link → SHT_STRTAB   │
        │                                   └────────────────────────┘
        ▼
 ┌────────────────────┐
 │  symbol_list_free  │──▶ munmap()
 │  free each t_symbol│
 └────────────────────┘
```

---

### 🏗️ Internal Data Structures

**`t_elf_ctx`** — one instance per file, lives on the stack:
```c
typedef struct s_elf_ctx {
    void    *mapped_data;   // base pointer of the mmap'd file
    size_t   file_size;     // total byte size (from fstat)
    int      is_64bit;      // ELFCLASS64 → 1, ELFCLASS32 → 0
    int      is_big_endian; // ELFDATA2MSB → 1, ELFDATA2LSB → 0
    t_u64    shoff;         // byte offset of the section header table
    t_u16    shentsize;     // byte size of one section header entry
    t_u16    shnum;         // total number of section headers
    t_u16    shstrndx;      // index of the section-name string section
} t_elf_ctx;
```

**`t_symbol`** — one `malloc` per kept symbol:
```c
typedef struct s_symbol {
    const char  *name;          // pointer INSIDE the mmap'd strtab (zero-copy)
    size_t       name_max_len;  // strtab_size - st_name (safe upper bound)
    t_u64        value;         // symbol address / value
    char         type;          // nm letter: T, D, B, R, U, W, V …
    int          is_undef;      // 1 if SHN_UNDEF (print spaces, not address)
} t_symbol;
```

**`t_symbol_list`** — growable pointer array (doubles capacity on overflow):
```c
typedef struct s_symbol_list {
    t_symbol  **array;    // heap-allocated array of pointers
    int         count;    // current number of symbols
    int         capacity; // allocated slots (starts at 64, doubles each time)
} t_symbol_list;
```

> **Zero-copy names**: `t_symbol.name` points directly into the `mmap`'d strtab — no `strdup`, no extra allocation. The `name_max_len` field prevents reads past the end of the strtab even if the null terminator is missing.

---

### 🛡️ Bounds Checking — `bounds_ok()`

Every access to the mapped file goes through this guard:

```c
static inline int bounds_ok(const t_elf_ctx *ctx, size_t offset, size_t size) {
    return (size <= ctx->file_size && offset <= ctx->file_size - size);
}
```

**Why this formula avoids integer overflow:**

A naive check `offset + size <= file_size` wraps around when `offset + size > SIZE_MAX`. The safe version checks `size` first (bounding the subtraction), then compares `offset` against `file_size - size`, which is always valid. This pattern is used for:
- The ELF header itself (`sizeof(Elf64_Ehdr)`)
- The entire section header table (`shoff + shnum × shentsize`)
- Every symtab entry (`symtab_off + i × sizeof(Elf64_Sym)`)
- Every strtab (`strtab_off + strtab_size`)

---

### 🔡 Symbol Type Classification

The type letter is determined by `symbol_get_type()` using this decision tree:

```
st_info
  │
  ├─ binding == STB_WEAK ?
  │     ├─ shndx == SHN_UNDEF          → 'w'
  │     ├─ sym_type == STT_OBJECT      → 'V'
  │     └─ otherwise                   → 'W'
  │
  ├─ binding == STB_GNU_UNIQUE         → 'u'
  │
  ├─ shndx == SHN_UNDEF               → 'U'
  ├─ shndx == SHN_ABS                 → 'A' / 'a'  (global / local)
  ├─ shndx == SHN_COMMON              → 'C' / 'c'
  │
  └─ section flags (sh_flags, sh_type) :
        ├─ SHF_EXECINSTR               → 'T' / 't'   (.text)
        ├─ SHT_NOBITS                  → 'B' / 'b'   (.bss)
        ├─ SHF_ALLOC && !SHF_WRITE    → 'R' / 'r'   (.rodata)
        ├─ SHF_ALLOC                  → 'D' / 'd'   (.data)
        └─ otherwise                  → '?'
```

`uppercase_if_global()` converts the base letter to uppercase when `binding == STB_GLOBAL` or `binding == STB_WEAK` (for defined weak symbols).

---

### 🔡 Symbol Type Letters

| Letter | Meaning |
|--------|---------|
| `T` / `t` | Text section (executable code) — global / local |
| `D` / `d` | Data section (initialised variables) — global / local |
| `B` / `b` | BSS section (uninitialised variables) — global / local |
| `R` / `r` | Read-only data section (.rodata) — global / local |
| `U`       | Undefined external symbol |
| `W` / `w` | Weak symbol (defined / undefined) |
| `V`       | Weak object symbol |
| `A` / `a` | Absolute symbol (SHN_ABS) |
| `C` / `c` | Common / tentative symbol (SHN_COMMON) |
| `u`       | GNU unique symbol (STB_GNU_UNIQUE) |
| `?`       | Unknown / unclassified |

---

### 📦 Supported File Types

| Format | 32-bit | 64-bit |
|--------|--------|--------|
| Object file (`.o`) | ✓ | ✓ |
| Shared library (`.so`) | ✓ | ✓ |
| Executable | ✓ | ✓ |

---

### 🧪 Robustness — Error Cases Handled

| Input | Behaviour |
|-------|-----------|
| Non-existent file | `ft_nm: path: No such file or directory` — exit 1 |
| Empty file | `ft_nm: path: file is empty` — exit 1 |
| Non-ELF file (text, PDF…) | `ft_nm: path: file format not recognized` — exit 1 |
| ELF with unsupported class | `ft_nm: path: unsupported ELF class` — exit 1 |
| Truncated ELF (< 16 bytes) | `ft_nm: path: file format not recognized` — exit 1 |
| Directory passed as argument | mmap fails gracefully — exit 1 |
| Stripped binary (no .symtab) | `ft_nm: path: no symbols` — exit 1 |
| `sh_offset` pointing outside file | `bounds_ok()` catches it — exit 0, no symbols |
| `sh_link` index ≥ shnum | Rejected before any dereference — exit 0 |
| `shnum = 65535` (malformed) | Section table OOB, caught — exit 1 |
| `shentsize = 0` (malformed) | Division-by-zero guard, treated as no symtab |
| `st_name ≥ strtab_size` | Symbol silently skipped |
| Non-null-terminated strtab | `ft_putstr_bounded_fd` stops at `name_max_len` |
| No arguments | Usage message to stderr — exit 1 |

---

### 💬 Output Examples

**Single file:**
```
$ ./ft_nm mylib.o
0000000000000000 T func_global_a
0000000000000010 T func_global_b
0000000000000004 D g_init_char
0000000000000000 D g_init_double
0000000000000000 D g_init_int
0000000000000000 R g_rodata
0000000000000004 B g_uninit_int
0000000000000000 B g_uninit_long
                 U printf
                 U puts
0000000000000000 d s_init
0000000000000000 b s_uninit
0000000000000000 V weak_sym
```

**Multiple files:**
```
$ ./ft_nm mylib.o myexec

mylib.o:
0000000000000000 T func_global_a
...

myexec:
0000000000001149 T func_global_a
...
```

**Error on one file, others still processed:**
```
$ ./ft_nm valid.o /no/such/path valid2.o
ft_nm: /no/such/path: No such file or directory
[symbols from valid.o and valid2.o are printed normally]
```

---

### 🔃 Sort Order

Symbols are sorted using plain `strcmp` on raw names, identical to GNU nm in `LC_ALL=C`. This means:
- `_` (ASCII 95) sorts **before** lowercase letters (ASCII 97+)
- `__foo` appears before `bar`
- Sort is **case-sensitive** and **locale-independent**

```
__libc_start_main   ← underscore (95) < letters
_start
func_a
func_b
main
```

To compare against system `nm`, always run with `LC_ALL=C`:
```bash
LC_ALL=C nm myfile.o
LC_ALL=C ./ft_nm myfile.o
```

---

### **Features Summary**
**Identical output to nm:** *Produces byte-for-byte identical output to `nm` (verified with diff, LC_ALL=C).*<br>
**32 and 64-bit ELF:** *Separate code paths for Elf32_* and Elf64_* structures.*<br>
**Robust error handling:** *Never segfaults or crashes on malformed input — every offset and size is validated through `bounds_ok()` before any dereference.*<br>
**No forbidden functions:** *All output via `write(2)`, errors via `strerror(3)` — no printf, sprintf, or puts.*<br>
**Multi-file mode:** *Passes multiple files on the command line; each file gets a blank line and `filename:` header, including the first.*<br>
**Zero memory leaks:** *Verified with Valgrind on valid, stripped, and deliberately malformed ELF files.*<br>
**Zero-copy name handling:** *Symbol names point directly into the mmap'd strtab — no strdup, no extra allocation.*<br>

---

### 🚀 Installation
```bash
# Clone the repository
git clone https://github.com/HaruSnak/42-nm
cd 42-nm

# Build  (-Wall -Wextra -Werror)
make

# Clean objects only
make clean

# Full clean (objects + binary)
make fclean

# Rebuild from scratch
make re
```

### 💻 Usage
```bash
# Single file
./ft_nm <file>

# Multiple files
./ft_nm <file1> <file2> ...

# Examples
./ft_nm myprogram.o
./ft_nm /usr/lib/crt1.o
./ft_nm libfoo.so /usr/lib/crti.o myprogram
```

### 📂 Project Structure
```text
42-nm/
├── Makefile                     # Build rules: all, clean, fclean, re
├── includes/
│   └── ft_nm.h                  # Types, structs, bounds_ok(), all prototypes
└── srcs/
    ├── main.c                   # Entry point, single/multi-file dispatch
    ├── ft_utils.c               # String helpers, write-based I/O, error output
    ├── elf_loader.c             # mmap/munmap, ELF magic validation
    ├── elf_parse_header.c       # ELF header parsing (32/64, endianness)
    ├── elf_parse_sections.c     # Section header walk, symtab/strtab lookup
    ├── symbol_extract.c         # Symbol iteration, filtering, dynamic list
    ├── symbol_type.c            # nm type-letter classification logic
    ├── symbol_sort.c            # Insertion sort (strcmp, LC_ALL=C)
    └── symbol_print.c           # Formatted output (address + type + name)
```

### 📖 Credits
- **ELF Specification**: [System V ABI — ELF](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- **GNU Binutils nm**: [sourceware.org/binutils](https://sourceware.org/binutils/)
- **Linux man pages**: `man nm`, `man elf`, `man mmap`

### 📄 License
This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

</details>

---

## 🇫🇷 Français

<details>
<summary><b>📖 Cliquez pour développer/réduire la version française</b></summary>

### 📖 À propos
**ft_nm** est un projet de l'école 42 qui réimplémente la commande Unix `nm` en C, sans aucune option. Il lit des binaires au format ELF (Executable and Linkable Format) et affiche leur table des symboles dans le même format que le `nm` système, avec le type du symbole, son adresse et son nom.

Le programme fonctionne exclusivement via des appels système bas niveau : les fichiers sont chargés en mémoire via `mmap`, parsés entièrement dans la zone mappée, et chaque accès à un octet est borné pour prévenir toute lecture hors limites. Aucune fonction de sortie standard (`printf`, `sprintf`, `puts`) n'est utilisée — toute la sortie passe par `write(2)`.

---

### 🧠 Compétences acquises
- **Format ELF** : Compréhension approfondie des headers ELF, section headers, tables de symboles et tables de chaînes pour les architectures 32 et 64 bits.
- **Projection mémoire** : Utilisation de `mmap`/`munmap` pour projeter un fichier dans l'espace d'adressage du processus et le parcourir sans risque.
- **Vérification de bornes** : Validation systématique de chaque offset et taille avant tout déréférencement, pour résister aux binaires malformés ou malveillants.
- **C bas niveau** : Restreindre l'intégralité du programme à un ensemble minimal de syscalls et de fonctions libc imposé par le sujet.
- **Classification de symboles** : Reproduire la logique des lettres de type de GNU nm (T/D/B/R/U/W/V/…) à partir des flags de section et des métadonnées ELF.

---

### 🗂️ Structure d'un fichier ELF

Un fichier ELF est organisé de la façon suivante. `ft_nm` navigue dans cette structure entièrement depuis la zone `mmap` :

```
┌──────────────────────────────────────┐  offset 0
│           En-tête ELF                │  (Elf64_Ehdr / Elf32_Ehdr)
│  magic · classe · endianness         │
│  e_shoff · e_shentsize · e_shnum     │
│  e_shstrndx                          │
├──────────────────────────────────────┤
│       Table des en-têtes programme   │  (segments — ignorés par nm)
│              ...                     │
├──────────────────────────────────────┤
│          section .text               │  code exécutable
├──────────────────────────────────────┤
│          section .data               │  variables initialisées
├──────────────────────────────────────┤
│         section .rodata              │  constantes en lecture seule
├──────────────────────────────────────┤
│          section .bss                │  variables non initialisées (pas de données)
├──────────────────────────────────────┤
│         section .symtab              │  ← tableau d'entrées Elf64_Sym
│   st_name · st_info · st_shndx      │
│   st_value · st_size                 │
├──────────────────────────────────────┤
│         section .strtab              │  ← noms de symboles null-terminés
│   "\0func_global_a\0main\0..."       │    st_name = offset en octets ici
├──────────────────────────────────────┤
│    Table des en-têtes de section     │  offset = e_shoff
│   [0] NULL · [1] .text · [2] .data  │
│   [...] SHT_SYMTAB → sh_link →      │
│          SHT_STRTAB                  │
└──────────────────────────────────────┘
```

> `ft_nm` ne suit jamais les en-têtes de programme. Il va directement à la table des sections via `e_shoff`, trouve `SHT_SYMTAB`, puis atteint `.strtab` par `sh_link`.

---

### ⚙️ Pipeline de traitement

```
 ┌─────────┐    ┌──────────┐    ┌────────────────┐    ┌────────────────┐
 │  open() │───▶│ fstat()  │───▶│   mmap()       │───▶│ validate_magic │
 │         │    │ get size │    │ PROT_READ      │    │ 0x7f E L F     │
 └─────────┘    └──────────┘    │ MAP_PRIVATE    │    └───────┬────────┘
                                 │ close fd after │            │
                                 └────────────────┘            ▼
 ┌──────────────┐    ┌──────────────┐    ┌───────────────────────────────┐
 │ symbol_sort  │◀───│ symbol_      │◀───│       parse_header            │
 │ (strcmp,     │    │ extract_all  │    │ class(32/64) · endianness     │
 │  LC_ALL=C)   │    │ malloc par   │    │ shoff · shentsize · shnum     │
 └──────┬───────┘    │ t_symbol     │    └──────────────┬────────────────┘
        │            └──────────────┘                   │
        ▼                    ▲                          ▼
 ┌──────────────┐            │              ┌────────────────────────┐
 │ symbol_print │            └──────────────│    find_symtab         │
 │ write(2) only│                           │ parcours SHT_SYMTAB    │
 └──────┬───────┘                           │ sh_link → SHT_STRTAB   │
        │                                   └────────────────────────┘
        ▼
 ┌────────────────────┐
 │  symbol_list_free  │──▶ munmap()
 │  free chaque sym   │
 └────────────────────┘
```

---

### 🏗️ Structures de données internes

**`t_elf_ctx`** — une instance par fichier, allouée sur la pile :
```c
typedef struct s_elf_ctx {
    void    *mapped_data;   // pointeur de base du fichier mmap'd
    size_t   file_size;     // taille totale en octets (depuis fstat)
    int      is_64bit;      // ELFCLASS64 → 1, ELFCLASS32 → 0
    int      is_big_endian; // ELFDATA2MSB → 1, ELFDATA2LSB → 0
    t_u64    shoff;         // offset de la section header table
    t_u16    shentsize;     // taille en octets d'une entrée de section
    t_u16    shnum;         // nombre total de section headers
    t_u16    shstrndx;      // index de la section des noms de sections
} t_elf_ctx;
```

**`t_symbol`** — un `malloc` par symbole conservé :
```c
typedef struct s_symbol {
    const char  *name;          // pointeur DANS la strtab mmap'd (zero-copy)
    size_t       name_max_len;  // strtab_size - st_name (borne sûre)
    t_u64        value;         // adresse / valeur du symbole
    char         type;          // lettre nm : T, D, B, R, U, W, V…
    int          is_undef;      // 1 si SHN_UNDEF (afficher espaces, pas adresse)
} t_symbol;
```

**`t_symbol_list`** — tableau de pointeurs à capacité croissante (doublement) :
```c
typedef struct s_symbol_list {
    t_symbol  **array;    // tableau heap-alloué de pointeurs
    int         count;    // nombre de symboles actuels
    int         capacity; // slots alloués (démarre à 64, double à chaque fois)
} t_symbol_list;
```

> **Noms zero-copy** : `t_symbol.name` pointe directement dans la strtab `mmap`'d — pas de `strdup`, pas d'allocation supplémentaire. Le champ `name_max_len` empêche de lire au-delà de la fin de la strtab même si le null-terminator est absent.

---

### 🛡️ Vérification de bornes — `bounds_ok()`

Chaque accès au fichier mappé passe par ce garde :

```c
static inline int bounds_ok(const t_elf_ctx *ctx, size_t offset, size_t size) {
    return (size <= ctx->file_size && offset <= ctx->file_size - size);
}
```

**Pourquoi cette formule évite l'integer overflow :**

Une vérification naïve `offset + size <= file_size` déborde si `offset + size > SIZE_MAX`. La version sûre vérifie `size` d'abord (bornant la soustraction), puis compare `offset` contre `file_size - size`, ce qui est toujours valide. Ce motif est utilisé pour :
- Le header ELF lui-même (`sizeof(Elf64_Ehdr)`)
- L'intégralité de la section header table (`shoff + shnum × shentsize`)
- Chaque entrée de la symtab (`symtab_off + i × sizeof(Elf64_Sym)`)
- Chaque strtab (`strtab_off + strtab_size`)

---

### 🔡 Classification des types de symboles

La lettre de type est déterminée par `symbol_get_type()` selon cet arbre de décision :

```
st_info
  │
  ├─ binding == STB_WEAK ?
  │     ├─ shndx == SHN_UNDEF          → 'w'
  │     ├─ sym_type == STT_OBJECT      → 'V'
  │     └─ sinon                       → 'W'
  │
  ├─ binding == STB_GNU_UNIQUE         → 'u'
  │
  ├─ shndx == SHN_UNDEF               → 'U'
  ├─ shndx == SHN_ABS                 → 'A' / 'a'  (global / local)
  ├─ shndx == SHN_COMMON              → 'C' / 'c'
  │
  └─ flags de section (sh_flags, sh_type) :
        ├─ SHF_EXECINSTR               → 'T' / 't'   (.text)
        ├─ SHT_NOBITS                  → 'B' / 'b'   (.bss)
        ├─ SHF_ALLOC && !SHF_WRITE    → 'R' / 'r'   (.rodata)
        ├─ SHF_ALLOC                  → 'D' / 'd'   (.data)
        └─ sinon                      → '?'
```

`uppercase_if_global()` convertit la lettre base en majuscule quand `binding == STB_GLOBAL` ou `binding == STB_WEAK` (pour les weak symbols définis).

---

### 🔡 Lettres de type de symbole

| Lettre | Signification |
|--------|--------------|
| `T` / `t` | Section texte (code exécutable) — global / local |
| `D` / `d` | Section data (variables initialisées) — global / local |
| `B` / `b` | Section BSS (variables non initialisées) — global / local |
| `R` / `r` | Section lecture seule (.rodata) — global / local |
| `U`       | Symbole externe non défini |
| `W` / `w` | Symbole faible (défini / non défini) |
| `V`       | Symbole objet faible |
| `A` / `a` | Symbole absolu (SHN_ABS) |
| `C` / `c` | Symbole commun / tentative (SHN_COMMON) |
| `u`       | Symbole unique GNU (STB_GNU_UNIQUE) |
| `?`       | Inconnu / non classifié |

---

### 📦 Formats supportés

| Format | 32 bits | 64 bits |
|--------|---------|---------|
| Fichier objet (`.o`) | ✓ | ✓ |
| Bibliothèque partagée (`.so`) | ✓ | ✓ |
| Exécutable | ✓ | ✓ |

---

### 🧪 Robustesse — Cas d'erreur gérés

| Entrée | Comportement |
|--------|-------------|
| Fichier inexistant | `ft_nm: path: No such file or directory` — exit 1 |
| Fichier vide | `ft_nm: path: file is empty` — exit 1 |
| Fichier non-ELF (texte, PDF…) | `ft_nm: path: file format not recognized` — exit 1 |
| ELF avec classe non supportée | `ft_nm: path: unsupported ELF class` — exit 1 |
| ELF tronqué (< 16 octets) | `ft_nm: path: file format not recognized` — exit 1 |
| Répertoire passé en argument | mmap échoue proprement — exit 1 |
| Binaire strippé (sans .symtab) | `ft_nm: path: no symbols` — exit 1 |
| `sh_offset` hors du fichier | `bounds_ok()` intercède — exit 0, aucun symbole |
| Index `sh_link` ≥ shnum | Rejeté avant tout déréférencement — exit 0 |
| `shnum = 65535` (malformé) | Table OOB, intercepté — exit 1 |
| `shentsize = 0` (malformé) | Garde division par zéro, traité comme sans symtab |
| `st_name ≥ strtab_size` | Symbole silencieusement ignoré |
| strtab sans null-terminator | `ft_putstr_bounded_fd` s'arrête à `name_max_len` |
| Aucun argument | Message d'usage vers stderr — exit 1 |

---

### 💬 Exemples de sortie

**Fichier unique :**
```
$ ./ft_nm mylib.o
0000000000000000 T func_global_a
0000000000000010 T func_global_b
0000000000000004 D g_init_char
0000000000000000 D g_init_double
0000000000000000 D g_init_int
0000000000000000 R g_rodata
0000000000000004 B g_uninit_int
0000000000000000 B g_uninit_long
                 U printf
                 U puts
0000000000000000 d s_init
0000000000000000 b s_uninit
0000000000000000 V weak_sym
```

**Plusieurs fichiers :**
```
$ ./ft_nm mylib.o myexec

mylib.o:
0000000000000000 T func_global_a
...

myexec:
0000000000001149 T func_global_a
...
```

**Erreur sur un fichier, les autres continuent :**
```
$ ./ft_nm valid.o /no/such/path valid2.o
ft_nm: /no/such/path: No such file or directory
[symboles de valid.o et valid2.o affichés normalement]
```

---

### 🔃 Ordre de tri

Les symboles sont triés avec un `strcmp` brut sur les noms bruts, identique à GNU nm en `LC_ALL=C`. Cela signifie :
- `_` (ASCII 95) trie **avant** les lettres minuscules (ASCII 97+)
- `__foo` apparaît avant `bar`
- Le tri est **sensible à la casse** et **indépendant de la locale**

```
__libc_start_main   ← underscore (95) < lettres
_start
func_a
func_b
main
```

Pour comparer contre le `nm` système, toujours utiliser `LC_ALL=C` :
```bash
LC_ALL=C nm monfichier.o
LC_ALL=C ./ft_nm monfichier.o
```

---

### **Résumé des fonctionnalités**
**Sortie identique à nm :** *Produit une sortie octet-pour-octet identique à `nm` (vérifié par diff, LC_ALL=C).*<br>
**ELF 32 et 64 bits :** *Chemins de code distincts pour les structures Elf32_* et Elf64_*.*<br>
**Gestion d'erreurs robuste :** *Jamais de segfault ni de crash sur une entrée malformée — chaque offset et taille est validé par `bounds_ok()` avant tout déréférencement.*<br>
**Aucune fonction interdite :** *Toute sortie via `write(2)`, erreurs via `strerror(3)` — pas de printf, sprintf, ni puts.*<br>
**Mode multi-fichiers :** *Traitement de plusieurs fichiers en une commande ; chaque fichier reçoit une ligne vide et un header `fichier:`, y compris le premier.*<br>
**Zéro fuite mémoire :** *Vérifié avec Valgrind sur des fichiers ELF valides, strippés et délibérément malformés.*<br>
**Noms zero-copy :** *Les noms de symboles pointent directement dans la strtab mmap'd — pas de strdup, pas d'allocation supplémentaire.*<br>

---

### 🚀 Installation
```bash
# Cloner le dépôt
git clone https://github.com/HaruSnak/42-nm
cd 42-nm

# Compiler  (-Wall -Wextra -Werror)
make

# Nettoyer les objets uniquement
make clean

# Nettoyage complet (objets + binaire)
make fclean

# Recompiler depuis zéro
make re
```

### 💻 Utilisation
```bash
# Fichier unique
./ft_nm <fichier>

# Plusieurs fichiers
./ft_nm <fichier1> <fichier2> ...

# Exemples
./ft_nm monprogramme.o
./ft_nm /usr/lib/crt1.o
./ft_nm libfoo.so /usr/lib/crti.o monprogramme
```

### 📂 Structure du projet
```text
42-nm/
├── Makefile                     # Règles de build : all, clean, fclean, re
├── includes/
│   └── ft_nm.h                  # Types, structs, bounds_ok(), tous les prototypes
└── srcs/
    ├── main.c                   # Point d'entrée, dispatch mono/multi-fichier
    ├── ft_utils.c               # Helpers string, I/O via write, messages d'erreur
    ├── elf_loader.c             # mmap/munmap, validation du magic ELF
    ├── elf_parse_header.c       # Parsing du header ELF (32/64, endianness)
    ├── elf_parse_sections.c     # Parcours section headers, recherche symtab/strtab
    ├── symbol_extract.c         # Itération symboles, filtrage, liste dynamique
    ├── symbol_type.c            # Logique de classification des lettres nm
    ├── symbol_sort.c            # Tri par insertion (strcmp, LC_ALL=C)
    └── symbol_print.c           # Formatage de la sortie (adresse + type + nom)
```

### 📖 Crédits
- **Spécification ELF** : [System V ABI — ELF](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- **GNU Binutils nm** : [sourceware.org/binutils](https://sourceware.org/binutils/)
- **Pages de manuel Linux** : `man nm`, `man elf`, `man mmap`

### 📄 Licence
Ce projet est sous licence **MIT** - voir le fichier [LICENSE](LICENSE) pour plus de détails.

</details>

---

[contributors-shield]: https://img.shields.io/github/contributors/HaruSnak/42-nm.svg?style=for-the-badge
[contributors-url]: https://github.com/HaruSnak/42-nm/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/HaruSnak/42-nm.svg?style=for-the-badge
[forks-url]: https://github.com/HaruSnak/42-nm/network/members
[stars-shield]: https://img.shields.io/github/stars/HaruSnak/42-nm.svg?style=for-the-badge
[stars-url]: https://github.com/HaruSnak/42-nm/stargazers
[issues-shield]: https://img.shields.io/github/issues/HaruSnak/42-nm.svg?style=for-the-badge
[issues-url]: https://github.com/HaruSnak/42-nm/issues
[license-shield]: https://img.shields.io/github/license/HaruSnak/42-nm.svg?style=for-the-badge
[license-url]: https://github.com/HaruSnak/42-nm/blob/master/LICENSE
