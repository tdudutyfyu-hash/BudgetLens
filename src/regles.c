/*
 * BudgetLens — regles.c
 * Le validateur est utilisé au chargement, à la restauration, à l'import et
 * à la fin de chaque transaction. Codes des règles : voir la conception
 * technique, section 8 (V01 à V14).
 */
#include "regles.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calendrier.h"
#include "index_id.h"
#include "soldes.h"
#include "texte.h"
#include "valeurs.h"

/* ------------------------------------------------------------------ */
/* Recherches                                                          */
/* ------------------------------------------------------------------ */

Compte *regles_compte(const Donnees *d, Id id)
{
    return (Compte *)tableau_trouver_id(&d->comptes, id, NULL);
}

Categorie *regles_categorie(const Donnees *d, Id id)
{
    return (Categorie *)tableau_trouver_id(&d->categories, id, NULL);
}

Tiers *regles_tiers(const Donnees *d, Id id)
{
    return (Tiers *)tableau_trouver_id(&d->tiers, id, NULL);
}

Vehicule *regles_vehicule(const Donnees *d, Id id)
{
    return (Vehicule *)tableau_trouver_id(&d->vehicules, id, NULL);
}

BudgetMensuel *regles_budget(const Donnees *d, Id id)
{
    return (BudgetMensuel *)tableau_trouver_id(&d->budgets, id, NULL);
}

/* ------------------------------------------------------------------ */
/* Usages                                                              */
/* ------------------------------------------------------------------ */

static Usages usages_vides(void)
{
    Usages u;
    memset(&u, 0, sizeof u);
    return u;
}

Usages regles_usages_compte(const Donnees *d, Id id)
{
    Usages u = usages_vides();
    size_t i;

    for (i = 0; i < d->operations.taille; i++) {
        const Operation *o = ELEMENT(&d->operations, Operation, i);
        if (o->id_compte_source == id || o->id_compte_destination == id) {
            u.operations++;
        }
    }
    for (i = 0; i < d->modeles.taille; i++) {
        const ModeleOperation *m = ELEMENT(&d->modeles, ModeleOperation, i);
        if (m->id_compte_source == id || m->id_compte_destination == id) {
            u.modeles++;
        }
    }
    return u;
}

Usages regles_usages_tiers(const Donnees *d, Id id)
{
    Usages u = usages_vides();
    size_t i;

    for (i = 0; i < d->operations.taille; i++) {
        if (ELEMENT(&d->operations, Operation, i)->id_tiers == id) {
            u.operations++;
        }
    }
    for (i = 0; i < d->modeles.taille; i++) {
        if (ELEMENT(&d->modeles, ModeleOperation, i)->id_tiers == id) {
            u.modeles++;
        }
    }
    return u;
}

Usages regles_usages_vehicule(const Donnees *d, Id id)
{
    Usages u = usages_vides();
    size_t i;

    for (i = 0; i < d->lignes_operations.taille; i++) {
        if (ELEMENT(&d->lignes_operations, LigneOperation, i)->id_vehicule == id) {
            u.lignes_operations++;
        }
    }
    for (i = 0; i < d->lignes_modeles.taille; i++) {
        if (ELEMENT(&d->lignes_modeles, LigneModele, i)->id_vehicule == id) {
            u.lignes_modeles++;
        }
    }
    return u;
}

Usages regles_usages_categorie(const Donnees *d, Id id)
{
    Usages u = usages_vides();
    size_t i;

    for (i = 0; i < d->lignes_operations.taille; i++) {
        if (ELEMENT(&d->lignes_operations, LigneOperation, i)->id_categorie == id) {
            u.lignes_operations++;
        }
    }
    for (i = 0; i < d->lignes_modeles.taille; i++) {
        if (ELEMENT(&d->lignes_modeles, LigneModele, i)->id_categorie == id) {
            u.lignes_modeles++;
        }
    }
    for (i = 0; i < d->budgets.taille; i++) {
        if (ELEMENT(&d->budgets, BudgetMensuel, i)->id_categorie == id) {
            u.budgets++;
        }
    }
    for (i = 0; i < d->categories.taille; i++) {
        if (ELEMENT(&d->categories, Categorie, i)->id_parent == id) {
            u.enfants++;
        }
    }
    return u;
}

bool regles_categorie_est_feuille(const Donnees *d, Id id)
{
    size_t i;

    for (i = 0; i < d->categories.taille; i++) {
        if (ELEMENT(&d->categories, Categorie, i)->id_parent == id) {
            return false;
        }
    }
    return true;
}

void regles_chemin_categorie(const Donnees *d, Id id, char *sortie, size_t taille)
{
    enum { PROFONDEUR_MAX = 32 };
    const Categorie *chaine[PROFONDEUR_MAX];
    size_t n = 0;
    size_t utilise = 0;
    const Categorie *c = regles_categorie(d, id);

    if (taille == 0) {
        return;
    }
    sortie[0] = '\0';
    while (c != NULL && n < PROFONDEUR_MAX) {
        chaine[n++] = c;
        c = (c->id_parent == ID_AUCUN) ? NULL : regles_categorie(d, c->id_parent);
    }
    while (n > 0) {
        const char *nom = chaine[--n]->nom;
        int ecrit = snprintf(sortie + utilise, taille - utilise, "%s%s",
                             utilise > 0 ? " > " : "", nom);
        if (ecrit < 0 || (size_t)ecrit >= taille - utilise) {
            return;   /* tronqué : texte_copier n'est pas nécessaire ici */
        }
        utilise += (size_t)ecrit;
    }
}

bool regles_nom_compte_libre(const Donnees *d, const char *nom, Id id_exclu)
{
    size_t i;

    for (i = 0; i < d->comptes.taille; i++) {
        const Compte *c = ELEMENT(&d->comptes, Compte, i);
        if (c->id != id_exclu && texte_egal_sans_casse(c->nom, nom)) {
            return false;
        }
    }
    return true;
}

bool regles_nom_categorie_libre(const Donnees *d, Id id_parent, const char *nom, Id id_exclu)
{
    size_t i;

    for (i = 0; i < d->categories.taille; i++) {
        const Categorie *c = ELEMENT(&d->categories, Categorie, i);
        if (c->id != id_exclu && c->id_parent == id_parent
            && texte_egal_sans_casse(c->nom, nom)) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Validateur                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const Donnees *d;
    RapportErreurs *r;
    bool ok;
    Index ix[NB_ENTITES];
} Contexte;

static void erreur(Contexte *c, const char *format, ...)
{
    char message[MESSAGE_MAX];
    va_list args;

    c->ok = false;
    va_start(args, format);
    if (vsnprintf(message, sizeof message, format, args) < 0) {
        message[0] = '\0';
    }
    va_end(args);
    rapport_ajouter(c->r, "%s", message);
}

/* V01 : identifiants strictement positifs et uniques. */
static void valider_identifiants(Contexte *c)
{
    int e;

    for (e = 0; e < NB_ENTITES; e++) {
        const Index *ix = &c->ix[e];
        size_t k;
        for (k = 0; k < ix->n; k++) {
            if (ix->entrees[k].id <= 0) {
                erreur(c, "%s : identifiant invalide (%d)", entite_libelle((Entite)e), ix->entrees[k].id);
            } else if (k > 0 && ix->entrees[k].id == ix->entrees[k - 1].id) {
                erreur(c, "%s %d : identifiant en double", entite_libelle((Entite)e), ix->entrees[k].id);
            }
        }
    }
}

/* V14 : compteurs persistants supérieurs au plus grand identifiant. */
static void valider_compteurs(Contexte *c)
{
    int e;

    for (e = 0; e < NB_ENTITES; e++) {
        Id max = donnees_id_max(c->d, (Entite)e);
        if (c->d->prochain_id[e] < 1 || c->d->prochain_id[e] <= max) {
            c->ok = false;
            rapport_ajouter(c->r, "Compteur %s (%d) inférieur ou égal au plus grand identifiant (%d)",
                            entite_code((Entite)e), c->d->prochain_id[e], max);
        }
    }
}

static void valider_texte(Contexte *c, Entite e, Id id, const char *champ,
                          const char *valeur, size_t max, bool obligatoire)
{
    EtatTexte etat = texte_verifier(valeur, max, obligatoire);
    if (etat != TEXTE_OK) {
        c->ok = false;
        rapport_ajouter(c->r, "%s %d, champ « %s » : %s", entite_libelle(e), id, champ,
                        texte_message(etat));
    }
}

static void reference(Contexte *c, Entite e, Id id, const char *champ, Id cible,
                      Entite e_cible, bool obligatoire)
{
    if (cible == ID_AUCUN) {
        if (obligatoire) {
            c->ok = false;
            rapport_ajouter(c->r, "%s %d : référence « %s » obligatoire absente",
                            entite_libelle(e), id, champ);
        }
        return;
    }
    if (!index_contient(&c->ix[e_cible], cible)) {
        c->ok = false;
        rapport_ajouter(c->r, "%s %d : « %s » désigne un(e) %s inexistant(e) (%d)",
                        entite_libelle(e), id, champ, entite_libelle(e_cible), cible);
    }
}

/* V04 : comptes. */
static void valider_comptes(Contexte *c)
{
    const Tableau *t = &c->d->comptes;
    size_t i;
    size_t j;

    for (i = 0; i < t->taille; i++) {
        const Compte *x = ELEMENT(t, Compte, i);
        valider_texte(c, ENT_COMPTE, x->id, "nom", x->nom, NOM_MAX, true);
        valider_texte(c, ENT_COMPTE, x->id, "nom_banque", x->nom_banque, NOM_BANQUE_MAX, false);
        valider_texte(c, ENT_COMPTE, x->id, "reference_compte", x->reference_compte,
                      REFERENCE_MAX, false);
        if (x->type != COMPTE_BANQUE && x->type != COMPTE_ESPECES) {
            erreur(c, "Compte %d : type de compte invalide", x->id);
        }
        if (x->solde_initial_ct < 0 || x->solde_initial_ct > MONTANT_MAX_CT) {
            erreur(c, "Compte %d : solde initial hors limites (il doit être positif ou nul)", x->id);
        }
        if (!date_valide(x->date_solde_initial)) {
            erreur(c, "Compte %d : date du solde initial invalide", x->id);
        }
        if (x->a_seuil && (x->seuil_minimal_ct < 0 || x->seuil_minimal_ct > MONTANT_MAX_CT)) {
            erreur(c, "Compte %d : seuil minimal hors limites (il doit être positif ou nul)", x->id);
        }
        if (x->type == COMPTE_ESPECES && (x->nom_banque[0] != '\0' || x->reference_compte[0] != '\0')) {
            erreur(c, "Compte %d : un compte d'espèces n'a ni nom de banque ni référence", x->id);
        }
        for (j = i + 1; j < t->taille; j++) {
            const Compte *y = ELEMENT(t, Compte, j);
            if (texte_egal_sans_casse(x->nom, y->nom)) {
                erreur(c, "Compte %d : nom « %s » déjà utilisé par le compte %d", y->id, y->nom, x->id);
            }
        }
    }
}

/* V05 : catégories (hiérarchie, type, unicité locale du nom). */
static void valider_categories(Contexte *c)
{
    const Tableau *t = &c->d->categories;
    size_t i;
    size_t j;

    for (i = 0; i < t->taille; i++) {
        const Categorie *x = ELEMENT(t, Categorie, i);
        valider_texte(c, ENT_CATEGORIE, x->id, "nom", x->nom, NOM_MAX, true);
        if (strchr(x->nom, '>') != NULL) {
            erreur(c, "Catégorie %d : le caractère « > » est interdit dans un nom de catégorie", x->id);
        }
        if (x->type != CAT_DEPENSE && x->type != CAT_REVENU) {
            erreur(c, "Catégorie %d : type de catégorie invalide", x->id);
        }
        reference(c, ENT_CATEGORIE, x->id, "id_parent", x->id_parent, ENT_CATEGORIE, false);
        if (x->id_parent != ID_AUCUN) {
            long ip = index_indice(&c->ix[ENT_CATEGORIE], x->id_parent);
            size_t pas = 0;
            Id p = x->id_parent;

            if (ip >= 0 && ELEMENT(t, Categorie, (size_t)ip)->type != x->type) {
                erreur(c, "Catégorie %d : son type doit être celui de sa catégorie parente", x->id);
            }
            /* Remonter la chaîne des parents : revenir sur x signale un cycle. */
            while (p != ID_AUCUN && pas <= t->taille) {
                long k;
                if (p == x->id) {
                    erreur(c, "Catégorie %d : cycle dans la hiérarchie des catégories", x->id);
                    break;
                }
                k = index_indice(&c->ix[ENT_CATEGORIE], p);
                if (k < 0) {
                    break;
                }
                p = ELEMENT(t, Categorie, (size_t)k)->id_parent;
                pas++;
            }
        }
        for (j = i + 1; j < t->taille; j++) {
            const Categorie *y = ELEMENT(t, Categorie, j);
            if (x->id_parent == y->id_parent && texte_egal_sans_casse(x->nom, y->nom)) {
                erreur(c, "Catégorie %d : nom « %s » déjà utilisé sous le même parent (catégorie %d)", y->id, y->nom, x->id);
            }
        }
    }
}

/* V03/V06 : tiers et véhicule unique. */
static void valider_tiers_vehicules(Contexte *c)
{
    size_t i;

    for (i = 0; i < c->d->tiers.taille; i++) {
        const Tiers *x = ELEMENT(&c->d->tiers, Tiers, i);
        valider_texte(c, ENT_TIERS, x->id, "nom", x->nom, NOM_MAX, true);
    }
    for (i = 0; i < c->d->vehicules.taille; i++) {
        const Vehicule *x = ELEMENT(&c->d->vehicules, Vehicule, i);
        valider_texte(c, ENT_VEHICULE, x->id, "nom", x->nom, NOM_MAX, true);
    }
    if (c->d->vehicules.taille > 1) {
        c->ok = false;
        rapport_ajouter(c->r, "Un seul véhicule est autorisé (%lu présents)",
                        (unsigned long)c->d->vehicules.taille);
    }
}

/* V07 : budgets mensuels. */
static void valider_budgets(Contexte *c)
{
    const Tableau *t = &c->d->budgets;
    size_t i;
    size_t j;

    for (i = 0; i < t->taille; i++) {
        const BudgetMensuel *x = ELEMENT(t, BudgetMensuel, i);
        if (x->annee < ANNEE_MIN || x->annee > ANNEE_MAX || x->mois < 1 || x->mois > 12) {
            erreur(c, "Budget %d : année ou mois invalide", x->id);
        }
        if (x->plafond_ct < 0 || x->plafond_ct > MONTANT_MAX_CT) {
            erreur(c, "Budget %d : plafond hors limites", x->id);
        }
        reference(c, ENT_BUDGET, x->id, "id_categorie", x->id_categorie, ENT_CATEGORIE, false);
        if (x->id_categorie != ID_AUCUN) {
            long k = index_indice(&c->ix[ENT_CATEGORIE], x->id_categorie);
            if (k >= 0 && ELEMENT(&c->d->categories, Categorie, (size_t)k)->type != CAT_DEPENSE) {
                erreur(c, "Budget %d : un budget ne peut viser qu'une catégorie de dépense", x->id);
            }
        }
        for (j = i + 1; j < t->taille; j++) {
            const BudgetMensuel *y = ELEMENT(t, BudgetMensuel, j);
            if (x->annee == y->annee && x->mois == y->mois && x->id_categorie == y->id_categorie) {
                erreur(c, "Budget %d : %s", y->id,
                       y->id_categorie == ID_AUCUN
                           ? "un budget global existe déjà pour ce mois"
                           : "cette catégorie a déjà un budget pour ce mois");
            }
        }
    }
}

/* V02/V03 : références et textes des modèles, récurrences et opérations. */
static void valider_references_operations(Contexte *c)
{
    const Donnees *d = c->d;
    size_t i;

    for (i = 0; i < d->modeles.taille; i++) {
        const ModeleOperation *x = ELEMENT(&d->modeles, ModeleOperation, i);
        valider_texte(c, ENT_MODELE, x->id, "nom", x->nom, NOM_MAX, true);
        valider_texte(c, ENT_MODELE, x->id, "libelle", x->libelle, LIBELLE_MAX, true);
        reference(c, ENT_MODELE, x->id, "id_compte_source", x->id_compte_source, ENT_COMPTE, false);
        reference(c, ENT_MODELE, x->id, "id_compte_destination", x->id_compte_destination,
                  ENT_COMPTE, false);
        reference(c, ENT_MODELE, x->id, "id_tiers", x->id_tiers, ENT_TIERS, false);
    }
    for (i = 0; i < d->lignes_modeles.taille; i++) {
        const LigneModele *x = ELEMENT(&d->lignes_modeles, LigneModele, i);
        reference(c, ENT_LIGNE_MODELE, x->id, "id_modele", x->id_modele, ENT_MODELE, true);
        reference(c, ENT_LIGNE_MODELE, x->id, "id_categorie", x->id_categorie, ENT_CATEGORIE, true);
        reference(c, ENT_LIGNE_MODELE, x->id, "id_vehicule", x->id_vehicule, ENT_VEHICULE, false);
    }
    for (i = 0; i < d->recurrences.taille; i++) {
        const Recurrence *x = ELEMENT(&d->recurrences, Recurrence, i);
        reference(c, ENT_RECURRENCE, x->id, "id_modele", x->id_modele, ENT_MODELE, true);
    }
    for (i = 0; i < d->operations.taille; i++) {
        const Operation *x = ELEMENT(&d->operations, Operation, i);
        valider_texte(c, ENT_OPERATION, x->id, "libelle", x->libelle, LIBELLE_MAX, true);
        reference(c, ENT_OPERATION, x->id, "id_compte_source", x->id_compte_source, ENT_COMPTE, false);
        reference(c, ENT_OPERATION, x->id, "id_compte_destination", x->id_compte_destination,
                  ENT_COMPTE, false);
        reference(c, ENT_OPERATION, x->id, "id_tiers", x->id_tiers, ENT_TIERS, false);
        reference(c, ENT_OPERATION, x->id, "id_recurrence", x->id_recurrence, ENT_RECURRENCE, false);
    }
    for (i = 0; i < d->lignes_operations.taille; i++) {
        const LigneOperation *x = ELEMENT(&d->lignes_operations, LigneOperation, i);
        reference(c, ENT_LIGNE_OPERATION, x->id, "id_operation", x->id_operation, ENT_OPERATION, true);
        reference(c, ENT_LIGNE_OPERATION, x->id, "id_categorie", x->id_categorie, ENT_CATEGORIE, true);
        reference(c, ENT_LIGNE_OPERATION, x->id, "id_vehicule", x->id_vehicule, ENT_VEHICULE, false);
    }
}

/* ------------------------------------------------------------------ */
/* V08 / V10 / V13 : forme des modèles et des opérations               */
/* ------------------------------------------------------------------ */

/* Résumé d'un en-tête (modèle ou opération) et de ses lignes. */
typedef struct {
    size_t nb_lignes;
    long long somme_ct;
    bool somme_debordee;
} CumulLignes;

static const char *libelle_nature_attendue(Nature n)
{
    return n == NATURE_DEPENSE ? "dépense" : (n == NATURE_REVENU ? "revenu" : "virement");
}

/* Contrôles d'une ligne rattachée à un en-tête de nature donnée. */
static void valider_ligne(Contexte *c, Entite e_ligne, Id id_ligne, Entite e_tete, Id id_tete,
                          Nature nature, Id id_cat, Id id_veh, long long montant, const bool *a_enfants)
{
    long k;

    if (montant <= 0 || montant > MONTANT_MAX_CT) {
        erreur(c, "%s %d : montant hors limites (strictement positif, au plus %lld ct)",
               entite_libelle(e_ligne), id_ligne, MONTANT_MAX_CT);
    }
    k = index_indice(&c->ix[ENT_CATEGORIE], id_cat);
    if (k >= 0) {
        const Categorie *cat = ELEMENT(&c->d->categories, Categorie, (size_t)k);

        if (a_enfants[k]) {
            erreur(c, "%s %d : la catégorie « %s » n'est pas terminale (elle a des sous-catégories)",
                   entite_libelle(e_ligne), id_ligne, cat->nom);
        }
        if (nature == NATURE_VIREMENT) {
            erreur(c, "%s %d : un virement ne comporte aucune ligne de catégorie",
                   entite_libelle(e_ligne), id_ligne);
        } else if ((nature == NATURE_DEPENSE) != (cat->type == CAT_DEPENSE)) {
            erreur(c, "%s %d : la catégorie « %s » n'est pas compatible avec %s %d (%s)",
                   entite_libelle(e_ligne), id_ligne, cat->nom, entite_libelle(e_tete), id_tete,
                   libelle_nature_attendue(nature));
        }
    }
    if (id_veh != ID_AUCUN && nature != NATURE_DEPENSE) {
        erreur(c, "%s %d : le véhicule n'est admis que sur une ligne de dépense",
               entite_libelle(e_ligne), id_ligne);
    }
}

/* Matrice nature / comptes, total et ventilation d'un en-tête. */
static void valider_en_tete(Contexte *c, Entite e_tete, Id id, Nature nature, long long total,
                            Id source, Id destination, const CumulLignes *cumul)
{
    const char *nom = entite_libelle(e_tete);

    if (nature != NATURE_DEPENSE && nature != NATURE_REVENU && nature != NATURE_VIREMENT) {
        erreur(c, "%s %d : nature invalide", nom, id);
        return;
    }
    if (total <= 0 || total > MONTANT_MAX_CT) {
        erreur(c, "%s %d : montant total hors limites (strictement positif, au plus %lld ct)", nom,
               id, MONTANT_MAX_CT);
    }
    switch (nature) {
    case NATURE_DEPENSE:
        if (source == ID_AUCUN) {
            erreur(c, "%s %d : une dépense exige un compte source", nom, id);
        }
        if (destination != ID_AUCUN) {
            erreur(c, "%s %d : une dépense n'a pas de compte destination", nom, id);
        }
        break;
    case NATURE_REVENU:
        if (destination == ID_AUCUN) {
            erreur(c, "%s %d : un revenu exige un compte destination", nom, id);
        }
        if (source != ID_AUCUN) {
            erreur(c, "%s %d : un revenu n'a pas de compte source", nom, id);
        }
        break;
    default:
        if (source == ID_AUCUN || destination == ID_AUCUN) {
            erreur(c, "%s %d : un virement exige un compte source et un compte destination", nom, id);
        } else if (source == destination) {
            erreur(c, "%s %d : le compte source et le compte destination doivent être différents",
                   nom, id);
        }
        break;
    }
    if (nature == NATURE_VIREMENT) {
        if (cumul->nb_lignes > 0) {
            erreur(c, "%s %d : un virement ne comporte aucune ligne de catégorie", nom, id);
        }
    } else if (cumul->nb_lignes == 0) {
        erreur(c, "%s %d : au moins une ligne de catégorie est obligatoire", nom, id);
    } else if (cumul->somme_debordee) {
        erreur(c, "%s %d : la somme des lignes déborde la capacité des montants", nom, id);
    } else if (cumul->somme_ct != total) {
        erreur(c, "%s %d : la somme des lignes (%lld ct) diffère du montant total (%lld ct)", nom, id,
               cumul->somme_ct, total);
    }
}

/* Modèles (V08) ou opérations (V10, partie commune) avec leurs lignes. */
static void valider_forme(Contexte *c, bool operations, const bool *a_enfants)
{
    const Tableau *th = operations ? &c->d->operations : &c->d->modeles;
    const Tableau *tl = operations ? &c->d->lignes_operations : &c->d->lignes_modeles;
    const Index *ixh = &c->ix[operations ? ENT_OPERATION : ENT_MODELE];
    const Entite e_tete = operations ? ENT_OPERATION : ENT_MODELE;
    const Entite e_ligne = operations ? ENT_LIGNE_OPERATION : ENT_LIGNE_MODELE;
    CumulLignes *cumul = calloc(th->taille > 0 ? th->taille : 1, sizeof *cumul);
    size_t i;

    if (cumul == NULL) {
        erreur(c, "Mémoire insuffisante pour valider les ventilations.");
        return;
    }
    for (i = 0; i < tl->taille; i++) {
        Id id_ligne;
        Id id_tete;
        Id id_cat;
        Id id_veh;
        long long montant;
        long k;
        Nature nature;

        if (operations) {
            const LigneOperation *x = ELEMENT(tl, LigneOperation, i);
            id_ligne = x->id; id_tete = x->id_operation; id_cat = x->id_categorie;
            id_veh = x->id_vehicule; montant = x->montant_ct;
        } else {
            const LigneModele *x = ELEMENT(tl, LigneModele, i);
            id_ligne = x->id; id_tete = x->id_modele; id_cat = x->id_categorie;
            id_veh = x->id_vehicule; montant = x->montant_ct;
        }
        k = index_indice(ixh, id_tete);
        if (k < 0) {
            continue;   /* référence manquante : déjà signalée par V02 */
        }
        nature = operations ? ELEMENT(th, Operation, (size_t)k)->nature
                            : ELEMENT(th, ModeleOperation, (size_t)k)->nature;
        cumul[k].nb_lignes++;
        if (!cumul[k].somme_debordee
            && !montant_additionner(cumul[k].somme_ct, montant, &cumul[k].somme_ct)) {
            cumul[k].somme_debordee = true;
        }
        valider_ligne(c, e_ligne, id_ligne, e_tete, id_tete, nature, id_cat, id_veh, montant,
                      a_enfants);
    }
    for (i = 0; i < th->taille; i++) {
        if (operations) {
            const Operation *x = ELEMENT(th, Operation, i);
            valider_en_tete(c, e_tete, x->id, x->nature, x->montant_total_ct, x->id_compte_source,
                            x->id_compte_destination, &cumul[i]);
        } else {
            const ModeleOperation *x = ELEMENT(th, ModeleOperation, i);
            valider_en_tete(c, e_tete, x->id, x->nature, x->montant_total_ct, x->id_compte_source,
                            x->id_compte_destination, &cumul[i]);
        }
    }
    free(cumul);
}

typedef struct {
    Id id_recurrence;
    long jours;
    Id id_operation;
} CleOccurrence;

static int comparer_cles(const void *a, const void *b)
{
    const CleOccurrence *x = a;
    const CleOccurrence *y = b;

    if (x->id_recurrence != y->id_recurrence) {
        return x->id_recurrence < y->id_recurrence ? -1 : 1;
    }
    return (x->jours > y->jours) - (x->jours < y->jours);
}

/* V10 (statuts, dates, occurrences), V11 (Q3). */
static void valider_operations(Contexte *c)
{
    const Tableau *t = &c->d->operations;
    CleOccurrence *cles = malloc((t->taille > 0 ? t->taille : 1) * sizeof *cles);
    size_t nc = 0;
    size_t i;

    for (i = 0; i < t->taille; i++) {
        const Operation *x = ELEMENT(t, Operation, i);
        const bool manuelle = (x->id_recurrence == ID_AUCUN);

        if (x->statut != STATUT_A_CONFIRMER && x->statut != STATUT_CONFIRMEE
            && x->statut != STATUT_ANNULEE) {
            erreur(c, "Opération %d : statut invalide", x->id);
            continue;
        }
        if (manuelle && x->statut != STATUT_CONFIRMEE) {
            erreur(c, "Opération %d : une opération manuelle est toujours confirmée (G2)", x->id);
        }
        if (manuelle != date_absente(x->date_prevue_origine)) {
            erreur(c, "Opération %d : id_recurrence et date_prevue_origine doivent être présents "
                      "ensemble ou absents ensemble", x->id);
        }
        if (!date_absente(x->date_prevue_origine) && !date_valide(x->date_prevue_origine)) {
            erreur(c, "Opération %d : date prévue d'origine invalide", x->id);
        }
        if (x->statut == STATUT_CONFIRMEE && !date_valide(x->date_realisation)) {
            erreur(c, "Opération %d : la date de réalisation est obligatoire et doit être valide "
                      "pour une opération confirmée", x->id);
        } else if (x->statut == STATUT_A_CONFIRMER && !date_absente(x->date_realisation)) {
            erreur(c, "Opération %d : une opération à confirmer n'a pas de date de réalisation", x->id);
        } else if (x->statut == STATUT_ANNULEE && !date_absente(x->date_realisation)
                   && !date_valide(x->date_realisation)) {
            erreur(c, "Opération %d : date de réalisation invalide", x->id);
        }
        if (!manuelle && date_valide(x->date_prevue_origine)) {
            /* V10 : l'occurrence est sur le calendrier de sa récurrence, dans ses bornes. */
            long ir = index_indice(&c->ix[ENT_RECURRENCE], x->id_recurrence);
            if (ir >= 0) {
                const Recurrence *rec = ELEMENT(&c->d->recurrences, Recurrence, (size_t)ir);
                if (date_valide(rec->date_debut) && !cal_sur_calendrier(rec, x->date_prevue_origine)) {
                    erreur(c, "Opération %d : la date prévue d'origine n'est pas une échéance du "
                              "calendrier de la récurrence %d", x->id, rec->id);
                }
            }
        }
        if (!manuelle && cles != NULL && date_valide(x->date_prevue_origine)) {
            cles[nc].id_recurrence = x->id_recurrence;
            cles[nc].jours = date_vers_jours(x->date_prevue_origine);
            cles[nc].id_operation = x->id;
            nc++;
        }
        /* V11 (Q3) : aucune opération confirmée avant la date du solde initial d'un compte touché. */
        if (x->statut == STATUT_CONFIRMEE && date_valide(x->date_realisation)) {
            const Id comptes[2] = {x->id_compte_source, x->id_compte_destination};
            int k;

            for (k = 0; k < 2; k++) {
                long ic = comptes[k] == ID_AUCUN ? -1 : index_indice(&c->ix[ENT_COMPTE], comptes[k]);

                if (ic >= 0) {
                    const Compte *cpt = ELEMENT(&c->d->comptes, Compte, (size_t)ic);
                    if (date_valide(cpt->date_solde_initial)
                        && date_comparer(x->date_realisation, cpt->date_solde_initial) < 0) {
                        char d1[DATE_TEXTE];
                        char d2[DATE_TEXTE];
                        date_formater(x->date_realisation, d1);
                        date_formater(cpt->date_solde_initial, d2);
                        erreur(c, "Opération %d : la date %s précède la date du solde initial (%s) "
                                  "du compte « %s »", x->id, d1, d2, cpt->nom);
                    }
                }
            }
        }
    }
    if (cles == NULL) {
        erreur(c, "Mémoire insuffisante pour vérifier l'unicité des occurrences.");
        return;
    }
    qsort(cles, nc, sizeof *cles, comparer_cles);
    for (i = 1; i < nc; i++) {
        if (comparer_cles(&cles[i - 1], &cles[i]) == 0) {
            char d[DATE_TEXTE];
            date_formater(jours_vers_date(cles[i].jours), d);
            erreur(c, "Opération %d : la récurrence %d a déjà une occurrence (opération %d) à la "
                      "date prévue %s", cles[i].id_operation, cles[i].id_recurrence,
                   cles[i - 1].id_operation, d);
        }
    }
    free(cles);
}

/* V09 (partie structurelle) : le calendrier et le curseur sont contrôlés avec les récurrences. */
static void valider_recurrences(Contexte *c)
{
    const Tableau *t = &c->d->recurrences;
    size_t i;

    for (i = 0; i < t->taille; i++) {
        const Recurrence *x = ELEMENT(t, Recurrence, i);

        if (x->frequence != FREQ_HEBDOMADAIRE && x->frequence != FREQ_MENSUELLE) {
            erreur(c, "Récurrence %d : fréquence invalide", x->id);
        }
        if (!date_valide(x->date_debut)) {
            erreur(c, "Récurrence %d : date de début invalide", x->id);
        }
        if (!date_absente(x->date_fin)
            && (!date_valide(x->date_fin) || date_comparer(x->date_fin, x->date_debut) < 0)) {
            erreur(c, "Récurrence %d : la date de fin doit être valide et au moins égale à la date "
                      "de début", x->id);
        }
        if ((x->frequence == FREQ_MENSUELLE) != (x->jour_mensuel_souhaite != 0)
            || x->jour_mensuel_souhaite < 0 || x->jour_mensuel_souhaite > 31) {
            erreur(c, "Récurrence %d : le jour mensuel (1 à 31) est obligatoire pour une récurrence "
                      "mensuelle et absent pour une hebdomadaire", x->id);
        }
        if (!date_absente(x->date_prochaine_echeance) && !date_valide(x->date_prochaine_echeance)) {
            erreur(c, "Récurrence %d : date de prochaine échéance invalide", x->id);
        } else if (date_absente(x->date_prochaine_echeance)) {
            /* Curseur NULL = plus d'échéance : impossible sans date de fin. */
            if (date_absente(x->date_fin)) {
                erreur(c, "Récurrence %d : sans date de fin, la prochaine échéance ne peut pas être "
                          "absente", x->id);
            }
        } else if (date_valide(x->date_debut)) {
            /* Invariants du curseur (MLD §6.4) : sur le calendrier, dans les bornes, strictement
               postérieur à toutes les occurrences existantes. */
            size_t k;

            if (!cal_sur_calendrier(x, x->date_prochaine_echeance)) {
                erreur(c, "Récurrence %d : la prochaine échéance n'est pas sur le calendrier ou "
                          "sort des bornes de la récurrence", x->id);
            }
            for (k = 0; k < c->d->operations.taille; k++) {
                const Operation *o = ELEMENT(&c->d->operations, Operation, k);
                if (o->id_recurrence == x->id && date_valide(o->date_prevue_origine)
                    && date_comparer(o->date_prevue_origine, x->date_prochaine_echeance) >= 0) {
                    erreur(c, "Récurrence %d : la prochaine échéance doit être postérieure à "
                              "l'occurrence %d", x->id, o->id);
                    break;
                }
            }
        }
    }
}

bool regles_valider(const Donnees *d, RapportErreurs *r)
{
    Contexte c;
    int e;
    bool memoire = true;
    bool *a_enfants = NULL;

    c.d = d;
    c.r = r;
    c.ok = true;
    for (e = 0; e < NB_ENTITES; e++) {
        c.ix[e].entrees = NULL;
        c.ix[e].n = 0;
    }
    for (e = 0; e < NB_ENTITES && memoire; e++) {
        memoire = index_construire(donnees_table_const(d, (Entite)e), &c.ix[e]);
    }
    if (memoire) {
        size_t i;
        a_enfants = calloc(d->categories.taille > 0 ? d->categories.taille : 1, sizeof *a_enfants);
        memoire = (a_enfants != NULL);
        for (i = 0; memoire && i < d->categories.taille; i++) {
            long k = index_indice(&c.ix[ENT_CATEGORIE],
                                  ELEMENT(&d->categories, Categorie, i)->id_parent);
            if (k >= 0) {
                a_enfants[k] = true;
            }
        }
    }
    if (!memoire) {
        rapport_ajouter(r, "Mémoire insuffisante pour valider les données.");
        c.ok = false;
    } else {
        valider_identifiants(&c);
        valider_compteurs(&c);
        valider_comptes(&c);
        valider_categories(&c);
        valider_tiers_vehicules(&c);
        valider_budgets(&c);
        valider_references_operations(&c);
        valider_forme(&c, false, a_enfants);      /* V08 : modèles et leurs lignes */
        valider_forme(&c, true, a_enfants);       /* V10 : opérations et leurs lignes */
        valider_recurrences(&c);                  /* V09 (structure) */
        valider_operations(&c);                   /* V10, V11 */
        if (!soldes_verifier(d, r)) {             /* V12 (G10) ; V13 : débordements */
            c.ok = false;
        }
    }
    free(a_enfants);
    for (e = 0; e < NB_ENTITES; e++) {
        index_liberer(&c.ix[e]);
    }
    return c.ok;
}

bool regles_categorie_dans_sous_arbre(const Donnees *d, Id racine, Id categorie)
{
    size_t pas = 0;
    Id courant = categorie;

    while (courant != ID_AUCUN && pas <= d->categories.taille) {
        const Categorie *c;

        if (courant == racine) {
            return true;
        }
        c = regles_categorie(d, courant);
        if (c == NULL) {
            return false;
        }
        courant = c->id_parent;
        pas++;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Transactions                                                        */
/* ------------------------------------------------------------------ */

Resultat transaction_debut(const Application *app, Donnees *copie)
{
    return donnees_copier(copie, &app->donnees) ? RES_OK : RES_ERR_MEMOIRE;
}

Resultat transaction_valider(Application *app, Donnees *copie, RapportErreurs *r)
{
    if (!regles_valider(copie, r)) {
        donnees_liberer(copie);
        return RES_ERR_VALIDATION;
    }
    donnees_liberer(&app->donnees);
    app->donnees = *copie;
    app->modifie = true;
    app->nb_modifications++;
    return RES_OK;
}

Resultat transaction_valider_seuil(Application *app, Donnees *copie, bool accord_seuil,
                                   RapportErreurs *avertissements, RapportErreurs *r)
{
    if (!regles_valider(copie, r)) {
        donnees_liberer(copie);
        return RES_ERR_VALIDATION;
    }
    if (!accord_seuil) {
        RapportErreurs w;
        size_t nb;

        rapport_init(&w);
        nb = soldes_avertissements_seuil(&app->donnees, copie, &w);
        if (nb > 0) {
            if (avertissements != NULL) {
                rapport_fusionner(avertissements, &w, "");
            }
            rapport_liberer(&w);
            donnees_liberer(copie);
            return RES_CONFIRMATION_REQUISE;
        }
        rapport_liberer(&w);
    }
    donnees_liberer(&app->donnees);
    app->donnees = *copie;
    app->modifie = true;
    app->nb_modifications++;
    return RES_OK;
}

void transaction_annuler(Donnees *copie)
{
    donnees_liberer(copie);
}
