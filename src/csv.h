/*
 * BudgetLens — csv.h
 * Format CSV restreint (décision T1) : séparateur « ; », aucun guillemet,
 * valeur absente = champ vide, UTF-8 valide obligatoire.
 */
#ifndef BUDGETLENS_CSV_H
#define BUDGETLENS_CSV_H

#include <stdio.h>

#include "app.h"
#include "types.h"

#define CSV_LIGNE_MAX 1024
#define CSV_CHAMPS_MAX 16

typedef enum {
    LECTURE_OK,
    LECTURE_FIN,
    LECTURE_TROP_LONGUE,
    LECTURE_ERREUR
} EtatLecture;

/* Lit une ligne sans « \n » ni « \r » final ; une BOM UTF-8 initiale est retirée si premiere. */
EtatLecture csv_lire_ligne(FILE *f, char *tampon, size_t taille, bool premiere);

/* Découpe sur place ; renvoie le nombre de champs (max + 1 s'il y en a trop). */
size_t csv_decouper(char *ligne, char *champs[], size_t max);

/*
 * Analyse des champs d'une ligne : la première erreur est conservée dans
 * message, avec le nom du champ concerné.
 */
typedef struct {
    bool ok;
    char message[MESSAGE_MAX];
} AnalyseCsv;

void csv_analyse_init(AnalyseCsv *a);
void csv_echec(AnalyseCsv *a, const char *champ, const char *motif);

Id csv_id(AnalyseCsv *a, const char *s, const char *champ, bool facultatif);
long csv_entier(AnalyseCsv *a, const char *s, const char *champ, long min, long max, bool facultatif);
long long csv_centimes(AnalyseCsv *a, const char *s, const char *champ,
                       long long min, long long max, bool facultatif, bool *present);
Date csv_date(AnalyseCsv *a, const char *s, const char *champ, bool facultatif);
bool csv_booleen(AnalyseCsv *a, const char *s, const char *champ);
void csv_texte(AnalyseCsv *a, const char *s, const char *champ, char *dest, size_t max_octets,
               bool obligatoire);

#endif
