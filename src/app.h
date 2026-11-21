/*
 * BudgetLens — app.h
 * Données métier (Donnees), contexte d'exécution (Application) et rapport
 * d'erreurs. Aucune variable globale : le contexte est passé par pointeur.
 */
#ifndef BUDGETLENS_APP_H
#define BUDGETLENS_APP_H

#include <stdio.h>

#include "tableau.h"
#include "types.h"

typedef enum {
    RES_OK = 0,
    RES_ERR_MEMOIRE,
    RES_ERR_VALIDATION,
    RES_ERR_FICHIER,
    RES_ERR_INTROUVABLE,
    RES_ABANDON,
    /* Rien n'a été modifié : des avertissements (seuil minimal) attendent l'accord de l'utilisateur. */
    RES_CONFIRMATION_REQUISE
} Resultat;

/* ---- Rapport d'erreurs ou d'avertissements ---- */
#define MESSAGE_MAX 320
#define RAPPORT_MESSAGES_MAX 200

typedef struct {
    Tableau messages;        /* éléments : char[MESSAGE_MAX] */
    size_t nb_ignores;       /* messages au-delà de la limite ou sans mémoire */
} RapportErreurs;

void rapport_init(RapportErreurs *r);
void rapport_vider(RapportErreurs *r);
void rapport_liberer(RapportErreurs *r);
void rapport_ajouter(RapportErreurs *r, const char *format, ...);
size_t rapport_nombre(const RapportErreurs *r);
const char *rapport_message(const RapportErreurs *r, size_t indice);
/* Copie les messages de src dans dest, précédés de prefixe. */
void rapport_fusionner(RapportErreurs *dest, const RapportErreurs *src, const char *prefixe);
void rapport_afficher(const RapportErreurs *r, FILE *flux);

/* ---- Données métier : tout ce qui est enregistré, exporté, restauré ---- */
typedef struct {
    Tableau comptes;             /* Compte */
    Tableau categories;          /* Categorie */
    Tableau tiers;               /* Tiers */
    Tableau vehicules;           /* Vehicule */
    Tableau budgets;             /* BudgetMensuel */
    Tableau modeles;             /* ModeleOperation */
    Tableau lignes_modeles;      /* LigneModele */
    Tableau recurrences;         /* Recurrence */
    Tableau operations;          /* Operation */
    Tableau lignes_operations;   /* LigneOperation */
    Id prochain_id[NB_ENTITES];  /* metadata.csv : ne diminue jamais */
} Donnees;

void donnees_init(Donnees *d);
void donnees_liberer(Donnees *d);
/* Copie profonde ; en cas d'échec, dest est vide et false est renvoyé. */
bool donnees_copier(Donnees *dest, const Donnees *src);
Tableau *donnees_table(Donnees *d, Entite e);
const Tableau *donnees_table_const(const Donnees *d, Entite e);
/* Attribue le prochain identifiant ; ID_AUCUN si le compteur est épuisé. */
Id donnees_nouvel_id(Donnees *d, Entite e);
Id donnees_id_max(const Donnees *d, Entite e);
/* Relève un compteur inférieur au plus grand identifiant + 1 (avertissement). */
void donnees_relever_compteurs(Donnees *d, RapportErreurs *avertissements);

const char *entite_code(Entite e);       /* "COMPTE", ... (metadata.csv) */
const char *entite_libelle(Entite e);    /* "compte", ... (messages) */

/* ---- Contexte d'exécution ---- */
typedef struct {
    Donnees donnees;
    Date date_reference;         /* date système ou --date */
    bool date_simulee;
    bool modifie;                /* modifications non enregistrées */
    long nb_modifications;       /* actions validées depuis le dernier chargement ou enregistrement */
    int emplacement_charge;      /* 0 = aucun, 1 = A, 2 = B */
    long generation_chargee;
    long generation_max;         /* plus haute génération rencontrée */
    char dossier_donnees[CHEMIN_MAX + 1];
    char dossier_sauvegardes[CHEMIN_MAX + 1];   /* backups/ (par défaut : le dossier de données) */
} Application;

void app_init(Application *app, Date reference, bool simulee, const char *dossier);
void app_liberer(Application *app);

#endif
