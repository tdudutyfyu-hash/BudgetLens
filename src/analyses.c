/*
 * BudgetLens — analyses.c
 */
#include "analyses.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "index_id.h"
#include "operations.h"
#include "regles.h"
#include "texte.h"
#include "valeurs.h"

void filtre_init(FiltreOperations *f)
{
    memset(f, 0, sizeof *f);
    f->id_categorie = ID_AUCUN;
    f->id_compte = ID_AUCUN;
    f->id_tiers = ID_AUCUN;
    f->debut = date_aucune();
    f->fin = date_aucune();
}

/*
 * Montant attribuable de chaque opération au sous-arbre de id_categorie
 * (somme de ses lignes dans le sous-arbre), indexé par position dans
 * Donnees.operations. Un seul passage sur les lignes : pas de double comptage.
 * NULL si la mémoire manque. Le tableau est alloué avec calloc.
 */
static long long *attribuables(const Donnees *d, Id id_categorie, bool *debordement)
{
    long long *montants = calloc(d->operations.taille > 0 ? d->operations.taille : 1, sizeof *montants);
    bool *dans = calloc(d->categories.taille > 0 ? d->categories.taille : 1, sizeof *dans);
    Index ix_op;
    Index ix_cat;
    size_t i;

    *debordement = false;
    if (montants == NULL || dans == NULL) {
        free(montants);
        free(dans);
        return NULL;
    }
    if (!index_construire(&d->operations, &ix_op)) {
        free(montants);
        free(dans);
        return NULL;
    }
    if (!index_construire(&d->categories, &ix_cat)) {
        index_liberer(&ix_op);
        free(montants);
        free(dans);
        return NULL;
    }
    for (i = 0; i < d->categories.taille; i++) {
        dans[i] = regles_categorie_dans_sous_arbre(d, id_categorie,
                                                   ELEMENT(&d->categories, Categorie, i)->id);
    }
    for (i = 0; i < d->lignes_operations.taille; i++) {
        const LigneOperation *l = ELEMENT(&d->lignes_operations, LigneOperation, i);
        long ic = index_indice(&ix_cat, l->id_categorie);
        long io = index_indice(&ix_op, l->id_operation);

        if (ic >= 0 && io >= 0 && dans[ic]
            && !montant_additionner(montants[io], l->montant_ct, &montants[io])) {
            *debordement = true;
        }
    }
    index_liberer(&ix_op);
    index_liberer(&ix_cat);
    free(dans);
    return montants;
}

bool analyses_rechercher(const Donnees *d, const FiltreOperations *f, Tableau *resultats)
{
    long long *attrib = NULL;
    bool debordement = false;
    size_t i;

    tableau_vider(resultats);
    if (f->id_categorie != ID_AUCUN) {
        attrib = attribuables(d, f->id_categorie, &debordement);
        if (attrib == NULL) {
            return false;
        }
    }
    for (i = 0; i < d->operations.taille; i++) {
        const Operation *o = ELEMENT(&d->operations, Operation, i);
        Date date = op_date_effective(o);
        ResultatRecherche x;

        if (f->par_nature && o->nature != f->nature) {
            continue;
        }
        if (f->par_statut && o->statut != f->statut) {
            continue;
        }
        if (f->par_periode) {
            if (date_absente(date) || date_comparer(date, f->debut) < 0
                || date_comparer(date, f->fin) > 0) {
                continue;
            }
        }
        if (f->id_compte != ID_AUCUN && o->id_compte_source != f->id_compte
            && o->id_compte_destination != f->id_compte) {
            continue;
        }
        if (f->id_tiers != ID_AUCUN && o->id_tiers != f->id_tiers) {
            continue;
        }
        if (f->texte[0] != '\0' && !texte_contient_sans_casse(o->libelle, f->texte)) {
            continue;
        }
        if (attrib != NULL && attrib[i] <= 0) {
            continue;   /* aucune ligne dans le sous-arbre : virement ou autre catégorie */
        }
        x.indice = i;
        x.id = o->id;
        x.jours = date_absente(date) ? 0 : date_vers_jours(date);
        x.total_ct = o->montant_total_ct;
        x.attribuable_ct = attrib != NULL ? attrib[i] : o->montant_total_ct;
        x.cle = 0;
        if (!tableau_ajouter(resultats, &x)) {
            free(attrib);
            return false;
        }
    }
    free(attrib);
    return true;
}

static int comparer_croissant(const void *a, const void *b)
{
    const ResultatRecherche *x = a;
    const ResultatRecherche *y = b;

    if (x->cle != y->cle) {
        return x->cle < y->cle ? -1 : 1;
    }
    return (x->id > y->id) - (x->id < y->id);
}

static int comparer_decroissant(const void *a, const void *b)
{
    const ResultatRecherche *x = a;
    const ResultatRecherche *y = b;

    if (x->cle != y->cle) {
        return x->cle > y->cle ? -1 : 1;
    }
    return (x->id > y->id) - (x->id < y->id);   /* égalités : identifiant croissant */
}

void analyses_trier(Tableau *resultats, CleTri cle, bool croissant)
{
    size_t i;

    for (i = 0; i < resultats->taille; i++) {
        ResultatRecherche *x = ELEMENT(resultats, ResultatRecherche, i);
        switch (cle) {
        case TRI_DATE:
            x->cle = x->jours;
            break;
        case TRI_MONTANT:
            x->cle = x->total_ct;
            break;
        default:
            x->cle = x->attribuable_ct;
            break;
        }
    }
    if (resultats->taille > 1) {
        qsort(resultats->donnees, resultats->taille, sizeof(ResultatRecherche),
              croissant ? comparer_croissant : comparer_decroissant);
    }
}

static int comparer_categories_trouvees(const void *a, const void *b)
{
    const CategorieTrouvee *x = a;
    const CategorieTrouvee *y = b;
    int c = texte_comparer_sans_casse(x->chemin, y->chemin);

    return c != 0 ? c : (x->id > y->id) - (x->id < y->id);
}

bool analyses_categories_par_nom(const Donnees *d, TypeCategorie type, const char *motif,
                                 Tableau *resultats)
{
    size_t i;

    tableau_vider(resultats);
    for (i = 0; i < d->categories.taille; i++) {
        const Categorie *c = ELEMENT(&d->categories, Categorie, i);
        CategorieTrouvee t;

        if (c->type != type || !regles_categorie_est_feuille(d, c->id)) {
            continue;
        }
        t.id = c->id;
        regles_chemin_categorie(d, c->id, t.chemin, sizeof t.chemin);
        if (motif[0] != '\0' && !texte_contient_sans_casse(t.chemin, motif)) {
            continue;
        }
        if (motif[0] != '\0' && texte_egal_sans_casse(t.chemin, motif)) {
            /* Chemin complet exact : unique (B2), il l'emporte sur les correspondances partielles. */
            tableau_vider(resultats);
            return tableau_ajouter(resultats, &t);
        }
        if (!tableau_ajouter(resultats, &t)) {
            return false;
        }
    }
    if (resultats->taille > 1) {
        qsort(resultats->donnees, resultats->taille, sizeof(CategorieTrouvee),
              comparer_categories_trouvees);
    }
    return true;
}

bool analyses_dernieres_operations(const Donnees *d, size_t max, Tableau *resultats, size_t *total)
{
    FiltreOperations f;

    filtre_init(&f);
    if (!analyses_rechercher(d, &f, resultats)) {
        return false;
    }
    *total = resultats->taille;
    analyses_trier(resultats, TRI_DATE, false);
    if (resultats->taille > max) {
        resultats->taille = max;    /* seul l'affichage est limité */
    }
    return true;
}

bool analyses_statistiques(const Donnees *d, Date debut, Date fin, Id id_categorie,
                           StatistiquesDepenses *s)
{
    long long *attrib = NULL;
    bool debordement = false;
    size_t i;

    memset(s, 0, sizeof *s);
    if (id_categorie != ID_AUCUN) {
        attrib = attribuables(d, id_categorie, &debordement);
        if (attrib == NULL || debordement) {
            free(attrib);
            return false;
        }
    }
    for (i = 0; i < d->operations.taille; i++) {
        const Operation *o = ELEMENT(&d->operations, Operation, i);
        long long montant;

        if (o->nature != NATURE_DEPENSE || o->statut != STATUT_CONFIRMEE
            || !date_valide(o->date_realisation)
            || date_comparer(o->date_realisation, debut) < 0
            || date_comparer(o->date_realisation, fin) > 0) {
            continue;
        }
        montant = attrib != NULL ? attrib[i] : o->montant_total_ct;
        if (attrib != NULL && montant <= 0) {
            continue;
        }
        if (s->nombre == 0 || montant < s->minimum_ct) {
            s->minimum_ct = montant;
        }
        if (s->nombre == 0 || montant > s->maximum_ct) {
            s->maximum_ct = montant;
        }
        if (!montant_additionner(s->total_ct, montant, &s->total_ct)) {
            free(attrib);
            return false;
        }
        s->nombre++;
    }
    free(attrib);
    if (s->nombre > 0) {
        /* Moyenne arrondie au plus proche, demi au-dessus : (2 * total + n) / (2 * n). */
        long long n = (long long)s->nombre;
        if (s->total_ct > (LLONG_MAX - n) / 2) {
            return false;
        }
        s->moyenne_ct = (2 * s->total_ct + n) / (2 * n);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Doublons potentiels (I6)                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    Id categorie;
    long long montant_ct;
    Id vehicule;
} Triplet;

typedef struct {
    Id id;
    Nature nature;
    long jours;
    long long total_ct;
    Id source;
    Id destination;
    Id tiers;
    char libelle[LIBELLE_MAX + 1];     /* libellé normalisé */
    size_t nb;
    Triplet *triplets;                 /* triés : multiensemble indépendant de l'ordre */
} Signature;

static int comparer_triplets(const void *a, const void *b)
{
    const Triplet *x = a;
    const Triplet *y = b;

    if (x->categorie != y->categorie) {
        return x->categorie < y->categorie ? -1 : 1;
    }
    if (x->montant_ct != y->montant_ct) {
        return x->montant_ct < y->montant_ct ? -1 : 1;
    }
    return (x->vehicule > y->vehicule) - (x->vehicule < y->vehicule);
}

static int comparer_sans_id(const Signature *x, const Signature *y)
{
    size_t i;
    int c;

    if (x->nature != y->nature) {
        return x->nature < y->nature ? -1 : 1;
    }
    if (x->jours != y->jours) {
        return x->jours < y->jours ? -1 : 1;
    }
    if (x->total_ct != y->total_ct) {
        return x->total_ct < y->total_ct ? -1 : 1;
    }
    if (x->source != y->source) {
        return x->source < y->source ? -1 : 1;
    }
    if (x->destination != y->destination) {
        return x->destination < y->destination ? -1 : 1;
    }
    if (x->tiers != y->tiers) {
        return x->tiers < y->tiers ? -1 : 1;
    }
    c = strcmp(x->libelle, y->libelle);
    if (c != 0) {
        return c < 0 ? -1 : 1;
    }
    if (x->nb != y->nb) {
        return x->nb < y->nb ? -1 : 1;
    }
    for (i = 0; i < x->nb; i++) {
        c = comparer_triplets(&x->triplets[i], &y->triplets[i]);
        if (c != 0) {
            return c;
        }
    }
    return 0;
}

static int comparer_signatures(const void *a, const void *b)
{
    const Signature *x = a;
    const Signature *y = b;
    int c = comparer_sans_id(x, y);

    return c != 0 ? c : (x->id > y->id) - (x->id < y->id);
}

bool analyses_doublons(const Donnees *d, Tableau *groupes, Tableau *membres)
{
    const size_t n_ops = d->operations.taille;
    Signature *sigs = malloc((n_ops > 0 ? n_ops : 1) * sizeof *sigs);
    long *pos = malloc((n_ops > 0 ? n_ops : 1) * sizeof *pos);
    Triplet *pool = malloc((d->lignes_operations.taille > 0 ? d->lignes_operations.taille : 1)
                           * sizeof *pool);
    size_t *remplis = calloc(n_ops > 0 ? n_ops : 1, sizeof *remplis);
    Index ix_op;
    bool index_ok = index_construire(&d->operations, &ix_op);
    size_t ns = 0;
    size_t i;
    bool ok = false;

    tableau_vider(groupes);
    tableau_vider(membres);
    if (sigs == NULL || pos == NULL || pool == NULL || remplis == NULL || !index_ok) {
        goto fin;
    }
    for (i = 0; i < n_ops; i++) {
        pos[i] = -1;
    }
    for (i = 0; i < n_ops; i++) {
        const Operation *o = ELEMENT(&d->operations, Operation, i);
        Signature *s;

        if (o->statut != STATUT_CONFIRMEE) {
            continue;
        }
        s = &sigs[ns];
        s->id = o->id;
        s->nature = o->nature;
        s->jours = date_valide(o->date_realisation) ? date_vers_jours(o->date_realisation) : 0;
        s->total_ct = o->montant_total_ct;
        s->source = o->id_compte_source;
        s->destination = o->id_compte_destination;
        s->tiers = o->id_tiers;
        texte_normaliser(o->libelle, s->libelle, sizeof s->libelle);
        s->nb = 0;
        s->triplets = NULL;
        pos[i] = (long)ns;
        ns++;
    }
    /* Répartition des lignes dans un réservoir unique (aucune réallocation ensuite). */
    for (i = 0; i < d->lignes_operations.taille; i++) {
        long io = index_indice(&ix_op, ELEMENT(&d->lignes_operations, LigneOperation, i)->id_operation);
        if (io >= 0 && pos[io] >= 0) {
            sigs[pos[io]].nb++;
        }
    }
    {
        size_t decalage = 0;
        for (i = 0; i < ns; i++) {
            sigs[i].triplets = pool + decalage;
            decalage += sigs[i].nb;
        }
    }
    for (i = 0; i < d->lignes_operations.taille; i++) {
        const LigneOperation *l = ELEMENT(&d->lignes_operations, LigneOperation, i);
        long io = index_indice(&ix_op, l->id_operation);

        if (io >= 0 && pos[io] >= 0) {
            Signature *s = &sigs[pos[io]];
            Triplet *t = &s->triplets[remplis[pos[io]]++];
            t->categorie = l->id_categorie;
            t->montant_ct = l->montant_ct;
            t->vehicule = l->id_vehicule;
        }
    }
    for (i = 0; i < ns; i++) {
        if (sigs[i].nb > 1) {
            qsort(sigs[i].triplets, sigs[i].nb, sizeof(Triplet), comparer_triplets);
        }
    }
    if (ns > 1) {
        qsort(sigs, ns, sizeof *sigs, comparer_signatures);
    }
    ok = true;
    for (i = 0; i < ns && ok;) {
        size_t j = i + 1;

        while (j < ns && comparer_sans_id(&sigs[i], &sigs[j]) == 0) {
            j++;
        }
        if (j - i >= 2) {
            GroupeDoublons g;
            size_t k;

            g.debut = membres->taille;
            g.nombre = j - i;
            for (k = i; k < j && ok; k++) {
                ok = tableau_ajouter(membres, &sigs[k].id);
            }
            if (ok) {
                ok = tableau_ajouter(groupes, &g);
            }
        }
        i = j;
    }
    if (!ok) {
        tableau_vider(groupes);
        tableau_vider(membres);
    }
fin:
    if (index_ok) {
        index_liberer(&ix_op);
    }
    free(sigs);
    free(pos);
    free(pool);
    free(remplis);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Totaux par catégorie, budgets                                       */
/* ------------------------------------------------------------------ */

bool analyses_totaux_categories(const Donnees *d, Date debut, Date fin, Nature nature,
                                long long *totaux, long long *total_global)
{
    Index ix_op;
    Index ix_cat;
    size_t i;
    bool ok = true;

    *total_global = 0;
    for (i = 0; i < d->categories.taille; i++) {
        totaux[i] = 0;
    }
    if (!index_construire(&d->operations, &ix_op)) {
        return false;
    }
    if (!index_construire(&d->categories, &ix_cat)) {
        index_liberer(&ix_op);
        return false;
    }
    for (i = 0; i < d->operations.taille && ok; i++) {
        const Operation *o = ELEMENT(&d->operations, Operation, i);

        if (o->nature == nature && o->statut == STATUT_CONFIRMEE && date_valide(o->date_realisation)
            && date_comparer(o->date_realisation, debut) >= 0
            && date_comparer(o->date_realisation, fin) <= 0) {
            ok = montant_additionner(*total_global, o->montant_total_ct, total_global);
        }
    }
    for (i = 0; i < d->lignes_operations.taille && ok; i++) {
        const LigneOperation *l = ELEMENT(&d->lignes_operations, LigneOperation, i);
        long io = index_indice(&ix_op, l->id_operation);
        long ic;
        size_t pas = 0;
        const Operation *o;

        if (io < 0) {
            continue;
        }
        o = ELEMENT(&d->operations, Operation, (size_t)io);
        if (o->nature != nature || o->statut != STATUT_CONFIRMEE || !date_valide(o->date_realisation)
            || date_comparer(o->date_realisation, debut) < 0
            || date_comparer(o->date_realisation, fin) > 0) {
            continue;
        }
        /* La ligne remonte de sa catégorie à la racine : une seule fois par ancêtre. */
        ic = index_indice(&ix_cat, l->id_categorie);
        while (ic >= 0 && pas <= d->categories.taille && ok) {
            const Categorie *c = ELEMENT(&d->categories, Categorie, (size_t)ic);
            ok = montant_additionner(totaux[ic], l->montant_ct, &totaux[ic]);
            ic = (c->id_parent == ID_AUCUN) ? -1 : index_indice(&ix_cat, c->id_parent);
            pas++;
        }
    }
    index_liberer(&ix_op);
    index_liberer(&ix_cat);
    return ok;
}

SituationBudget analyses_situation_budget(const Donnees *d, int annee, int mois, Id id_categorie,
                                          long long depense_ct)
{
    SituationBudget s;
    size_t i;

    memset(&s, 0, sizeof s);
    s.depense_ct = depense_ct;
    for (i = 0; i < d->budgets.taille; i++) {
        const BudgetMensuel *b = ELEMENT(&d->budgets, BudgetMensuel, i);
        if (b->annee == annee && b->mois == mois && b->id_categorie == id_categorie) {
            s.defini = true;
            s.plafond_ct = b->plafond_ct;
            s.reste_ct = depense_ct <= b->plafond_ct ? b->plafond_ct - depense_ct : 0;
            s.depassement_ct = depense_ct > b->plafond_ct ? depense_ct - b->plafond_ct : 0;
            break;
        }
    }
    return s;
}

/* ------------------------------------------------------------------ */
/* Dépenses du véhicule                                                */
/* ------------------------------------------------------------------ */

static int comparer_lignes_vehicule(const void *a, const void *b)
{
    const LigneVehicule *x = a;
    const LigneVehicule *y = b;

    if (x->jours != y->jours) {
        return x->jours < y->jours ? -1 : 1;
    }
    if (x->id_operation != y->id_operation) {
        return x->id_operation < y->id_operation ? -1 : 1;
    }
    return (x->id_categorie > y->id_categorie) - (x->id_categorie < y->id_categorie);
}

static int comparer_totaux_categories(const void *a, const void *b)
{
    const TotalCategorie *x = a;
    const TotalCategorie *y = b;
    int c = texte_comparer_sans_casse(x->chemin, y->chemin);

    return c != 0 ? c : (x->id_categorie > y->id_categorie) - (x->id_categorie < y->id_categorie);
}

bool analyses_vehicule(const Donnees *d, Id id_vehicule, Date debut, Date fin, Tableau *lignes,
                       Tableau *par_categorie, long long *total_ct)
{
    Index ix_op;
    size_t i;
    bool ok = true;

    tableau_vider(lignes);
    tableau_vider(par_categorie);
    *total_ct = 0;
    if (!index_construire(&d->operations, &ix_op)) {
        return false;
    }
    for (i = 0; i < d->lignes_operations.taille && ok; i++) {
        const LigneOperation *l = ELEMENT(&d->lignes_operations, LigneOperation, i);
        long io;
        const Operation *o;
        LigneVehicule lv;
        size_t k;
        bool trouve = false;

        if (l->id_vehicule != id_vehicule) {
            continue;
        }
        io = index_indice(&ix_op, l->id_operation);
        if (io < 0) {
            continue;
        }
        o = ELEMENT(&d->operations, Operation, (size_t)io);
        if (o->nature != NATURE_DEPENSE || o->statut != STATUT_CONFIRMEE
            || !date_valide(o->date_realisation) || date_comparer(o->date_realisation, debut) < 0
            || date_comparer(o->date_realisation, fin) > 0) {
            continue;
        }
        lv.id_operation = o->id;
        lv.jours = date_vers_jours(o->date_realisation);
        lv.id_categorie = l->id_categorie;
        lv.montant_ct = l->montant_ct;
        ok = tableau_ajouter(lignes, &lv) && montant_additionner(*total_ct, l->montant_ct, total_ct);
        for (k = 0; ok && k < par_categorie->taille; k++) {
            TotalCategorie *t = ELEMENT(par_categorie, TotalCategorie, k);
            if (t->id_categorie == l->id_categorie) {
                ok = montant_additionner(t->total_ct, l->montant_ct, &t->total_ct);
                trouve = true;
                break;
            }
        }
        if (ok && !trouve) {
            TotalCategorie t;
            t.id_categorie = l->id_categorie;
            t.total_ct = l->montant_ct;
            regles_chemin_categorie(d, l->id_categorie, t.chemin, sizeof t.chemin);
            ok = tableau_ajouter(par_categorie, &t);
        }
    }
    index_liberer(&ix_op);
    if (!ok) {
        tableau_vider(lignes);
        tableau_vider(par_categorie);
        *total_ct = 0;
        return false;
    }
    if (lignes->taille > 1) {
        qsort(lignes->donnees, lignes->taille, sizeof(LigneVehicule), comparer_lignes_vehicule);
    }
    if (par_categorie->taille > 1) {
        qsort(par_categorie->donnees, par_categorie->taille, sizeof(TotalCategorie),
              comparer_totaux_categories);
    }
    return true;
}
