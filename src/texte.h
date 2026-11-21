/*
 * BudgetLens — texte.h
 * Textes UTF-8 : validation, caractères interdits (décision T1),
 * comparaison insensible à la casse (français), espaces.
 */
#ifndef BUDGETLENS_TEXTE_H
#define BUDGETLENS_TEXTE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TEXTE_OK,
    TEXTE_VIDE,
    TEXTE_TROP_LONG,
    TEXTE_UTF8_INVALIDE,
    TEXTE_CARACTERE_INTERDIT
} EtatTexte;

bool utf8_valide(const char *s);

/*
 * Vérifie un texte : longueur <= max_octets, UTF-8 bien formé, aucun « ; »,
 * aucun caractère de contrôle (U+0000–U+001F, U+007F, U+0080–U+009F).
 * Un texte obligatoire ne peut être vide ni composé uniquement d'espaces.
 */
EtatTexte texte_verifier(const char *s, size_t max_octets, bool obligatoire);
const char *texte_message(EtatTexte etat);

/* Comparaison sans distinction de casse : A–Z et majuscules latines accentuées. */
int texte_comparer_sans_casse(const char *a, const char *b);
bool texte_egal_sans_casse(const char *a, const char *b);
bool texte_contient_sans_casse(const char *texte, const char *motif);

/*
 * Libellé normalisé (comparaison I6) : minuscules (même repli que
 * texte_comparer_sans_casse), espaces de début et de fin retirés, suites
 * d'espaces réduites à un seul. Aucune suppression d'accent ni normalisation
 * Unicode. Le résultat est tronqué si dest est trop petit.
 */
void texte_normaliser(const char *src, char *dest, size_t taille);

/* Retire les espaces de début et de fin (sur place). */
void texte_rogner(char *s);

/* Nombre de caractères affichés (points de code). */
size_t texte_largeur(const char *s);

/* Copie sûre (tronque si nécessaire, toujours terminée par un zéro). */
void texte_copier(char *dest, size_t taille, const char *src);

#endif
