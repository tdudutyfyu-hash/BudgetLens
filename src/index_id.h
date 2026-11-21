/*
 * BudgetLens — index_id.h
 * Index trié identifiant -> position dans un tableau d'entités (qsort +
 * bsearch). Évite les recherches linéaires répétées du validateur et des
 * analyses. L'index n'est valable que tant que le tableau n'est pas modifié.
 */
#ifndef BUDGETLENS_INDEX_ID_H
#define BUDGETLENS_INDEX_ID_H

#include <stdbool.h>
#include <stddef.h>

#include "tableau.h"
#include "types.h"

typedef struct {
    Id id;
    size_t indice;
} EntreeIndex;

typedef struct {
    EntreeIndex *entrees;   /* triées par identifiant croissant */
    size_t n;
} Index;

/* Faux si la mémoire manque (l'index reste vide et libérable). */
bool index_construire(const Tableau *t, Index *ix);
void index_liberer(Index *ix);
/* Position de l'identifiant dans le tableau, ou -1 s'il est absent. */
long index_indice(const Index *ix, Id id);
bool index_contient(const Index *ix, Id id);

#endif
