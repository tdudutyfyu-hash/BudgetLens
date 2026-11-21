/*
 * BudgetLens — types.h
 * Constantes, énumérations et structures des dix entités du MLD.
 * Chaque structure d'entité commence par son identifiant (Id) : cette
 * convention permet la recherche générique par identifiant (tableau.c).
 */
#ifndef BUDGETLENS_TYPES_H
#define BUDGETLENS_TYPES_H

#include <stdbool.h>
#include <stddef.h>

/* Identifiants : entiers strictement positifs ; 0 = référence absente. */
typedef int Id;
#define ID_AUCUN 0
#define ID_MAX 2147483646

/* Date du calendrier ; annee == 0 représente une date absente (NULL). */
typedef struct {
    int annee;
    int mois;
    int jour;
} Date;

#define DATE_TEXTE 11            /* "AAAA-MM-JJ" + zéro final */
#define ANNEE_MIN 1900
#define ANNEE_MAX 9999

/* Longueurs maximales des textes, en octets UTF-8 (zéro final non compris). */
#define NOM_MAX 60
#define LIBELLE_MAX 120
#define NOM_BANQUE_MAX 60
#define REFERENCE_MAX 40
#define CHEMIN_MAX 200

/* Montants en centimes : 100 000 000 000 ct = 1 milliard de DH. */
#define MONTANT_MAX_CT 100000000000LL
#define MONTANT_TEXTE 48

typedef enum { COMPTE_BANQUE, COMPTE_ESPECES } TypeCompte;
typedef enum { CAT_DEPENSE, CAT_REVENU } TypeCategorie;
typedef enum { NATURE_DEPENSE, NATURE_REVENU, NATURE_VIREMENT } Nature;
typedef enum { STATUT_A_CONFIRMER, STATUT_CONFIRMEE, STATUT_ANNULEE } Statut;
typedef enum { FREQ_HEBDOMADAIRE, FREQ_MENSUELLE } Frequence;

/* Ordre identique à celui des fichiers CSV et de metadata.csv. */
typedef enum {
    ENT_COMPTE,
    ENT_CATEGORIE,
    ENT_TIERS,
    ENT_VEHICULE,
    ENT_BUDGET,
    ENT_MODELE,
    ENT_LIGNE_MODELE,
    ENT_RECURRENCE,
    ENT_OPERATION,
    ENT_LIGNE_OPERATION,
    NB_ENTITES
} Entite;

typedef struct {
    Id id;
    char nom[NOM_MAX + 1];
    TypeCompte type;
    long long solde_initial_ct;          /* >= 0 (décision G10) */
    Date date_solde_initial;
    bool a_seuil;                        /* seuil minimal renseigné ? */
    long long seuil_minimal_ct;          /* >= 0, avertissement seulement */
    char nom_banque[NOM_BANQUE_MAX + 1];
    char reference_compte[REFERENCE_MAX + 1];
} Compte;

typedef struct {
    Id id;
    char nom[NOM_MAX + 1];
    TypeCategorie type;
    Id id_parent;                        /* ID_AUCUN = racine */
} Categorie;

typedef struct {
    Id id;
    char nom[NOM_MAX + 1];
} Tiers;

typedef struct {
    Id id;
    char nom[NOM_MAX + 1];
} Vehicule;

typedef struct {
    Id id;
    int annee;
    int mois;
    long long plafond_ct;                /* >= 0 */
    Id id_categorie;                     /* ID_AUCUN = budget global */
} BudgetMensuel;

typedef struct {
    Id id;
    char nom[NOM_MAX + 1];
    Nature nature;
    char libelle[LIBELLE_MAX + 1];
    long long montant_total_ct;
    Id id_compte_source;
    Id id_compte_destination;
    Id id_tiers;
} ModeleOperation;

typedef struct {
    Id id;
    Id id_modele;
    Id id_categorie;
    Id id_vehicule;
    long long montant_ct;
} LigneModele;

typedef struct {
    Id id;
    Id id_modele;
    Frequence frequence;
    Date date_debut;
    Date date_fin;                       /* facultative, inclusive */
    int jour_mensuel_souhaite;           /* 1..31 si mensuelle, 0 sinon */
    bool active;
    Date date_prochaine_echeance;        /* absente = plus d'échéance */
} Recurrence;

typedef struct {
    Id id;
    Nature nature;
    Statut statut;
    Date date_realisation;
    char libelle[LIBELLE_MAX + 1];
    long long montant_total_ct;
    Id id_compte_source;
    Id id_compte_destination;
    Id id_tiers;
    Id id_recurrence;                    /* ID_AUCUN = opération manuelle */
    Date date_prevue_origine;
} Operation;

typedef struct {
    Id id;
    Id id_operation;
    Id id_categorie;
    Id id_vehicule;
    long long montant_ct;
} LigneOperation;

#endif
