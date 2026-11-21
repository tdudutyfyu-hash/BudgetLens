/*
 * BudgetLens — referentiel.c
 * Les pré-contrôles produisent des messages précis ; le validateur global
 * garantit ensuite l'ensemble des règles avant le remplacement des données.
 */
#include "referentiel.h"

#include <stdlib.h>
#include <string.h>

#include "analyses.h"
#include "regles.h"
#include "texte.h"
#include "valeurs.h"

/* ------------------------------------------------------------------ */
/* Opérations génériques sur une entité (premier champ = identifiant)  */
/* ------------------------------------------------------------------ */

static Resultat debut(Application *app, Donnees *copie, RapportErreurs *r)
{
    Resultat res = transaction_debut(app, copie);
    if (res != RES_OK) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
    }
    return res;
}

static Resultat ajouter(Application *app, Entite e, void *element, Id *id_cree, RapportErreurs *r)
{
    Donnees copie;
    Id id;
    Resultat res = debut(app, &copie, r);

    if (res != RES_OK) {
        return res;
    }
    id = donnees_nouvel_id(&copie, e);
    if (id == ID_AUCUN) {
        rapport_ajouter(r, "Plus aucun identifiant disponible pour : %s.", entite_libelle(e));
        transaction_annuler(&copie);
        return RES_ERR_VALIDATION;
    }
    memcpy(element, &id, sizeof id);
    if (!tableau_ajouter(donnees_table(&copie, e), element)) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        transaction_annuler(&copie);
        return RES_ERR_MEMOIRE;
    }
    res = transaction_valider(app, &copie, r);
    if (res == RES_OK && id_cree != NULL) {
        *id_cree = id;
    }
    return res;
}

static Resultat remplacer(Application *app, Entite e, const void *element, RapportErreurs *r)
{
    Donnees copie;
    Tableau *t;
    void *cible;
    Id id;
    Resultat res;

    memcpy(&id, element, sizeof id);
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    t = donnees_table(&copie, e);
    cible = tableau_trouver_id(t, id, NULL);
    if (cible == NULL) {
        rapport_ajouter(r, "%s %d introuvable.", entite_libelle(e), id);
        transaction_annuler(&copie);
        return RES_ERR_INTROUVABLE;
    }
    memcpy(cible, element, t->taille_element);
    return transaction_valider(app, &copie, r);
}

static Resultat retirer(Application *app, Entite e, Id id, RapportErreurs *r)
{
    Donnees copie;
    size_t indice;
    Resultat res = debut(app, &copie, r);

    if (res != RES_OK) {
        return res;
    }
    if (tableau_trouver_id(donnees_table(&copie, e), id, &indice) == NULL) {
        rapport_ajouter(r, "%s %d introuvable.", entite_libelle(e), id);
        transaction_annuler(&copie);
        return RES_ERR_INTROUVABLE;
    }
    tableau_retirer(donnees_table(&copie, e), indice);
    return transaction_valider(app, &copie, r);
}

static bool nom_valide(const char *nom, RapportErreurs *r)
{
    EtatTexte etat = texte_verifier(nom, NOM_MAX, true);
    if (etat != TEXTE_OK) {
        rapport_ajouter(r, "Nom refusé : %s.", texte_message(etat));
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Comptes                                                             */
/* ------------------------------------------------------------------ */

Resultat ref_compte_creer(Application *app, const Compte *saisie, Id *id_cree, RapportErreurs *r)
{
    Compte c = *saisie;

    if (!regles_nom_compte_libre(&app->donnees, c.nom, ID_AUCUN)) {
        rapport_ajouter(r, "Un compte porte déjà le nom « %s ».", c.nom);
        return RES_ERR_VALIDATION;
    }
    return ajouter(app, ENT_COMPTE, &c, id_cree, r);
}

Resultat ref_compte_modifier(Application *app, const Compte *nouveau, bool accord_seuil,
                             RapportErreurs *avert, RapportErreurs *r)
{
    Donnees copie;
    Compte *cible;
    Resultat res;

    if (regles_compte(&app->donnees, nouveau->id) == NULL) {
        rapport_ajouter(r, "Compte %d introuvable.", nouveau->id);
        return RES_ERR_INTROUVABLE;
    }
    if (!regles_nom_compte_libre(&app->donnees, nouveau->nom, nouveau->id)) {
        rapport_ajouter(r, "Un autre compte porte déjà le nom « %s ».", nouveau->nom);
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    cible = tableau_trouver_id(&copie.comptes, nouveau->id, NULL);
    if (cible == NULL) {
        transaction_annuler(&copie);
        return RES_ERR_INTROUVABLE;
    }
    *cible = *nouveau;
    /* Q3 et G10 sont revalidés (solde initial, date) ; le seuil est un simple avertissement. */
    return transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
}

Resultat ref_compte_supprimer(Application *app, Id id, RapportErreurs *r)
{
    Usages u;

    if (regles_compte(&app->donnees, id) == NULL) {
        rapport_ajouter(r, "Compte %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    u = regles_usages_compte(&app->donnees, id);
    if (u.operations > 0 || u.modeles > 0) {
        rapport_ajouter(r, "Compte utilisé par %lu opération(s) et %lu modèle(s) : "
                        "réaffectez d'abord ces liens.",
                        (unsigned long)u.operations, (unsigned long)u.modeles);
        return RES_ERR_VALIDATION;
    }
    return retirer(app, ENT_COMPTE, id, r);
}

/* ------------------------------------------------------------------ */
/* Tiers                                                               */
/* ------------------------------------------------------------------ */

Resultat ref_tiers_creer(Application *app, const char *nom, Id *id_cree, RapportErreurs *r)
{
    Tiers t;

    if (!nom_valide(nom, r)) {
        return RES_ERR_VALIDATION;
    }
    memset(&t, 0, sizeof t);
    texte_copier(t.nom, sizeof t.nom, nom);
    return ajouter(app, ENT_TIERS, &t, id_cree, r);
}

Resultat ref_tiers_renommer(Application *app, Id id, const char *nom, RapportErreurs *r)
{
    const Tiers *actuel = regles_tiers(&app->donnees, id);
    Tiers t;

    if (actuel == NULL) {
        rapport_ajouter(r, "Tiers %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (!nom_valide(nom, r)) {
        return RES_ERR_VALIDATION;
    }
    t = *actuel;
    texte_copier(t.nom, sizeof t.nom, nom);
    return remplacer(app, ENT_TIERS, &t, r);
}

Resultat ref_tiers_supprimer(Application *app, Id id, RapportErreurs *r)
{
    Usages u;

    if (regles_tiers(&app->donnees, id) == NULL) {
        rapport_ajouter(r, "Tiers %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    u = regles_usages_tiers(&app->donnees, id);
    if (u.operations > 0 || u.modeles > 0) {
        rapport_ajouter(r, "Tiers utilisé par %lu opération(s) et %lu modèle(s) : "
                        "réaffectez d'abord ces liens.",
                        (unsigned long)u.operations, (unsigned long)u.modeles);
        return RES_ERR_VALIDATION;
    }
    return retirer(app, ENT_TIERS, id, r);
}

/* ------------------------------------------------------------------ */
/* Véhicule                                                            */
/* ------------------------------------------------------------------ */

Resultat ref_vehicule_creer(Application *app, const char *nom, Id *id_cree, RapportErreurs *r)
{
    Vehicule v;

    if (app->donnees.vehicules.taille > 0) {
        rapport_ajouter(r, "Un véhicule existe déjà : BudgetLens suit un seul véhicule.");
        return RES_ERR_VALIDATION;
    }
    if (!nom_valide(nom, r)) {
        return RES_ERR_VALIDATION;
    }
    memset(&v, 0, sizeof v);
    texte_copier(v.nom, sizeof v.nom, nom);
    return ajouter(app, ENT_VEHICULE, &v, id_cree, r);
}

Resultat ref_vehicule_renommer(Application *app, Id id, const char *nom, RapportErreurs *r)
{
    const Vehicule *actuel = regles_vehicule(&app->donnees, id);
    Vehicule v;

    if (actuel == NULL) {
        rapport_ajouter(r, "Véhicule %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (!nom_valide(nom, r)) {
        return RES_ERR_VALIDATION;
    }
    v = *actuel;
    texte_copier(v.nom, sizeof v.nom, nom);
    return remplacer(app, ENT_VEHICULE, &v, r);
}

Resultat ref_vehicule_supprimer(Application *app, Id id, RapportErreurs *r)
{
    Usages u;

    if (regles_vehicule(&app->donnees, id) == NULL) {
        rapport_ajouter(r, "Véhicule %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    u = regles_usages_vehicule(&app->donnees, id);
    if (u.lignes_operations > 0 || u.lignes_modeles > 0) {
        rapport_ajouter(r, "Véhicule rattaché à %lu ligne(s) d'opération et %lu ligne(s) de modèle : "
                        "suppression impossible.",
                        (unsigned long)u.lignes_operations, (unsigned long)u.lignes_modeles);
        return RES_ERR_VALIDATION;
    }
    return retirer(app, ENT_VEHICULE, id, r);
}

/* ------------------------------------------------------------------ */
/* Catégories                                                          */
/* ------------------------------------------------------------------ */

static bool nom_categorie_valide(const char *nom, RapportErreurs *r)
{
    if (!nom_valide(nom, r)) {
        return false;
    }
    if (strchr(nom, '>') != NULL) {
        rapport_ajouter(r, "Le caractère « > » est réservé au chemin des catégories.");
        return false;
    }
    return true;
}

Resultat ref_categorie_creer(Application *app, const char *nom, Id id_parent,
                             TypeCategorie type_racine, Id *id_cree, RapportErreurs *r)
{
    RapportErreurs info;
    Resultat res;

    /* Sans accord explicite, une migration I3 est refusée (voir ref_categorie_creer_i3). */
    rapport_init(&info);
    res = ref_categorie_creer_i3(app, nom, id_parent, type_racine, false, id_cree, &info, r);
    if (res == RES_CONFIRMATION_REQUISE) {
        rapport_fusionner(r, &info, "");
        res = RES_ERR_VALIDATION;
    }
    rapport_liberer(&info);
    return res;
}

Resultat ref_categorie_renommer(Application *app, Id id, const char *nom, RapportErreurs *r)
{
    const Categorie *actuelle = regles_categorie(&app->donnees, id);
    Categorie c;

    if (actuelle == NULL) {
        rapport_ajouter(r, "Catégorie %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (!nom_categorie_valide(nom, r)) {
        return RES_ERR_VALIDATION;
    }
    if (!regles_nom_categorie_libre(&app->donnees, actuelle->id_parent, nom, id)) {
        rapport_ajouter(r, "Le nom « %s » existe déjà à ce niveau de la hiérarchie.", nom);
        return RES_ERR_VALIDATION;
    }
    c = *actuelle;
    texte_copier(c.nom, sizeof c.nom, nom);
    return remplacer(app, ENT_CATEGORIE, &c, r);
}

Resultat ref_categorie_supprimer(Application *app, Id id, bool avec_budgets, RapportErreurs *r)
{
    Donnees copie;
    Usages u;
    size_t indice;
    size_t i;
    Resultat res;

    if (regles_categorie(&app->donnees, id) == NULL) {
        rapport_ajouter(r, "Catégorie %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    u = regles_usages_categorie(&app->donnees, id);
    if (u.enfants > 0) {
        rapport_ajouter(r, "La catégorie a %lu sous-catégorie(s) : supprimez-les ou déplacez-les d'abord.",
                        (unsigned long)u.enfants);
        return RES_ERR_VALIDATION;
    }
    if (u.lignes_operations > 0 || u.lignes_modeles > 0) {
        rapport_ajouter(r, "Catégorie utilisée par %lu ligne(s) d'opération et %lu ligne(s) de modèle : "
                        "réaffectez d'abord ces liens.",
                        (unsigned long)u.lignes_operations, (unsigned long)u.lignes_modeles);
        return RES_ERR_VALIDATION;
    }
    if (u.budgets > 0 && !avec_budgets) {
        rapport_ajouter(r, "La suppression des %lu budget(s) rattaché(s) n'a pas été confirmée.",
                        (unsigned long)u.budgets);
        return RES_ABANDON;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    /* Budgets directement rattachés : supprimés avec la catégorie (accord donné). */
    for (i = copie.budgets.taille; i > 0; i--) {
        if (ELEMENT(&copie.budgets, BudgetMensuel, i - 1)->id_categorie == id) {
            tableau_retirer(&copie.budgets, i - 1);
        }
    }
    if (tableau_trouver_id(&copie.categories, id, &indice) != NULL) {
        tableau_retirer(&copie.categories, indice);
    }
    return transaction_valider(app, &copie, r);
}

/* ------------------------------------------------------------------ */
/* Budgets mensuels                                                    */
/* ------------------------------------------------------------------ */

Resultat ref_budget_creer(Application *app, int annee, int mois, Id id_categorie,
                          long long plafond_ct, Id *id_cree, RapportErreurs *r)
{
    BudgetMensuel b;
    size_t i;

    if (id_categorie != ID_AUCUN) {
        const Categorie *c = regles_categorie(&app->donnees, id_categorie);
        if (c == NULL) {
            rapport_ajouter(r, "Catégorie %d introuvable.", id_categorie);
            return RES_ERR_INTROUVABLE;
        }
        if (c->type != CAT_DEPENSE) {
            rapport_ajouter(r, "Un budget ne peut viser qu'une catégorie de dépense.");
            return RES_ERR_VALIDATION;
        }
    }
    for (i = 0; i < app->donnees.budgets.taille; i++) {
        const BudgetMensuel *x = ELEMENT(&app->donnees.budgets, BudgetMensuel, i);
        if (x->annee == annee && x->mois == mois && x->id_categorie == id_categorie) {
            rapport_ajouter(r, "Un budget %s existe déjà pour %04d-%02d (budget %d) : modifiez son plafond.",
                            id_categorie == ID_AUCUN ? "global" : "pour cette catégorie",
                            annee, mois, x->id);
            return RES_ERR_VALIDATION;
        }
    }
    memset(&b, 0, sizeof b);
    b.annee = annee;
    b.mois = mois;
    b.id_categorie = id_categorie;
    b.plafond_ct = plafond_ct;
    return ajouter(app, ENT_BUDGET, &b, id_cree, r);
}

Resultat ref_budget_modifier(Application *app, Id id, long long plafond_ct, RapportErreurs *r)
{
    const BudgetMensuel *actuel = regles_budget(&app->donnees, id);
    BudgetMensuel b;

    if (actuel == NULL) {
        rapport_ajouter(r, "Budget %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    b = *actuel;
    b.plafond_ct = plafond_ct;
    return remplacer(app, ENT_BUDGET, &b, r);
}

Resultat ref_budget_supprimer(Application *app, Id id, RapportErreurs *r)
{
    return retirer(app, ENT_BUDGET, id, r);
}

/* ------------------------------------------------------------------ */
/* Catégories avancées : I3, déplacement, réaffectation 1A             */
/* ------------------------------------------------------------------ */

static bool est_autres(const char *nom)
{
    return texte_egal_sans_casse(nom, "Autres");
}

static Resultat ajouter_categorie_copie(Donnees *copie, const char *nom, Id parent,
                                        TypeCategorie type, Id *id, RapportErreurs *r)
{
    Categorie c;

    memset(&c, 0, sizeof c);
    c.id = donnees_nouvel_id(copie, ENT_CATEGORIE);
    if (c.id == ID_AUCUN) {
        rapport_ajouter(r, "Plus aucun identifiant disponible pour les catégories.");
        return RES_ERR_VALIDATION;
    }
    texte_copier(c.nom, sizeof c.nom, nom);
    c.type = type;
    c.id_parent = parent;
    if (!tableau_ajouter(&copie->categories, &c)) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return RES_ERR_MEMOIRE;
    }
    *id = c.id;
    return RES_OK;
}

/* Remplace la catégorie « de » par « vers » dans toutes les lignes d'opération et de modèle. */
static void deplacer_lignes_categorie(Donnees *copie, Id de, Id vers)
{
    size_t i;

    for (i = 0; i < copie->lignes_operations.taille; i++) {
        LigneOperation *l = ELEMENT(&copie->lignes_operations, LigneOperation, i);
        if (l->id_categorie == de) {
            l->id_categorie = vers;
        }
    }
    for (i = 0; i < copie->lignes_modeles.taille; i++) {
        LigneModele *l = ELEMENT(&copie->lignes_modeles, LigneModele, i);
        if (l->id_categorie == de) {
            l->id_categorie = vers;
        }
    }
}

/* Message d'information I3 : combien de lignes iront vers « Autres ». */
static void expliquer_migration(const Donnees *d, Id parent, RapportErreurs *avert)
{
    char chemin[CHEMIN_MAX + 1];
    Usages u = regles_usages_categorie(d, parent);

    if (avert == NULL) {
        return;
    }
    regles_chemin_categorie(d, parent, chemin, sizeof chemin);
    rapport_ajouter(avert, "La catégorie « %s » est terminale et utilisée par %lu ligne(s) d'opération "
                           "et %lu ligne(s) de modèle : elles seront déplacées (montants inchangés, "
                           "tous statuts) vers une sous-catégorie « Autres ». Ses budgets restent sur "
                           "« %s ».",
                    chemin, (unsigned long)u.lignes_operations, (unsigned long)u.lignes_modeles, chemin);
}

static bool feuille_utilisee(const Donnees *d, Id id)
{
    Usages u = regles_usages_categorie(d, id);

    return u.enfants == 0 && (u.lignes_operations > 0 || u.lignes_modeles > 0);
}

Resultat ref_categorie_creer_i3(Application *app, const char *nom, Id id_parent,
                                TypeCategorie type_racine, bool accord_migration, Id *id_cree,
                                RapportErreurs *avert, RapportErreurs *r)
{
    Donnees copie;
    TypeCategorie type = type_racine;
    bool migration = false;
    bool demande_est_autres = false;
    Id cible = ID_AUCUN;
    Id nouveau = ID_AUCUN;
    Resultat res;

    if (!nom_categorie_valide(nom, r)) {
        return RES_ERR_VALIDATION;
    }
    if (id_parent != ID_AUCUN) {
        const Categorie *parent = regles_categorie(&app->donnees, id_parent);
        if (parent == NULL) {
            rapport_ajouter(r, "Catégorie parente %d introuvable.", id_parent);
            return RES_ERR_INTROUVABLE;
        }
        type = parent->type;   /* l'enfant hérite du type de son parent */
        migration = feuille_utilisee(&app->donnees, id_parent);
    }
    if (!regles_nom_categorie_libre(&app->donnees, id_parent, nom, ID_AUCUN)) {
        rapport_ajouter(r, "Le nom « %s » existe déjà à ce niveau de la hiérarchie.", nom);
        return RES_ERR_VALIDATION;
    }
    if (migration && !accord_migration) {
        expliquer_migration(&app->donnees, id_parent, avert);
        return RES_CONFIRMATION_REQUISE;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    if (migration) {
        demande_est_autres = est_autres(nom);
        res = ajouter_categorie_copie(&copie, demande_est_autres ? nom : "Autres", id_parent, type,
                                      &cible, r);
        if (res != RES_OK) {
            transaction_annuler(&copie);
            return res;
        }
        deplacer_lignes_categorie(&copie, id_parent, cible);
        nouveau = cible;
    }
    if (!demande_est_autres) {
        res = ajouter_categorie_copie(&copie, nom, id_parent, type, &nouveau, r);
        if (res != RES_OK) {
            transaction_annuler(&copie);
            return res;
        }
    }
    res = transaction_valider(app, &copie, r);
    if (res == RES_OK && id_cree != NULL) {
        *id_cree = nouveau;
    }
    return res;
}

/* Marque, dans drapeaux (indexé par position), « racine » et ses descendants. */
static void marquer_sous_arbre(const Donnees *d, Id racine, bool *drapeaux)
{
    size_t i;

    for (i = 0; i < d->categories.taille; i++) {
        drapeaux[i] = regles_categorie_dans_sous_arbre(d, racine,
                                                       ELEMENT(&d->categories, Categorie, i)->id);
    }
}

/* Position d'une catégorie dans le tableau, ou -1. */
static long position_categorie(const Donnees *d, Id id)
{
    size_t indice;

    return tableau_trouver_id(&d->categories, id, &indice) != NULL ? (long)indice : -1;
}

/* Aperçu : budgets d'ancêtres dont les dépenses cumulées changent entre avant et après. */
static size_t apercu_budgets(const Donnees *avant, const Donnees *apres, RapportErreurs *avert)
{
    size_t nb = 0;
    size_t i;

    for (i = 0; i < avant->budgets.taille; i++) {
        const BudgetMensuel *b = ELEMENT(&avant->budgets, BudgetMensuel, i);
        long long *ta;
        long long *tb;
        long long glob;
        long pa;
        long pb;
        Date debut_mois;
        Date fin_mois;

        if (b->id_categorie == ID_AUCUN) {
            continue;
        }
        pa = position_categorie(avant, b->id_categorie);
        pb = position_categorie(apres, b->id_categorie);
        if (pa < 0 || pb < 0) {
            continue;
        }
        ta = calloc(avant->categories.taille + 1, sizeof *ta);
        tb = calloc(apres->categories.taille + 1, sizeof *tb);
        debut_mois = date_creer(b->annee, b->mois, 1);
        fin_mois = date_creer(b->annee, b->mois, jours_dans_mois(b->annee, b->mois));
        if (ta != NULL && tb != NULL
            && analyses_totaux_categories(avant, debut_mois, fin_mois, NATURE_DEPENSE, ta, &glob)
            && analyses_totaux_categories(apres, debut_mois, fin_mois, NATURE_DEPENSE, tb, &glob)
            && ta[pa] != tb[pb]) {
            char chemin[CHEMIN_MAX + 1];
            char m1[MONTANT_TEXTE];
            char m2[MONTANT_TEXTE];
            char m3[MONTANT_TEXTE];

            regles_chemin_categorie(avant, b->id_categorie, chemin, sizeof chemin);
            montant_formater(b->plafond_ct, m1, sizeof m1);
            montant_formater(ta[pa], m2, sizeof m2);
            montant_formater(tb[pb], m3, sizeof m3);
            rapport_ajouter(avert, "Budget « %s » %04d-%02d : plafond %s ; dépenses cumulées %s avant, "
                                   "%s après le déplacement",
                            chemin, b->annee, b->mois, m1, m2, m3);
            nb++;
        }
        free(ta);
        free(tb);
    }
    return nb;
}

Resultat ref_categorie_deplacer(Application *app, Id id, Id nouveau_parent,
                                TypeCategorie type_racine, bool accord, RapportErreurs *avert,
                                RapportErreurs *r)
{
    const Categorie *cat = regles_categorie(&app->donnees, id);
    TypeCategorie nouveau_type = type_racine;
    bool *sous_arbre = NULL;
    bool migration = false;
    Donnees copie;
    Resultat res;
    size_t i;

    if (cat == NULL) {
        rapport_ajouter(r, "Catégorie %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (nouveau_parent != ID_AUCUN) {
        const Categorie *np = regles_categorie(&app->donnees, nouveau_parent);
        if (np == NULL) {
            rapport_ajouter(r, "Catégorie parente %d introuvable.", nouveau_parent);
            return RES_ERR_INTROUVABLE;
        }
        if (regles_categorie_dans_sous_arbre(&app->donnees, id, nouveau_parent)) {
            rapport_ajouter(r, "Une catégorie ne peut pas être déplacée sous elle-même ni sous "
                               "l'un de ses descendants.");
            return RES_ERR_VALIDATION;
        }
        nouveau_type = np->type;
    }
    if (nouveau_parent == cat->id_parent && nouveau_type == cat->type) {
        rapport_ajouter(r, "La catégorie est déjà à cet emplacement.");
        return RES_ERR_VALIDATION;
    }
    if (!regles_nom_categorie_libre(&app->donnees, nouveau_parent, cat->nom, id)) {
        rapport_ajouter(r, "Le nom « %s » existe déjà parmi les catégories du nouvel emplacement.",
                        cat->nom);
        return RES_ERR_VALIDATION;
    }
    sous_arbre = calloc(app->donnees.categories.taille + 1, sizeof *sous_arbre);
    if (sous_arbre == NULL) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return RES_ERR_MEMOIRE;
    }
    marquer_sous_arbre(&app->donnees, id, sous_arbre);
    if (nouveau_type != cat->type) {
        /* Changement de type : le sous-arbre ne doit être utilisé par aucune ligne ni aucun budget. */
        bool bloque = false;
        for (i = 0; i < app->donnees.categories.taille; i++) {
            if (sous_arbre[i]) {
                const Categorie *c = ELEMENT(&app->donnees.categories, Categorie, i);
                Usages u = regles_usages_categorie(&app->donnees, c->id);
                if (u.lignes_operations > 0 || u.lignes_modeles > 0 || u.budgets > 0) {
                    char chemin[CHEMIN_MAX + 1];
                    regles_chemin_categorie(&app->donnees, c->id, chemin, sizeof chemin);
                    rapport_ajouter(r, "Changement de type impossible : « %s » est utilisée par %lu "
                                       "ligne(s) d'opération, %lu ligne(s) de modèle et %lu budget(s).",
                                    chemin, (unsigned long)u.lignes_operations,
                                    (unsigned long)u.lignes_modeles, (unsigned long)u.budgets);
                    bloque = true;
                }
            }
        }
        if (bloque) {
            free(sous_arbre);
            return RES_ERR_VALIDATION;
        }
    }
    if (nouveau_parent != ID_AUCUN && feuille_utilisee(&app->donnees, nouveau_parent)) {
        migration = true;
        if (est_autres(cat->nom)) {
            rapport_ajouter(r, "Renommez d'abord « %s » : ce nom est réservé à la migration I3 vers "
                               "la feuille utilisée de destination.", cat->nom);
            free(sous_arbre);
            return RES_ERR_VALIDATION;
        }
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        free(sous_arbre);
        return res;
    }
    if (migration) {
        Id cible = ID_AUCUN;
        res = ajouter_categorie_copie(&copie, "Autres", nouveau_parent, nouveau_type, &cible, r);
        if (res != RES_OK) {
            transaction_annuler(&copie);
            free(sous_arbre);
            return res;
        }
        deplacer_lignes_categorie(&copie, nouveau_parent, cible);
    }
    {
        Categorie *c = tableau_trouver_id(&copie.categories, id, NULL);
        c->id_parent = nouveau_parent;
    }
    if (nouveau_type != cat->type) {
        for (i = 0; i < app->donnees.categories.taille; i++) {
            if (sous_arbre[i]) {
                ELEMENT(&copie.categories, Categorie, i)->type = nouveau_type;
            }
        }
    }
    free(sous_arbre);
    if (!accord) {
        RapportErreurs apercu;
        size_t nb;

        rapport_init(&apercu);
        nb = apercu_budgets(&app->donnees, &copie, &apercu);
        if (migration || nb > 0) {
            if (avert != NULL) {
                if (migration) {
                    expliquer_migration(&app->donnees, nouveau_parent, avert);
                }
                rapport_fusionner(avert, &apercu, "");
            }
            rapport_liberer(&apercu);
            transaction_annuler(&copie);
            return RES_CONFIRMATION_REQUISE;
        }
        rapport_liberer(&apercu);
    }
    return transaction_valider(app, &copie, r);
}

Resultat ref_categorie_reaffecter(Application *app, Id x, Id y, RapportErreurs *r)
{
    const Categorie *cx = regles_categorie(&app->donnees, x);
    const Categorie *cy = regles_categorie(&app->donnees, y);
    Donnees copie;
    Usages u;
    Resultat res;

    if (cx == NULL || cy == NULL) {
        rapport_ajouter(r, "Catégorie %d introuvable.", cx == NULL ? x : y);
        return RES_ERR_INTROUVABLE;
    }
    if (x == y) {
        rapport_ajouter(r, "La catégorie de destination doit être différente de la catégorie source.");
        return RES_ERR_VALIDATION;
    }
    if (cx->type != cy->type) {
        rapport_ajouter(r, "La catégorie de destination doit être du même type (%s).",
                        type_categorie_libelle(cx->type));
        return RES_ERR_VALIDATION;
    }
    if (!regles_categorie_est_feuille(&app->donnees, y)) {
        rapport_ajouter(r, "La catégorie de destination doit être terminale (sans sous-catégorie).");
        return RES_ERR_VALIDATION;
    }
    u = regles_usages_categorie(&app->donnees, x);
    if (u.lignes_operations == 0 && u.lignes_modeles == 0) {
        rapport_ajouter(r, "Aucune ligne d'opération ni de modèle n'utilise la catégorie source.");
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    deplacer_lignes_categorie(&copie, x, y);
    return transaction_valider(app, &copie, r);
}

Resultat ref_compte_reaffecter(Application *app, Id x, Id y, bool accord_seuil,
                               RapportErreurs *avert, RapportErreurs *r)
{
    Donnees copie;
    size_t i;
    size_t conflits = 0;
    size_t touches = 0;
    Resultat res;

    if (regles_compte(&app->donnees, x) == NULL || regles_compte(&app->donnees, y) == NULL) {
        rapport_ajouter(r, "Compte %d introuvable.",
                        regles_compte(&app->donnees, x) == NULL ? x : y);
        return RES_ERR_INTROUVABLE;
    }
    if (x == y) {
        rapport_ajouter(r, "Le compte de destination doit être différent du compte source.");
        return RES_ERR_VALIDATION;
    }
    for (i = 0; i < app->donnees.operations.taille; i++) {
        const Operation *o = ELEMENT(&app->donnees.operations, Operation, i);
        if ((o->id_compte_source == x && o->id_compte_destination == y)
            || (o->id_compte_source == y && o->id_compte_destination == x)) {
            rapport_ajouter(r, "Conflit : l'opération %d est un virement entre ces deux comptes.", o->id);
            conflits++;
        }
        if (o->id_compte_source == x || o->id_compte_destination == x) {
            touches++;
        }
    }
    for (i = 0; i < app->donnees.modeles.taille; i++) {
        const ModeleOperation *m = ELEMENT(&app->donnees.modeles, ModeleOperation, i);
        if ((m->id_compte_source == x && m->id_compte_destination == y)
            || (m->id_compte_source == y && m->id_compte_destination == x)) {
            rapport_ajouter(r, "Conflit : le modèle %d est un virement entre ces deux comptes.", m->id);
            conflits++;
        }
        if (m->id_compte_source == x || m->id_compte_destination == x) {
            touches++;
        }
    }
    if (conflits > 0) {
        rapport_ajouter(r, "Réaffectation refusée en bloc : %lu conflit(s).", (unsigned long)conflits);
        return RES_ERR_VALIDATION;
    }
    if (touches == 0) {
        rapport_ajouter(r, "Aucune opération ni aucun modèle n'utilise le compte source.");
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    for (i = 0; i < copie.operations.taille; i++) {
        Operation *o = ELEMENT(&copie.operations, Operation, i);
        if (o->id_compte_source == x) {
            o->id_compte_source = y;
        }
        if (o->id_compte_destination == x) {
            o->id_compte_destination = y;
        }
    }
    for (i = 0; i < copie.modeles.taille; i++) {
        ModeleOperation *m = ELEMENT(&copie.modeles, ModeleOperation, i);
        if (m->id_compte_source == x) {
            m->id_compte_source = y;
        }
        if (m->id_compte_destination == x) {
            m->id_compte_destination = y;
        }
    }
    return transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
}

Resultat ref_tiers_reaffecter(Application *app, Id x, Id y, RapportErreurs *r)
{
    Donnees copie;
    size_t i;
    Usages u;
    Resultat res;

    if (regles_tiers(&app->donnees, x) == NULL || regles_tiers(&app->donnees, y) == NULL) {
        rapport_ajouter(r, "Tiers %d introuvable.", regles_tiers(&app->donnees, x) == NULL ? x : y);
        return RES_ERR_INTROUVABLE;
    }
    if (x == y) {
        rapport_ajouter(r, "Le tiers de destination doit être différent du tiers source.");
        return RES_ERR_VALIDATION;
    }
    u = regles_usages_tiers(&app->donnees, x);
    if (u.operations == 0 && u.modeles == 0) {
        rapport_ajouter(r, "Aucune opération ni aucun modèle n'utilise le tiers source.");
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    for (i = 0; i < copie.operations.taille; i++) {
        Operation *o = ELEMENT(&copie.operations, Operation, i);
        if (o->id_tiers == x) {
            o->id_tiers = y;
        }
    }
    for (i = 0; i < copie.modeles.taille; i++) {
        ModeleOperation *m = ELEMENT(&copie.modeles, ModeleOperation, i);
        if (m->id_tiers == x) {
            m->id_tiers = y;
        }
    }
    return transaction_valider(app, &copie, r);
}
