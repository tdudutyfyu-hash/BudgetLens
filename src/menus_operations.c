/*
 * BudgetLens — menus_operations.c
 * Opérations manuelles (ajout, ventilation, correction, suppression),
 * recherche et tri, statistiques des dépenses.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analyses.h"
#include "menus.h"
#include "operations.h"
#include "regles.h"
#include "saisie.h"
#include "texte.h"
#include "valeurs.h"

/* ------------------------------------------------------------------ */
/* Sélection d'un compte, d'un tiers, d'une catégorie terminale        */
/* ------------------------------------------------------------------ */

static void afficher_comptes_courts(const Application *app)
{
    size_t i;

    for (i = 0; i < app->donnees.comptes.taille; i++) {
        const Compte *c = ELEMENT(&app->donnees.comptes, Compte, i);
        printf("  [%d] %s (%s)\n", c->id, c->nom, type_compte_libelle(c->type));
    }
}

/* Compte existant : SAISIE_OK, SAISIE_VIDE (Entrée) ou SAISIE_FIN. */
static EtatSaisie saisir_compte(const Application *app, const char *invite, Id *id)
{
    for (;;) {
        EtatSaisie s = saisie_id(invite, id);

        if (s != SAISIE_OK) {
            return s;
        }
        if (regles_compte(&app->donnees, *id) != NULL) {
            return SAISIE_OK;
        }
        printf("Compte %d introuvable.\n", *id);
    }
}

static void afficher_tiers_courts(const Application *app)
{
    size_t i;

    for (i = 0; i < app->donnees.tiers.taille; i++) {
        const Tiers *t = ELEMENT(&app->donnees.tiers, Tiers, i);
        printf("  [%d] %s\n", t->id, t->nom);
    }
}

/* Tiers existant ; avec zero_efface, 0 signifie « aucun » (*id = ID_AUCUN, SAISIE_EFFACER). */
static EtatSaisie saisir_tiers(const Application *app, const char *invite, bool zero_efface, Id *id)
{
    for (;;) {
        long v;
        EtatSaisie s = saisie_entier(invite, zero_efface ? 0 : 1, ID_MAX, &v);

        if (s != SAISIE_OK) {
            return s;
        }
        if (v == 0) {
            *id = ID_AUCUN;
            return SAISIE_EFFACER;
        }
        *id = (Id)v;
        if (regles_tiers(&app->donnees, *id) != NULL) {
            return SAISIE_OK;
        }
        printf("Tiers %d introuvable.\n", *id);
    }
}

static void afficher_categories_trouvees(const Tableau *liste)
{
    size_t i;

    for (i = 0; i < liste->taille; i++) {
        const CategorieTrouvee *c = ELEMENT(liste, CategorieTrouvee, i);
        printf("  [%d] %s\n", c->id, c->chemin);
    }
}

/* Affiche les catégories terminales du type donné ; renvoie leur nombre. */
static size_t afficher_categories_feuilles(const Application *app, TypeCategorie type)
{
    Tableau liste;
    size_t n;

    tableau_init(&liste, sizeof(CategorieTrouvee));
    if (!analyses_categories_par_nom(&app->donnees, type, "", &liste)) {
        printf("Mémoire insuffisante pour afficher les catégories.\n");
        tableau_liberer(&liste);
        return 0;
    }
    printf("Catégories de %s utilisables (terminales) :\n",
           type == CAT_DEPENSE ? "dépense" : "revenu");
    afficher_categories_trouvees(&liste);
    n = liste.taille;
    tableau_liberer(&liste);
    return n;
}

/* Vrai si le texte n'est composé que de chiffres (saisie d'un identifiant). */
static bool que_des_chiffres(const char *s)
{
    if (*s == '\0') {
        return false;
    }
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
    }
    return true;
}

/* Identifiant saisi : vrai s'il désigne une catégorie terminale du bon type (message sinon). */
static bool categorie_id_valide(const Application *app, TypeCategorie type, Id id)
{
    const Categorie *c = regles_categorie(&app->donnees, id);

    if (c == NULL) {
        printf("Catégorie %d introuvable.\n", id);
    } else if (c->type != type) {
        printf("Cette catégorie est de type %s : choisissez une catégorie de %s.\n",
               type_categorie_libelle(c->type), type == CAT_DEPENSE ? "dépense" : "revenu");
    } else if (!regles_categorie_est_feuille(&app->donnees, id)) {
        printf("Cette catégorie a des sous-catégories : choisissez une catégorie terminale.\n");
    } else {
        return true;
    }
    return false;
}

/*
 * Plusieurs candidates : l'utilisateur choisit l'identifiant de l'une d'elles.
 * SAISIE_OK (choix fait), SAISIE_VIDE (nouvelle recherche) ou SAISIE_FIN.
 */
static EtatSaisie choisir_parmi(const Tableau *candidates, Id *id)
{
    for (;;) {
        size_t i;
        EtatSaisie s = saisie_id("Identifiant de la catégorie choisie (Entrée = nouvelle recherche) : ",
                                 id);

        if (s != SAISIE_OK) {
            return s;
        }
        for (i = 0; i < candidates->taille; i++) {
            const CategorieTrouvee *c = ELEMENT(candidates, CategorieTrouvee, i);
            if (c->id == *id) {
                printf("Catégorie retenue : %s\n", c->chemin);
                return SAISIE_OK;
            }
        }
        printf("L'identifiant %d ne fait pas partie des catégories proposées.\n", *id);
    }
}

/*
 * Catégorie terminale du type donné, par identifiant ou par nom (partie du
 * chemin complet, sans distinction de casse) : SAISIE_OK, SAISIE_VIDE ou
 * SAISIE_FIN. Sans résultat, la question est reposée : les champs déjà saisis
 * de l'opération sont conservés par l'appelant.
 */
static EtatSaisie saisir_categorie_feuille(const Application *app, const char *invite,
                                           TypeCategorie type, Id *id)
{
    char tampon[SAISIE_LIGNE_MAX];
    Tableau candidates;

    tableau_init(&candidates, sizeof(CategorieTrouvee));
    for (;;) {
        EtatSaisie s = saisie_ligne(invite, tampon, sizeof tampon);

        if (s != SAISIE_OK) {
            tableau_liberer(&candidates);
            return s;
        }
        if (que_des_chiffres(tampon)) {
            long long v = strlen(tampon) > 12 ? -1 : strtoll(tampon, NULL, 10);

            if (v < 1 || v > ID_MAX) {
                printf("Identifiant attendu entre 1 et %ld, ou un nom de catégorie.\n", (long)ID_MAX);
                continue;
            }
            *id = (Id)v;
            if (categorie_id_valide(app, type, *id)) {
                tableau_liberer(&candidates);
                return SAISIE_OK;
            }
            continue;
        }
        if (!analyses_categories_par_nom(&app->donnees, type, tampon, &candidates)) {
            printf("Mémoire insuffisante : recherche de catégorie impossible.\n");
            continue;
        }
        if (candidates.taille == 0) {
            printf("Aucune catégorie de %s terminale ne correspond à « %s ». Réessayez "
                   "(identifiant ou autre partie du nom) ; les champs déjà saisis sont conservés.\n",
                   type == CAT_DEPENSE ? "dépense" : "revenu", tampon);
            continue;
        }
        if (candidates.taille == 1) {
            const CategorieTrouvee *c = ELEMENT(&candidates, CategorieTrouvee, 0);
            *id = c->id;
            printf("Catégorie retenue : %s\n", c->chemin);
            tableau_liberer(&candidates);
            return SAISIE_OK;
        }
        printf("%lu catégories correspondent à « %s » :\n", (unsigned long)candidates.taille, tampon);
        afficher_categories_trouvees(&candidates);
        s = choisir_parmi(&candidates, id);
        if (s != SAISIE_VIDE) {
            tableau_liberer(&candidates);
            return s;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Ventilation                                                         */
/* ------------------------------------------------------------------ */

/*
 * Saisit les lignes d'une dépense ou d'un revenu jusqu'à ce que le reste à
 * répartir soit nul. Renvoie faux en cas d'abandon ou de fin d'entrée
 * (lignes alors incomplètes, à ignorer).
 */
static bool saisir_ventilation(const Application *app, Nature nature, long long total,
                               Tableau *lignes)
{
    const TypeCategorie type = (nature == NATURE_DEPENSE) ? CAT_DEPENSE : CAT_REVENU;
    const Vehicule *vehicule = (nature == NATURE_DEPENSE && app->donnees.vehicules.taille > 0)
                                   ? ELEMENT(&app->donnees.vehicules, Vehicule, 0) : NULL;
    long long reste = total;

    tableau_vider(lignes);
    if (afficher_categories_feuilles(app, type) == 0) {
        printf("Aucune catégorie terminale de ce type : créez-en une dans le menu Catégories.\n");
        return false;
    }
    while (reste > 0) {
        char invite[200];
        char texte_reste[MONTANT_TEXTE];
        LigneSaisie l;
        EtatSaisie s;
        unsigned long numero = (unsigned long)lignes->taille + 1;

        montant_formater(reste, texte_reste, sizeof texte_reste);
        printf("Reste à répartir : %s\n", texte_reste);
        memset(&l, 0, sizeof l);
        snprintf(invite, sizeof invite,
                 "Ligne %lu - catégorie (identifiant, ou nom / partie du chemin, ex. courses) : ",
                 numero);
        s = saisir_categorie_feuille(app, invite, type, &l.id_categorie);
        if (s == SAISIE_FIN) {
            return false;
        }
        if (s == SAISIE_VIDE) {
            bool oui;
            if (lignes->taille == 0) {
                return false;
            }
            if (saisie_oui_non("Abandonner la saisie de cette opération ? (o/n) : ", &oui)
                    != SAISIE_OK || oui) {
                return false;
            }
            continue;
        }
        snprintf(invite, sizeof invite, "Ligne %lu - montant en DH [Entrée = %s] : ", numero,
                 texte_reste);
        for (;;) {
            s = saisie_montant(invite, false, false, &l.montant_ct);
            if (s == SAISIE_FIN) {
                return false;
            }
            if (s == SAISIE_VIDE) {
                l.montant_ct = reste;
                break;
            }
            if (l.montant_ct <= reste) {
                break;
            }
            printf("Ce montant dépasse le reste à répartir (%s).\n", texte_reste);
        }
        if (vehicule != NULL) {
            bool oui;
            snprintf(invite, sizeof invite, "Ligne %lu - rattacher au véhicule « %s » ? (o/n) : ",
                     numero, vehicule->nom);
            if (saisie_oui_non(invite, &oui) != SAISIE_OK) {
                return false;
            }
            l.id_vehicule = oui ? vehicule->id : ID_AUCUN;
        }
        if (!tableau_ajouter(lignes, &l)) {
            printf("Mémoire insuffisante.\n");
            return false;
        }
        reste -= l.montant_ct;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Exécution d'une action avec accord sur les avertissements de seuil  */
/* ------------------------------------------------------------------ */

typedef enum { ACT_CREER, ACT_CORRIGER, ACT_SUPPRIMER } Action;

typedef struct {
    Action action;
    Id id;                          /* opération visée (correction, suppression) */
    OperationSaisie saisie;
    const LigneSaisie *lignes;
    size_t nb_lignes;
    Id id_cree;
} Demande;

static Resultat lancer(Application *app, Demande *dm, bool accord, RapportErreurs *w,
                       RapportErreurs *r)
{
    switch (dm->action) {
    case ACT_CREER:
        return op_creer(app, &dm->saisie, dm->lignes, dm->nb_lignes, accord, &dm->id_cree, w, r);
    case ACT_CORRIGER:
        return op_corriger(app, dm->id, &dm->saisie, dm->lignes, dm->nb_lignes, accord, w, r);
    default:
        return op_supprimer(app, dm->id, accord, w, r);
    }
}

/*
 * Récapitulatif d'une opération créée, relu dans les données validées (et non
 * dans la saisie) : nature, montant total, catégorie (ou nombre de catégories
 * distinctes d'une ventilation), comptes d'un virement.
 */
static void afficher_recapitulatif_creation(const Application *app, Id id)
{
    const Donnees *d = &app->donnees;
    const Operation *o = regles_operation(d, id);
    char montant[MONTANT_TEXTE];
    Id premiere = ID_AUCUN;
    size_t distinctes = 0;
    size_t i;
    size_t j;

    if (o == NULL) {
        return;
    }
    montant_formater(o->montant_total_ct, montant, sizeof montant);
    printf("%s · %s · ", o->nature == NATURE_DEPENSE ? "Dépense ajoutée"
                         : (o->nature == NATURE_REVENU ? "Revenu ajouté" : "Virement ajouté"),
           montant);
    if (o->nature == NATURE_VIREMENT) {
        char comptes[2 * NOM_MAX + 8];
        op_description_comptes(d, o, comptes, sizeof comptes);
        printf("%s\n", comptes);
        return;
    }
    /* Catégories distinctes : une ligne compte si aucune ligne précédente de l'opération n'a la même. */
    for (i = 0; i < d->lignes_operations.taille; i++) {
        const LigneOperation *l = ELEMENT(&d->lignes_operations, LigneOperation, i);
        bool deja = false;

        if (l->id_operation != id) {
            continue;
        }
        for (j = 0; j < i && !deja; j++) {
            const LigneOperation *m = ELEMENT(&d->lignes_operations, LigneOperation, j);
            deja = (m->id_operation == id && m->id_categorie == l->id_categorie);
        }
        if (!deja) {
            if (distinctes == 0) {
                premiere = l->id_categorie;
            }
            distinctes++;
        }
    }
    if (distinctes == 1) {
        char chemin[CHEMIN_MAX + 1];
        regles_chemin_categorie(d, premiere, chemin, sizeof chemin);
        printf("%s\n", chemin);
    } else {
        printf("ventilée sur %lu catégories (%lu lignes)\n", (unsigned long)distinctes,
               (unsigned long)op_nombre_lignes(d, id));
    }
}

static void executer(Application *app, Demande *dm)
{
    RapportErreurs r;
    RapportErreurs w;
    char message[96];
    Resultat res;

    rapport_init(&r);
    rapport_init(&w);
    res = lancer(app, dm, false, &w, &r);
    if (res == RES_CONFIRMATION_REQUISE) {
        if (menus_confirmer_avertissements(&w)) {
            rapport_vider(&r);
            res = lancer(app, dm, true, NULL, &r);
        } else {
            rapport_ajouter(&r, "Aucune modification n'a été faite.");
            res = RES_ABANDON;
        }
    }
    switch (dm->action) {
    case ACT_CREER:
        snprintf(message, sizeof message, "Opération créée et confirmée (identifiant %d).",
                 dm->id_cree);
        break;
    case ACT_CORRIGER:
        snprintf(message, sizeof message, "Opération %d corrigée.", dm->id);
        break;
    default:
        snprintf(message, sizeof message, "Opération %d supprimée.", dm->id);
        break;
    }
    if (dm->action == ACT_CREER && res == RES_OK) {
        /* Uniquement après validation complète (avertissement de seuil accepté compris). */
        afficher_recapitulatif_creation(app, dm->id_cree);
    }
    menus_resultat(res, &r, message);
    if (dm->action == ACT_CREER && res == RES_OK) {
        printf("Modifications en mémoire. Pour enregistrer, ouvrez 13. Fichiers.\n");
    }
    rapport_liberer(&w);
    rapport_liberer(&r);
}

/* ------------------------------------------------------------------ */
/* Affichage                                                           */
/* ------------------------------------------------------------------ */

static void afficher_operation(const Application *app, const Operation *o)
{
    const Donnees *d = &app->donnees;
    char montant[MONTANT_TEXTE];
    char date[DATE_TEXTE];
    char comptes[2 * NOM_MAX + 8];
    size_t i;

    montant_formater(o->montant_total_ct, montant, sizeof montant);
    op_description_comptes(d, o, comptes, sizeof comptes);
    printf("Opération %d : %s\n", o->id, o->libelle);
    printf("  Nature        : %s\n", nature_libelle(o->nature));
    printf("  Statut        : %s\n", statut_libelle(o->statut));
    date_formater(o->date_realisation, date);
    printf("  Date réelle   : %s\n", date[0] != '\0' ? date : "-");
    if (o->id_recurrence != ID_AUCUN) {
        date_formater(o->date_prevue_origine, date);
        printf("  Récurrence    : %d (échéance prévue le %s)\n", o->id_recurrence, date);
    }
    printf("  Montant total : %s\n", montant);
    printf("  Compte(s)     : %s\n", comptes);
    if (o->id_tiers != ID_AUCUN) {
        const Tiers *t = regles_tiers(d, o->id_tiers);
        printf("  Tiers         : %s\n", t != NULL ? t->nom : "?");
    } else {
        printf("  Tiers         : -\n");
    }
    if (o->nature == NATURE_VIREMENT) {
        printf("  Ventilation   : aucune (virement)\n");
        return;
    }
    printf("  Ventilation   :\n");
    for (i = 0; i < d->lignes_operations.taille; i++) {
        const LigneOperation *l = ELEMENT(&d->lignes_operations, LigneOperation, i);
        char chemin[CHEMIN_MAX + 1];
        char ml[MONTANT_TEXTE];

        if (l->id_operation != o->id) {
            continue;
        }
        regles_chemin_categorie(d, l->id_categorie, chemin, sizeof chemin);
        montant_formater(l->montant_ct, ml, sizeof ml);
        printf("    - ");
        menus_colonne_tronquee(chemin, 40);
        printf(" %s", ml);
        if (l->id_vehicule != ID_AUCUN) {
            const Vehicule *v = regles_vehicule(d, l->id_vehicule);
            printf("  [véhicule : %s]", v != NULL ? v->nom : "?");
        }
        printf("\n");
    }
}

static void consulter_operation(const Application *app)
{
    Id id;
    const Operation *o;

    if (saisie_id("Identifiant de l'opération : ", &id) != SAISIE_OK) {
        return;
    }
    o = regles_operation(&app->donnees, id);
    if (o == NULL) {
        printf("Opération %d introuvable.\n", id);
        return;
    }
    afficher_operation(app, o);
}

static void afficher_resultats(const Application *app, const Tableau *res, bool avec_attribuable)
{
    size_t i;

    printf("%-5s ", "Id");
    menus_colonne("Date", 11);
    menus_colonne("Nature", 9);
    menus_colonne("Statut", 12);
    menus_colonne("Libellé", 26);
    menus_colonne("Compte(s)", 26);
    menus_colonne("Montant", 16);
    if (avec_attribuable) {
        printf("Attribuable");
    }
    printf("\n");
    for (i = 0; i < res->taille; i++) {
        const ResultatRecherche *x = ELEMENT(res, ResultatRecherche, i);
        const Operation *o = ELEMENT(&app->donnees.operations, Operation, x->indice);
        char date[DATE_TEXTE + 2];
        char montant[MONTANT_TEXTE];
        char comptes[2 * NOM_MAX + 8];

        date_formater(op_date_effective(o), date);
        if (date_absente(o->date_realisation)) {
            size_t n = strlen(date);
            date[n] = '*';          /* date prévue (échéance non confirmée) */
            date[n + 1] = '\0';
        }
        montant_formater(o->montant_total_ct, montant, sizeof montant);
        op_description_comptes(&app->donnees, o, comptes, sizeof comptes);
        printf("%-5d ", o->id);
        menus_colonne(date, 11);
        menus_colonne(nature_libelle(o->nature), 9);
        menus_colonne(statut_libelle(o->statut), 12);
        menus_colonne_tronquee(o->libelle, 26);
        menus_colonne_tronquee(comptes, 26);
        menus_colonne(montant, 16);
        if (avec_attribuable) {
            montant_formater(x->attribuable_ct, montant, sizeof montant);
            printf("%s", montant);
        }
        printf("\n");
    }
}

/* ------------------------------------------------------------------ */
/* Dernières opérations (entrée 14 du menu principal)                  */
/* ------------------------------------------------------------------ */

#define DERNIERES_MAX 10

/* Tableau compact (78 colonnes) : identifiant, date, nature, libellé, montant, statut. */
static void afficher_dernieres(const Application *app, const Tableau *res)
{
    size_t i;

    printf("%-5s ", "Id");
    menus_colonne("Date", 12);
    menus_colonne("Nature", 9);
    menus_colonne("Libellé", 24);
    menus_colonne("Montant", 15);
    printf("Statut\n");
    for (i = 0; i < res->taille; i++) {
        const ResultatRecherche *x = ELEMENT(res, ResultatRecherche, i);
        const Operation *o = ELEMENT(&app->donnees.operations, Operation, x->indice);
        char date[DATE_TEXTE + 2];
        char montant[MONTANT_TEXTE];

        date_formater(op_date_effective(o), date);
        if (date_absente(o->date_realisation)) {
            size_t n = strlen(date);
            date[n] = '*';          /* date prévue : échéance non confirmée */
            date[n + 1] = '\0';
        }
        montant_formater(o->montant_total_ct, montant, sizeof montant);
        printf("%-5d ", o->id);
        menus_colonne(date, 12);
        menus_colonne(nature_libelle(o->nature), 9);
        menus_colonne_tronquee(o->libelle, 23);
        printf(" ");
        menus_colonne(montant, 15);
        printf("%s\n", statut_libelle(o->statut));
    }
}

void menu_dernieres_operations(Application *app)
{
    for (;;) {
        Tableau res;
        size_t total = 0;
        char invite[96];
        Id id;
        const Operation *o;
        EtatSaisie s;

        menus_bandeau(app, "Dernières opérations");
        tableau_init(&res, sizeof(ResultatRecherche));
        if (!analyses_dernieres_operations(&app->donnees, DERNIERES_MAX, &res, &total)) {
            printf("Mémoire insuffisante : liste impossible.\n");
            tableau_liberer(&res);
            return;
        }
        if (res.taille == 0) {
            printf("Aucune opération pour l'instant.\n"
                   "Pour ajouter la première : 1. Opérations, puis 3. Ajouter une dépense "
                   "(4. revenu, 5. virement).\n");
            tableau_liberer(&res);
            return;
        }
        afficher_dernieres(app, &res);
        printf("%lu plus récente(s) sur %lu opération(s), de la plus récente à la plus ancienne "
               "(même date : identifiant croissant).\n", (unsigned long)res.taille,
               (unsigned long)total);
        printf("Date suivie de * : date prévue d'une échéance à confirmer. "
               "Recherche avancée : 1. Opérations > 1.\n");
        tableau_liberer(&res);
        snprintf(invite, sizeof invite, "Identifiant à consulter (Entrée = retour au menu principal) : ");
        s = saisie_id(invite, &id);
        if (s != SAISIE_OK) {
            return;
        }
        o = regles_operation(&app->donnees, id);
        if (o == NULL) {
            printf("Opération %d introuvable.\n", id);
        } else {
            afficher_operation(app, o);
        }
        if (saisie_ligne("Entrée pour revenir à la liste : ", invite, sizeof invite) == SAISIE_FIN) {
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Recherche, filtres et tri                                           */
/* ------------------------------------------------------------------ */

/* Renvoie faux si l'entrée est perdue ou si le filtre est abandonné. */
static bool saisir_filtre(const Application *app, FiltreOperations *f)
{
    long v;
    EtatSaisie s;
    bool toutes;
    Id id;

    filtre_init(f);
    printf("(Entrée : pas de filtre)\n");
    s = saisie_entier("Nature (1 = Dépense, 2 = Revenu, 3 = Virement) : ", 1, 3, &v);
    if (s == SAISIE_FIN) {
        return false;
    }
    if (s == SAISIE_OK) {
        f->par_nature = true;
        f->nature = (Nature)(v - 1);
    }
    s = saisie_entier("Statut (1 = À confirmer, 2 = Confirmée, 3 = Annulée) : ", 1, 3, &v);
    if (s == SAISIE_FIN) {
        return false;
    }
    if (s == SAISIE_OK) {
        f->par_statut = true;
        f->statut = (Statut)(v - 1);
    }
    if (!menus_saisir_periode(app, true, &toutes, &f->debut, &f->fin)) {
        return false;
    }
    f->par_periode = !toutes;
    if (app->donnees.categories.taille > 0) {
        menus_afficher_arbre_categories(app);
        for (;;) {
            s = saisie_id("Catégorie (identifiant, descendants inclus) : ", &id);
            if (s == SAISIE_FIN) {
                return false;
            }
            if (s == SAISIE_VIDE) {
                break;
            }
            if (regles_categorie(&app->donnees, id) != NULL) {
                f->id_categorie = id;
                break;
            }
            printf("Catégorie %d introuvable.\n", id);
        }
    }
    if (app->donnees.comptes.taille > 0) {
        afficher_comptes_courts(app);
        s = saisir_compte(app, "Compte (identifiant) : ", &id);
        if (s == SAISIE_FIN) {
            return false;
        }
        if (s == SAISIE_OK) {
            f->id_compte = id;
        }
    }
    if (app->donnees.tiers.taille > 0) {
        afficher_tiers_courts(app);
        s = saisir_tiers(app, "Tiers (identifiant) : ", false, &id);
        if (s == SAISIE_FIN) {
            return false;
        }
        if (s == SAISIE_OK) {
            f->id_tiers = id;
        }
    }
    s = saisie_texte("Texte contenu dans le libellé : ", f->texte, LIBELLE_MAX, false);
    if (s == SAISIE_FIN) {
        return false;
    }
    return true;
}

static void rechercher_operations(const Application *app)
{
    FiltreOperations f;
    Tableau res;
    long cle = 1;
    long ordre = 1;
    EtatSaisie s;
    CleTri tri;

    if (!saisir_filtre(app, &f)) {
        if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            menus_abandon();
        }
        return;
    }
    s = saisie_entier(f.id_categorie != ID_AUCUN
                          ? "Tri par (1 = date, 2 = montant total, 3 = montant attribuable) [1] : "
                          : "Tri par (1 = date, 2 = montant total) [1] : ",
                      1, f.id_categorie != ID_AUCUN ? 3 : 2, &cle);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_VIDE) {
        cle = 1;
    }
    s = saisie_entier("Ordre (1 = croissant, 2 = décroissant) [1] : ", 1, 2, &ordre);
    if (s == SAISIE_FIN) {
        return;
    }
    if (s == SAISIE_VIDE) {
        ordre = 1;
    }
    tri = cle == 1 ? TRI_DATE : (cle == 2 ? TRI_MONTANT : TRI_ATTRIBUABLE);
    tableau_init(&res, sizeof(ResultatRecherche));
    if (!analyses_rechercher(&app->donnees, &f, &res)) {
        printf("Mémoire insuffisante : recherche impossible.\n");
        tableau_liberer(&res);
        return;
    }
    if (res.taille == 0) {
        printf("Aucune opération ne correspond aux critères.\n");
    } else {
        analyses_trier(&res, tri, ordre == 1);
        afficher_resultats(app, &res, f.id_categorie != ID_AUCUN);
        printf("%lu opération(s). Les dates suivies de * sont des dates prévues "
               "(échéances non confirmées).\n", (unsigned long)res.taille);
        if (f.id_categorie != ID_AUCUN) {
            printf("« Attribuable » : part de l'opération située dans la catégorie filtrée "
                   "(descendants inclus).\n");
        }
    }
    tableau_liberer(&res);
}

/* ------------------------------------------------------------------ */
/* Ajout                                                               */
/* ------------------------------------------------------------------ */

/*
 * Collecte les champs d'une nouvelle opération (ou d'un nouveau modèle, sans date) ;
 * faux si abandon ou fin d'entrée.
 */
bool menus_collecter_saisie(const Application *app, OperationSaisie *s, Tableau *lignes,
                            bool avec_date)
{
    char invite[160];
    char ref[DATE_TEXTE];
    EtatSaisie st;
    Id id;

    if (app->donnees.comptes.taille == 0) {
        printf("Aucun compte : créez d'abord un compte (menu Comptes).\n");
        return false;
    }
    printf("(Entrée sur un champ obligatoire : abandon)\n");
    if (saisie_texte("Libellé : ", s->libelle, LIBELLE_MAX, false) != SAISIE_OK) {
        return false;
    }
    if (avec_date) {
        date_formater(app->date_reference, ref);
        snprintf(invite, sizeof invite, "Date de réalisation [Entrée = %s] : ", ref);
        st = saisie_date(invite, false, &s->date_realisation);
        if (st == SAISIE_FIN) {
            return false;
        }
        if (st == SAISIE_VIDE) {
            s->date_realisation = app->date_reference;
        }
    }
    afficher_comptes_courts(app);
    if (s->nature != NATURE_REVENU) {
        st = saisir_compte(app, s->nature == NATURE_DEPENSE
                                    ? "Compte débité (identifiant) : "
                                    : "Compte source (identifiant) : ", &id);
        if (st != SAISIE_OK) {
            return false;
        }
        s->id_compte_source = id;
    }
    if (s->nature != NATURE_DEPENSE) {
        st = saisir_compte(app, s->nature == NATURE_REVENU
                                    ? "Compte crédité (identifiant) : "
                                    : "Compte destination (identifiant) : ", &id);
        if (st != SAISIE_OK) {
            return false;
        }
        s->id_compte_destination = id;
    }
    if (saisie_montant("Montant total en DH : ", false, false, &s->montant_total_ct) != SAISIE_OK) {
        return false;
    }
    if (s->nature != NATURE_VIREMENT
        && !saisir_ventilation(app, s->nature, s->montant_total_ct, lignes)) {
        return false;
    }
    if (app->donnees.tiers.taille > 0) {
        afficher_tiers_courts(app);
        st = saisir_tiers(app, "Tiers (identifiant, Entrée = aucun) : ", false, &id);
        if (st == SAISIE_FIN) {
            return false;
        }
        if (st == SAISIE_OK) {
            s->id_tiers = id;
        }
    }
    return true;
}

static void ajouter_operation(Application *app, Nature nature)
{
    Demande dm;
    Tableau lignes;

    memset(&dm, 0, sizeof dm);
    dm.action = ACT_CREER;
    dm.saisie.nature = nature;
    tableau_init(&lignes, sizeof(LigneSaisie));
    if (menus_collecter_saisie(app, &dm.saisie, &lignes, true)) {
        dm.lignes = (const LigneSaisie *)lignes.donnees;
        dm.nb_lignes = lignes.taille;
        executer(app, &dm);
    } else if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
        menus_abandon();
    }
    tableau_liberer(&lignes);
}

/* ------------------------------------------------------------------ */
/* Correction et suppression                                           */
/* ------------------------------------------------------------------ */

/* Prompts de correction : Entrée conserve la valeur actuelle. Faux si la fin d'entrée interrompt. */
bool menus_collecter_correction(const Application *app, const Operation *actuelle,
                                 OperationSaisie *s, LigneSaisie **lignes_existantes,
                                 size_t *nb_existantes, Tableau *nouvelles, bool *remplacer)
{
    const Donnees *d = &app->donnees;
    const bool occurrence = (actuelle->id_recurrence != ID_AUCUN);
    char invite[LIBELLE_MAX + 64];
    char tampon[LIBELLE_MAX + 1];
    char montant[MONTANT_TEXTE];
    char date[DATE_TEXTE];
    long long nouveau_total;
    EtatSaisie st;
    Id id;

    *remplacer = false;
    memset(s, 0, sizeof *s);
    s->nature = actuelle->nature;
    s->date_realisation = actuelle->date_realisation;
    texte_copier(s->libelle, sizeof s->libelle, actuelle->libelle);
    s->montant_total_ct = actuelle->montant_total_ct;
    s->id_compte_source = actuelle->id_compte_source;
    s->id_compte_destination = actuelle->id_compte_destination;
    s->id_tiers = actuelle->id_tiers;

    printf("(Entrée : valeur inchangée)\n");
    snprintf(invite, sizeof invite, "Libellé [%s] : ", actuelle->libelle);
    st = saisie_texte(invite, tampon, LIBELLE_MAX, false);
    if (st == SAISIE_FIN) {
        return false;
    }
    if (st == SAISIE_OK) {
        texte_copier(s->libelle, sizeof s->libelle, tampon);
    }
    if (!occurrence) {
        Date nd;
        date_formater(actuelle->date_realisation, date);
        snprintf(invite, sizeof invite, "Date de réalisation [%s] : ", date);
        st = saisie_date(invite, false, &nd);
        if (st == SAISIE_FIN) {
            return false;
        }
        if (st == SAISIE_OK) {
            s->date_realisation = nd;
        }
    }
    montant_formater(actuelle->montant_total_ct, montant, sizeof montant);
    snprintf(invite, sizeof invite, "Montant total [%s] : ", montant);
    st = saisie_montant(invite, false, false, &nouveau_total);
    if (st == SAISIE_FIN) {
        return false;
    }
    if (st == SAISIE_OK) {
        s->montant_total_ct = nouveau_total;
    }
    afficher_comptes_courts(app);
    if (actuelle->nature != NATURE_REVENU) {
        const Compte *c = regles_compte(d, actuelle->id_compte_source);
        snprintf(invite, sizeof invite, "%s [%s] : ",
                 actuelle->nature == NATURE_DEPENSE ? "Compte débité" : "Compte source",
                 c != NULL ? c->nom : "?");
        st = saisir_compte(app, invite, &id);
        if (st == SAISIE_FIN) {
            return false;
        }
        if (st == SAISIE_OK) {
            s->id_compte_source = id;
        }
    }
    if (actuelle->nature != NATURE_DEPENSE) {
        const Compte *c = regles_compte(d, actuelle->id_compte_destination);
        snprintf(invite, sizeof invite, "%s [%s] : ",
                 actuelle->nature == NATURE_REVENU ? "Compte crédité" : "Compte destination",
                 c != NULL ? c->nom : "?");
        st = saisir_compte(app, invite, &id);
        if (st == SAISIE_FIN) {
            return false;
        }
        if (st == SAISIE_OK) {
            s->id_compte_destination = id;
        }
    }
    if (d->tiers.taille > 0) {
        const Tiers *t = actuelle->id_tiers != ID_AUCUN ? regles_tiers(d, actuelle->id_tiers) : NULL;
        afficher_tiers_courts(app);
        snprintf(invite, sizeof invite, "Tiers [%s] (0 = aucun) : ", t != NULL ? t->nom : "aucun");
        st = saisir_tiers(app, invite, true, &id);
        if (st == SAISIE_FIN) {
            return false;
        }
        if (st == SAISIE_OK || st == SAISIE_EFFACER) {
            s->id_tiers = id;
        }
    }
    if (actuelle->nature == NATURE_VIREMENT) {
        return true;
    }
    /* Ventilation : ajustement automatique d'une ligne unique, sinon nouvelle saisie. */
    if (s->montant_total_ct != actuelle->montant_total_ct) {
        if (*nb_existantes == 1) {
            (*lignes_existantes)[0].montant_ct = s->montant_total_ct;
            printf("La ligne de ventilation unique est ajustée au nouveau montant total.\n");
        } else {
            printf("Le montant total a changé : saisissez la nouvelle ventilation.\n");
            *remplacer = true;
        }
    } else {
        bool oui;
        if (saisie_oui_non("Modifier la ventilation ? (o/n) : ", &oui) != SAISIE_OK) {
            return false;
        }
        *remplacer = oui;
    }
    if (*remplacer && !saisir_ventilation(app, actuelle->nature, s->montant_total_ct, nouvelles)) {
        return false;
    }
    return true;
}

static void corriger_operation(Application *app)
{
    Id id;
    const Operation *op;
    Operation actuelle;
    Demande dm;
    LigneSaisie *existantes;
    size_t nb_existantes;
    Tableau nouvelles;
    bool remplacer = false;
    bool complet;

    if (saisie_id("Identifiant de l'opération à corriger : ", &id) != SAISIE_OK) {
        return;
    }
    op = regles_operation(&app->donnees, id);
    if (op == NULL) {
        printf("Opération %d introuvable.\n", id);
        return;
    }
    actuelle = *op;
    if (actuelle.id_recurrence != ID_AUCUN && actuelle.statut != STATUT_A_CONFIRMER) {
        printf("L'occurrence %d est %s : elle n'est plus modifiable (seule son annulation reste "
               "possible, depuis le menu des échéances).\n", id, statut_libelle(actuelle.statut));
        return;
    }
    nb_existantes = op_nombre_lignes(&app->donnees, id);
    existantes = malloc((nb_existantes > 0 ? nb_existantes : 1) * sizeof *existantes);
    if (existantes == NULL) {
        printf("Mémoire insuffisante.\n");
        return;
    }
    op_lignes_copier(&app->donnees, id, existantes, nb_existantes);
    tableau_init(&nouvelles, sizeof(LigneSaisie));
    memset(&dm, 0, sizeof dm);
    dm.action = ACT_CORRIGER;
    dm.id = id;
    complet = menus_collecter_correction(app, &actuelle, &dm.saisie, &existantes, &nb_existantes,
                                   &nouvelles, &remplacer);
    if (complet) {
        if (remplacer) {
            dm.lignes = (const LigneSaisie *)nouvelles.donnees;
            dm.nb_lignes = nouvelles.taille;
        } else {
            dm.lignes = existantes;
            dm.nb_lignes = nb_existantes;
        }
        executer(app, &dm);
    } else if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
        menus_abandon();
    }
    tableau_liberer(&nouvelles);
    free(existantes);
}

static void supprimer_operation(Application *app)
{
    Id id;
    const Operation *op;
    Demande dm;
    bool oui;

    if (saisie_id("Identifiant de l'opération à supprimer : ", &id) != SAISIE_OK) {
        return;
    }
    op = regles_operation(&app->donnees, id);
    if (op == NULL) {
        printf("Opération %d introuvable.\n", id);
        return;
    }
    if (op->id_recurrence != ID_AUCUN) {
        printf("L'opération %d est une occurrence de récurrence : elle n'est jamais supprimée "
               "physiquement. Annulez-la depuis le menu des échéances.\n", id);
        return;
    }
    afficher_operation(app, op);
    if (saisie_oui_non("Supprimer définitivement cette opération et ses lignes ? (o/n) : ", &oui)
            != SAISIE_OK || !oui) {
        printf("Suppression annulée.\n");
        return;
    }
    memset(&dm, 0, sizeof dm);
    dm.action = ACT_SUPPRIMER;
    dm.id = id;
    executer(app, &dm);
}

/* ------------------------------------------------------------------ */
/* Menus                                                               */
/* ------------------------------------------------------------------ */

void menu_operations(Application *app)
{
    for (;;) {
        int choix;

        menus_bandeau(app, "Opérations");
        printf("1. Lister, rechercher et trier\n2. Consulter une opération\n"
               "3. Ajouter une dépense\n4. Ajouter un revenu\n5. Ajouter un virement\n"
               "6. Corriger une opération\n7. Supprimer une opération\n"
               "8. Ajouter depuis un modèle\n0. Retour\n");
        choix = saisie_menu(8);
        if (choix <= 0) {
            return;
        }
        switch (choix) {
        case 1:
            rechercher_operations(app);
            break;
        case 2:
            consulter_operation(app);
            break;
        case 3:
            ajouter_operation(app, NATURE_DEPENSE);
            break;
        case 4:
            ajouter_operation(app, NATURE_REVENU);
            break;
        case 5:
            ajouter_operation(app, NATURE_VIREMENT);
            break;
        case 6:
            corriger_operation(app);
            break;
        case 8:
            menus_operation_depuis_modele(app);
            break;
        default:
            supprimer_operation(app);
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}

bool menus_saisir_ventilation(const Application *app, Nature nature, long long total, Tableau *lignes)
{
    return saisir_ventilation(app, nature, total, lignes);
}

void menus_corriger_operation(Application *app)
{
    corriger_operation(app);
}

void menus_consulter_operation(const Application *app)
{
    consulter_operation(app);
}

void menus_afficher_operation(const Application *app, const Operation *o)
{
    afficher_operation(app, o);
}

/* ------------------------------------------------------------------ */
/* Statistiques des dépenses                                           */
/* ------------------------------------------------------------------ */

void menu_statistiques(Application *app)
{
    bool toutes;
    Date debut;
    Date fin;
    Id id_categorie = ID_AUCUN;
    StatistiquesDepenses st;
    char d1[DATE_TEXTE];
    char d2[DATE_TEXTE];
    char chemin[CHEMIN_MAX + 1];
    char texte[MONTANT_TEXTE];

    menus_bandeau(app, "Statistiques des dépenses");
    printf("Dépenses confirmées uniquement (revenus, virements, échéances non confirmées et "
           "opérations annulées exclus).\n");
    if (!menus_saisir_periode(app, false, &toutes, &debut, &fin)) {
        if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            menus_abandon();
        }
        return;
    }
    if (app->donnees.categories.taille > 0) {
        menus_afficher_arbre_categories(app);
        for (;;) {
            EtatSaisie s = saisie_id("Catégorie à analyser (identifiant, Entrée = toutes) : ",
                                     &id_categorie);
            if (s == SAISIE_FIN) {
                return;
            }
            if (s == SAISIE_VIDE) {
                id_categorie = ID_AUCUN;
                break;
            }
            if (regles_categorie(&app->donnees, id_categorie) != NULL) {
                break;
            }
            printf("Catégorie %d introuvable.\n", id_categorie);
        }
    }
    date_formater(debut, d1);
    date_formater(fin, d2);
    if (id_categorie != ID_AUCUN) {
        regles_chemin_categorie(&app->donnees, id_categorie, chemin, sizeof chemin);
        printf("\nDépenses confirmées du %s au %s, catégorie « %s » (descendants inclus, "
               "montants attribuables) :\n", d1, d2, chemin);
    } else {
        printf("\nDépenses confirmées du %s au %s :\n", d1, d2);
    }
    if (!analyses_statistiques(&app->donnees, debut, fin, id_categorie, &st)) {
        printf("Calcul impossible (mémoire insuffisante ou débordement).\n");
        return;
    }
    if (st.nombre == 0) {
        printf("  Aucune dépense confirmée sur cette période : statistiques non calculables.\n");
        return;
    }
    printf("  Nombre de dépenses : %lu\n", (unsigned long)st.nombre);
    montant_formater(st.total_ct, texte, sizeof texte);
    printf("  Total              : %s\n", texte);
    montant_formater(st.moyenne_ct, texte, sizeof texte);
    printf("  Moyenne            : %s\n", texte);
    montant_formater(st.minimum_ct, texte, sizeof texte);
    printf("  Minimum            : %s\n", texte);
    montant_formater(st.maximum_ct, texte, sizeof texte);
    printf("  Maximum            : %s\n", texte);
}
