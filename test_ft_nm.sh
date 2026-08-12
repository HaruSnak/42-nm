#!/bin/bash
# ============================================================
#  test_ft_nm.sh  —  Batterie de tests pour ft_nm
#  Usage : bash test_ft_nm.sh [--no-valgrind] [--no-color]
# ============================================================

set -u

# ── Options ──────────────────────────────────────────────────
USE_VALGRIND=1
USE_COLOR=1
for arg in "$@"; do
    [ "$arg" = "--no-valgrind" ] && USE_VALGRIND=0
    [ "$arg" = "--no-color"   ] && USE_COLOR=0
done

# ── Couleurs ──────────────────────────────────────────────────
if [ "$USE_COLOR" -eq 1 ] && [ -t 1 ]; then
    GRN="\033[32m"; RED="\033[31m"; YEL="\033[33m"
    CYN="\033[36m"; BLD="\033[1m";  RST="\033[0m"
else
    GRN=""; RED=""; YEL=""; CYN=""; BLD=""; RST=""
fi

# ── Compteurs ────────────────────────────────────────────────
PASS=0; FAIL=0; SKIP=0

ok()   { printf "  ${GRN}[PASS]${RST} %s\n" "$1"; PASS=$((PASS+1)); }
fail() { printf "  ${RED}[FAIL]${RST} %s\n" "$1"; [ -n "${2-}" ] && echo "         ↳ $2"; FAIL=$((FAIL+1)); }
skip() { printf "  ${YEL}[SKIP]${RST} %s\n" "$1"; SKIP=$((SKIP+1)); }
hdr()  { printf "\n${BLD}${CYN}═══ %s ${RST}\n" "$1"; }

# ── Prérequis ────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
FT_NM="$SCRIPT_DIR/ft_nm"
TMPDIR_TESTS="/tmp/ft_nm_tests_$$"
mkdir -p "$TMPDIR_TESTS"
trap 'rm -rf "$TMPDIR_TESTS"' EXIT

check_dep() {
    command -v "$1" >/dev/null 2>&1 || { echo "Dépendance manquante : $1"; exit 1; }
}
check_dep gcc
check_dep nm
[ "$USE_VALGRIND" -eq 1 ] && check_dep valgrind

# ── Compilation ───────────────────────────────────────────────
hdr "0. COMPILATION ════════════════════════════════"
if ! make re > /dev/null 2>&1; then
    fail "make re" "la compilation a échoué — tests annulés"
    exit 1
fi
[ -x "$FT_NM" ] && ok "make re : binaire ft_nm produit" || { fail "binaire absent"; exit 1; }

# ── Construction des binaires de test ─────────────────────────
hdr "PRÉPARATION DES BINAIRES DE TEST ═════════════"

T="$TMPDIR_TESTS"

cat > "$T/test_src.c" << 'CSRC'
#include <stdio.h>

int          g_init_int    = 42;
char         g_init_char   = 'Z';
double       g_init_double = 2.71;

int          g_uninit_int;
long         g_uninit_long;

static int   s_init   = 9;
static int   s_uninit;

const int    g_rodata = 100;

void func_global_a(void) { printf("a\n"); }
void func_global_b(void) { puts("b"); }

static void  func_static(void) {}

int weak_sym __attribute__((weak)) = 0;

int main(void) {
    func_global_a();
    func_global_b();
    func_static();
    return s_init + s_uninit;
}
CSRC

# Compile les différentes cibles
gcc -c "$T/test_src.c"    -o "$T/obj64.o"      2>/dev/null && ok ".o 64-bit compilé"    || fail ".o 64-bit"
gcc    "$T/test_src.c"    -o "$T/exec64"        2>/dev/null && ok "exec 64-bit compilé"  || fail "exec 64-bit"
gcc -shared -fPIC "$T/test_src.c" -o "$T/lib64.so" 2>/dev/null && ok ".so 64-bit compilé" || fail ".so 64-bit"
gcc -m32 -c "$T/test_src.c" -o "$T/obj32.o"    2>/dev/null && ok ".o 32-bit compilé"    || skip ".o 32-bit (multilib absent ?)"
gcc -m32    "$T/test_src.c" -o "$T/exec32"      2>/dev/null && ok "exec 32-bit compilé"  || skip "exec 32-bit"
gcc -m32 -shared -fPIC "$T/test_src.c" -o "$T/lib32.so" 2>/dev/null && ok ".so 32-bit compilé" || skip ".so 32-bit"

# Fichiers erreurs
echo "not elf data" > "$T/not_elf.txt"
touch "$T/empty_file"
printf '\x7fELF\x00\x00\x00\x00' > "$T/elf_truncated"
printf '\x7fELF\x09\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00' > "$T/elf_bad_class"

# ELF avec sh_offset hors limites (corrompt une section header)
python3 - "$T/obj64.o" "$T/elf_bad_shoffset" << 'PY'
import struct, sys
with open(sys.argv[1], 'rb') as f: d = bytearray(f.read())
shoff = struct.unpack_from('<Q', d, 40)[0]
she   = struct.unpack_from('<H', d, 58)[0]
struct.pack_into('<Q', d, shoff + she + 24, 0xDEADBEEFDEAD0000)
open(sys.argv[2], 'wb').write(d)
PY

# ELF avec sh_link invalide sur la symtab
python3 - "$T/obj64.o" "$T/elf_bad_shlink" << 'PY'
import struct, sys
with open(sys.argv[1], 'rb') as f: d = bytearray(f.read())
shoff = struct.unpack_from('<Q', d, 40)[0]
she   = struct.unpack_from('<H', d, 58)[0]
shnum = struct.unpack_from('<H', d, 60)[0]
for i in range(shnum):
    o = shoff + i * she
    if struct.unpack_from('<I', d, o)[0] == 2:
        struct.pack_into('<I', d, o + 28, 0xFFFF)
        break
open(sys.argv[2], 'wb').write(d)
PY

# ELF avec shnum=65535
python3 - "$T/obj64.o" "$T/elf_giant_shnum" << 'PY'
import struct, sys
with open(sys.argv[1], 'rb') as f: d = bytearray(f.read())
struct.pack_into('<H', d, 60, 0xFFFF)
open(sys.argv[2], 'wb').write(d)
PY

# ELF avec shentsize=0
python3 - "$T/obj64.o" "$T/elf_shentsize_zero" << 'PY'
import struct, sys
with open(sys.argv[1], 'rb') as f: d = bytearray(f.read())
struct.pack_into('<H', d, 58, 0)
open(sys.argv[2], 'wb').write(d)
PY

# ELF avec st_name hors strtab
python3 - "$T/obj64.o" "$T/elf_bad_stname" << 'PY'
import struct, sys
with open(sys.argv[1], 'rb') as f: d = bytearray(f.read())
shoff = struct.unpack_from('<Q', d, 40)[0]
she   = struct.unpack_from('<H', d, 58)[0]
shnum = struct.unpack_from('<H', d, 60)[0]
for i in range(shnum):
    o = shoff + i * she
    if struct.unpack_from('<I', d, o)[0] == 2:
        sym_off = struct.unpack_from('<Q', d, o + 24)[0]
        if sym_off + 24 + 4 <= len(d):
            struct.pack_into('<I', d, sym_off + 24, 0x0FFFFFFF)
        break
open(sys.argv[2], 'wb').write(d)
PY

ok "fichiers ELF malformés créés"

# ═══════════════════════════════════════════════════════════
hdr "1. SORTIE IDENTIQUE À nm (LC_ALL=C) ════════════"

diff_nm() {
    local label="$1" file="$2"
    [ -f "$file" ] || { skip "$label (fichier absent)"; return; }
    local ft ref
    ft=$(LANG=C LC_ALL=C "$FT_NM" "$file" 2>/dev/null)
    ref=$(LANG=C LC_ALL=C nm      "$file" 2>/dev/null)
    if [ "$ft" = "$ref" ]; then
        ok "$label"
    else
        fail "$label" "$(diff <(echo "$ft") <(echo "$ref") | head -5)"
    fi
}

diff_nm "objet 64-bit (.o)"           "$T/obj64.o"
diff_nm "exécutable 64-bit"           "$T/exec64"
diff_nm "shared library 64-bit (.so)" "$T/lib64.so"
diff_nm "objet 32-bit (.o)"           "$T/obj32.o"
diff_nm "exécutable 32-bit"           "$T/exec32"
diff_nm "shared library 32-bit (.so)" "$T/lib32.so"
diff_nm "crt1.o  (système)"           "/usr/lib/crt1.o"
diff_nm "crti.o  (système)"           "/usr/lib/crti.o"
diff_nm "Scrt1.o (système)"           "/usr/lib/Scrt1.o"

# ═══════════════════════════════════════════════════════════
hdr "2. TYPES DE SYMBOLES (lettre nm) ══════════════"

check_type() {
    local file="$1" sym="$2" expected="$3"
    [ -f "$file" ] || { skip "type '$sym' (fichier absent)"; return; }
    local got
    got=$(LANG=C LC_ALL=C "$FT_NM" "$file" 2>/dev/null | awk -v s="$sym" '
        NF==3 && $3==s { print $2; found=1 }
        NF==2 && $2==s { print $1; found=1 }
        END { if (!found) print "?" }')
    if [ "$got" = "$expected" ]; then
        ok "symbole '$sym' → type '$expected'"
    else
        fail "symbole '$sym'" "attendu '$expected', obtenu '$got'"
    fi
}

F="$T/obj64.o"
check_type "$F" "g_init_int"    "D"   # global int initialisé  → .data
check_type "$F" "g_init_char"   "D"   # global char initialisé → .data
check_type "$F" "g_uninit_int"  "B"   # global non-initialisé  → .bss
check_type "$F" "g_uninit_long" "B"   # global long non-init   → .bss
check_type "$F" "g_rodata"      "R"   # const global           → .rodata
check_type "$F" "s_init"        "d"   # static initialisé      → .data (minuscule)
check_type "$F" "s_uninit"      "b"   # static non-initialisé  → .bss  (minuscule)
check_type "$F" "func_global_a" "T"   # fonction globale       → .text
check_type "$F" "func_global_b" "T"   # fonction globale       → .text
check_type "$F" "func_static"   "t"   # fonction statique      → .text (minuscule)
check_type "$F" "weak_sym"      "V"   # weak objet défini      → V
check_type "$F" "puts"          "U"   # extern undefined       → U

# ═══════════════════════════════════════════════════════════
hdr "3. FORMAT D'AFFICHAGE ══════════════════════════"

# Adresses 64-bit sur 16 chiffres
addr=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj64.o" 2>/dev/null | awk 'NF==3{print $1; exit}')
if [ "${#addr}" -eq 16 ] 2>/dev/null; then
    ok "adresses 64-bit : 16 chiffres hex (ex: $addr)"
else
    fail "adresses 64-bit" "longueur ${#addr} pour '$addr' (attendu 16)"
fi

# Adresses 32-bit sur 8 chiffres
if [ -f "$T/obj32.o" ]; then
    addr=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj32.o" 2>/dev/null | awk 'NF==3{print $1; exit}')
    if [ "${#addr}" -eq 8 ] 2>/dev/null; then
        ok "adresses 32-bit : 8 chiffres hex (ex: $addr)"
    else
        fail "adresses 32-bit" "longueur ${#addr} pour '$addr' (attendu 8)"
    fi
else
    skip "adresses 32-bit (fichier absent)"
fi

# Symbole undefined → espaces au lieu d'adresse (64-bit)
line=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj64.o" 2>/dev/null | grep " U " | head -1)
prefix="${line:0:16}"
if [ "$prefix" = "                " ]; then
    ok "symbole undefined 64-bit : 16 espaces avant type"
else
    fail "undefined 64-bit" "obtenu: '${prefix}'"
fi

# Adresses en minuscules hex (pas de A-F)
upper=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj64.o" 2>/dev/null | awk 'NF==3{print $1}' | tr -d '0-9a-f')
if [ -z "$upper" ]; then
    ok "adresses hex en minuscules (0-9, a-f uniquement)"
else
    fail "adresses hex" "caractères majuscules détectés"
fi

# ═══════════════════════════════════════════════════════════
hdr "4. TRI DES SYMBOLES ══════════════════════════════"

check_sort() {
    local label="$1" file="$2"
    [ -f "$file" ] || { skip "tri $label (fichier absent)"; return; }
    if LANG=C LC_ALL=C "$FT_NM" "$file" 2>/dev/null | awk '{print $NF}' | LC_ALL=C sort -C 2>/dev/null; then
        ok "tri $label (ordre strcmp LC_ALL=C)"
    else
        fail "tri $label" "ordre incorrect"
    fi
}

check_sort ".o 64-bit"  "$T/obj64.o"
check_sort ".o 32-bit"  "$T/obj32.o"
check_sort "exec 64-bit" "$T/exec64"
check_sort ".so 64-bit"  "$T/lib64.so"
check_sort "crt1.o"     "/usr/lib/crt1.o"

# ═══════════════════════════════════════════════════════════
hdr "5. GESTION D'ERREURS ══════════════════════════"

no_crash() {
    local label="$1"; shift
    local rc=0
    "$FT_NM" "$@" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -ge 128 ]; then
        fail "$label" "CRASH — signal $((rc - 128))"
    else
        ok "$label (exit=$rc)"
    fi
}

no_crash "fichier inexistant"           /no/such/path/file.o
no_crash "fichier non-ELF"             "$T/not_elf.txt"
no_crash "fichier vide"                "$T/empty_file"
no_crash "répertoire"                  /tmp
no_crash "binaire strippé"             /bin/ls
no_crash "ELF tronqué (8 octets)"      "$T/elf_truncated"
no_crash "ELF classe invalide"         "$T/elf_bad_class"
no_crash "ELF sh_offset hors bornes"   "$T/elf_bad_shoffset"
no_crash "ELF sh_link invalide"        "$T/elf_bad_shlink"
no_crash "ELF shnum=65535"             "$T/elf_giant_shnum"
no_crash "ELF shentsize=0"             "$T/elf_shentsize_zero"
no_crash "ELF st_name hors strtab"     "$T/elf_bad_stname"
no_crash "aucun argument"

# Code de sortie non-nul pour les cas d'erreur
check_exit_nonzero() {
    local label="$1"; shift
    local rc=0
    "$FT_NM" "$@" >/dev/null 2>&1 || rc=$?
    [ "$rc" -ne 0 ] && ok "exit non-nul : $label" || fail "exit non-nul : $label" "exit=0 attendu ≠0"
}

check_exit_nonzero "fichier inexistant" /no/such/path
check_exit_nonzero "fichier non-ELF"   "$T/not_elf.txt"
check_exit_nonzero "strippé"           /bin/ls

# Messages d'erreur
msg=$("$FT_NM" /no/such/path 2>&1 || true)
[[ "$msg" == "ft_nm: "* ]] && ok "préfixe 'ft_nm:' dans les messages d'erreur" \
    || fail "préfixe erreur" "obtenu: '$msg'"

msg=$("$FT_NM" /bin/ls 2>&1 || true)
echo "$msg" | grep -qi "no symbols\|aucun" \
    && ok "message 'no symbols' pour binaire strippé" \
    || fail "message no symbols" "obtenu: '$msg'"

msg=$("$FT_NM" "$T/not_elf.txt" 2>&1 || true)
echo "$msg" | grep -qi "not recognized\|format" \
    && ok "message format non reconnu pour non-ELF" \
    || fail "message non-ELF" "obtenu: '$msg'"

# ═══════════════════════════════════════════════════════════
hdr "6. MODE MULTI-FICHIERS ═════════════════════════"

# Sortie identique à nm en multi-fichiers
if [ -f "$T/obj32.o" ]; then
    ft=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj64.o" "$T/obj32.o" 2>/dev/null)
    ref=$(LANG=C LC_ALL=C nm      "$T/obj64.o" "$T/obj32.o" 2>/dev/null)
    [ "$ft" = "$ref" ] && ok "multi-fichiers : sortie identique à nm" \
        || fail "multi-fichiers" "$(diff <(echo "$ft") <(echo "$ref") | head -5)"
else
    ft=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj64.o" "$T/lib64.so" 2>/dev/null)
    ref=$(LANG=C LC_ALL=C nm      "$T/obj64.o" "$T/lib64.so" 2>/dev/null)
    [ "$ft" = "$ref" ] && ok "multi-fichiers : sortie identique à nm" \
        || fail "multi-fichiers" "$(diff <(echo "$ft") <(echo "$ref") | head -5)"
fi

# Ligne vide avant le premier fichier
first=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj64.o" "$T/exec64" 2>/dev/null | head -1)
[ -z "$first" ] && ok "multi-fichiers : ligne vide avant le 1er header" \
    || fail "multi-fichiers ligne vide" "1ère ligne non vide : '$first'"

# Header filename:
second=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj64.o" "$T/exec64" 2>/dev/null | sed -n '2p')
[[ "$second" == *":" ]] && ok "multi-fichiers : header 'fichier:' sur la 2e ligne" \
    || fail "multi-fichiers header" "obtenu: '$second'"

# Fichier invalide intercalé → les fichiers valides sont quand même traités
out=$(LANG=C LC_ALL=C "$FT_NM" "$T/obj64.o" /no/such/path "$T/exec64" 2>&1)
echo "$out" | grep -q "g_init_int" \
    && ok "multi-fichiers : fichiers valides traités malgré erreur intercalée" \
    || fail "multi-fichiers erreur intercalée" "symboles absents"
echo "$out" | grep -qi "No such file\|not found" \
    && ok "multi-fichiers : message d'erreur pour fichier manquant" \
    || fail "multi-fichiers erreur message" "message absent"

# ═══════════════════════════════════════════════════════════
if [ "$USE_VALGRIND" -eq 1 ]; then
hdr "7. VALGRIND — fuites mémoire ══════════════════"

vg() {
    local label="$1"; shift
    local first_arg="${1-}"
    # Skip uniquement si c'est un binaire de test compilé qui n'existe pas (ex: 32-bit sans multilib)
    [[ "$first_arg" == "$T/"* ]] && [ ! -f "$first_arg" ] && { skip "valgrind $label (fichier absent)"; return; }
    local rc=0
    valgrind --leak-check=full --error-exitcode=99 --quiet "$FT_NM" "$@" >/dev/null 2>&1 || rc=$?
    # 99 = valgrind a détecté une erreur mémoire ; tout autre code = ft_nm qui échoue normalement
    [ "$rc" -eq 99 ] && fail "valgrind : $label" || ok "valgrind : $label"
}

vg "objet 64-bit"              "$T/obj64.o"
vg "objet 32-bit"              "$T/obj32.o"
vg "exécutable 64-bit"         "$T/exec64"
vg "exécutable 32-bit"         "$T/exec32"
vg "shared lib 64-bit"         "$T/lib64.so"
vg "shared lib 32-bit"         "$T/lib32.so"
vg "fichier inexistant"        /no/such/path
vg "fichier non-ELF"           "$T/not_elf.txt"
vg "fichier vide"              "$T/empty_file"
vg "ELF tronqué"               "$T/elf_truncated"
vg "ELF sh_offset OOB"        "$T/elf_bad_shoffset"
vg "ELF sh_link invalide"      "$T/elf_bad_shlink"
vg "ELF shnum=65535"           "$T/elf_giant_shnum"
vg "ELF shentsize=0"           "$T/elf_shentsize_zero"
vg "ELF st_name OOB"          "$T/elf_bad_stname"
vrc=0
valgrind --leak-check=full --error-exitcode=99 --quiet \
    "$FT_NM" "$T/obj64.o" "$T/exec64" "$T/lib64.so" >/dev/null 2>&1 || vrc=$?
[ "$vrc" -eq 99 ] && fail "valgrind : multi-fichiers" || ok "valgrind : multi-fichiers"
fi

# ═══════════════════════════════════════════════════════════
hdr "8. MAKEFILE ════════════════════════════════════"

make clean > /dev/null 2>&1
find srcs/ -name "*.o" | grep -q . \
    && fail "make clean : des .o subsistent" \
    || ok "make clean : tous les .o supprimés"
[ -f ft_nm ] && ok "make clean : conserve le binaire ft_nm" \
    || fail "make clean : binaire supprimé à tort"
make fclean > /dev/null 2>&1
[ -f ft_nm ] && fail "make fclean : le binaire ft_nm devrait être supprimé" \
    || ok "make fclean : binaire ft_nm supprimé"
make all > /dev/null 2>&1
[ -x ft_nm ] && ok "make all : binaire ft_nm produit" \
    || fail "make all : binaire absent après compilation"
make all 2>&1 | grep -qi "nothing to be done\|rien à faire\|is up to date\|à jour" \
    && ok "make all (2x) : idempotent — rien recompilé" \
    || fail "make all (2x) : recompile inutilement"
make re > /dev/null 2>&1
[ -x ft_nm ] && ok "make re : recompilation complète OK" \
    || fail "make re : binaire absent après re"

# ═══════════════════════════════════════════════════════════
hdr "9. FONCTIONS AUTORISÉES (sujet) ════════════════"

ALLOWED="open close mmap munmap write fstat malloc free exit perror strerror getpagesize"
RUNTIME="__libc_start_main __stack_chk_fail __errno_location"

forbidden=0
while IFS= read -r fn; do
    fn_clean="${fn%%@*}"
    echo "$RUNTIME" | grep -qw "$fn_clean" && continue
    echo "$ALLOWED" | grep -qw "$fn_clean" && continue
    fail "fonction interdite appelée" "$fn_clean"
    forbidden=$((forbidden+1))
done < <(LANG=C nm ft_nm 2>/dev/null | awk '/ U /{print $2}')
[ $forbidden -eq 0 ] && ok "aucune fonction interdite dans le binaire"

# ═══════════════════════════════════════════════════════════
# ── Résumé ───────────────────────────────────────────────
TOTAL=$((PASS + FAIL + SKIP))
echo ""
printf "${BLD}╔══════════════════════════════════════════╗${RST}\n"
printf "${BLD}║  RÉSULTAT FINAL  :  %-3d tests lancés    ║${RST}\n" "$TOTAL"
printf "${BLD}║  ${GRN}PASS${RST}${BLD} %-3d  ${RED}FAIL${RST}${BLD} %-3d  ${YEL}SKIP${RST}${BLD} %-3d           ║${RST}\n" \
    "$PASS" "$FAIL" "$SKIP"
printf "${BLD}╚══════════════════════════════════════════╝${RST}\n"
echo ""

if [ $FAIL -eq 0 ]; then
    printf "${GRN}${BLD}Tous les tests passent.${RST}\n\n"
    exit 0
else
    printf "${RED}${BLD}%d test(s) échoué(s).${RST}\n\n" "$FAIL"
    exit 1
fi
