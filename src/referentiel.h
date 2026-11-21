/*
 * BudgetLens — referentiel.h
 * Comptes, tiers, véhicule, catégories et budgets mensuels. Chaque fonction
 * est une transaction : rien n'est modifié si une règle est violée.
 */
#ifndef BUDGETLENS_REFERENTIEL_H
#define BUDGETLENS_REFERENTIEL_H

#include "app.h"

/* ---- Comptes ---- */
Resultat ref_compte_creer(Application *app, const Compte *saisie, Id *id_cree, RapportErreurs *r);
/*
 * Le solde initial et sa date sont revalidés (Q3, G10). Si la modification
 * abaisse un solde sous le seuil minimal d'un compte et que accord_seuil est
 * faux, RES_CONFIRMATION_REQUISE est renvoyé sans rien modifier.
 */
Resultat ref_compte_modifier(Application *app, const Compte *nouveau, bool accord_seuil,
                             RapportErreurs *avert, RapportErreurs *r);
Resultat ref_compte_supprimer(Application *app, Id id, RapportErreurs *r);

/* ---- Tiers ---- */
Resultat ref_tiers_creer(Application *app, const char *nom, Id *id_cree, RapportErreurs *r);
Resultat ref_tiers_renommer(Application *app, Id id, const char *nom, RapportErreurs *r);
Resultat ref_tiers_supprimer(Application *app, Id id, RapportErreurs *r);

/* ---- Véhicule (au plus un) ---- */
Resultat ref_vehicule_creer(Application *app, const char *nom, Id *id_cree, RapportErreurs *r);
Resultat ref_vehicule_renommer(Application *app, Id id, const char *nom, RapportErreurs *r);
Resultat ref_vehicule_supprimer(Application *app, Id id, RapportErreurs *r);

/* ---- Catégories ---- */
/* type_racine n'est utilisé que pour une catégorie racine (id_parent absent). */
Resultat ref_categorie_creer(Application *app, const char *nom, Id id_parent,
                             TypeCategorie type_racine, Id *id_cree, RapportErreurs *r);
Resultat ref_categorie_renommer(Application *app, Id id, const char *nom, RapportErreurs *r);
/* avec_budgets : accord explicite pour supprimer les budgets directement rattachés. */
Resultat ref_categorie_supprimer(Application *app, Id id, bool avec_budgets, RapportErreurs *r);

/* ---- Budgets mensuels ---- */
/* id_categorie = ID_AUCUN pour un budget global. */
Resultat ref_budget_creer(Application *app, int annee, int mois, Id id_categorie,
                          long long plafond_ct, Id *id_cree, RapportErreurs *r);
Resultat ref_budget_modifier(Application *app, Id id, long long plafond_ct, RapportErreurs *r);
Resultat ref_budget_supprimer(Application *app, Id id, RapportErreurs *r);

/* ---- Catégories avancées : I3, déplacement, réaffectation 1A ---- */
/*
 * Création avec migration I3. Si le parent est une feuille déjà utilisée par des
 * lignes d'opération ou de modèle, ces lignes (tous statuts) sont déplacées vers
 * un enfant terminal « Autres » créé dans la même transaction (ou vers l'enfant
 * demandé s'il s'appelle « Autres »). Sans accord_migration, RES_CONFIRMATION_REQUISE
 * est renvoyé avec l'explication dans « avert » et rien n'est modifié. Les budgets
 * restent sur le parent.
 */
Resultat ref_categorie_creer_i3(Application *app, const char *nom, Id id_parent,
                                TypeCategorie type_racine, bool accord_migration, Id *id_cree,
                                RapportErreurs *avert, RapportErreurs *r);

/*
 * Déplace une catégorie (et son sous-arbre) sous nouveau_parent (ID_AUCUN : racine, de
 * type type_racine). Refusé sous elle-même ou un descendant, en cas de nom déjà pris,
 * ou si le type change alors que le sous-arbre est utilisé par des lignes ou des budgets.
 * Si le nouveau parent est une feuille utilisée, la migration I3 s'applique d'abord.
 * Si des budgets d'ancêtres voient leurs dépenses cumulées changer, ou si une migration
 * est nécessaire, RES_CONFIRMATION_REQUISE est renvoyé (aperçu dans « avert ») tant que
 * accord est faux.
 */
Resultat ref_categorie_deplacer(Application *app, Id id, Id nouveau_parent,
                                TypeCategorie type_racine, bool accord, RapportErreurs *avert,
                                RapportErreurs *r);

/*
 * Réaffectation 1A X -> Y, en une transaction, de toutes les références (opérations de
 * tous statuts et modèles). Tout ou rien : le moindre conflit annule l'ensemble.
 * Catégorie : Y est une feuille du même type que X ; compte : un virement (opération ou
 * modèle) dont l'autre compte est déjà Y fait refuser la réaffectation, Q3 et G10
 * s'appliquent ; tiers : Y différent de X.
 */
Resultat ref_categorie_reaffecter(Application *app, Id x, Id y, RapportErreurs *r);
Resultat ref_compte_reaffecter(Application *app, Id x, Id y, bool accord_seuil,
                               RapportErreurs *avert, RapportErreurs *r);
Resultat ref_tiers_reaffecter(Application *app, Id x, Id y, RapportErreurs *r);

#endif
