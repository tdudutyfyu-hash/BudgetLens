/*
 * BudgetLens — menus_fichiers.c
 * Enregistrer, exporter, sauvegarder, restaurer, importer des opérations.
 */
#include <stdio.h>
#include <string.h>

#include "import.h"
#include "menus.h"
#include "operations.h"
#include "regles.h"
#include "sauvegardes.h"
#include "saisie.h"
#include "texte.h"
#include "valeurs.h"

/* Saisit un chemin de dossier ; faux si abandon (Entrée), fin d'entrée ou chemin invalide. */
static bool saisir_dossier(const char *invite, char *dossier, size_t taille)
{
    char tampon[SAISIE_LIGNE_MAX];
    EtatSaisie s = saisie_ligne(invite, tampon, sizeof tampon);

    if (s != SAISIE_OK) {
        return false;
    }
    if (strlen(tampon) > CHEMIN_MAX - 40 || texte_verifier(tampon, CHEMIN_MAX - 40, true) != TEXTE_OK) {
        printf("Chemin refusé : trop long ou contenant un caractère interdit.\n");
        return false;
    }
    texte_copier(dossier, taille, tampon);
    return true;
}

/* ------------------------------------------------------------------ */
/* Exporter                                                            */
/* ------------------------------------------------------------------ */

static void exporter(Application *app)
{
    char dossier[CHEMIN_MAX + 1];
    RapportErreurs r;
    bool oui;
    Resultat res;

    if (!saisir_dossier("Dossier de destination (il doit exister ; Entrée = abandon) : ", dossier, sizeof dossier)) {
        return;
    }
    if (export_manifeste_present(dossier)) {
        if (saisie_oui_non("Un export existe déjà dans ce dossier : l'écraser ? (o/n) : ", &oui) != SAISIE_OK
            || !oui) {
            printf("Export annulé.\n");
            return;
        }
    }
    rapport_init(&r);
    res = fich_exporter(app, dossier, &r);
    if (res == RES_OK) {
        printf("Export terminé dans « %s » (onze fichiers CSV et manifest.csv). Les données en mémoire "
               "et l'indicateur de modifications ne changent pas ; l'export peut être restauré.\n", dossier);
        rapport_vider(&r);
    } else {
        menus_resultat(res, &r, "");
    }
    rapport_liberer(&r);
}

/* ------------------------------------------------------------------ */
/* Sauvegarder                                                         */
/* ------------------------------------------------------------------ */

static void sauvegarder(Application *app)
{
    RapportErreurs r;
    char prefixe[48];
    Resultat res;

    if (app->modifie) {
        int choix;

        printf("Des modifications ne sont pas enregistrées : une sauvegarde représente un état enregistré.\n"
               "1. Enregistrer d'abord, puis sauvegarder\n0. Annuler\n");
        choix = saisie_menu(1);
        if (choix != 1 || !menus_enregistrer(app)) {
            printf("Sauvegarde annulée.\n");
            return;
        }
    }
    rapport_init(&r);
    res = fich_sauvegarder(app, NULL, prefixe, sizeof prefixe, &r);
    if (res == RES_OK) {
        printf("Sauvegarde créée dans « %s » avec le préfixe %s (ajoutée à %s).\n", app->dossier_sauvegardes,
               prefixe, NOM_INDEX_SAUVEGARDES);
        rapport_vider(&r);
    } else {
        menus_resultat(res, &r, "");
    }
    rapport_liberer(&r);
}

/* ------------------------------------------------------------------ */
/* Restaurer                                                           */
/* ------------------------------------------------------------------ */

static void restaurer(Application *app)
{
    Tableau liste;
    RapportErreurs r;
    RapportErreurs info;
    Donnees restaure;
    ApercuRestauration ap;
    char dossier[CHEMIN_MAX + 1];
    char prefixe[48] = "";
    bool oui = false;
    size_t i;
    size_t nb_rattrapees = 0;
    Resultat res;

    tableau_init(&liste, sizeof(EntreeSauvegarde));
    rapport_init(&r);
    rapport_init(&info);
    donnees_init(&restaure);
    texte_copier(dossier, sizeof dossier, app->dossier_sauvegardes);
    if (fich_lister_sauvegardes(app->dossier_sauvegardes, &liste, &r) != RES_OK) {
        printf("Index des sauvegardes illisible :\n");
        rapport_afficher(&r, stdout);
        rapport_vider(&r);
        liste.taille = 0;
    }
    if (liste.taille > 0) {
        long choix;
        EtatSaisie s;

        printf("Sauvegardes disponibles :\n");
        for (i = 0; i < liste.taille; i++) {
            const EntreeSauvegarde *e = ELEMENT(&liste, EntreeSauvegarde, i);
            printf("  [%lu] %s | %s | génération %ld | %ld opération(s)\n", (unsigned long)(i + 1), e->prefixe,
                   e->horodatage, e->generation, e->nb_operations);
        }
        s = saisie_entier("Numéro de la sauvegarde (Entrée = restaurer un dossier d'export) : ", 1,
                          (long)liste.taille, &choix);
        if (s == SAISIE_FIN) {
            goto fin;
        }
        if (s == SAISIE_OK) {
            texte_copier(prefixe, sizeof prefixe, ELEMENT(&liste, EntreeSauvegarde, (size_t)choix - 1)->prefixe);
        }
    } else {
        printf("Aucune sauvegarde indexée dans « %s ».\n", app->dossier_sauvegardes);
    }
    if (prefixe[0] == '\0' && !saisir_dossier("Dossier de l'export à restaurer (Entrée = abandon) : ", dossier, sizeof dossier)) {
        goto fin;
    }
    res = fich_restaurer_charger(app, dossier, prefixe, &restaure, &ap, &r);
    if (res != RES_OK) {
        printf("Restauration impossible : la source est refusée, l'état courant est intact.\n");
        rapport_afficher(&r, stdout);
        goto fin;
    }
    printf("Source valide. Enregistrements par entité (courant -> restauré) :\n");
    for (i = 0; i < NB_ENTITES; i++) {
        printf("  %-18s %6lu -> %6lu\n", entite_libelle((Entite)i), (unsigned long)ap.courant[i],
               (unsigned long)ap.restaure[i]);
    }
    if (ap.compteurs_releves) {
        printf("Les compteurs d'identifiants ne diminuent jamais : les compteurs courants plus élevés "
               "sont conservés.\n");
    }
    printf("Les données absentes de cette source ne figureront plus dans l'état restauré.\n");
    if (app->modifie) {
        printf("ATTENTION : %ld modification(s) non enregistrée(s) seront perdue(s).\n", app->nb_modifications);
    }
    if (saisie_oui_non("Remplacer les données en mémoire par cette source ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
        printf("Restauration annulée : aucune donnée modifiée.\n");
        goto fin;
    }
    res = fich_restaurer_appliquer(app, &restaure, &nb_rattrapees, &info);
    if (res == RES_OK) {
        printf("Restauration effectuée EN MÉMOIRE seulement. Utilisez « Enregistrer » pour la rendre "
               "persistante ; quitter sans enregistrer rétablit l'état enregistré.\n");
        if (nb_rattrapees > 0) {
            printf("%lu échéance(s) de récurrence générée(s) sur l'état restauré (menu « Échéances »).\n",
                   (unsigned long)nb_rattrapees);
        }
        if (rapport_nombre(&info) > 0) {
            rapport_afficher(&info, stdout);
        }
    }
fin:
    donnees_liberer(&restaure);
    tableau_liberer(&liste);
    rapport_liberer(&info);
    rapport_liberer(&r);
}

/* ------------------------------------------------------------------ */
/* Importer                                                            */
/* ------------------------------------------------------------------ */

static void afficher_groupes_import(const ImportPrepare *p, const Tableau *groupes, const Tableau *membres)
{
    size_t g;
    size_t k;

    for (g = 0; g < groupes->taille; g++) {
        const GroupeDoublons *gr = ELEMENT(groupes, GroupeDoublons, g);
        printf("  Groupe %lu :\n", (unsigned long)(g + 1));
        for (k = 0; k < gr->nombre; k++) {
            Id id = *ELEMENT(membres, Id, gr->debut + k);
            const Operation *o = regles_operation(&p->candidat, id);
            bool importee = false;
            size_t m;
            char date[DATE_TEXTE];
            char montant[MONTANT_TEXTE];

            for (m = 0; m < p->ids_importes.taille; m++) {
                importee = importee || *ELEMENT(&p->ids_importes, Id, m) == id;
            }
            if (o == NULL) {
                continue;
            }
            date_formater(o->date_realisation, date);
            montant_formater(o->montant_total_ct, montant, sizeof montant);
            printf("    [%d] %s | %s | %s | %s\n", o->id, date, o->libelle, montant,
                   importee ? "importée" : "déjà présente");
        }
    }
}

static void importer(Application *app)
{
    char dossier[CHEMIN_MAX + 1];
    ImportPrepare p;
    RapportErreurs erreurs;
    Tableau groupes;
    Tableau membres;
    Tableau exclus;
    Resultat res;

    if (!saisir_dossier("Dossier contenant import_operations.csv et import_lignes.csv (Entrée = abandon) : ",
                        dossier, sizeof dossier)) {
        return;
    }
    imp_init(&p);
    rapport_init(&erreurs);
    tableau_init(&groupes, sizeof(GroupeDoublons));
    tableau_init(&membres, sizeof(Id));
    tableau_init(&exclus, sizeof(Id));
    res = imp_analyser(app, dossier, &p, &erreurs);
    if (res != RES_OK) {
        printf("Import REFUSÉ en entier ; aucune donnée n'a été modifiée (%lu erreur(s)) :\n",
               (unsigned long)rapport_nombre(&erreurs));
        rapport_afficher(&erreurs, stdout);
        goto fin;
    }
    printf("Import valide : %lu opération(s) manuelle(s) confirmée(s) et %lu ligne(s) de ventilation "
           "(nouveaux identifiants attribués à l'ajout).\n",
           (unsigned long)p.nb_operations, (unsigned long)p.nb_lignes);
    if (!imp_doublons(&p, &groupes, &membres, &exclus)) {
        printf("Détection des doublons impossible (mémoire insuffisante) : import abandonné.\n");
        goto fin;
    }
    if (groupes.taille > 0) {
        printf("Doublons potentiels (critères I6) entre les opérations importées et existantes :\n");
        afficher_groupes_import(&p, &groupes, &membres);
        for (;;) {
            int choix;

            printf("1. Tout importer\n2. Exclure les opérations importées signalées (%lu)\n"
                   "0. Annuler l'import\n", (unsigned long)exclus.taille);
            choix = saisie_menu(2);
            if (choix <= 0) {
                printf("Import annulé : aucune donnée modifiée.\n");
                goto fin;
            }
            if (choix == 1) {
                break;
            }
            rapport_vider(&erreurs);
            if (imp_exclure(&p, &exclus, &erreurs) == RES_OK) {
                printf("%lu opération(s) exclue(s) ; l'ensemble restant a été revalidé.\n",
                       (unsigned long)exclus.taille);
                break;
            }
            rapport_afficher(&erreurs, stdout);
            rapport_vider(&erreurs);
        }
    }
    {
        bool oui;
        RapportErreurs r;
        RapportErreurs w;
        char message[96];

        if (p.nb_operations == 0) {
            printf("Plus aucune opération à importer.\n");
            goto fin;
        }
        printf("%lu opération(s) seront ajoutées aux données existantes (rien n'est supprimé).\n",
               (unsigned long)p.nb_operations);
        if (saisie_oui_non("Importer maintenant ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
            printf("Import annulé : aucune donnée modifiée.\n");
            goto fin;
        }
        rapport_init(&r);
        rapport_init(&w);
        res = imp_appliquer(app, &p, false, &w, &r);
        if (res == RES_CONFIRMATION_REQUISE) {
            if (menus_confirmer_avertissements(&w)) {
                rapport_vider(&r);
                res = imp_appliquer(app, &p, true, NULL, &r);
            } else {
                rapport_ajouter(&r, "Aucune modification n'a été faite.");
                res = RES_ABANDON;
            }
        }
        snprintf(message, sizeof message, "Import terminé : %lu opération(s) ajoutée(s).",
                 (unsigned long)p.nb_operations);
        menus_resultat(res, &r, message);
        rapport_liberer(&w);
        rapport_liberer(&r);
    }
fin:
    tableau_liberer(&groupes);
    tableau_liberer(&membres);
    tableau_liberer(&exclus);
    rapport_liberer(&erreurs);
    imp_liberer(&p);
}

/* ------------------------------------------------------------------ */

void menu_fichiers(Application *app)
{
    for (;;) {
        int choix;

        menus_bandeau(app, "Fichiers");
        printf("1. Enregistrer\n2. Exporter (CSV)\n3. Créer une sauvegarde\n"
               "4. Restaurer une sauvegarde ou un export\n5. Importer des opérations\n0. Retour\n");
        choix = saisie_menu(5);
        if (choix <= 0) {
            return;
        }
        switch (choix) {
        case 1:
            menus_enregistrer(app);
            break;
        case 2:
            exporter(app);
            break;
        case 3:
            sauvegarder(app);
            break;
        case 4:
            restaurer(app);
            break;
        default:
            importer(app);
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}
