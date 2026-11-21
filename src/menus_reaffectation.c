/*
 * BudgetLens — menus_reaffectation.c
 * Réaffectation 1A (comptes, catégories, tiers) et déplacement de catégorie.
 */
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "referentiel.h"
#include "regles.h"
#include "saisie.h"
#include "texte.h"
#include "valeurs.h"

/* Identifiant d'un élément existant ; faux si abandon ou fin d'entrée. */
static bool demander_compte(const Application *app, const char *invite, Id *id)
{
    size_t i;

    for (i = 0; i < app->donnees.comptes.taille; i++) {
        const Compte *c = ELEMENT(&app->donnees.comptes, Compte, i);
        printf("  [%d] %s\n", c->id, c->nom);
    }
    if (saisie_id(invite, id) != SAISIE_OK) {
        return false;
    }
    if (regles_compte(&app->donnees, *id) == NULL) {
        printf("Compte %d introuvable.\n", *id);
        return false;
    }
    return true;
}

static bool demander_tiers(const Application *app, const char *invite, Id *id)
{
    size_t i;

    for (i = 0; i < app->donnees.tiers.taille; i++) {
        const Tiers *t = ELEMENT(&app->donnees.tiers, Tiers, i);
        printf("  [%d] %s\n", t->id, t->nom);
    }
    if (saisie_id(invite, id) != SAISIE_OK) {
        return false;
    }
    if (regles_tiers(&app->donnees, *id) == NULL) {
        printf("Tiers %d introuvable.\n", *id);
        return false;
    }
    return true;
}

static bool demander_categorie(const Application *app, const char *invite, Id *id)
{
    menus_afficher_arbre_categories(app);
    if (saisie_id(invite, id) != SAISIE_OK) {
        return false;
    }
    if (regles_categorie(&app->donnees, *id) == NULL) {
        printf("Catégorie %d introuvable.\n", *id);
        return false;
    }
    return true;
}

bool menus_reaffecter_tiers(Application *app, Id x)
{
    RapportErreurs r;
    Id y;
    bool oui;
    Usages u;
    Resultat res;

    if (x == ID_AUCUN && !demander_tiers(app, "Tiers source (identifiant) : ", &x)) {
        return false;
    }
    u = regles_usages_tiers(&app->donnees, x);
    printf("Le tiers source est utilisé par %lu opération(s) et %lu modèle(s).\n",
           (unsigned long)u.operations, (unsigned long)u.modeles);
    if (!demander_tiers(app, "Tiers de destination (identifiant) : ", &y)) {
        return false;
    }
    printf("Toutes les opérations (tous statuts) et tous les modèles passeront de « %s » à « %s ».\n",
           regles_tiers(&app->donnees, x)->nom, regles_tiers(&app->donnees, y)->nom);
    if (saisie_oui_non("Confirmer la réaffectation ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
        printf("Réaffectation annulée.\n");
        return false;
    }
    rapport_init(&r);
    res = ref_tiers_reaffecter(app, x, y, &r);
    menus_resultat(res, &r, "Tiers réaffecté.");
    rapport_liberer(&r);
    return res == RES_OK;
}

bool menus_reaffecter_compte(Application *app, Id x)
{
    RapportErreurs r;
    RapportErreurs w;
    Id y;
    bool oui;
    Usages u;
    Resultat res;

    if (x == ID_AUCUN && !demander_compte(app, "Compte source (identifiant) : ", &x)) {
        return false;
    }
    u = regles_usages_compte(&app->donnees, x);
    printf("Le compte source est utilisé par %lu opération(s) et %lu modèle(s).\n",
           (unsigned long)u.operations, (unsigned long)u.modeles);
    if (!demander_compte(app, "Compte de destination (identifiant) : ", &y)) {
        return false;
    }
    printf("Toutes les opérations (tous statuts) et tous les modèles passeront de « %s » à « %s » "
           "(date d'ouverture, soldes et seuil seront revérifiés).\n",
           regles_compte(&app->donnees, x)->nom, regles_compte(&app->donnees, y)->nom);
    if (saisie_oui_non("Confirmer la réaffectation ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
        printf("Réaffectation annulée.\n");
        return false;
    }
    rapport_init(&r);
    rapport_init(&w);
    res = ref_compte_reaffecter(app, x, y, false, &w, &r);
    if (res == RES_CONFIRMATION_REQUISE) {
        if (menus_confirmer_avertissements(&w)) {
            rapport_vider(&r);
            res = ref_compte_reaffecter(app, x, y, true, NULL, &r);
        } else {
            rapport_ajouter(&r, "Aucune modification n'a été faite.");
            res = RES_ABANDON;
        }
    }
    menus_resultat(res, &r, "Compte réaffecté.");
    rapport_liberer(&w);
    rapport_liberer(&r);
    return res == RES_OK;
}

bool menus_reaffecter_categorie(Application *app, Id x)
{
    RapportErreurs r;
    char chemin_x[CHEMIN_MAX + 1];
    char chemin_y[CHEMIN_MAX + 1];
    Id y;
    bool oui;
    Usages u;
    Resultat res;

    if (x == ID_AUCUN && !demander_categorie(app, "Catégorie source (identifiant) : ", &x)) {
        return false;
    }
    u = regles_usages_categorie(&app->donnees, x);
    printf("La catégorie source est utilisée par %lu ligne(s) d'opération et %lu ligne(s) de modèle.\n",
           (unsigned long)u.lignes_operations, (unsigned long)u.lignes_modeles);
    printf("La destination doit être une catégorie terminale du même type.\n");
    if (!demander_categorie(app, "Catégorie de destination (identifiant) : ", &y)) {
        return false;
    }
    regles_chemin_categorie(&app->donnees, x, chemin_x, sizeof chemin_x);
    regles_chemin_categorie(&app->donnees, y, chemin_y, sizeof chemin_y);
    printf("Toutes les lignes (tous statuts, opérations et modèles) passeront de « %s » à « %s » ; "
           "les montants ne changent pas.\n", chemin_x, chemin_y);
    if (saisie_oui_non("Confirmer la réaffectation ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
        printf("Réaffectation annulée.\n");
        return false;
    }
    rapport_init(&r);
    res = ref_categorie_reaffecter(app, x, y, &r);
    menus_resultat(res, &r, "Catégorie réaffectée.");
    rapport_liberer(&r);
    return res == RES_OK;
}

void menus_deplacer_categorie(Application *app)
{
    RapportErreurs r;
    RapportErreurs w;
    const Categorie *c;
    char chemin[CHEMIN_MAX + 1];
    Id id;
    Id parent = ID_AUCUN;
    TypeCategorie type;
    EtatSaisie s;
    Resultat res;

    if (!demander_categorie(app, "Catégorie à déplacer (identifiant) : ", &id)) {
        return;
    }
    c = regles_categorie(&app->donnees, id);
    type = c->type;
    regles_chemin_categorie(&app->donnees, id, chemin, sizeof chemin);
    printf("Déplacement de « %s » avec toutes ses sous-catégories.\n", chemin);
    s = saisie_id("Nouveau parent (identifiant, Entrée = catégorie racine) : ", &parent);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_VIDE) {
        long t;
        parent = ID_AUCUN;
        s = saisie_entier("Type de la racine (1 = Dépense, 2 = Revenu, Entrée = type actuel) : ", 1, 2, &t);
        if (s == SAISIE_FIN) {
            return;
        }
        if (s == SAISIE_OK) {
            type = (t == 1) ? CAT_DEPENSE : CAT_REVENU;
        }
    }
    rapport_init(&r);
    rapport_init(&w);
    res = ref_categorie_deplacer(app, id, parent, type, false, &w, &r);
    if (res == RES_CONFIRMATION_REQUISE) {
        bool oui = false;

        printf("Effets du déplacement :\n");
        rapport_afficher(&w, stdout);
        if (saisie_oui_non("Appliquer le déplacement ? (o/n) : ", &oui) == SAISIE_OK && oui) {
            rapport_vider(&r);
            res = ref_categorie_deplacer(app, id, parent, type, true, NULL, &r);
        } else {
            rapport_ajouter(&r, "Aucune modification n'a été faite.");
            res = RES_ABANDON;
        }
    }
    menus_resultat(res, &r, "Catégorie déplacée.");
    rapport_liberer(&w);
    rapport_liberer(&r);
}
