/*
 * BudgetLens — calendrier.h
 * Calendrier des récurrences (conception technique §11.1). Fonctions pures,
 * sans accès aux données : utilisables par le validateur et par la génération.
 * Une date absente (annee == 0) représente « aucune échéance ».
 */
#ifndef BUDGETLENS_CALENDRIER_H
#define BUDGETLENS_CALENDRIER_H

#include "types.h"

/*
 * Première échéance : HEBDOMADAIRE = date_debut ; MENSUELLE = la plus petite
 * date effective >= date_debut, au jour min(jour souhaité, jours du mois).
 */
Date cal_premiere_echeance(Frequence f, Date date_debut, int jour_souhaite);

/*
 * Échéance qui suit « courante » : +7 jours, ou mois suivant au jour
 * min(jour souhaité, jours du mois) — le jour souhaité est conservé après un
 * mois court. Date absente si l'on dépasse l'année maximale.
 */
Date cal_echeance_suivante(Frequence f, Date courante, int jour_souhaite);

/* Date absente si date_fin est renseignée et que d la dépasse (fin incluse). */
Date cal_borner(Date d, Date date_fin);

/* d est-elle une échéance du calendrier de r (début, fin incluse, jour) ? */
bool cal_sur_calendrier(const Recurrence *r, Date d);

#endif
