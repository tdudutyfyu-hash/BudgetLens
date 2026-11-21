/*
 * BudgetLens — saisie.h
 * Saisie console contrôlée : lecture par fgets, conversion vérifiée,
 * nouvelle demande tant que la valeur est invalide.
 *
 * Conventions : une saisie vide renvoie SAISIE_VIDE (abandon, valeur absente
 * ou valeur conservée selon le contexte) ; « - » renvoie SAISIE_EFFACER si
 * l'effacement est permis ; la fin de l'entrée standard renvoie SAISIE_FIN.
 */
#ifndef BUDGETLENS_SAISIE_H
#define BUDGETLENS_SAISIE_H

#include <stddef.h>

#include "types.h"

typedef enum { SAISIE_OK, SAISIE_VIDE, SAISIE_EFFACER, SAISIE_FIN } EtatSaisie;

/*
 * État de l'entrée standard (H10), lu sur le flux lui-même (feof / ferror) :
 * aucune variable globale. Une fois l'entrée FIN ou ERREUR, elle est
 * définitivement indisponible : aucune nouvelle lecture n'est tentée.
 */
typedef enum { ENTREE_DISPONIBLE, ENTREE_FIN, ENTREE_ERREUR } EtatEntree;

EtatEntree saisie_etat_entree(void);

#define SAISIE_LIGNE_MAX 512

EtatSaisie saisie_ligne(const char *invite, char *tampon, size_t taille);
EtatSaisie saisie_texte(const char *invite, char *dest, size_t max_octets, bool effacement_permis);
EtatSaisie saisie_entier(const char *invite, long min, long max, long *valeur);
EtatSaisie saisie_id(const char *invite, Id *id);
EtatSaisie saisie_montant(const char *invite, bool zero_permis, bool effacement_permis,
                          long long *ct);
EtatSaisie saisie_date(const char *invite, bool effacement_permis, Date *d);
/* Réponse o/n obligatoire ; renvoie SAISIE_FIN en fin d'entrée. */
EtatSaisie saisie_oui_non(const char *invite, bool *oui);
/* Choix de menu entre 0 et max ; -1 en fin d'entrée. */
int saisie_menu(int max);

#endif
