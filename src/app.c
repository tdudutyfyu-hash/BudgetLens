/*
 * BudgetLens — app.c
 */
#include "app.h"

#include <stdarg.h>
#include <string.h>

#include "texte.h"

/* ------------------------------------------------------------------ */
/* Rapport d'erreurs                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    char texte[MESSAGE_MAX];
} Message;

void rapport_init(RapportErreurs *r)
{
    tableau_init(&r->messages, sizeof(Message));
    r->nb_ignores = 0;
}

void rapport_vider(RapportErreurs *r)
{
    tableau_vider(&r->messages);
    r->nb_ignores = 0;
}

void rapport_liberer(RapportErreurs *r)
{
    tableau_liberer(&r->messages);
    r->nb_ignores = 0;
}

void rapport_ajouter(RapportErreurs *r, const char *format, ...)
{
    Message m;
    va_list args;

    if (r->messages.taille >= RAPPORT_MESSAGES_MAX) {
        r->nb_ignores++;
        return;
    }
    va_start(args, format);
    if (vsnprintf(m.texte, sizeof m.texte, format, args) < 0) {
        m.texte[0] = '\0';
    }
    va_end(args);
    if (!tableau_ajouter(&r->messages, &m)) {
        r->nb_ignores++;
    }
}

size_t rapport_nombre(const RapportErreurs *r)
{
    return r->messages.taille + r->nb_ignores;
}

const char *rapport_message(const RapportErreurs *r, size_t indice)
{
    return ELEMENT(&r->messages, Message, indice)->texte;
}

void rapport_fusionner(RapportErreurs *dest, const RapportErreurs *src, const char *prefixe)
{
    size_t i;

    for (i = 0; i < src->messages.taille; i++) {
        rapport_ajouter(dest, "%s%s", prefixe, rapport_message(src, i));
    }
    dest->nb_ignores += src->nb_ignores;
}

void rapport_afficher(const RapportErreurs *r, FILE *flux)
{
    size_t i;

    for (i = 0; i < r->messages.taille; i++) {
        fprintf(flux, "  - %s\n", rapport_message(r, i));
    }
    if (r->nb_ignores > 0) {
        fprintf(flux, "  ... et %lu autre(s) message(s).\n", (unsigned long)r->nb_ignores);
    }
}

/* ------------------------------------------------------------------ */
/* Données métier                                                      */
/* ------------------------------------------------------------------ */

void donnees_init(Donnees *d)
{
    int e;

    tableau_init(&d->comptes, sizeof(Compte));
    tableau_init(&d->categories, sizeof(Categorie));
    tableau_init(&d->tiers, sizeof(Tiers));
    tableau_init(&d->vehicules, sizeof(Vehicule));
    tableau_init(&d->budgets, sizeof(BudgetMensuel));
    tableau_init(&d->modeles, sizeof(ModeleOperation));
    tableau_init(&d->lignes_modeles, sizeof(LigneModele));
    tableau_init(&d->recurrences, sizeof(Recurrence));
    tableau_init(&d->operations, sizeof(Operation));
    tableau_init(&d->lignes_operations, sizeof(LigneOperation));
    for (e = 0; e < NB_ENTITES; e++) {
        d->prochain_id[e] = 1;
    }
}

Tableau *donnees_table(Donnees *d, Entite e)
{
    switch (e) {
    case ENT_COMPTE: return &d->comptes;
    case ENT_CATEGORIE: return &d->categories;
    case ENT_TIERS: return &d->tiers;
    case ENT_VEHICULE: return &d->vehicules;
    case ENT_BUDGET: return &d->budgets;
    case ENT_MODELE: return &d->modeles;
    case ENT_LIGNE_MODELE: return &d->lignes_modeles;
    case ENT_RECURRENCE: return &d->recurrences;
    case ENT_OPERATION: return &d->operations;
    case ENT_LIGNE_OPERATION: return &d->lignes_operations;
    case NB_ENTITES: break;
    }
    return &d->comptes;
}

const Tableau *donnees_table_const(const Donnees *d, Entite e)
{
    return donnees_table((Donnees *)d, e);
}

void donnees_liberer(Donnees *d)
{
    int e;
    for (e = 0; e < NB_ENTITES; e++) {
        tableau_liberer(donnees_table(d, (Entite)e));
    }
}

bool donnees_copier(Donnees *dest, const Donnees *src)
{
    int e;

    donnees_init(dest);
    for (e = 0; e < NB_ENTITES; e++) {
        if (!tableau_copier(donnees_table(dest, (Entite)e), donnees_table_const(src, (Entite)e))) {
            donnees_liberer(dest);
            return false;
        }
    }
    memcpy(dest->prochain_id, src->prochain_id, sizeof dest->prochain_id);
    return true;
}

Id donnees_nouvel_id(Donnees *d, Entite e)
{
    Id id = d->prochain_id[e];

    if (id < 1 || id > ID_MAX) {
        return ID_AUCUN;
    }
    d->prochain_id[e] = id + 1;
    return id;
}

Id donnees_id_max(const Donnees *d, Entite e)
{
    const Tableau *t = donnees_table_const(d, e);
    Id max = 0;
    size_t i;

    for (i = 0; i < t->taille; i++) {
        Id id = tableau_id(t, i);
        if (id > max) {
            max = id;
        }
    }
    return max;
}

void donnees_relever_compteurs(Donnees *d, RapportErreurs *avertissements)
{
    int e;

    for (e = 0; e < NB_ENTITES; e++) {
        Id max = donnees_id_max(d, (Entite)e);
        if (max < ID_MAX + 1 && d->prochain_id[e] <= max) {
            rapport_ajouter(avertissements,
                            "Compteur %s relevé de %d à %d (plus grand identifiant présent : %d).",
                            entite_code((Entite)e), d->prochain_id[e], max + 1, max);
            d->prochain_id[e] = max + 1;
        }
    }
}

const char *entite_code(Entite e)
{
    static const char *const codes[NB_ENTITES] = {
        "COMPTE", "CATEGORIE", "TIERS", "VEHICULE", "BUDGET_MENSUEL",
        "MODELE_OPERATION", "LIGNE_MODELE", "RECURRENCE", "OPERATION", "LIGNE_OPERATION"
    };
    return ((unsigned)e < (unsigned)NB_ENTITES) ? codes[e] : "?";
}

const char *entite_libelle(Entite e)
{
    static const char *const libelles[NB_ENTITES] = {
        "compte", "catégorie", "tiers", "véhicule", "budget mensuel",
        "modèle d'opération", "ligne de modèle", "récurrence", "opération",
        "ligne d'opération"
    };
    return ((unsigned)e < (unsigned)NB_ENTITES) ? libelles[e] : "?";
}

/* ------------------------------------------------------------------ */
/* Contexte                                                            */
/* ------------------------------------------------------------------ */

void app_init(Application *app, Date reference, bool simulee, const char *dossier)
{
    donnees_init(&app->donnees);
    app->date_reference = reference;
    app->date_simulee = simulee;
    app->modifie = false;
    app->nb_modifications = 0;
    app->emplacement_charge = 0;
    app->generation_chargee = 0;
    app->generation_max = 0;
    texte_copier(app->dossier_donnees, sizeof app->dossier_donnees, dossier);
    texte_copier(app->dossier_sauvegardes, sizeof app->dossier_sauvegardes, dossier);
}

void app_liberer(Application *app)
{
    donnees_liberer(&app->donnees);
}
