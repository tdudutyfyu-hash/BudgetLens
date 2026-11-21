/*
 * BudgetLens — menus_principal.c
 * Menu principal, fichiers et sortie du programme.
 */
#include <stdio.h>

#include "menus.h"
#include "persistance.h"
#include "saisie.h"
#include "texte.h"
#include "valeurs.h"

void menus_bandeau(const Application *app, const char *titre)
{
    char date[DATE_TEXTE];

    date_formater(app->date_reference, date);
    printf("\n----------------------------------------------------------------\n");
    printf("BudgetLens | Date de référence : %s%s%s\n", date,
           app->date_simulee ? " (SIMULÉE)" : "",
           app->modifie ? " | * modifications non enregistrées" : "");
    printf("== %s ==\n", titre);
}

void menus_resultat(Resultat res, RapportErreurs *r, const char *message_succes)
{
    if (res == RES_OK) {
        printf("%s\n", message_succes);
    } else if (res == RES_ABANDON) {
        printf("Action abandonnée.\n");
        rapport_afficher(r, stdout);
    } else {
        printf("Action refusée, aucune donnée modifiée :\n");
        rapport_afficher(r, stdout);
    }
    rapport_vider(r);
}

void menus_colonne(const char *texte, size_t largeur)
{
    size_t n = texte_largeur(texte);

    printf("%s", texte);
    while (n < largeur) {
        putchar(' ');
        n++;
    }
}

void menus_colonne_tronquee(const char *texte, size_t largeur)
{
    size_t n = texte_largeur(texte);
    const char *p = texte;
    size_t visibles = 0;

    if (n <= largeur || largeur < 2) {
        menus_colonne(texte, largeur);
        return;
    }
    /* On garde largeur - 1 caractères (points de code) puis « … » : jamais au milieu d'un caractère. */
    while (*p != '\0' && visibles < largeur - 1) {
        p++;
        visibles++;
        while (((unsigned char)*p & 0xC0u) == 0x80u) {
            p++;
        }
    }
    printf("%.*s…", (int)(p - texte), texte);
}

void menus_abandon(void)
{
    printf("Saisie abandonnée, aucune donnée modifiée.\n");
}

bool menus_confirmer_avertissements(const RapportErreurs *avertissements)
{
    bool oui = false;

    printf("Avertissement :\n");
    rapport_afficher(avertissements, stdout);
    printf("Le solde reste positif ou nul ; poursuivre est possible.\n");
    if (saisie_oui_non("Poursuivre malgré l'avertissement ? (o/n) : ", &oui) != SAISIE_OK) {
        return false;
    }
    return oui;
}

bool menus_saisir_mois(const Application *app, int *annee, int *mois)
{
    char invite[64];
    long v;
    EtatSaisie s;

    snprintf(invite, sizeof invite, "Année [%d] : ", app->date_reference.annee);
    s = saisie_entier(invite, ANNEE_MIN, ANNEE_MAX, &v);
    if (s == SAISIE_FIN) {
        return false;
    }
    *annee = (s == SAISIE_OK) ? (int)v : app->date_reference.annee;
    snprintf(invite, sizeof invite, "Mois (1-12) [%d] : ", app->date_reference.mois);
    s = saisie_entier(invite, 1, 12, &v);
    if (s == SAISIE_FIN) {
        return false;
    }
    *mois = (s == SAISIE_OK) ? (int)v : app->date_reference.mois;
    return true;
}

bool menus_saisir_periode(const Application *app, bool toutes_permis, bool *toutes,
                          Date *debut, Date *fin)
{
    long choix;
    EtatSaisie s;

    *toutes = false;
    if (toutes_permis) {
        s = saisie_entier("Période (Entrée = toutes les dates, 1 = un mois, 2 = un intervalle) : ",
                          1, 2, &choix);
        if (s == SAISIE_FIN) {
            return false;
        }
        if (s == SAISIE_VIDE) {
            *toutes = true;
            return true;
        }
    } else {
        s = saisie_entier("Période (Entrée ou 1 = un mois, 2 = un intervalle) : ", 1, 2, &choix);
        if (s == SAISIE_FIN) {
            return false;
        }
        if (s == SAISIE_VIDE) {
            choix = 1;
        }
    }
    if (choix == 1) {
        int annee;
        int mois;

        if (!menus_saisir_mois(app, &annee, &mois)) {
            return false;
        }
        *debut = date_creer(annee, mois, 1);
        *fin = date_creer(annee, mois, jours_dans_mois(annee, mois));
        return true;
    }
    for (;;) {
        if (saisie_date("Date de début (AAAA-MM-JJ) : ", false, debut) != SAISIE_OK
            || saisie_date("Date de fin, incluse (AAAA-MM-JJ) : ", false, fin) != SAISIE_OK) {
            return false;
        }
        if (date_comparer(*debut, *fin) <= 0) {
            return true;
        }
        printf("La date de fin doit être postérieure ou égale à la date de début.\n");
    }
}

static void bientot(void)
{
    printf("Cette fonction n'est pas encore disponible dans cette version de BudgetLens.\n");
}

/* Enregistre ; renvoie true en cas de succès. */
bool menus_enregistrer(Application *app)
{
    RapportErreurs r;
    Resultat res;

    rapport_init(&r);
    if (app->date_simulee) {
        printf("Rappel : la date de référence est simulée ; les données qui en dépendent "
               "seront enregistrées telles quelles.\n");
    }
    res = persistance_enregistrer(app, &r);
    if (res == RES_OK) {
        printf("Données enregistrées dans « %s » (emplacement %c, génération %ld).\n",
               app->dossier_donnees, persistance_lettre_emplacement(app->emplacement_charge),
               app->generation_chargee);
    } else {
        printf("ÉCHEC de l'enregistrement ; les modifications restent en mémoire "
               "et le dernier enregistrement valide est intact :\n");
        rapport_afficher(&r, stdout);
    }
    rapport_liberer(&r);
    return res == RES_OK;
}

/* Renvoie true si le programme doit s'arrêter. */
static bool quitter(Application *app)
{
    bool oui;

    if (!app->modifie) {
        if (saisie_oui_non("Quitter BudgetLens ? (o/n) : ", &oui) != SAISIE_OK) {
            return true;
        }
        return oui;
    }
    for (;;) {
        int choix;

        printf("\nDes modifications ne sont pas enregistrées.\n"
               "1. Enregistrer et quitter\n2. Quitter sans enregistrer\n0. Annuler\n");
        choix = saisie_menu(2);
        if (choix < 0) {
            return true;   /* entrée perdue : l'appelant signale les modifications perdues */
        }
        if (choix == 0) {
            return false;
        }
        if (choix == 2) {
            printf("Modifications abandonnées.\n");
            return true;
        }
        if (menus_enregistrer(app)) {
            return true;
        }
        printf("L'enregistrement a échoué : BudgetLens reste ouvert.\n");
    }
}

/* Sortie imposée par la perte de l'entrée standard (H10) : aucun enregistrement implicite. */
static SortieProgramme sortie_entree_perdue(const Application *app)
{
    EtatEntree etat = saisie_etat_entree();

    printf("\n%s : arrêt de BudgetLens sans enregistrer.\n",
           etat == ENTREE_ERREUR ? "ERREUR de lecture sur l'entrée standard"
                                 : "Fin de l'entrée standard");
    if (app->modifie) {
        printf("ATTENTION : %ld modification(s) non enregistrée(s) abandonnée(s).\n",
               app->nb_modifications);
    } else {
        printf("Aucune modification non enregistrée.\n");
    }
    if (app->generation_chargee > 0) {
        printf("Le dernier enregistrement (génération %ld) est intact.\n", app->generation_chargee);
    }
    return etat == ENTREE_ERREUR ? SORTIE_ERREUR_ENTREE : SORTIE_FIN_ENTREE;
}

SortieProgramme menu_principal(Application *app)
{
    for (;;) {
        int choix;

        menus_bandeau(app, "Menu principal");
        printf("Au quotidien\n"
               "   1. Opérations\n");
        {
            size_t nb = menus_nombre_echeances(app);

            if (nb > 0) {
                printf("   2. Échéances à confirmer (%lu à traiter)\n", (unsigned long)nb);
            } else {
                printf("   2. Échéances à confirmer (aucune)\n");
            }
        }
        printf("  14. Dernières opérations\n"
               "Analyses\n"
               "   3. Bilan mensuel\n"
               "   4. Statistiques des dépenses\n"
               "   5. Véhicule\n"
               "   6. Doublons\n"
               "Gestion\n"
               "   7. Comptes\n"
               "   8. Catégories\n"
               "   9. Budgets\n"
               "  10. Tiers\n"
               "  11. Modèles\n"
               "  12. Récurrences\n"
               "  13. Fichiers (enregistrer, exporter, importer)\n"
               "   0. Quitter\n");
        choix = saisie_menu(14);
        if (choix < 0) {
            return sortie_entree_perdue(app);
        }
        switch (choix) {
        case 0:
            if (quitter(app)) {
                if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
                    return sortie_entree_perdue(app);
                }
                printf("Au revoir.\n");
                return SORTIE_NORMALE;
            }
            break;
        case 1:
            menu_operations(app);
            break;
        case 2:
            menu_echeances(app);
            break;
        case 3:
            menu_bilan(app);
            break;
        case 4:
            menu_statistiques(app);
            break;
        case 5:
            menu_vehicule(app);
            break;
        case 6:
            menu_doublons(app);
            break;
        case 7:
            menu_comptes(app);
            break;
        case 8:
            menu_categories(app);
            break;
        case 9:
            menu_budgets(app);
            break;
        case 10:
            menu_tiers(app);
            break;
        case 11:
            menu_modeles(app);
            break;
        case 12:
            menu_recurrences(app);
            break;
        case 13:
            menu_fichiers(app);
            break;
        case 14:
            menu_dernieres_operations(app);
            break;
        default:
            bientot();
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return sortie_entree_perdue(app);
        }
    }
}
