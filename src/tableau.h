/*
 * BudgetLens — tableau.h
 * Tableau dynamique générique (éléments de taille fixe, contigus).
 */
#ifndef BUDGETLENS_TABLEAU_H
#define BUDGETLENS_TABLEAU_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void *donnees;
    size_t taille;           /* nombre d'éléments utilisés */
    size_t capacite;         /* nombre d'éléments alloués */
    size_t taille_element;   /* sizeof d'un élément */
} Tableau;

void tableau_init(Tableau *t, size_t taille_element);
bool tableau_reserver(Tableau *t, size_t capacite);
bool tableau_ajouter(Tableau *t, const void *element);
void *tableau_element(const Tableau *t, size_t indice);
void tableau_retirer(Tableau *t, size_t indice);
void tableau_vider(Tableau *t);
void tableau_liberer(Tableau *t);

/* dest est (ré)initialisé ; renvoie false si la mémoire manque (dest vide). */
bool tableau_copier(Tableau *dest, const Tableau *src);

/*
 * Fonctions réservées aux tableaux dont les éléments commencent par un
 * identifiant de type int (convention des entités de types.h).
 */
int tableau_id(const Tableau *t, size_t indice);
void *tableau_trouver_id(const Tableau *t, int id, size_t *indice);

/* Accès typé : ELEMENT(&d->comptes, Compte, i) */
#define ELEMENT(tab, type, i) ((type *)tableau_element((tab), (i)))

#endif
