/*
 * BudgetLens — main.c
 * Gestion et analyse des dépenses personnelles avec suivi budgétaire.
 *
 * Usage : BudgetLens [--date AAAA-MM-JJ] [--donnees DOSSIER] [--sauvegardes DOSSIER]
 *   --date     date de référence simulée (tests et démonstrations)
 *   --donnees  dossier de données (par défaut : data)
 *   --sauvegardes  dossier des sauvegardes (par défaut : backups avec data,
 *              sinon le dossier de données lui-même)
 *
 * Codes de sortie :
 *   0  sortie normale (ou fin de l'entrée sans modification perdue)
 *   1  date du système illisible
 *   2  arguments invalides
 *   3  fin de l'entrée standard : modifications non enregistrées abandonnées
 *   4  erreur de lecture sur l'entrée standard
 *   5  aucun enregistrement valide dans le dossier de données (fichiers laissés intacts)
 *   6  dossier déjà verrouillé par une autre instance (ou verrou périmé)
 *   7  verrou impossible à créer (dossier absent ou non accessible en écriture)
 */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "menus.h"
#include "persistance.h"
#include "recurrences.h"
#include "saisie.h"
#include "texte.h"
#include "valeurs.h"

#define DOSSIER_PAR_DEFAUT "data"
/* Place réservée au préfixe et au nom de fichier le plus long. */
#define DOSSIER_MAX (CHEMIN_MAX - 40)

static void usage(void)
{
    fprintf(stderr, "Usage : BudgetLens [--date AAAA-MM-JJ] [--donnees DOSSIER] [--sauvegardes DOSSIER]\n");
}

enum {
    CODE_NORMAL = 0,
    CODE_DATE_SYSTEME = 1,
    CODE_USAGE = 2,
    CODE_FIN_ENTREE = 3,
    CODE_ERREUR_ENTREE = 4,
    CODE_DONNEES_INVALIDES = 5,
    CODE_VERROU_PRIS = 6,
    CODE_VERROU_ERREUR = 7
};

static int verrou_refuse(EtatVerrou etat, const char *dossier, const char *chemin)
{
    if (etat == VERROU_PRIS) {
        fprintf(stderr,
                "Le dossier de données « %s » est déjà utilisé : le verrou\n  %s\nexiste.\n"
                "Une autre fenêtre BudgetLens est sans doute ouverte sur ce dossier : fermez-la.\n"
                "Si BudgetLens s'est arrêté brutalement, le verrou est périmé. Après avoir vérifié\n"
                "qu'AUCUNE instance de BudgetLens n'est ouverte, supprimez ce fichier à la main,\n"
                "puis relancez BudgetLens.\n",
                dossier, chemin);
        return CODE_VERROU_PRIS;
    }
    fprintf(stderr,
            "Impossible de créer le verrou %s.\n"
            "Vérifiez que le dossier de données « %s » existe et est accessible en écriture\n"
            "(le script lancer.bat le crée).\n",
            chemin, dossier);
    return CODE_VERROU_ERREUR;
}

int main(int argc, char **argv)
{
    Application app;
    RapportErreurs infos;
    EtatChargement etat;
    Date reference = date_aucune();
    bool simulee = false;
    const char *dossier = DOSSIER_PAR_DEFAUT;
    const char *sauvegardes = NULL;
    char verrou[CHEMIN_MAX + 64];
    EtatVerrou etat_verrou;
    int code;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--date") == 0 && i + 1 < argc) {
            if (!date_lire(argv[++i], &reference)) {
                fprintf(stderr, "Date simulée invalide : « %s » (format AAAA-MM-JJ, date existante).\n",
                        argv[i]);
                return CODE_USAGE;
            }
            simulee = true;
        } else if (strcmp(argv[i], "--donnees") == 0 && i + 1 < argc) {
            dossier = argv[++i];
            if (dossier[0] == '\0' || strlen(dossier) > DOSSIER_MAX
                || texte_verifier(dossier, DOSSIER_MAX, true) != TEXTE_OK) {
                fprintf(stderr, "Dossier de données invalide ou trop long.\n");
                return CODE_USAGE;
            }
        } else if (strcmp(argv[i], "--sauvegardes") == 0 && i + 1 < argc) {
            sauvegardes = argv[++i];
            if (sauvegardes[0] == '\0' || strlen(sauvegardes) > DOSSIER_MAX
                || texte_verifier(sauvegardes, DOSSIER_MAX, true) != TEXTE_OK) {
                fprintf(stderr, "Dossier de sauvegardes invalide ou trop long.\n");
                return CODE_USAGE;
            }
        } else {
            usage();
            return CODE_USAGE;
        }
    }
    if (!simulee) {
        reference = date_aujourdhui();
        if (!date_valide(reference)) {
            fprintf(stderr, "Impossible de lire la date du système ; utilisez --date AAAA-MM-JJ.\n");
            return CODE_DATE_SYSTEME;
        }
    }

    etat_verrou = persistance_verrouiller(dossier, verrou, sizeof verrou);
    if (etat_verrou != VERROU_OK) {
        return verrou_refuse(etat_verrou, dossier, verrou);
    }

    app_init(&app, reference, simulee, dossier);
    if (sauvegardes != NULL) {
        texte_copier(app.dossier_sauvegardes, sizeof app.dossier_sauvegardes, sauvegardes);
    } else if (strcmp(dossier, DOSSIER_PAR_DEFAUT) == 0) {
        texte_copier(app.dossier_sauvegardes, sizeof app.dossier_sauvegardes, "backups");
    }
    rapport_init(&infos);
    printf("BudgetLens — gestion et analyse des dépenses personnelles\n");
    if (simulee) {
        char texte[DATE_TEXTE];
        date_formater(reference, texte);
        printf("MODE SIMULÉ : date de référence %s.\n", texte);
    }

    etat = persistance_charger(&app, &infos);
    switch (etat) {
    case CHARGEMENT_VIDE:
        printf("Aucune donnée enregistrée dans « %s » : démarrage avec des données vides.\n",
               app.dossier_donnees);
        break;
    case CHARGEMENT_OK:
        printf("Données chargées depuis « %s » (emplacement %c, génération %ld).\n",
               app.dossier_donnees, persistance_lettre_emplacement(app.emplacement_charge),
               app.generation_chargee);
        break;
    case CHARGEMENT_REPLI:
        printf("ATTENTION : l'enregistrement le plus récent est invalide ; l'enregistrement "
               "précédent (emplacement %c, génération %ld) a été chargé.\n",
               persistance_lettre_emplacement(app.emplacement_charge), app.generation_chargee);
        break;
    case CHARGEMENT_ECHEC:
        printf("ERREUR : aucun enregistrement valide dans « %s ».\n", app.dossier_donnees);
        break;
    }
    if (rapport_nombre(&infos) > 0) {
        rapport_afficher(&infos, stdout);
    }
    rapport_liberer(&infos);

    if (etat == CHARGEMENT_ECHEC) {
        /* Ni démarrage vide silencieux, ni écrasement des fichiers abîmés. */
        printf("BudgetLens s'arrête sans rien modifier. Pour repartir :\n"
               "  - déplacez le contenu de « %s » dans un autre dossier (pour analyse),\n"
               "  - relancez BudgetLens (démarrage vide),\n"
               "  - restaurez une sauvegarde (menu Fichiers > Restaurer une sauvegarde).\n",
               app.dossier_donnees);
        code = CODE_DONNEES_INVALIDES;
    } else {
        /* Rattrapage des récurrences actives (une seule transaction ; rien si rien n'est dû). */
        RapportErreurs rattrapage;
        size_t nb_creees = 0;
        Resultat res_rattrapage;

        rapport_init(&rattrapage);
        res_rattrapage = rec_rattraper(&app, &nb_creees, &rattrapage);
        if (res_rattrapage == RES_OK && nb_creees > 0) {
            printf("%lu échéance(s) générée(s) au démarrage : à confirmer dans le menu « Échéances ».\n",
                   (unsigned long)nb_creees);
        } else if (res_rattrapage != RES_OK) {
            printf("Le rattrapage des récurrences a échoué ; aucune échéance n'a été générée :\n");
        }
        if (rapport_nombre(&rattrapage) > 0) {
            rapport_afficher(&rattrapage, stdout);
        }
        rapport_liberer(&rattrapage);
        switch (menu_principal(&app)) {
        case SORTIE_NORMALE:
            code = CODE_NORMAL;
            break;
        case SORTIE_FIN_ENTREE:
            code = app.modifie ? CODE_FIN_ENTREE : CODE_NORMAL;
            break;
        case SORTIE_ERREUR_ENTREE:
        default:
            code = CODE_ERREUR_ENTREE;
            break;
        }
    }
    app_liberer(&app);
    persistance_deverrouiller(verrou);
    return code;
}
