/*
 * BudgetLens — import.h
 * Import additif d'opérations manuelles (décisions G1, B1, B2, G14).
 *
 * Le dossier choisi contient deux fichiers au format CSV restreint :
 *   import_operations.csv : ref;nature;date_realisation;libelle;montant_total_ct;
 *                           compte_source;compte_destination;tiers
 *   import_lignes.csv     : ref;categorie;vehicule;montant_ct
 * « ref » relie les lignes à leur opération. Les références sont des clés naturelles
 * résolues dans les données courantes : nom de compte (sans distinction de casse),
 * chemin complet de catégorie (« Alimentation > Courses »), nom de tiers (un nom
 * ambigu ou absent rejette l'import), véhicule vide ou « 1 » (le véhicule unique).
 * Les opérations importées sont créées CONFIRMEE avec de nouveaux identifiants.
 *
 * Tout ou rien : toute erreur (fichier, ligne, motif) rejette l'import entier ; les
 * données courantes, les compteurs et l'indicateur « modifié » ne changent pas.
 */
#ifndef BUDGETLENS_IMPORT_H
#define BUDGETLENS_IMPORT_H

#include "analyses.h"
#include "app.h"

#define NOM_IMPORT_OPERATIONS "import_operations.csv"
#define NOM_IMPORT_LIGNES "import_lignes.csv"
#define ENTETE_IMPORT_OPERATIONS \
    "ref;nature;date_realisation;libelle;montant_total_ct;compte_source;compte_destination;tiers"
#define ENTETE_IMPORT_LIGNES "ref;categorie;vehicule;montant_ct"

typedef struct {
    Donnees candidat;           /* données courantes + opérations importées (copie de travail) */
    Tableau ids_importes;       /* Id des opérations importées présentes dans le candidat */
    size_t nb_operations;
    size_t nb_lignes;
    bool pret;
} ImportPrepare;

void imp_init(ImportPrepare *p);
void imp_liberer(ImportPrepare *p);

/*
 * Analyse complète : lecture des deux fichiers en collectant toutes les erreurs
 * (fichier, ligne, motif), résolution des références, assemblage dans une copie
 * de travail et validation globale (Q3, G10 fusionné, sommes, catégories terminales),
 * B3 incluse (date réelle pas dans le futur). Les erreurs de validation globale sont
 * rattachées à la ligne de l'opération importée en cause. Succès : p->pret.
 */
Resultat imp_analyser(const Application *app, const char *dossier, ImportPrepare *p,
                      RapportErreurs *erreurs);

/*
 * Doublons I6 sur (existant + importé) comprenant au moins une opération importée.
 * « groupes »/« membres » : voir analyses_doublons (filtrés). « a_exclure » (Tableau de Id)
 * reçoit les opérations importées que « Exclure les doublons » retirerait : tous les
 * membres importés d'un groupe qui contient une opération existante, sinon tous sauf le
 * plus petit identifiant.
 */
bool imp_doublons(const ImportPrepare *p, Tableau *groupes, Tableau *membres, Tableau *a_exclure);

/* Retire des opérations importées, puis revalide l'ensemble (un revenu exclu peut rendre un solde négatif). */
Resultat imp_exclure(ImportPrepare *p, const Tableau *ids, RapportErreurs *r);

/*
 * Applique l'import : le candidat remplace les données courantes (compteurs relevés,
 * jamais baissés), « modifié » devient vrai. Avertissements de seuil comme op_creer.
 * p reste utilisable si RES_CONFIRMATION_REQUISE est renvoyé.
 */
Resultat imp_appliquer(Application *app, ImportPrepare *p, bool accord_seuil, RapportErreurs *avert,
                       RapportErreurs *r);

#endif
