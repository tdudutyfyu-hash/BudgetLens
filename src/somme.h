/*
 * BudgetLens — somme.h
 * Somme de contrôle FNV-1a 64 bits (détection de corruption accidentelle).
 * FNV-1a n'est PAS cryptographique : elle ne protège ni contre une modification
 * volontaire ni, de façon garantie, contre les collisions.
 */
#ifndef SOMME_H
#define SOMME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FNV1A64_BASE UINT64_C(0xcbf29ce484222325)
#define FNV1A64_PREMIER UINT64_C(0x00000100000001b3)

/* Poursuit le calcul : h vaut FNV1A64_BASE au départ. */
uint64_t fnv1a64_ajouter(uint64_t h, const void *octets, size_t taille);
uint64_t fnv1a64(const void *octets, size_t taille);
/* Somme des octets réels du fichier (lecture binaire). Faux si illisible. */
bool fichier_fnv1a64(const char *chemin, uint64_t *somme);

#endif
