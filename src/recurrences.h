/*
 * BudgetLens — recurrences.h
 * Récurrences : création rétroactive (G5), rattrapage, désactivation et
 * réactivation (Q4), modification du calendrier avant la première occurrence,
 * clôture (G6) et suppression. Chaque fonction est une transaction.
 *
 * Le curseur date_prochaine_echeance est la prochaine échéance à examiner ;
 * il est absent (NULL) quand il n'y a plus d'échéance (date de fin dépassée),
 * ce qui est distinct d'une récurrence en pause (active == false).
 */
#ifndef BUDGETLENS_RECURRENCES_H
#define BUDGETLENS_RECURRENCES_H

#include "app.h"

/* Au-delà, une action est refusée (pas de génération massive involontaire). */
#define ECHEANCES_DUES_MAX 500

typedef struct {
    Id id_modele;
    Frequence frequence;
    Date date_debut;
    Date date_fin;                  /* absente = sans fin ; inclusive */
    int jour_mensuel_souhaite;      /* 1..31 si mensuelle, 0 sinon */
} RecurrenceSaisie;

typedef enum { RETRO_GENERER, RETRO_IGNORER, RETRO_ANNULER } ChoixRetro;

typedef struct {
    size_t nombre;                  /* échéances dues (<= date de référence) */
    Date premiere;
    Date derniere;
    Date curseur;                   /* première échéance bornée (absente : aucune échéance) */
} ApercuEcheances;

/* Contrôle de la saisie (modèle existant, dates, jour mensuel) sans rien modifier. */
bool rec_controler_saisie(const Donnees *d, const RecurrenceSaisie *s, RapportErreurs *r);

/* Échéances déjà dues d'un calendrier, à la date de référence (pour proposer G5). */
bool rec_apercu(const RecurrenceSaisie *s, Date reference, ApercuEcheances *a);

/*
 * Création. S'il existe des échéances dues, choix = RETRO_GENERER (occurrences
 * A_CONFIRMER), RETRO_IGNORER (curseur placé strictement après la date de
 * référence, rien généré) ou RETRO_ANNULER (aucune modification, RES_ABANDON).
 */
Resultat rec_creer(Application *app, const RecurrenceSaisie *s, ChoixRetro choix, Id *id_cree,
                   size_t *nb_generees, RapportErreurs *r);

/* Calendrier modifiable seulement avant la première occurrence ; modèle immuable. */
Resultat rec_modifier_calendrier(Application *app, Id id, const RecurrenceSaisie *s,
                                 ChoixRetro choix, size_t *nb_generees, RapportErreurs *r);

/*
 * Clôture (G6) : ajoute ou raccourcit la date de fin, jamais en deçà de la date
 * prévue d'une occurrence existante (les conflits sont listés). Le curseur
 * devient NULL si la prochaine échéance dépasse la nouvelle fin.
 */
Resultat rec_cloturer(Application *app, Id id, Date nouvelle_fin, RapportErreurs *r);

/* Rattrape d'abord les échéances dues, puis met en pause. */
Resultat rec_desactiver(Application *app, Id id, size_t *nb_generees, RapportErreurs *r);
/* Reprend sans rattraper la période inactive : curseur strictement futur. */
Resultat rec_reactiver(Application *app, Id id, RapportErreurs *r);
/* Seulement si la récurrence n'a aucune occurrence. */
Resultat rec_supprimer(Application *app, Id id, RapportErreurs *r);

/*
 * Rattrapage (démarrage, après restauration) : pour chaque récurrence active,
 * crée les occurrences A_CONFIRMER manquantes jusqu'à la date de référence
 * et avance le curseur, en une seule transaction. Rien n'est modifié (ni
 * l'indicateur « modifié ») s'il n'y a rien à faire. Une récurrence dont le
 * nombre d'échéances dues dépasse ECHEANCES_DUES_MAX est signalée dans
 * « messages » et laissée intacte.
 */
Resultat rec_rattraper(Application *app, size_t *nb_creees, RapportErreurs *messages);

Recurrence *regles_recurrence(const Donnees *d, Id id);
size_t rec_nombre_occurrences(const Donnees *d, Id id_recurrence);

#endif
