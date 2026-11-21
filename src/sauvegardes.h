/*
 * BudgetLens — sauvegardes.h
 * Export, sauvegarde horodatée avec index, restauration. Le même format de
 * jeu (onze fichiers + manifeste, voir persistance.h) sert aux trois.
 */
#ifndef BUDGETLENS_SAUVEGARDES_H
#define BUDGETLENS_SAUVEGARDES_H

#include "app.h"

#define NOM_INDEX_SAUVEGARDES "index.csv"
#define ENTETE_INDEX "prefixe;horodatage;generation;nb_operations"

/* Un dossier d'export contient-il déjà un manifeste (confirmation d'écrasement) ? */
bool export_manifeste_present(const char *dossier);

/*
 * Export : écrit un jeu complet (sans préfixe) dans un dossier existant. Les
 * données en mémoire et l'indicateur « modifié » ne changent pas. L'export est
 * restaurable (voir fich_restaurer_charger).
 */
Resultat fich_exporter(const Application *app, const char *dossier, RapportErreurs *r);

/*
 * Sauvegarde dans app->dossier_sauvegardes avec le préfixe AAAAMMJJ-HHMMSS_
 * (heure système réelle, même en mode simulé ; « -2 », « -3 » si le préfixe
 * existe) puis une ligne dans index.csv. Les modifications doivent être
 * enregistrées avant (app->modifie faux). horodatage_force (AAAAMMJJ-HHMMSS)
 * remplace l'heure système, pour les tests. prefixe_cree reçoit le préfixe.
 */
Resultat fich_sauvegarder(const Application *app, const char *horodatage_force, char *prefixe_cree,
                          size_t taille, RapportErreurs *r);

typedef struct {
    char prefixe[48];
    char horodatage[24];
    long generation;
    long nb_operations;
} EntreeSauvegarde;

/* Lit index.csv (absent = liste vide). « entrees » : Tableau d'EntreeSauvegarde, ordre du fichier. */
Resultat fich_lister_sauvegardes(const char *dossier, Tableau *entrees, RapportErreurs *r);

typedef struct {
    size_t courant[NB_ENTITES];
    size_t restaure[NB_ENTITES];
    bool compteurs_releves;        /* un compteur a été relevé (valeur courante plus haute) */
} ApercuRestauration;

/*
 * Charge et valide un jeu dans « restaure » (initialisé par l'appelant), à
 * part des données courantes : en cas d'erreur ou de manque de mémoire, rapport
 * et état courant intact. Compteurs : max(courant, sauvegarde, plus grand id + 1).
 */
Resultat fich_restaurer_charger(const Application *app, const char *dossier, const char *prefixe,
                                Donnees *restaure, ApercuRestauration *apercu, RapportErreurs *r);

/*
 * Remplace les données en mémoire par « restaure » (consommé), marque l'état
 * comme modifié (le fichier de données n'est touché que par « Enregistrer »),
 * puis rattrape les récurrences (nb_rattrapees occurrences A_CONFIRMER).
 */
Resultat fich_restaurer_appliquer(Application *app, Donnees *restaure, size_t *nb_rattrapees,
                                  RapportErreurs *messages);

#endif
