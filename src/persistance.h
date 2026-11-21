/*
 * BudgetLens — persistance.h
 * Jeux de fichiers CSV (dossier + préfixe) et enregistrement cohérent dans
 * deux emplacements A/B de data/ (manifeste écrit en dernier).
 */
#ifndef BUDGETLENS_PERSISTANCE_H
#define BUDGETLENS_PERSISTANCE_H

#include "app.h"

#define NB_FICHIERS 11          /* 10 entités + metadata.csv */
#define FORMAT_VERSION 1

typedef enum {
    CHARGEMENT_VIDE,            /* aucune donnée enregistrée : premier lancement */
    CHARGEMENT_OK,              /* emplacement le plus récent chargé */
    CHARGEMENT_REPLI,           /* le plus récent était invalide : emplacement précédent */
    CHARGEMENT_ECHEC            /* des données existent mais aucun emplacement n'est valide */
} EtatChargement;

const char *persistance_nom_fichier(int indice);   /* 0..NB_FICHIERS-1 */

/* Lit un jeu complet (manifeste compris) dans d, initialisé et vide. */
bool jeu_lire(const char *dossier, const char *prefixe, Donnees *d, long *generation,
              RapportErreurs *r);
/* Écrit un jeu complet ; le manifeste est supprimé d'abord et écrit en dernier. */
bool jeu_ecrire(const char *dossier, const char *prefixe, const Donnees *d, long generation,
                RapportErreurs *r);
/* Supprime les fichiers d'un jeu (utilisé par les tests). */
void jeu_supprimer(const char *dossier, const char *prefixe);

EtatChargement persistance_charger(Application *app, RapportErreurs *infos);
Resultat persistance_enregistrer(Application *app, RapportErreurs *r);
char persistance_lettre_emplacement(int emplacement);

/*
 * Verrou d'exécution : fichier DOSSIER/budgetlens.verrou créé en mode
 * exclusif (fopen "wx", C11). Il empêche deux instances d'utiliser le même
 * dossier. Après un arrêt brutal, le fichier subsiste : il doit être supprimé
 * à la main, après avoir vérifié qu'aucune instance n'utilise ce dossier.
 */
#define NOM_VERROU "budgetlens.verrou"

typedef enum {
    VERROU_OK,                  /* verrou créé : l'instance est seule */
    VERROU_PRIS,                /* le fichier verrou existe déjà */
    VERROU_ERREUR               /* création impossible pour une autre raison */
} EtatVerrou;

EtatVerrou persistance_verrouiller(const char *dossier, char *chemin, size_t taille);
/* Supprime le verrou créé par cette instance. */
void persistance_deverrouiller(const char *chemin);

#endif
