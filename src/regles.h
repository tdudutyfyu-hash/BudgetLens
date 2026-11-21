/*
 * BudgetLens — regles.h
 * Recherches, requêtes d'usage, validateur global des invariants du MLD et
 * transactions « copier, modifier, valider, remplacer ».
 */
#ifndef BUDGETLENS_REGLES_H
#define BUDGETLENS_REGLES_H

#include "app.h"

/* ---- Recherche par identifiant (NULL si absent) ---- */
Compte *regles_compte(const Donnees *d, Id id);
Categorie *regles_categorie(const Donnees *d, Id id);
Tiers *regles_tiers(const Donnees *d, Id id);
Vehicule *regles_vehicule(const Donnees *d, Id id);
BudgetMensuel *regles_budget(const Donnees *d, Id id);

/* ---- Usages : nombre d'enregistrements qui référencent une cible ---- */
typedef struct {
    size_t operations;          /* opérations (comptes, tiers) */
    size_t modeles;             /* modèles (comptes, tiers) */
    size_t lignes_operations;   /* lignes d'opération (catégories, véhicule) */
    size_t lignes_modeles;      /* lignes de modèle (catégories, véhicule) */
    size_t budgets;             /* budgets directement rattachés (catégories) */
    size_t enfants;             /* sous-catégories directes (catégories) */
} Usages;

Usages regles_usages_compte(const Donnees *d, Id id);
Usages regles_usages_tiers(const Donnees *d, Id id);
Usages regles_usages_vehicule(const Donnees *d, Id id);
Usages regles_usages_categorie(const Donnees *d, Id id);

bool regles_categorie_est_feuille(const Donnees *d, Id id);
/* "Alimentation > Courses" ; tronqué si nécessaire. */
void regles_chemin_categorie(const Donnees *d, Id id, char *sortie, size_t taille);

/* Unicités sans distinction de casse ; id_exclu = enregistrement modifié. */
bool regles_nom_compte_libre(const Donnees *d, const char *nom, Id id_exclu);
bool regles_nom_categorie_libre(const Donnees *d, Id id_parent, const char *nom, Id id_exclu);

/*
 * Validateur global : renvoie true si toutes les règles sont respectées ;
 * sinon ajoute un message par violation dans r.
 */
bool regles_valider(const Donnees *d, RapportErreurs *r);

/* La catégorie « categorie » est-elle « racine » ou un de ses descendants ? */
bool regles_categorie_dans_sous_arbre(const Donnees *d, Id racine, Id categorie);

/* ---- Transactions ---- */
/* Copie profonde des données courantes dans copie. */
Resultat transaction_debut(const Application *app, Donnees *copie);
/* Valide la copie ; si elle est valide, elle remplace les données courantes. */
Resultat transaction_valider(Application *app, Donnees *copie, RapportErreurs *r);
/*
 * Comme transaction_valider, avec les avertissements de seuil minimal (G10).
 * Si l'action abaisse un solde de fin de journée sous le seuil d'un compte et
 * que accord_seuil est faux, rien n'est remplacé, la copie est libérée, les
 * avertissements sont ajoutés à « avertissements » et RES_CONFIRMATION_REQUISE
 * est renvoyé. Dans tous les cas la copie est consommée.
 */
Resultat transaction_valider_seuil(Application *app, Donnees *copie, bool accord_seuil,
                                   RapportErreurs *avertissements, RapportErreurs *r);
void transaction_annuler(Donnees *copie);

#endif
