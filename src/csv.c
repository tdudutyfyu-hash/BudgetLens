/*
 * BudgetLens — csv.c
 */
#include "csv.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "texte.h"
#include "valeurs.h"

EtatLecture csv_lire_ligne(FILE *f, char *tampon, size_t taille, bool premiere)
{
    size_t n;

    if (fgets(tampon, (int)taille, f) == NULL) {
        return ferror(f) ? LECTURE_ERREUR : LECTURE_FIN;
    }
    n = strlen(tampon);
    if (n > 0 && tampon[n - 1] == '\n') {
        tampon[--n] = '\0';
    } else if (!feof(f)) {
        int c;
        /* Ligne trop longue : consommer la fin de la ligne. */
        while ((c = fgetc(f)) != EOF && c != '\n') {
        }
        return LECTURE_TROP_LONGUE;
    }
    if (n > 0 && tampon[n - 1] == '\r') {
        tampon[--n] = '\0';
    }
    if (premiere && (unsigned char)tampon[0] == 0xEF && (unsigned char)tampon[1] == 0xBB
        && (unsigned char)tampon[2] == 0xBF) {
        memmove(tampon, tampon + 3, n - 2);
    }
    return LECTURE_OK;
}

size_t csv_decouper(char *ligne, char *champs[], size_t max)
{
    size_t n = 0;
    char *p = ligne;

    for (;;) {
        char *separateur = strchr(p, ';');
        if (n == max) {
            return max + 1;
        }
        champs[n++] = p;
        if (separateur == NULL) {
            break;
        }
        *separateur = '\0';
        p = separateur + 1;
    }
    return n;
}

void csv_analyse_init(AnalyseCsv *a)
{
    a->ok = true;
    a->message[0] = '\0';
}

void csv_echec(AnalyseCsv *a, const char *champ, const char *motif)
{
    if (a->ok) {
        a->ok = false;
        if (snprintf(a->message, sizeof a->message, "champ « %s » : %s", champ, motif) < 0) {
            a->message[0] = '\0';
        }
    }
}

/* Entier décimal strict : chiffres uniquement (signe « - » si permis). */
static bool entier_strict(const char *s, bool negatif_permis, long long *v)
{
    const char *p = s;
    char *fin;
    long long x;

    if (*p == '-' && negatif_permis) {
        p++;
    }
    if (!isdigit((unsigned char)*p)) {
        return false;
    }
    for (; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
    }
    errno = 0;
    x = strtoll(s, &fin, 10);
    if (errno == ERANGE || *fin != '\0') {
        return false;
    }
    *v = x;
    return true;
}

Id csv_id(AnalyseCsv *a, const char *s, const char *champ, bool facultatif)
{
    long long v;

    if (s[0] == '\0') {
        if (!facultatif) {
            csv_echec(a, champ, "identifiant obligatoire");
        }
        return ID_AUCUN;
    }
    if (!entier_strict(s, false, &v) || v < 1 || v > ID_MAX) {
        csv_echec(a, champ, "identifiant invalide");
        return ID_AUCUN;
    }
    return (Id)v;
}

long csv_entier(AnalyseCsv *a, const char *s, const char *champ, long min, long max, bool facultatif)
{
    long long v;

    if (s[0] == '\0') {
        if (!facultatif) {
            csv_echec(a, champ, "valeur obligatoire");
        }
        return 0;
    }
    if (!entier_strict(s, min < 0, &v) || v < min || v > max) {
        csv_echec(a, champ, "entier invalide ou hors limites");
        return 0;
    }
    return (long)v;
}

long long csv_centimes(AnalyseCsv *a, const char *s, const char *champ,
                       long long min, long long max, bool facultatif, bool *present)
{
    long long v;

    if (present != NULL) {
        *present = false;
    }
    if (s[0] == '\0') {
        if (!facultatif) {
            csv_echec(a, champ, "montant obligatoire");
        }
        return 0;
    }
    if (!entier_strict(s, min < 0, &v) || v < min || v > max) {
        csv_echec(a, champ, "montant en centimes invalide ou hors limites");
        return 0;
    }
    if (present != NULL) {
        *present = true;
    }
    return v;
}

Date csv_date(AnalyseCsv *a, const char *s, const char *champ, bool facultatif)
{
    Date d;

    if (s[0] == '\0') {
        if (!facultatif) {
            csv_echec(a, champ, "date obligatoire");
        }
        return date_aucune();
    }
    if (!date_lire(s, &d)) {
        csv_echec(a, champ, "date invalide (format AAAA-MM-JJ attendu)");
        return date_aucune();
    }
    return d;
}

bool csv_booleen(AnalyseCsv *a, const char *s, const char *champ)
{
    if (strcmp(s, "1") == 0) {
        return true;
    }
    if (strcmp(s, "0") != 0) {
        csv_echec(a, champ, "booléen invalide (0 ou 1 attendu)");
    }
    return false;
}

void csv_texte(AnalyseCsv *a, const char *s, const char *champ, char *dest, size_t max_octets,
               bool obligatoire)
{
    EtatTexte etat = texte_verifier(s, max_octets, obligatoire);

    dest[0] = '\0';
    if (etat != TEXTE_OK) {
        csv_echec(a, champ, texte_message(etat));
        return;
    }
    texte_copier(dest, max_octets + 1, s);
}
