/*
 * BudgetLens — menus_gestion.c
 * Comptes, catégories, budgets mensuels, tiers et véhicule.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus.h"
#include "referentiel.h"
#include "regles.h"
#include "saisie.h"
#include "soldes.h"
#include "texte.h"
#include "valeurs.h"

/* ------------------------------------------------------------------ */
/* Comptes                                                             */
/* ------------------------------------------------------------------ */

/* Soldes d'un compte : à la date de référence, puis après toutes les opérations confirmées. */
static void formater_soldes(const Application *app, const Compte *c, char *a_date, size_t n1,
                            char *final, size_t n2)
{
    long long solde = 0;
    long long solde_final = 0;
    bool ok_date = solde_compte_a_date(&app->donnees, c->id, app->date_reference, &solde);
    bool ok_final = solde_compte_final(&app->donnees, c->id, &solde_final);

    if (ok_date) {
        montant_formater(solde, a_date, n1);
    } else {
        texte_copier(a_date, n1, "-");   /* date de référence antérieure à l'ouverture du compte */
    }
    if (ok_final && (!ok_date || solde_final != solde)) {
        montant_formater(solde_final, final, n2);
    } else {
        final[0] = '\0';
    }
}

static void lister_comptes(const Application *app)
{
    const Tableau *t = &app->donnees.comptes;
    char date_ref[DATE_TEXTE];
    char titre[48];
    size_t i;

    if (t->taille == 0) {
        printf("Aucun compte.\n");
        return;
    }
    date_formater(app->date_reference, date_ref);
    snprintf(titre, sizeof titre, "Solde au %s", date_ref);
    printf("%-5s ", "Id");
    menus_colonne("Nom", 24);
    menus_colonne("Type", 9);
    menus_colonne(titre, 22);
    menus_colonne("Après opérations futures", 26);
    printf("Seuil minimal\n");
    for (i = 0; i < t->taille; i++) {
        const Compte *c = ELEMENT(t, Compte, i);
        char solde[MONTANT_TEXTE];
        char final[MONTANT_TEXTE];
        char seuil[MONTANT_TEXTE];

        formater_soldes(app, c, solde, sizeof solde, final, sizeof final);
        if (c->a_seuil) {
            montant_formater(c->seuil_minimal_ct, seuil, sizeof seuil);
        } else {
            texte_copier(seuil, sizeof seuil, "-");
        }
        printf("%-5d ", c->id);
        menus_colonne_tronquee(c->nom, 24);
        menus_colonne(type_compte_libelle(c->type), 9);
        menus_colonne(solde, 22);
        menus_colonne(final, 26);
        printf("%s\n", seuil);
    }
}

static void afficher_compte(const Application *app, const Compte *c)
{
    char montant[MONTANT_TEXTE];
    char date[DATE_TEXTE];
    Usages u = regles_usages_compte(&app->donnees, c->id);

    montant_formater(c->solde_initial_ct, montant, sizeof montant);
    date_formater(c->date_solde_initial, date);
    printf("Compte %d : %s\n", c->id, c->nom);
    printf("  Type              : %s\n", type_compte_libelle(c->type));
    printf("  Solde initial     : %s au %s\n", montant, date);
    {
        char solde[MONTANT_TEXTE];
        char final[MONTANT_TEXTE];
        char date_ref[DATE_TEXTE];

        formater_soldes(app, c, solde, sizeof solde, final, sizeof final);
        date_formater(app->date_reference, date_ref);
        printf("  Solde au %s : %s\n", date_ref, solde);
        if (final[0] != '\0') {
            printf("  Après toutes les opérations confirmées : %s\n", final);
        }
    }
    if (c->a_seuil) {
        montant_formater(c->seuil_minimal_ct, montant, sizeof montant);
        printf("  Seuil minimal     : %s\n", montant);
    } else {
        printf("  Seuil minimal     : aucun\n");
    }
    if (c->type == COMPTE_BANQUE) {
        printf("  Banque            : %s\n", c->nom_banque[0] ? c->nom_banque : "-");
        printf("  Référence         : %s\n", c->reference_compte[0] ? c->reference_compte : "-");
    }
    printf("  Utilisé par       : %lu opération(s), %lu modèle(s)\n",
           (unsigned long)u.operations, (unsigned long)u.modeles);
}

/* Saisit un compte existant par son identifiant ; NULL si abandon. */
static const Compte *choisir_compte(const Application *app)
{
    Id id;
    const Compte *c;

    if (app->donnees.comptes.taille == 0) {
        printf("Aucun compte.\n");
        return NULL;
    }
    if (saisie_id("Identifiant du compte : ", &id) != SAISIE_OK) {
        return NULL;
    }
    c = regles_compte(&app->donnees, id);
    if (c == NULL) {
        printf("Compte %d introuvable.\n", id);
    }
    return c;
}

static EtatSaisie saisir_nom_compte(const Application *app, const char *invite, char *nom, Id id_exclu)
{
    for (;;) {
        EtatSaisie s = saisie_texte(invite, nom, NOM_MAX, false);
        if (s != SAISIE_OK) {
            return s;
        }
        if (regles_nom_compte_libre(&app->donnees, nom, id_exclu)) {
            return SAISIE_OK;
        }
        printf("Un compte porte déjà ce nom (majuscules et minuscules confondues).\n");
    }
}

/* Champs bancaires facultatifs : Entrée = vide (création) ou inchangé (modification). */
static bool saisir_champs_banque(Compte *c, bool modification)
{
    char invite[160];
    EtatSaisie s;
    char tampon[NOM_BANQUE_MAX + 1];

    snprintf(invite, sizeof invite, "Nom de la banque%s : ",
             modification ? " [Entrée = inchangé, - = effacer]" : " (facultatif)");
    s = saisie_texte(invite, tampon, NOM_BANQUE_MAX, modification);
    if (s == SAISIE_FIN) {
        return false;
    }
    if (s == SAISIE_OK) {
        texte_copier(c->nom_banque, sizeof c->nom_banque, tampon);
    } else if (s == SAISIE_EFFACER || !modification) {
        c->nom_banque[0] = '\0';
    }
    snprintf(invite, sizeof invite, "Référence du compte%s : ",
             modification ? " [Entrée = inchangé, - = effacer]" : " (facultatif)");
    s = saisie_texte(invite, tampon, REFERENCE_MAX, modification);
    if (s == SAISIE_FIN) {
        return false;
    }
    if (s == SAISIE_OK) {
        texte_copier(c->reference_compte, sizeof c->reference_compte, tampon);
    } else if (s == SAISIE_EFFACER || !modification) {
        c->reference_compte[0] = '\0';
    }
    return true;
}

static void creer_compte(Application *app)
{
    RapportErreurs r;
    Compte c;
    long type;
    long long seuil;
    EtatSaisie s;
    Id id = ID_AUCUN;
    Resultat res;
    char message[96];

    memset(&c, 0, sizeof c);
    printf("(Entrée sur un champ obligatoire : abandon)\n");
    if (saisir_nom_compte(app, "Nom du compte : ", c.nom, ID_AUCUN) != SAISIE_OK
        || saisie_entier("Type (1 = Banque, 2 = Espèces) : ", 1, 2, &type) != SAISIE_OK
        || saisie_montant("Solde initial en DH (0 accepté) : ", true, false, &c.solde_initial_ct)
               != SAISIE_OK
        || saisie_date("Date du solde initial (AAAA-MM-JJ) : ", false, &c.date_solde_initial)
               != SAISIE_OK) {
        menus_abandon();
        return;
    }
    c.type = (type == 1) ? COMPTE_BANQUE : COMPTE_ESPECES;
    s = saisie_montant("Seuil minimal d'avertissement en DH (facultatif) : ", true, false, &seuil);
    if (s == SAISIE_FIN) {
        return;
    }
    c.a_seuil = (s == SAISIE_OK);
    c.seuil_minimal_ct = c.a_seuil ? seuil : 0;
    if (c.type == COMPTE_BANQUE && !saisir_champs_banque(&c, false)) {
        return;
    }
    rapport_init(&r);
    res = ref_compte_creer(app, &c, &id, &r);
    snprintf(message, sizeof message, "Compte créé (identifiant %d).", id);
    menus_resultat(res, &r, message);
    rapport_liberer(&r);
}

static void modifier_compte(Application *app)
{
    RapportErreurs r;
    RapportErreurs w;
    const Compte *actuel = choisir_compte(app);
    Compte c;
    char invite[200];
    char tampon[NOM_MAX + 1];
    char montant[MONTANT_TEXTE];
    char date[DATE_TEXTE];
    long type;
    long long valeur;
    Date d;
    EtatSaisie s;
    Resultat res;

    if (actuel == NULL) {
        return;
    }
    c = *actuel;
    printf("(Entrée : valeur inchangée ; « - » : effacer une valeur facultative)\n");
    snprintf(invite, sizeof invite, "Nom [%s] : ", c.nom);
    s = saisir_nom_compte(app, invite, tampon, c.id);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_OK) {
        texte_copier(c.nom, sizeof c.nom, tampon);
    }
    snprintf(invite, sizeof invite, "Type (1 = Banque, 2 = Espèces) [%s] : ",
             type_compte_libelle(c.type));
    s = saisie_entier(invite, 1, 2, &type);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_OK) {
        c.type = (type == 1) ? COMPTE_BANQUE : COMPTE_ESPECES;
    }
    montant_formater(c.solde_initial_ct, montant, sizeof montant);
    snprintf(invite, sizeof invite, "Solde initial [%s] : ", montant);
    s = saisie_montant(invite, true, false, &valeur);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_OK) {
        c.solde_initial_ct = valeur;
    }
    date_formater(c.date_solde_initial, date);
    snprintf(invite, sizeof invite, "Date du solde initial [%s] : ", date);
    s = saisie_date(invite, false, &d);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_OK) {
        c.date_solde_initial = d;
    }
    if (c.a_seuil) {
        montant_formater(c.seuil_minimal_ct, montant, sizeof montant);
    } else {
        texte_copier(montant, sizeof montant, "aucun");
    }
    snprintf(invite, sizeof invite, "Seuil minimal [%s] : ", montant);
    s = saisie_montant(invite, true, true, &valeur);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_OK) {
        c.a_seuil = true;
        c.seuil_minimal_ct = valeur;
    } else if (s == SAISIE_EFFACER) {
        c.a_seuil = false;
        c.seuil_minimal_ct = 0;
    }
    if (c.type == COMPTE_ESPECES) {
        if (c.nom_banque[0] != '\0' || c.reference_compte[0] != '\0') {
            printf("Compte d'espèces : le nom de banque et la référence sont effacés.\n");
        }
        c.nom_banque[0] = '\0';
        c.reference_compte[0] = '\0';
    } else if (!saisir_champs_banque(&c, true)) {
        return;
    }
    rapport_init(&r);
    rapport_init(&w);
    res = ref_compte_modifier(app, &c, false, &w, &r);
    if (res == RES_CONFIRMATION_REQUISE) {
        if (menus_confirmer_avertissements(&w)) {
            rapport_vider(&r);
            res = ref_compte_modifier(app, &c, true, NULL, &r);
        } else {
            res = RES_ABANDON;
        }
    }
    menus_resultat(res, &r, "Compte modifié.");
    rapport_liberer(&w);
    rapport_liberer(&r);
}

static void supprimer_compte(Application *app)
{
    RapportErreurs r;
    const Compte *c = choisir_compte(app);
    Usages u;
    bool oui;
    Id id;

    if (c == NULL) {
        return;
    }
    id = c->id;
    u = regles_usages_compte(&app->donnees, id);
    if (u.operations > 0 || u.modeles > 0) {
        printf("Le compte « %s » est utilisé par %lu opération(s) et %lu modèle(s) : "
               "suppression impossible tant que ces liens ne sont pas réaffectés.\n",
               c->nom, (unsigned long)u.operations, (unsigned long)u.modeles);
        if (saisie_oui_non("Réaffecter ces liens maintenant ? (o/n) : ", &oui) != SAISIE_OK
            || !oui || !menus_reaffecter_compte(app, id)) {
            return;
        }
        c = regles_compte(&app->donnees, id);   /* les données ont été remplacées : relire */
        if (c == NULL) {
            return;
        }
    }
    printf("Supprimer le compte %d « %s » ? (o/n) : ", c->id, c->nom);
    if (saisie_oui_non("", &oui) != SAISIE_OK || !oui) {
        printf("Suppression annulée.\n");
        return;
    }
    rapport_init(&r);
    menus_resultat(ref_compte_supprimer(app, id, &r), &r, "Compte supprimé.");
    rapport_liberer(&r);
}

void menu_comptes(Application *app)
{
    for (;;) {
        int choix;
        const Compte *c;

        menus_bandeau(app, "Comptes");
        printf("1. Lister\n2. Créer\n3. Consulter\n4. Modifier\n5. Supprimer\n6. Réaffecter\n0. Retour\n");
        choix = saisie_menu(6);
        if (choix <= 0) {
            return;
        }
        switch (choix) {
        case 6:
            menus_reaffecter_compte(app, ID_AUCUN);
            break;
        case 1:
            lister_comptes(app);
            break;
        case 2:
            creer_compte(app);
            break;
        case 3:
            c = choisir_compte(app);
            if (c != NULL) {
                afficher_compte(app, c);
            }
            break;
        case 4:
            modifier_compte(app);
            break;
        default:
            supprimer_compte(app);
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Catégories                                                          */
/* ------------------------------------------------------------------ */

static int comparer_categories(const void *a, const void *b)
{
    const Categorie *x = *(const Categorie *const *)a;
    const Categorie *y = *(const Categorie *const *)b;
    int c = texte_comparer_sans_casse(x->nom, y->nom);
    return c != 0 ? c : (x->id > y->id) - (x->id < y->id);
}

/* Affiche les enfants de id_parent triés par nom, récursivement. */
static void afficher_branche(const Application *app, Id id_parent, int profondeur)
{
    const Tableau *t = &app->donnees.categories;
    const Categorie **enfants;
    size_t n = 0;
    size_t i;

    if (profondeur > 32) {
        return;
    }
    enfants = malloc((t->taille > 0 ? t->taille : 1) * sizeof *enfants);
    if (enfants == NULL) {
        printf("Mémoire insuffisante pour afficher l'arbre.\n");
        return;
    }
    for (i = 0; i < t->taille; i++) {
        const Categorie *c = ELEMENT(t, Categorie, i);
        if (c->id_parent == id_parent) {
            enfants[n++] = c;
        }
    }
    qsort(enfants, n, sizeof *enfants, comparer_categories);
    for (i = 0; i < n; i++) {
        int k;
        for (k = 0; k < profondeur; k++) {
            printf("    ");
        }
        printf("[%d] %s", enfants[i]->id, enfants[i]->nom);
        if (profondeur == 0) {
            printf(" (%s)", type_categorie_libelle(enfants[i]->type));
        }
        printf("\n");
        afficher_branche(app, enfants[i]->id, profondeur + 1);
    }
    free(enfants);
}

static void afficher_arbre(const Application *app)
{
    if (app->donnees.categories.taille == 0) {
        printf("Aucune catégorie.\n");
        return;
    }
    afficher_branche(app, ID_AUCUN, 0);
}

void menus_afficher_arbre_categories(const Application *app)
{
    afficher_arbre(app);
}

static const Categorie *choisir_categorie(const Application *app, const char *invite)
{
    Id id;
    const Categorie *c;

    if (app->donnees.categories.taille == 0) {
        printf("Aucune catégorie.\n");
        return NULL;
    }
    if (saisie_id(invite, &id) != SAISIE_OK) {
        return NULL;
    }
    c = regles_categorie(&app->donnees, id);
    if (c == NULL) {
        printf("Catégorie %d introuvable.\n", id);
    }
    return c;
}

static EtatSaisie saisir_nom_categorie(const Application *app, const char *invite, Id id_parent,
                                       Id id_exclu, char *nom)
{
    for (;;) {
        EtatSaisie s = saisie_texte(invite, nom, NOM_MAX, false);
        if (s != SAISIE_OK) {
            return s;
        }
        if (strchr(nom, '>') != NULL) {
            printf("Le caractère « > » est réservé au chemin des catégories.\n");
            continue;
        }
        if (!regles_nom_categorie_libre(&app->donnees, id_parent, nom, id_exclu)) {
            printf("Ce nom existe déjà à ce niveau (majuscules et minuscules confondues).\n");
            continue;
        }
        return SAISIE_OK;
    }
}

static void creer_categorie(Application *app)
{
    RapportErreurs r;
    char nom[NOM_MAX + 1];
    char chemin[CHEMIN_MAX + 1];
    char message[CHEMIN_MAX + 64];
    Id id_parent = ID_AUCUN;
    Id id = ID_AUCUN;
    TypeCategorie type = CAT_DEPENSE;
    const Categorie *parent = NULL;
    Resultat res;
    RapportErreurs w;
    bool oui;

    printf("Catégories existantes (identifiants entre crochets)\n");
    afficher_arbre(app);
    for (;;) {
        EtatSaisie s = saisie_id("Identifiant du parent (Entrée = créer une catégorie racine) : ", &id_parent);
        if (s == SAISIE_FIN) {
            return;
        }
        if (s == SAISIE_VIDE) {
            id_parent = ID_AUCUN;
            break;
        }
        parent = regles_categorie(&app->donnees, id_parent);
        if (parent != NULL) {
            break;
        }
        printf("Catégorie %d introuvable. Choisis un identifiant affiché ci-dessus.\n", id_parent);
    }
    if (parent == NULL) {
        long t;
        if (saisie_entier("Type (1 = Dépense, 2 = Revenu) : ", 1, 2, &t) != SAISIE_OK) {
            menus_abandon();
            return;
        }
        type = (t == 1) ? CAT_DEPENSE : CAT_REVENU;
    } else {
        regles_chemin_categorie(&app->donnees, id_parent, chemin, sizeof chemin);
        printf("Sous-catégorie de « %s » (type %s, hérité).\n", chemin,
               type_categorie_libelle(parent->type));
    }
    if (saisir_nom_categorie(app, "Nom : ", id_parent, ID_AUCUN, nom) != SAISIE_OK) {
        menus_abandon();
        return;
    }
    rapport_init(&r);
    rapport_init(&w);
    res = ref_categorie_creer_i3(app, nom, id_parent, type, false, &id, &w, &r);
    if (res == RES_CONFIRMATION_REQUISE) {
        printf("Migration vers « Autres » (I3) :\n");
        rapport_afficher(&w, stdout);
        if (saisie_oui_non("Confirmer la migration puis la création ? (o/n) : ", &oui) == SAISIE_OK
            && oui) {
            rapport_vider(&r);
            res = ref_categorie_creer_i3(app, nom, id_parent, type, true, &id, NULL, &r);
        } else {
            rapport_ajouter(&r, "Aucune modification n'a été faite.");
            res = RES_ABANDON;
        }
    }
    rapport_liberer(&w);
    if (res == RES_OK) {
        regles_chemin_categorie(&app->donnees, id, chemin, sizeof chemin);
        snprintf(message, sizeof message, "Catégorie créée : [%d] %s", id, chemin);
    } else {
        message[0] = '\0';
    }
    menus_resultat(res, &r, message);
    rapport_liberer(&r);
}

static void renommer_categorie(Application *app)
{
    RapportErreurs r;
    const Categorie *c = choisir_categorie(app, "Identifiant de la catégorie : ");
    char nom[NOM_MAX + 1];
    char invite[NOM_MAX + 32];
    Id id;

    if (c == NULL) {
        return;
    }
    id = c->id;
    snprintf(invite, sizeof invite, "Nouveau nom [%s] : ", c->nom);
    if (saisir_nom_categorie(app, invite, c->id_parent, c->id, nom) != SAISIE_OK) {
        menus_abandon();
        return;
    }
    rapport_init(&r);
    menus_resultat(ref_categorie_renommer(app, id, nom, &r), &r, "Catégorie renommée.");
    rapport_liberer(&r);
}

static void supprimer_categorie(Application *app)
{
    RapportErreurs r;
    const Categorie *c = choisir_categorie(app, "Identifiant de la catégorie : ");
    char chemin[CHEMIN_MAX + 1];
    char montant[MONTANT_TEXTE];
    Usages u;
    bool oui;
    size_t i;
    Id id;

    if (c == NULL) {
        return;
    }
    id = c->id;
    regles_chemin_categorie(&app->donnees, id, chemin, sizeof chemin);
    u = regles_usages_categorie(&app->donnees, id);
    if (u.enfants > 0) {
        printf("« %s » a %lu sous-catégorie(s) : suppression impossible.\n", chemin,
               (unsigned long)u.enfants);
        return;
    }
    if (u.lignes_operations > 0 || u.lignes_modeles > 0) {
        printf("« %s » est utilisée par %lu ligne(s) d'opération et %lu ligne(s) de modèle : "
               "suppression impossible tant que ces liens ne sont pas réaffectés.\n",
               chemin, (unsigned long)u.lignes_operations, (unsigned long)u.lignes_modeles);
        if (saisie_oui_non("Réaffecter ces lignes maintenant ? (o/n) : ", &oui) != SAISIE_OK
            || !oui || !menus_reaffecter_categorie(app, id)) {
            return;
        }
        u = regles_usages_categorie(&app->donnees, id);   /* les données ont été remplacées */
    }
    if (u.budgets > 0) {
        printf("Budgets mensuels directement rattachés à « %s » :\n", chemin);
        for (i = 0; i < app->donnees.budgets.taille; i++) {
            const BudgetMensuel *b = ELEMENT(&app->donnees.budgets, BudgetMensuel, i);
            if (b->id_categorie == id) {
                montant_formater(b->plafond_ct, montant, sizeof montant);
                printf("  [%d] %04d-%02d : plafond %s\n", b->id, b->annee, b->mois, montant);
            }
        }
        printf("Supprimer la catégorie ET ces %lu budget(s) ? Les budgets des catégories "
               "parentes et les budgets globaux sont conservés. (o/n) : ",
               (unsigned long)u.budgets);
    } else {
        printf("Supprimer la catégorie « %s » ? (o/n) : ", chemin);
    }
    if (saisie_oui_non("", &oui) != SAISIE_OK || !oui) {
        printf("Suppression annulée.\n");
        return;
    }
    rapport_init(&r);
    menus_resultat(ref_categorie_supprimer(app, id, true, &r), &r, "Catégorie supprimée.");
    rapport_liberer(&r);
}

void menu_categories(Application *app)
{
    for (;;) {
        int choix;

        menus_bandeau(app, "Catégories");
        printf("1. Afficher l'arbre\n2. Créer\n3. Renommer\n4. Supprimer\n5. Déplacer\n"
               "6. Réaffecter\n0. Retour\n");
        choix = saisie_menu(6);
        if (choix <= 0) {
            return;
        }
        switch (choix) {
        case 1:
            afficher_arbre(app);
            break;
        case 2:
            creer_categorie(app);
            break;
        case 3:
            renommer_categorie(app);
            break;
        case 5:
            menus_deplacer_categorie(app);
            break;
        case 6:
            menus_reaffecter_categorie(app, ID_AUCUN);
            break;
        default:
            supprimer_categorie(app);
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Budgets mensuels                                                    */
/* ------------------------------------------------------------------ */

static void lister_budgets(const Application *app)
{
    const Tableau *t = &app->donnees.budgets;
    int annee;
    int mois;
    size_t i;
    size_t n = 0;

    if (!menus_saisir_mois(app, &annee, &mois)) {
        return;
    }
    printf("Budgets de %04d-%02d :\n", annee, mois);
    for (i = 0; i < t->taille; i++) {
        const BudgetMensuel *b = ELEMENT(t, BudgetMensuel, i);
        char montant[MONTANT_TEXTE];
        char chemin[CHEMIN_MAX + 1];

        if (b->annee != annee || b->mois != mois) {
            continue;
        }
        montant_formater(b->plafond_ct, montant, sizeof montant);
        if (b->id_categorie == ID_AUCUN) {
            texte_copier(chemin, sizeof chemin, "(budget global)");
        } else {
            regles_chemin_categorie(&app->donnees, b->id_categorie, chemin, sizeof chemin);
        }
        printf("  [%d] ", b->id);
        menus_colonne(chemin, 40);
        printf(" plafond %s\n", montant);
        n++;
    }
    if (n == 0) {
        printf("  Aucun budget défini pour ce mois.\n");
    }
}

static void creer_budget(Application *app)
{
    RapportErreurs r;
    int annee;
    int mois;
    Id id_categorie = ID_AUCUN;
    Id id = ID_AUCUN;
    long long plafond;
    EtatSaisie s;
    Resultat res;
    char message[64];

    if (!menus_saisir_mois(app, &annee, &mois)) {
        return;
    }
    s = saisie_id("Catégorie de dépense (identifiant, Entrée = budget global) : ", &id_categorie);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_VIDE) {
        id_categorie = ID_AUCUN;
    } else {
        const Categorie *c = regles_categorie(&app->donnees, id_categorie);
        if (c == NULL || c->type != CAT_DEPENSE) {
            printf("Catégorie de dépense %d introuvable.\n", id_categorie);
            return;
        }
    }
    if (saisie_montant("Plafond en DH (0 accepté) : ", true, false, &plafond) != SAISIE_OK) {
        menus_abandon();
        return;
    }
    rapport_init(&r);
    res = ref_budget_creer(app, annee, mois, id_categorie, plafond, &id, &r);
    snprintf(message, sizeof message, "Budget créé (identifiant %d).", id);
    menus_resultat(res, &r, message);
    rapport_liberer(&r);
}

static void modifier_budget(Application *app)
{
    RapportErreurs r;
    const BudgetMensuel *b;
    long long plafond;
    char montant[MONTANT_TEXTE];
    char invite[96];
    Id id;

    if (saisie_id("Identifiant du budget : ", &id) != SAISIE_OK) {
        return;
    }
    b = regles_budget(&app->donnees, id);
    if (b == NULL) {
        printf("Budget %d introuvable.\n", id);
        return;
    }
    montant_formater(b->plafond_ct, montant, sizeof montant);
    snprintf(invite, sizeof invite, "Nouveau plafond en DH [%s] : ", montant);
    if (saisie_montant(invite, true, false, &plafond) != SAISIE_OK) {
        menus_abandon();
        return;
    }
    rapport_init(&r);
    menus_resultat(ref_budget_modifier(app, id, plafond, &r), &r, "Plafond modifié.");
    rapport_liberer(&r);
}

static void supprimer_budget(Application *app)
{
    RapportErreurs r;
    bool oui;
    Id id;

    if (saisie_id("Identifiant du budget : ", &id) != SAISIE_OK) {
        return;
    }
    if (regles_budget(&app->donnees, id) == NULL) {
        printf("Budget %d introuvable.\n", id);
        return;
    }
    if (saisie_oui_non("Supprimer ce budget ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
        printf("Suppression annulée.\n");
        return;
    }
    rapport_init(&r);
    menus_resultat(ref_budget_supprimer(app, id, &r), &r, "Budget supprimé.");
    rapport_liberer(&r);
}

void menu_budgets(Application *app)
{
    for (;;) {
        int choix;

        menus_bandeau(app, "Budgets mensuels");
        printf("1. Lister les budgets d'un mois\n2. Créer\n3. Modifier le plafond\n"
               "4. Supprimer\n0. Retour\n");
        choix = saisie_menu(4);
        if (choix <= 0) {
            return;
        }
        switch (choix) {
        case 1:
            lister_budgets(app);
            break;
        case 2:
            creer_budget(app);
            break;
        case 3:
            modifier_budget(app);
            break;
        default:
            supprimer_budget(app);
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Tiers                                                               */
/* ------------------------------------------------------------------ */

static void lister_tiers(const Application *app)
{
    const Tableau *t = &app->donnees.tiers;
    size_t i;

    if (t->taille == 0) {
        printf("Aucun tiers.\n");
        return;
    }
    for (i = 0; i < t->taille; i++) {
        const Tiers *x = ELEMENT(t, Tiers, i);
        Usages u = regles_usages_tiers(&app->donnees, x->id);
        printf("  [%d] ", x->id);
        menus_colonne(x->nom, 30);
        printf(" %lu opération(s), %lu modèle(s)\n", (unsigned long)u.operations,
               (unsigned long)u.modeles);
    }
}

void menu_tiers(Application *app)
{
    for (;;) {
        RapportErreurs r;
        char nom[NOM_MAX + 1];
        char message[64];
        int choix;
        Id id = ID_AUCUN;
        bool oui;
        Resultat res;

        menus_bandeau(app, "Tiers");
        printf("1. Lister\n2. Créer\n3. Renommer\n4. Supprimer\n5. Réaffecter\n0. Retour\n");
        choix = saisie_menu(5);
        if (choix <= 0) {
            return;
        }
        rapport_init(&r);
        switch (choix) {
        case 1:
            lister_tiers(app);
            break;
        case 5:
            menus_reaffecter_tiers(app, ID_AUCUN);
            break;
        case 2:
            if (saisie_texte("Nom du tiers : ", nom, NOM_MAX, false) != SAISIE_OK) {
                menus_abandon();
                break;
            }
            res = ref_tiers_creer(app, nom, &id, &r);
            snprintf(message, sizeof message, "Tiers créé (identifiant %d).", id);
            menus_resultat(res, &r, message);
            break;
        case 3:
            if (saisie_id("Identifiant du tiers : ", &id) != SAISIE_OK) {
                break;
            }
            if (regles_tiers(&app->donnees, id) == NULL) {
                printf("Tiers %d introuvable.\n", id);
                break;
            }
            if (saisie_texte("Nouveau nom : ", nom, NOM_MAX, false) != SAISIE_OK) {
                menus_abandon();
                break;
            }
            menus_resultat(ref_tiers_renommer(app, id, nom, &r), &r, "Tiers renommé.");
            break;
        default:
            if (saisie_id("Identifiant du tiers : ", &id) != SAISIE_OK) {
                break;
            }
            if (regles_tiers(&app->donnees, id) == NULL) {
                printf("Tiers %d introuvable.\n", id);
                break;
            }
            {
                Usages u = regles_usages_tiers(&app->donnees, id);
                if (u.operations > 0 || u.modeles > 0) {
                    printf("Ce tiers est utilisé par %lu opération(s) et %lu modèle(s) : "
                           "réaffectation obligatoire avant suppression.\n",
                           (unsigned long)u.operations, (unsigned long)u.modeles);
                    if (saisie_oui_non("Réaffecter ces liens maintenant ? (o/n) : ", &oui) != SAISIE_OK
                        || !oui || !menus_reaffecter_tiers(app, id)) {
                        break;
                    }
                }
            }
            if (saisie_oui_non("Supprimer ce tiers ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
                printf("Suppression annulée.\n");
                break;
            }
            menus_resultat(ref_tiers_supprimer(app, id, &r), &r, "Tiers supprimé.");
            break;
        }
        rapport_liberer(&r);
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Véhicule                                                            */
/* ------------------------------------------------------------------ */

void menu_vehicule(Application *app)
{
    for (;;) {
        RapportErreurs r;
        char nom[NOM_MAX + 1];
        const Vehicule *v = (app->donnees.vehicules.taille > 0)
                                ? ELEMENT(&app->donnees.vehicules, Vehicule, 0)
                                : NULL;
        int choix;
        bool oui;
        Id id;

        menus_bandeau(app, "Véhicule");
        if (v == NULL) {
            printf("Aucun véhicule enregistré.\n");
        } else {
            Usages u = regles_usages_vehicule(&app->donnees, v->id);
            printf("Véhicule [%d] %s : %lu ligne(s) d'opération, %lu ligne(s) de modèle.\n", v->id,
                   v->nom, (unsigned long)u.lignes_operations, (unsigned long)u.lignes_modeles);
        }
        printf("1. Créer\n2. Renommer\n3. Supprimer\n4. Coûts sur une période\n0. Retour\n");
        choix = saisie_menu(4);
        if (choix <= 0) {
            return;
        }
        rapport_init(&r);
        if (choix == 1) {
            if (v != NULL) {
                printf("Un véhicule existe déjà : BudgetLens suit un seul véhicule.\n");
            } else if (saisie_texte("Nom du véhicule : ", nom, NOM_MAX, false) != SAISIE_OK) {
                menus_abandon();
            } else {
                menus_resultat(ref_vehicule_creer(app, nom, &id, &r), &r, "Véhicule créé.");
            }
        } else if (choix == 2 || choix == 3) {
            if (v == NULL) {
                printf("Aucun véhicule enregistré.\n");
            } else if (choix == 2) {
                id = v->id;
                if (saisie_texte("Nouveau nom : ", nom, NOM_MAX, false) != SAISIE_OK) {
                    menus_abandon();
                } else {
                    menus_resultat(ref_vehicule_renommer(app, id, nom, &r), &r, "Véhicule renommé.");
                }
            } else {
                id = v->id;
                if (saisie_oui_non("Supprimer le véhicule ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
                    printf("Suppression annulée.\n");
                } else {
                    menus_resultat(ref_vehicule_supprimer(app, id, &r), &r, "Véhicule supprimé.");
                }
            }
        } else {
            menus_cout_vehicule(app);
        }
        rapport_liberer(&r);
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}
