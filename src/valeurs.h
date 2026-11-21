/*
 * BudgetLens — valeurs.h
 * Dates du calendrier, montants en centimes et codes des énumérations.
 */
#ifndef BUDGETLENS_VALEURS_H
#define BUDGETLENS_VALEURS_H

#include "types.h"

/* ---- Dates ---- */
Date date_aucune(void);
Date date_creer(int annee, int mois, int jour);
bool date_absente(Date d);
bool annee_bissextile(int annee);
int jours_dans_mois(int annee, int mois);
bool date_valide(Date d);
int date_comparer(Date a, Date b);               /* <0, 0, >0 */
bool date_lire(const char *texte, Date *d);      /* format strict AAAA-MM-JJ */
void date_formater(Date d, char sortie[DATE_TEXTE]); /* "" si absente */
long date_vers_jours(Date d);                    /* jours depuis 1970-01-01 */
Date jours_vers_date(long jours);
Date date_ajouter_jours(Date d, long jours);
Date date_aujourdhui(void);                      /* absente en cas d'échec */

/* ---- Montants (centimes) ---- */
/* Accepte "120", "120,5", "120,50", "120.50" ; pas de signe ; <= MONTANT_MAX_CT. */
bool montant_lire(const char *texte, long long *ct);
/* "12 345,67 DH" ; sortie de MONTANT_TEXTE octets au moins. */
void montant_formater(long long ct, char *sortie, size_t taille);
bool montant_additionner(long long a, long long b, long long *resultat);

/*
 * Liste d'identifiants « 12,15,18 » (espaces tolérés autour des virgules).
 * Refuse un élément vide ou non numérique, un identifiant <= 0 ou trop grand,
 * un doublon et un dépassement de max. n reçoit le nombre d'identifiants lus.
 */
bool liste_ids_lire(const char *texte, Id *ids, size_t max, size_t *n);

/* ---- Énumérations : codes CSV et libellés d'affichage ---- */
const char *type_compte_code(TypeCompte t);
const char *type_compte_libelle(TypeCompte t);
bool type_compte_lire(const char *code, TypeCompte *t);

const char *type_categorie_code(TypeCategorie t);
const char *type_categorie_libelle(TypeCategorie t);
bool type_categorie_lire(const char *code, TypeCategorie *t);

const char *nature_code(Nature n);
const char *nature_libelle(Nature n);
bool nature_lire(const char *code, Nature *n);

const char *statut_code(Statut s);
const char *statut_libelle(Statut s);
bool statut_lire(const char *code, Statut *s);

const char *frequence_code(Frequence f);
const char *frequence_libelle(Frequence f);
bool frequence_lire(const char *code, Frequence *f);

#endif
