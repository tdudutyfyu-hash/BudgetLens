/*
 * BudgetLens — soldes.c
 */
#include "soldes.h"

#include <stdlib.h>

#include "valeurs.h"

typedef struct {
    long jour;
    long long delta_ct;
} Mouvement;

static int comparer_mouvements(const void *a, const void *b)
{
    long x = ((const Mouvement *)a)->jour;
    long y = ((const Mouvement *)b)->jour;
    return (x > y) - (x < y);
}

bool serie_construire(const Donnees *d, const Compte *c, SerieSolde *s)
{
    Mouvement *m;
    size_t nm = 0;
    size_t i;
    long long courant;
    size_t np = 0;

    s->points = NULL;
    s->n = 0;
    s->initial_ct = c->solde_initial_ct;
    s->jour_initial = date_vers_jours(c->date_solde_initial);

    /* Au plus un mouvement par opération (source et destination distinctes) + le jour initial. */
    m = malloc((d->operations.taille * 2 + 1) * sizeof *m);
    if (m == NULL) {
        return false;
    }
    m[nm].jour = s->jour_initial;
    m[nm].delta_ct = 0;
    nm++;
    for (i = 0; i < d->operations.taille; i++) {
        const Operation *o = ELEMENT(&d->operations, Operation, i);

        if (o->statut != STATUT_CONFIRMEE || !date_valide(o->date_realisation)) {
            continue;
        }
        if (o->id_compte_source == c->id) {
            m[nm].jour = date_vers_jours(o->date_realisation);
            m[nm].delta_ct = -o->montant_total_ct;
            nm++;
        }
        if (o->id_compte_destination == c->id) {
            m[nm].jour = date_vers_jours(o->date_realisation);
            m[nm].delta_ct = o->montant_total_ct;
            nm++;
        }
    }
    qsort(m, nm, sizeof *m, comparer_mouvements);
    s->points = malloc(nm * sizeof *s->points);
    if (s->points == NULL) {
        free(m);
        return false;
    }
    courant = c->solde_initial_ct;
    for (i = 0; i < nm; i++) {
        if (!montant_additionner(courant, m[i].delta_ct, &courant)) {
            free(m);
            serie_liberer(s);
            return false;
        }
        /* Dernier mouvement du jour : on enregistre le solde de fin de journée. */
        if (i + 1 == nm || m[i + 1].jour != m[i].jour) {
            s->points[np].jour = m[i].jour;
            s->points[np].solde_ct = courant;
            np++;
        }
    }
    s->n = np;
    free(m);
    return true;
}

void serie_liberer(SerieSolde *s)
{
    free(s->points);
    s->points = NULL;
    s->n = 0;
}

long long serie_solde_au(const SerieSolde *s, long jour)
{
    long long solde = s->initial_ct;
    size_t i;

    for (i = 0; i < s->n && s->points[i].jour <= jour; i++) {
        solde = s->points[i].solde_ct;
    }
    return solde;
}

bool soldes_verifier(const Donnees *d, RapportErreurs *r)
{
    bool ok = true;
    size_t k;

    for (k = 0; k < d->comptes.taille; k++) {
        const Compte *c = ELEMENT(&d->comptes, Compte, k);
        SerieSolde s;
        size_t i;

        if (!date_valide(c->date_solde_initial)) {
            continue;   /* déjà signalée par V04 */
        }
        if (!serie_construire(d, c, &s)) {
            rapport_ajouter(r, "Compte « %s » : calcul des soldes impossible (mémoire insuffisante "
                               "ou débordement de somme)", c->nom);
            ok = false;
            continue;
        }
        for (i = 0; i < s.n; i++) {
            if (s.points[i].solde_ct < 0) {
                char date[DATE_TEXTE];
                char solde[MONTANT_TEXTE];
                char manque[MONTANT_TEXTE];

                date_formater(jours_vers_date(s.points[i].jour), date);
                montant_formater(s.points[i].solde_ct, solde, sizeof solde);
                montant_formater(-s.points[i].solde_ct, manque, sizeof manque);
                rapport_ajouter(r, "Compte « %s » : le solde de fin de journée serait négatif le %s "
                                   "(%s) ; il manque %s",
                                c->nom, date, solde, manque);
                ok = false;
                break;
            }
        }
        serie_liberer(&s);
    }
    return ok;
}

bool solde_compte_a_date(const Donnees *d, Id id_compte, Date date, long long *solde_ct)
{
    const Compte *c = NULL;
    SerieSolde s;
    size_t k;
    long jour;
    bool ok;

    for (k = 0; k < d->comptes.taille; k++) {
        if (ELEMENT(&d->comptes, Compte, k)->id == id_compte) {
            c = ELEMENT(&d->comptes, Compte, k);
            break;
        }
    }
    if (c == NULL || !date_valide(date) || !date_valide(c->date_solde_initial)) {
        return false;
    }
    jour = date_vers_jours(date);
    if (jour < date_vers_jours(c->date_solde_initial)) {
        return false;
    }
    ok = serie_construire(d, c, &s);
    if (ok) {
        *solde_ct = serie_solde_au(&s, jour);
        serie_liberer(&s);
    }
    return ok;
}

bool solde_compte_final(const Donnees *d, Id id_compte, long long *solde_ct)
{
    const Compte *c = NULL;
    SerieSolde s;
    size_t k;
    bool ok;

    for (k = 0; k < d->comptes.taille; k++) {
        if (ELEMENT(&d->comptes, Compte, k)->id == id_compte) {
            c = ELEMENT(&d->comptes, Compte, k);
            break;
        }
    }
    if (c == NULL || !date_valide(c->date_solde_initial)) {
        return false;
    }
    ok = serie_construire(d, c, &s);
    if (ok) {
        *solde_ct = s.n > 0 ? s.points[s.n - 1].solde_ct : s.initial_ct;
        serie_liberer(&s);
    }
    return ok;
}

static int comparer_jours(const void *a, const void *b)
{
    long x = *(const long *)a;
    long y = *(const long *)b;
    return (x > y) - (x < y);
}

size_t soldes_avertissements_seuil(const Donnees *avant, const Donnees *apres, RapportErreurs *w)
{
    size_t nb = 0;
    size_t k;

    for (k = 0; k < apres->comptes.taille; k++) {
        const Compte *c = ELEMENT(&apres->comptes, Compte, k);
        const Compte *ancien = NULL;
        SerieSolde sa;
        SerieSolde sb;
        long *jours;
        size_t nj = 0;
        size_t i;
        size_t j;

        if (!c->a_seuil || !date_valide(c->date_solde_initial)) {
            continue;
        }
        for (j = 0; j < avant->comptes.taille; j++) {
            if (ELEMENT(&avant->comptes, Compte, j)->id == c->id) {
                ancien = ELEMENT(&avant->comptes, Compte, j);
                break;
            }
        }
        if (ancien == NULL || !date_valide(ancien->date_solde_initial)) {
            continue;   /* compte nouveau : aucune action n'a abaissé son solde */
        }
        if (!serie_construire(apres, c, &sa)) {
            continue;
        }
        if (!serie_construire(avant, ancien, &sb)) {
            serie_liberer(&sa);
            continue;
        }
        jours = malloc((sa.n + sb.n + 1) * sizeof *jours);
        if (jours != NULL) {
            for (i = 0; i < sa.n; i++) {
                jours[nj++] = sa.points[i].jour;
            }
            for (i = 0; i < sb.n; i++) {
                jours[nj++] = sb.points[i].jour;
            }
            qsort(jours, nj, sizeof *jours, comparer_jours);
            for (i = 0; i < nj; i++) {
                long long apres_ct;
                long long avant_ct;

                if (i > 0 && jours[i] == jours[i - 1]) {
                    continue;
                }
                if (jours[i] < sa.jour_initial) {
                    continue;
                }
                apres_ct = serie_solde_au(&sa, jours[i]);
                avant_ct = serie_solde_au(&sb, jours[i]);
                if (apres_ct < c->seuil_minimal_ct && apres_ct < avant_ct) {
                    char date[DATE_TEXTE];
                    char solde[MONTANT_TEXTE];
                    char seuil[MONTANT_TEXTE];

                    date_formater(jours_vers_date(jours[i]), date);
                    montant_formater(apres_ct, solde, sizeof solde);
                    montant_formater(c->seuil_minimal_ct, seuil, sizeof seuil);
                    rapport_ajouter(w, "Compte « %s » : le solde du %s tomberait à %s, sous le seuil "
                                       "minimal de %s",
                                    c->nom, date, solde, seuil);
                    nb++;
                    break;
                }
            }
            free(jours);
        }
        serie_liberer(&sa);
        serie_liberer(&sb);
    }
    return nb;
}
