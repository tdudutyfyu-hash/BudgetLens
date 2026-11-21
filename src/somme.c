/*
 * BudgetLens — somme.c
 * FNV-1a 64 bits : h = base ; pour chaque octet, h ^= octet puis h *= premier (mod 2^64).
 */
#include "somme.h"

#include <stdio.h>

uint64_t fnv1a64_ajouter(uint64_t h, const void *octets, size_t taille)
{
    const unsigned char *p = octets;
    size_t i;

    for (i = 0; i < taille; i++) {
        h ^= (uint64_t)p[i];
        h *= FNV1A64_PREMIER;
    }
    return h;
}

uint64_t fnv1a64(const void *octets, size_t taille)
{
    return fnv1a64_ajouter(FNV1A64_BASE, octets, taille);
}

bool fichier_fnv1a64(const char *chemin, uint64_t *somme)
{
    unsigned char tampon[4096];
    uint64_t h = FNV1A64_BASE;
    size_t n;
    bool ok;
    FILE *f = fopen(chemin, "rb");

    if (f == NULL) {
        return false;
    }
    while ((n = fread(tampon, 1, sizeof tampon, f)) > 0) {
        h = fnv1a64_ajouter(h, tampon, n);
    }
    ok = !ferror(f);
    if (fclose(f) != 0) {
        ok = false;
    }
    if (ok) {
        *somme = h;
    }
    return ok;
}
