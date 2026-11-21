/*
 * BudgetLens — menus.h
 * Interface textuelle : les menus collectent les saisies, appellent la
 * logique métier et affichent les résultats.
 */
#ifndef BUDGETLENS_MENUS_H
#define BUDGETLENS_MENUS_H

#include "app.h"
#include "operations.h"

/* menus_principal.c */
typedef enum {
    SORTIE_NORMALE,             /* choix Quitter */
    SORTIE_FIN_ENTREE,          /* fin de l'entrée standard (EOF) */
    SORTIE_ERREUR_ENTREE        /* erreur de lecture sur l'entrée standard */
} SortieProgramme;

/* Boucle principale ; ne fait jamais d'enregistrement implicite. */
SortieProgramme menu_principal(Application *app);
void menus_bandeau(const Application *app, const char *titre);
/* Affiche le succès ou les erreurs d'une action, puis vide le rapport. */
void menus_resultat(Resultat res, RapportErreurs *r, const char *message_succes);
void menus_colonne(const char *texte, size_t largeur);
/* Comme menus_colonne, en tronquant (avec « … ») un texte trop large. */
void menus_colonne_tronquee(const char *texte, size_t largeur);
void menus_abandon(void);
/* Enregistre dans data/ ; vrai en cas de succès. */
bool menus_enregistrer(Application *app);
/* Affiche les avertissements (seuil minimal) puis demande s'il faut poursuivre. */
bool menus_confirmer_avertissements(const RapportErreurs *avertissements);
/* Année et mois, la date de référence servant de valeur par défaut. Faux en fin d'entrée. */
bool menus_saisir_mois(const Application *app, int *annee, int *mois);
/*
 * Période d'analyse : 1 = un mois, 2 = un intervalle ; avec toutes_permis,
 * Entrée = toutes les dates (*toutes devient vrai). Faux si la saisie est abandonnée
 * (champ obligatoire vide) ou si l'entrée est perdue.
 */
bool menus_saisir_periode(const Application *app, bool toutes_permis, bool *toutes,
                          Date *debut, Date *fin);

/* menus_gestion.c */
void menu_comptes(Application *app);
void menu_categories(Application *app);
void menu_budgets(Application *app);
void menu_tiers(Application *app);
void menu_vehicule(Application *app);
void menus_afficher_arbre_categories(const Application *app);

/* menus_reaffectation.c : x = ID_AUCUN pour demander la source ; vrai si la réaffectation a eu lieu */
bool menus_reaffecter_tiers(Application *app, Id x);
bool menus_reaffecter_compte(Application *app, Id x);
bool menus_reaffecter_categorie(Application *app, Id x);
void menus_deplacer_categorie(Application *app);

/* menus_analyses.c */
void menu_bilan(Application *app);
void menu_doublons(Application *app);
void menus_cout_vehicule(Application *app);

/* menus_operations.c */
void menu_operations(Application *app);
void menu_statistiques(Application *app);
/* Entrée 14 du menu principal : les dix opérations les plus récentes, puis consultation par id. */
void menu_dernieres_operations(Application *app);
void menus_corriger_operation(Application *app);       /* occurrence A_CONFIRMER comprise */
void menus_consulter_operation(const Application *app);
void menus_afficher_operation(const Application *app, const Operation *o);
/* Saisie d'une opération (avec_date) ou d'un modèle (sans date) ; faux si abandon ou fin d'entrée. */
bool menus_collecter_saisie(const Application *app, OperationSaisie *s, Tableau *lignes,
                            bool avec_date);
/* Saisie de correction (Entrée = valeur actuelle) ; voir corriger_operation. */
bool menus_collecter_correction(const Application *app, const Operation *actuelle,
                                OperationSaisie *s, LigneSaisie **lignes_existantes,
                                size_t *nb_existantes, Tableau *nouvelles, bool *remplacer);

/* Saisie de la ventilation d'un montant total (reste à répartir affiché) ; faux si abandon. */
bool menus_saisir_ventilation(const Application *app, Nature nature, long long total, Tableau *lignes);

/* menus_modeles.c */
void menu_modeles(Application *app);
void menu_recurrences(Application *app);
void menus_operation_depuis_modele(Application *app);

/* menus_fichiers.c */
void menu_fichiers(Application *app);

/* menus_echeances.c */
void menu_echeances(Application *app);
/* Nombre d'échéances A_CONFIRMER présentées par « Lister les échéances » (même sélection). */
size_t menus_nombre_echeances(const Application *app);

#endif
