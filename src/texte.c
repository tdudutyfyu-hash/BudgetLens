/*
 * BudgetLens — texte.c
 * Les textes sont des chaînes UTF-8. Aucune modification silencieuse :
 * un texte non conforme est refusé avec un motif explicite.
 */
#include "texte.h"

#include <string.h>

/*
 * Décode le point de code suivant d'une chaîne UTF-8 supposée valide.
 * Un octet invalide est renvoyé tel quel (aucune lecture hors chaîne).
 */
static unsigned long point_suivant(const unsigned char **p)
{
    const unsigned char *s = *p;
    unsigned long cp;
    int suite;
    int i;

    if (s[0] < 0x80) {
        *p = s + 1;
        return s[0];
    }
    if (s[0] >= 0xC2 && s[0] <= 0xDF) {
        suite = 1;
        cp = s[0] & 0x1Fu;
    } else if (s[0] >= 0xE0 && s[0] <= 0xEF) {
        suite = 2;
        cp = s[0] & 0x0Fu;
    } else if (s[0] >= 0xF0 && s[0] <= 0xF4) {
        suite = 3;
        cp = s[0] & 0x07u;
    } else {
        *p = s + 1;
        return s[0];
    }
    for (i = 1; i <= suite; i++) {
        if ((s[i] & 0xC0u) != 0x80u) {
            *p = s + 1;
            return s[0];
        }
        cp = (cp << 6) | (s[i] & 0x3Fu);
    }
    *p = s + suite + 1;
    return cp;
}

bool utf8_valide(const char *texte)
{
    const unsigned char *p = (const unsigned char *)texte;

    while (*p != '\0') {
        unsigned char c = *p;
        unsigned long cp;
        int suite;
        int i;

        if (c < 0x80) {
            p++;
            continue;
        }
        if (c >= 0xC2 && c <= 0xDF) {
            suite = 1;
            cp = c & 0x1Fu;
        } else if (c >= 0xE0 && c <= 0xEF) {
            suite = 2;
            cp = c & 0x0Fu;
        } else if (c >= 0xF0 && c <= 0xF4) {
            suite = 3;
            cp = c & 0x07u;
        } else {
            return false;
        }
        for (i = 1; i <= suite; i++) {
            if ((p[i] & 0xC0u) != 0x80u) {
                return false;
            }
            cp = (cp << 6) | (p[i] & 0x3Fu);
        }
        if (suite == 2 && (cp < 0x800u || (cp >= 0xD800u && cp <= 0xDFFFu))) {
            return false;
        }
        if (suite == 3 && (cp < 0x10000u || cp > 0x10FFFFu)) {
            return false;
        }
        p += suite + 1;
    }
    return true;
}

EtatTexte texte_verifier(const char *s, size_t max_octets, bool obligatoire)
{
    const unsigned char *p;
    bool que_des_espaces = true;

    if (s[0] == '\0') {
        return obligatoire ? TEXTE_VIDE : TEXTE_OK;
    }
    if (strlen(s) > max_octets) {
        return TEXTE_TROP_LONG;
    }
    if (!utf8_valide(s)) {
        return TEXTE_UTF8_INVALIDE;
    }
    p = (const unsigned char *)s;
    while (*p != '\0') {
        unsigned long cp = point_suivant(&p);
        if (cp < 0x20u || cp == 0x7Fu || (cp >= 0x80u && cp <= 0x9Fu) || cp == ';') {
            return TEXTE_CARACTERE_INTERDIT;
        }
        if (cp != ' ') {
            que_des_espaces = false;
        }
    }
    if (obligatoire && que_des_espaces) {
        return TEXTE_VIDE;
    }
    return TEXTE_OK;
}

const char *texte_message(EtatTexte etat)
{
    switch (etat) {
    case TEXTE_OK:
        return "texte valide";
    case TEXTE_VIDE:
        return "texte obligatoire vide";
    case TEXTE_TROP_LONG:
        return "texte trop long";
    case TEXTE_UTF8_INVALIDE:
        return "encodage UTF-8 invalide";
    case TEXTE_CARACTERE_INTERDIT:
        return "caractère interdit (« ; », retour à la ligne ou caractère de contrôle)";
    }
    return "texte invalide";
}

/* Passage en minuscules : A–Z, U+00C0–U+00DE (sauf ×), Œ et Ÿ. */
static unsigned long minuscule(unsigned long cp)
{
    if (cp >= 'A' && cp <= 'Z') {
        return cp + 32u;
    }
    if (cp >= 0xC0u && cp <= 0xDEu && cp != 0xD7u) {
        return cp + 0x20u;
    }
    if (cp == 0x152u) {
        return 0x153u;
    }
    if (cp == 0x178u) {
        return 0xFFu;
    }
    return cp;
}

int texte_comparer_sans_casse(const char *a, const char *b)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    while (*pa != '\0' && *pb != '\0') {
        unsigned long ca = minuscule(point_suivant(&pa));
        unsigned long cb = minuscule(point_suivant(&pb));
        if (ca != cb) {
            return ca < cb ? -1 : 1;
        }
    }
    if (*pa == '\0' && *pb == '\0') {
        return 0;
    }
    return (*pa == '\0') ? -1 : 1;
}

bool texte_egal_sans_casse(const char *a, const char *b)
{
    return texte_comparer_sans_casse(a, b) == 0;
}

bool texte_contient_sans_casse(const char *texte, const char *motif)
{
    const unsigned char *debut = (const unsigned char *)texte;

    if (motif[0] == '\0') {
        return true;
    }
    while (*debut != '\0') {
        const unsigned char *p = debut;
        const unsigned char *m = (const unsigned char *)motif;
        const unsigned char *suivant = debut;

        point_suivant(&suivant);
        while (*p != '\0' && *m != '\0') {
            if (minuscule(point_suivant(&p)) != minuscule(point_suivant(&m))) {
                break;
            }
        }
        if (*m == '\0') {
            return true;
        }
        debut = suivant;
    }
    return false;
}

/* Écrit le point de code en UTF-8 (1 à 4 octets) ; renvoie le nombre d'octets. */
static size_t encoder_utf8(unsigned long cp, unsigned char *sortie)
{
    if (cp < 0x80u) {
        sortie[0] = (unsigned char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        sortie[0] = (unsigned char)(0xC0u | (cp >> 6));
        sortie[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        sortie[0] = (unsigned char)(0xE0u | (cp >> 12));
        sortie[1] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        sortie[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    sortie[0] = (unsigned char)(0xF0u | (cp >> 18));
    sortie[1] = (unsigned char)(0x80u | ((cp >> 12) & 0x3Fu));
    sortie[2] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
    sortie[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
    return 4;
}

void texte_normaliser(const char *src, char *dest, size_t taille)
{
    const unsigned char *p = (const unsigned char *)src;
    size_t n = 0;
    bool espace_en_attente = false;

    if (taille == 0) {
        return;
    }
    while (*p != '\0') {
        unsigned long cp = point_suivant(&p);
        unsigned char octets[4];
        size_t k;

        if (cp == ' ') {
            espace_en_attente = (n > 0);   /* pas d'espace de tête ; les suites sont réduites */
            continue;
        }
        cp = minuscule(cp);
        k = encoder_utf8(cp, octets);
        if (n + k + (espace_en_attente ? 1u : 0u) >= taille) {
            break;
        }
        if (espace_en_attente) {
            dest[n++] = ' ';
            espace_en_attente = false;
        }
        memcpy(dest + n, octets, k);
        n += k;
    }
    dest[n] = '\0';   /* un espace final en attente n'est jamais écrit : fin rognée */
}

void texte_rogner(char *s)
{
    size_t debut = 0;
    size_t fin = strlen(s);

    while (s[debut] == ' ') {
        debut++;
    }
    while (fin > debut && s[fin - 1] == ' ') {
        fin--;
    }
    if (debut > 0) {
        memmove(s, s + debut, fin - debut);
    }
    s[fin - debut] = '\0';
}

size_t texte_largeur(const char *s)
{
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;

    for (; *p != '\0'; p++) {
        if ((*p & 0xC0u) != 0x80u) {
            n++;
        }
    }
    return n;
}

void texte_copier(char *dest, size_t taille, const char *src)
{
    size_t n;

    if (taille == 0) {
        return;
    }
    n = strlen(src);
    if (n >= taille) {
        n = taille - 1;
        /* Ne pas couper au milieu d'un caractère UTF-8. */
        while (n > 0 && (((const unsigned char *)src)[n] & 0xC0u) == 0x80u) {
            n--;
        }
    }
    memcpy(dest, src, n);
    dest[n] = '\0';
}
