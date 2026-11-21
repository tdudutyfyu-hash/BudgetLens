/*
 * BudgetLens — analyses.h
 * Recherche et tri des opérations (F03, F04) et statistiques des dépenses
 * (F06). Fonctions pures : aucune saisie, aucune modification des données.
 */
#ifndef BUDGETLENS_ANALYSES_H
#define BUDGETLENS_ANALYSES_H

#include "app.h"

typedef struct {
    bool par_nature;
    Nature nature;
    bool par_statut;                /* faux : tous les statuts */
    Statut statut;
    bool par_periode;
    Date debut;                     /* inclusive */
    Date fin;                       /* inclusive */
    Id id_categorie;                /* ID_AUCUN = pas de filtre ; descendants inclus */
    Id id_compte;                   /* compte source ou destination */
    Id id_tiers;
    char texte[LIBELLE_MAX + 1];    /* contenu du libellé, sans distinction de casse ; "" = aucun */
} FiltreOperations;

void filtre_init(FiltreOperations *f);

typedef struct {
    size_t indice;                  /* position dans Donnees.operations */
    Id id;
    long jours;                     /* date effective en jours */
    long long total_ct;
    long long attribuable_ct;       /* lignes dans le sous-arbre filtré, sinon le total */
    long long cle;                  /* clé de tri (renseignée par analyses_trier) */
} ResultatRecherche;

/*
 * Remplit « resultats » (Tableau de ResultatRecherche, initialisé par
 * l'appelant) avec les opérations qui satisfont tous les filtres.
 * La période s'applique à la date réelle ou, à défaut, à la date prévue.
 * Avec un filtre de catégorie, seules les opérations ayant au moins une
 * ligne dans le sous-arbre sont retenues (les virements en sont exclus).
 * Faux si la mémoire manque.
 */
bool analyses_rechercher(const Donnees *d, const FiltreOperations *f, Tableau *resultats);

typedef enum { TRI_DATE, TRI_MONTANT, TRI_ATTRIBUABLE } CleTri;

/* Tri par qsort ; les égalités sont départagées par identifiant croissant. */
void analyses_trier(Tableau *resultats, CleTri cle, bool croissant);

typedef struct {
    size_t nombre;
    long long total_ct;
    long long moyenne_ct;           /* arrondie au centime le plus proche (demi au-dessus) */
    long long minimum_ct;
    long long maximum_ct;
} StatistiquesDepenses;

/*
 * Statistiques des dépenses CONFIRMEE dont la date réelle est dans
 * [debut, fin], limitées au sous-arbre id_categorie si renseigné (montant
 * attribuable). Chaque opération compte une seule fois. Avec nombre == 0, les
 * autres champs valent 0 (l'appelant affiche un message, pas de division).
 * Faux si la mémoire manque ou si une somme déborde.
 */
bool analyses_statistiques(const Donnees *d, Date debut, Date fin, Id id_categorie,
                           StatistiquesDepenses *s);

/* ---- Choix d'une catégorie par son nom ---- */
typedef struct {
    Id id;
    char chemin[CHEMIN_MAX + 1];    /* "Alimentation > Courses" */
} CategorieTrouvee;

/*
 * Catégories terminales du type donné dont le chemin complet contient « motif »
 * (sans distinction de casse, texte_contient_sans_casse ; "" = toutes). Si le
 * motif est exactement le chemin complet d'une candidate, elle seule est
 * retenue (chemin unique, B2). « resultats » : Tableau de CategorieTrouvee
 * (initialisé par l'appelant), trié par chemin puis identifiant. Aucune limite
 * de nombre. Faux si la mémoire manque.
 */
bool analyses_categories_par_nom(const Donnees *d, TypeCategorie type, const char *motif,
                                 Tableau *resultats);

/* ---- Dernières opérations ---- */
/*
 * Les « max » opérations les plus récentes (tous statuts) par date effective
 * (date réelle, ou date prévue d'une échéance) décroissante ; à date égale,
 * identifiant croissant (règle de analyses_trier). « resultats » : Tableau de
 * ResultatRecherche ; le stockage n'est pas limité, seul le résultat l'est.
 * *total reçoit le nombre total d'opérations. Faux si la mémoire manque.
 */
bool analyses_dernieres_operations(const Donnees *d, size_t max, Tableau *resultats, size_t *total);

/* ---- Doublons potentiels (I6) ---- */
typedef struct {
    size_t debut;               /* position du premier membre dans « membres » */
    size_t nombre;              /* au moins 2 */
} GroupeDoublons;

/*
 * Groupes d'opérations CONFIRMEE de même nature, date, total, comptes (mêmes
 * rôles), tiers (le même ou aucun), libellé normalisé et même multiensemble de
 * lignes (catégorie, montant, véhicule), quel que soit l'ordre des lignes.
 * « groupes » : Tableau de GroupeDoublons ; « membres » : Tableau de Id,
 * identifiants croissants dans un groupe (le premier est proposé à conserver).
 * Les occurrences récurrentes peuvent figurer dans un groupe. Faux si la
 * mémoire manque.
 */
bool analyses_doublons(const Donnees *d, Tableau *groupes, Tableau *membres);

/* ---- Totaux par catégorie, budgets, bilan mensuel (G13) ---- */
/*
 * Opérations CONFIRMEE de la nature donnée dont la date réelle est dans
 * [debut, fin] : totaux[i] (i = position dans Donnees.categories, tableau de
 * categories.taille entrées fourni par l'appelant) reçoit la somme des lignes
 * du sous-arbre de la catégorie i ; chaque ligne n'est comptée qu'une fois par
 * catégorie. *total_global = somme des montants des opérations (jamais la somme
 * des sous-totaux hiérarchiques). Faux si la mémoire manque ou si une somme déborde.
 */
bool analyses_totaux_categories(const Donnees *d, Date debut, Date fin, Nature nature,
                                long long *totaux, long long *total_global);

typedef struct {
    bool defini;                /* faux : « budget non défini » */
    long long plafond_ct;
    long long depense_ct;
    long long reste_ct;         /* max(0, plafond - dépense) ; 0 si non défini */
    long long depassement_ct;   /* max(0, dépense - plafond) ; 0 si non défini */
} SituationBudget;

/* id_categorie = ID_AUCUN pour le budget global. Un plafond nul est un budget défini. */
SituationBudget analyses_situation_budget(const Donnees *d, int annee, int mois, Id id_categorie,
                                          long long depense_ct);

/* ---- Dépenses du véhicule (G11) ---- */
typedef struct {
    Id id_operation;
    long jours;
    Id id_categorie;
    long long montant_ct;       /* montant de la ligne rattachée au véhicule */
} LigneVehicule;

typedef struct {
    Id id_categorie;
    long long total_ct;
    char chemin[CHEMIN_MAX + 1];
} TotalCategorie;

/*
 * Lignes de dépenses CONFIRMEE rattachées au véhicule sur la période (seules
 * les lignes véhicule comptent). « lignes » : Tableau de LigneVehicule trié par
 * date puis opération ; « par_categorie » : Tableau de TotalCategorie trié par
 * chemin complet. Faux si la mémoire manque ou si une somme déborde.
 */
bool analyses_vehicule(const Donnees *d, Id id_vehicule, Date debut, Date fin, Tableau *lignes,
                       Tableau *par_categorie, long long *total_ct);

#endif
