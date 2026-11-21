/*
 * BudgetLens — tableau.c
 * Tableau dynamique générique : la capacité double à chaque extension,
 * les débordements de taille sont contrôlés et realloc passe par un
 * pointeur temporaire pour ne jamais perdre le bloc d'origine.
 */
#include "tableau.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void tableau_init(Tableau *t, size_t taille_element)
{
    t->donnees = NULL;
    t->taille = 0;
    t->capacite = 0;
    t->taille_element = taille_element;
}

bool tableau_reserver(Tableau *t, size_t capacite)
{
    void *nouveau;

    if (capacite <= t->capacite) {
        return true;
    }
    if (t->taille_element == 0 || capacite > SIZE_MAX / t->taille_element) {
        return false;
    }
    nouveau = realloc(t->donnees, capacite * t->taille_element);
    if (nouveau == NULL) {
        return false;
    }
    t->donnees = nouveau;
    t->capacite = capacite;
    return true;
}

bool tableau_ajouter(Tableau *t, const void *element)
{
    if (t->taille == t->capacite) {
        size_t nouvelle = (t->capacite == 0) ? 8 : t->capacite * 2;
        if (nouvelle < t->capacite || !tableau_reserver(t, nouvelle)) {
            return false;
        }
    }
    memcpy((char *)t->donnees + t->taille * t->taille_element, element,
           t->taille_element);
    t->taille++;
    return true;
}

void *tableau_element(const Tableau *t, size_t indice)
{
    return (char *)t->donnees + indice * t->taille_element;
}

void tableau_retirer(Tableau *t, size_t indice)
{
    char *base = (char *)t->donnees;
    size_t s = t->taille_element;

    if (indice >= t->taille) {
        return;
    }
    memmove(base + indice * s, base + (indice + 1) * s,
            (t->taille - indice - 1) * s);
    t->taille--;
}

void tableau_vider(Tableau *t)
{
    t->taille = 0;
}

void tableau_liberer(Tableau *t)
{
    free(t->donnees);
    t->donnees = NULL;
    t->taille = 0;
    t->capacite = 0;
}

bool tableau_copier(Tableau *dest, const Tableau *src)
{
    tableau_init(dest, src->taille_element);
    if (src->taille == 0) {
        return true;
    }
    if (!tableau_reserver(dest, src->taille)) {
        return false;
    }
    memcpy(dest->donnees, src->donnees, src->taille * src->taille_element);
    dest->taille = src->taille;
    return true;
}

int tableau_id(const Tableau *t, size_t indice)
{
    int id;
    memcpy(&id, tableau_element(t, indice), sizeof id);
    return id;
}

void *tableau_trouver_id(const Tableau *t, int id, size_t *indice)
{
    size_t i;

    for (i = 0; i < t->taille; i++) {
        if (tableau_id(t, i) == id) {
            if (indice != NULL) {
                *indice = i;
            }
            return tableau_element(t, i);
        }
    }
    return NULL;
}
