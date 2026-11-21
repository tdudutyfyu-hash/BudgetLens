/*
 * BudgetLens — operations.h
 * Opérations manuelles (créées directement CONFIRMEE), ventilation en lignes,
 * correction et suppression physique. Aucune saisie console : les menus
 * collectent les valeurs et appellent ces fonctions.
 *
 * Chaque fonction est une transaction « copier, modifier, valider, remplacer » :
 * rien n'est modifié si une règle est violée. Le seuil minimal d'un compte est
 * un avertissement : si l'action l'abaisse sous le seuil et que accord_seuil
 * est faux, RES_CONFIRMATION_REQUISE est renvoyé (rien n'est modifié) et les
 * avertissements sont ajoutés à « avert » ; l'appelant peut alors redemander
 * l'accord de l'utilisateur et rappeler la fonction avec accord_seuil vrai.
 */
#ifndef BUDGETLENS_OPERATIONS_H
#define BUDGETLENS_OPERATIONS_H

#include "app.h"

typedef struct {
    Id id_categorie;
    Id id_vehicule;                 /* ID_AUCUN = pas de véhicule (dépense seulement) */
    long long montant_ct;
} LigneSaisie;

typedef struct {
    Nature nature;
    Date date_realisation;          /* ignorée pour une occurrence à confirmer */
    char libelle[LIBELLE_MAX + 1];
    long long montant_total_ct;
    Id id_compte_source;
    Id id_compte_destination;
    Id id_tiers;                    /* ID_AUCUN = sans tiers */
} OperationSaisie;

/* ---- Consultation ---- */
Operation *regles_operation(const Donnees *d, Id id);
size_t op_nombre_lignes(const Donnees *d, Id id_operation);
/* Copie au plus max lignes (ordre d'enregistrement) ; renvoie le nombre copié. */
size_t op_lignes_copier(const Donnees *d, Id id_operation, LigneSaisie *sortie, size_t max);
/* Date qui situe l'opération dans le temps : réelle si connue, sinon prévue. */
Date op_date_effective(const Operation *o);
/* Description lisible des comptes : « Banque », « Banque -> Espèces ». */
void op_description_comptes(const Donnees *d, const Operation *o, char *sortie, size_t taille);

/* ---- Opérations manuelles ---- */
/*
 * Vérifie une saisie sans rien modifier (message précis avant la validation
 * globale) : libellé, date (Q3, B3), montant, comptes selon la nature,
 * tiers, ventilation. avec_date est faux pour une occurrence non confirmée.
 */
bool op_controler_saisie(const Application *app, const OperationSaisie *s,
                         const LigneSaisie *lignes, size_t nb, bool avec_date,
                         RapportErreurs *r);

Resultat op_creer(Application *app, const OperationSaisie *s, const LigneSaisie *lignes,
                  size_t nb, bool accord_seuil, Id *id_cree, RapportErreurs *avert,
                  RapportErreurs *r);

/*
 * Corrige une opération manuelle (nature inchangée) ou une occurrence encore
 * A_CONFIRMER (date inchangée). Une occurrence CONFIRMEE ou ANNULEE n'est pas
 * modifiable (G3) ; seules l'annulation logique et la maintenance des
 * références s'y appliquent.
 */
Resultat op_corriger(Application *app, Id id, const OperationSaisie *s,
                     const LigneSaisie *lignes, size_t nb, bool accord_seuil,
                     RapportErreurs *avert, RapportErreurs *r);

/* Suppression physique d'une opération manuelle et de ses lignes. */
Resultat op_supprimer(Application *app, Id id, bool accord_seuil, RapportErreurs *avert,
                      RapportErreurs *r);

/*
 * Annulation logique d'une occurrence CONFIRMEE : statut ANNULEE, date réelle
 * conservée (Q7), plus aucun effet financier. G10 est revalidé (annuler un
 * revenu peut rendre un solde négatif). ANNULEE est un état final.
 */
Resultat op_annuler(Application *app, Id id, bool accord_seuil, RapportErreurs *avert,
                    RapportErreurs *r);

/*
 * Traitement d'un groupe de doublons, en une seule transaction : une opération
 * manuelle est supprimée physiquement avec ses lignes ; une occurrence
 * CONFIRMEE est seulement annulée (jamais supprimée). Tout ou rien.
 */
Resultat op_supprimer_ou_annuler(Application *app, const Id *ids, size_t nb, bool accord_seuil,
                                 RapportErreurs *avert, RapportErreurs *r);

/* ---- Occurrences à confirmer : confirmation, refus, lots (H5 : tout ou rien) ---- */
typedef struct {
    Id id;                          /* occurrence A_CONFIRMER */
    Date date_realisation;          /* explicite et obligatoire (jamais déduite) */
    long long nouveau_total_ct;     /* 0 = montant inchangé */
    size_t debut_lignes;            /* nouvelle ventilation : position dans le réservoir de lignes */
    size_t nb_lignes;               /* 0 = ventilation inchangée (ligne unique ajustée au besoin) */
} ConfirmationLot;

/*
 * Confirme des occurrences en une seule transaction : toutes les confirmations
 * sont appliquées à une copie de travail, le résultat est validé globalement
 * (Q3, G10 cumulés entre les échéances du lot, sommes) puis remplace les
 * données. La moindre erreur abandonne tout le lot et toutes les erreurs sont
 * listées. Date réelle : valide et pas dans le futur (B3). Un montant changé
 * ajuste automatiquement la ligne unique ; avec plusieurs lignes, une nouvelle
 * ventilation (réservoir « lignes ») est obligatoire. Avec « simuler », rien
 * n'est remplacé : le lot est seulement vérifié (erreurs signalées à la saisie).
 */
Resultat op_lot_confirmer(Application *app, const ConfirmationLot *items, size_t nb,
                          const LigneSaisie *lignes, bool simuler, bool accord_seuil,
                          RapportErreurs *avert, RapportErreurs *r);
Resultat op_confirmer(Application *app, Id id, Date date_realisation, long long nouveau_total_ct,
                      const LigneSaisie *lignes, size_t nb_lignes, bool accord_seuil,
                      RapportErreurs *avert, RapportErreurs *r);

/* Refus avant confirmation : ANNULEE sans date réelle. Un seul accord couvre tout le lot. */
Resultat op_lot_refuser(Application *app, const Id *ids, size_t nb, RapportErreurs *r);
Resultat op_refuser(Application *app, Id id, RapportErreurs *r);

#endif
