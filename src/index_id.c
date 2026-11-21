/*
 * BudgetLens — index_id.c
 */
#include "index_id.h"

#include <stdlib.h>

static int comparer_entrees(const void *a, const void *b)
{
    Id x = ((const EntreeIndex *)a)->id;
    Id y = ((const EntreeIndex *)b)->id;
    return (x > y) - (x < y);
}

bool index_construire(const Tableau *t, Index *ix)
{
    size_t i;

    ix->entrees = NULL;
    ix->n = 0;
    if (t->taille == 0) {
        return true;
    }
    if (t->taille > (size_t)-1 / sizeof(EntreeIndex)) {
        return false;
    }
    ix->entrees = malloc(t->taille * sizeof(EntreeIndex));
    if (ix->entrees == NULL) {
        return false;
    }
    ix->n = t->taille;
    for (i = 0; i < t->taille; i++) {
        ix->entrees[i].id = tableau_id(t, i);
        ix->entrees[i].indice = i;
    }
    qsort(ix->entrees, ix->n, sizeof(EntreeIndex), comparer_entrees);
    return true;
}

void index_liberer(Index *ix)
{
    free(ix->entrees);
    ix->entrees = NULL;
    ix->n = 0;
}

long index_indice(const Index *ix, Id id)
{
    EntreeIndex cle;
    const EntreeIndex *trouve;

    if (ix->n == 0) {
        return -1;
    }
    cle.id = id;
    cle.indice = 0;
    trouve = bsearch(&cle, ix->entrees, ix->n, sizeof(EntreeIndex), comparer_entrees);
    return trouve == NULL ? -1 : (long)trouve->indice;
}

bool index_contient(const Index *ix, Id id)
{
    return index_indice(ix, id) >= 0;
}
