/*
 * BudgetLens — soldes.h
 * Soldes chronologiques (décision G10) : le solde d'un compte est contrôlé
 * en fin de journée, toutes les opérations CONFIRMEE du jour étant appliquées
 * ensemble. Seules les opérations CONFIRMEE ont un effet financier.
 */
#ifndef BUDGETLENS_SOLDES_H
#define BUDGETLENS_SOLDES_H

#include "app.h"

/*
 * Série des soldes de fin de journée d'un compte : un point par jour où le
 * solde change, plus le jour du solde initial. Les points sont triés par jour.
 */
typedef struct {
    long jour;              /* jours depuis 1970-01-01 */
    long long solde_ct;     /* solde en fin de ce jour */
} PointSolde;

typedef struct {
    PointSolde *points;
    size_t n;
    long long initial_ct;   /* solde avant tout mouvement */
    long jour_initial;      /* date du solde initial, en jours */
} SerieSolde;

/* Faux si la mémoire manque ou si une somme déborde (la série est alors vide). */
bool serie_construire(const Donnees *d, const Compte *c, SerieSolde *s);
void serie_liberer(SerieSolde *s);
/* Solde en fin du jour donné : tous les mouvements de date <= jour (>= jour initial). */
long long serie_solde_au(const SerieSolde *s, long jour);

/*
 * G10 : ajoute une erreur par compte dont un solde de fin de journée est
 * négatif (compte, première date, solde obtenu, montant manquant).
 */
bool soldes_verifier(const Donnees *d, RapportErreurs *r);

/*
 * Solde d'un compte en fin de la date donnée. Faux si le compte est inconnu,
 * si la date précède la date du solde initial ou si une somme déborde.
 */
bool solde_compte_a_date(const Donnees *d, Id id_compte, Date date, long long *solde_ct);
/* Solde après toutes les opérations confirmées (quelle que soit leur date). */
bool solde_compte_final(const Donnees *d, Id id_compte, long long *solde_ct);

/*
 * Avertissements de seuil minimal : pour chaque compte ayant un seuil, ajoute
 * un message si l'action (avant -> apres) fait passer un solde de fin de
 * journée sous le seuil en l'abaissant. Renvoie le nombre d'avertissements.
 * Le solde reste >= 0 : le validateur a déjà été passé.
 */
size_t soldes_avertissements_seuil(const Donnees *avant, const Donnees *apres, RapportErreurs *w);

#endif
