/*
 * BudgetLens — saisie.c
 */
#include "saisie.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "texte.h"
#include "valeurs.h"

EtatEntree saisie_etat_entree(void)
{
    if (ferror(stdin)) {
        return ENTREE_ERREUR;
    }
    if (feof(stdin)) {
        return ENTREE_FIN;
    }
    return ENTREE_DISPONIBLE;
}

EtatSaisie saisie_ligne(const char *invite, char *tampon, size_t taille)
{
    tampon[0] = '\0';
    for (;;) {
        size_t n;

        /* Entrée déjà perdue (fin ou erreur) : ne jamais relire, pas de boucle infinie. */
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return SAISIE_FIN;
        }
        printf("%s", invite);
        fflush(stdout);
        if (fgets(tampon, (int)taille, stdin) == NULL) {
            tampon[0] = '\0';
            printf("\n");
            return SAISIE_FIN;
        }
        n = strlen(tampon);
        if (n > 0 && tampon[n - 1] == '\n') {
            tampon[--n] = '\0';
        } else if (ferror(stdin)) {
            tampon[0] = '\0';
            return SAISIE_FIN;
        } else if (!feof(stdin)) {
            int c;
            while ((c = getchar()) != EOF && c != '\n') {
            }
            if (c == EOF) {
                tampon[0] = '\0';
                return SAISIE_FIN;
            }
            printf("Saisie trop longue, recommencez.\n");
            continue;
        }
        if (n > 0 && tampon[n - 1] == '\r') {
            tampon[--n] = '\0';
        }
        texte_rogner(tampon);
        return tampon[0] == '\0' ? SAISIE_VIDE : SAISIE_OK;
    }
}

EtatSaisie saisie_texte(const char *invite, char *dest, size_t max_octets, bool effacement_permis)
{
    char tampon[SAISIE_LIGNE_MAX];

    for (;;) {
        EtatTexte etat;
        EtatSaisie s = saisie_ligne(invite, tampon, sizeof tampon);

        if (s != SAISIE_OK) {
            return s;
        }
        if (effacement_permis && strcmp(tampon, "-") == 0) {
            return SAISIE_EFFACER;
        }
        etat = texte_verifier(tampon, max_octets, true);
        if (etat != TEXTE_OK) {
            printf("Texte refusé : %s (au plus %lu octets).\n", texte_message(etat),
                   (unsigned long)max_octets);
            continue;
        }
        texte_copier(dest, max_octets + 1, tampon);
        return SAISIE_OK;
    }
}

static bool entier_strict(const char *s, long *v)
{
    const char *p = s;
    char *fin;
    long x;

    if (*p == '-') {
        p++;
    }
    if (!isdigit((unsigned char)*p)) {
        return false;
    }
    errno = 0;
    x = strtol(s, &fin, 10);
    if (errno == ERANGE || *fin != '\0') {
        return false;
    }
    *v = x;
    return true;
}

EtatSaisie saisie_entier(const char *invite, long min, long max, long *valeur)
{
    char tampon[SAISIE_LIGNE_MAX];

    for (;;) {
        long v;
        EtatSaisie s = saisie_ligne(invite, tampon, sizeof tampon);

        if (s != SAISIE_OK) {
            return s;
        }
        if (!entier_strict(tampon, &v) || v < min || v > max) {
            printf("Entier attendu entre %ld et %ld.\n", min, max);
            continue;
        }
        *valeur = v;
        return SAISIE_OK;
    }
}

EtatSaisie saisie_id(const char *invite, Id *id)
{
    long v;
    EtatSaisie s = saisie_entier(invite, 1, ID_MAX, &v);

    if (s == SAISIE_OK) {
        *id = (Id)v;
    }
    return s;
}

EtatSaisie saisie_montant(const char *invite, bool zero_permis, bool effacement_permis,
                          long long *ct)
{
    char tampon[SAISIE_LIGNE_MAX];

    for (;;) {
        long long v;
        EtatSaisie s = saisie_ligne(invite, tampon, sizeof tampon);

        if (s != SAISIE_OK) {
            return s;
        }
        if (effacement_permis && strcmp(tampon, "-") == 0) {
            return SAISIE_EFFACER;
        }
        if (!montant_lire(tampon, &v)) {
            printf("Montant invalide : exemples acceptés 120 ; 120,5 ; 120,50 ; 120.50 "
                   "(sans signe ni séparateur de milliers).\n");
            continue;
        }
        if (v == 0 && !zero_permis) {
            printf("Le montant doit être strictement positif.\n");
            continue;
        }
        *ct = v;
        return SAISIE_OK;
    }
}

EtatSaisie saisie_date(const char *invite, bool effacement_permis, Date *d)
{
    char tampon[SAISIE_LIGNE_MAX];

    for (;;) {
        Date x;
        EtatSaisie s = saisie_ligne(invite, tampon, sizeof tampon);

        if (s != SAISIE_OK) {
            return s;
        }
        if (effacement_permis && strcmp(tampon, "-") == 0) {
            return SAISIE_EFFACER;
        }
        if (!date_lire(tampon, &x)) {
            printf("Date invalide : format AAAA-MM-JJ, date existante entre %d et %d.\n",
                   ANNEE_MIN, ANNEE_MAX);
            continue;
        }
        *d = x;
        return SAISIE_OK;
    }
}

EtatSaisie saisie_oui_non(const char *invite, bool *oui)
{
    char tampon[SAISIE_LIGNE_MAX];

    for (;;) {
        EtatSaisie s = saisie_ligne(invite, tampon, sizeof tampon);

        if (s == SAISIE_FIN) {
            return s;
        }
        if (strcmp(tampon, "o") == 0 || strcmp(tampon, "O") == 0 || strcmp(tampon, "oui") == 0) {
            *oui = true;
            return SAISIE_OK;
        }
        if (strcmp(tampon, "n") == 0 || strcmp(tampon, "N") == 0 || strcmp(tampon, "non") == 0) {
            *oui = false;
            return SAISIE_OK;
        }
        printf("Répondez par o (oui) ou n (non).\n");
    }
}

int saisie_menu(int max)
{
    long v;

    for (;;) {
        EtatSaisie s = saisie_entier("Votre choix : ", 0, max, &v);
        if (s == SAISIE_FIN) {
            return -1;
        }
        if (s == SAISIE_OK) {
            return (int)v;
        }
    }
}
