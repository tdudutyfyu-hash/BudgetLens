/*
 * BudgetLens — modeles.h
 * Modèles d'opération (valeurs réutilisables) et leurs lignes. Chaque fonction
 * est une transaction : rien n'est modifié si une règle est violée (V08).
 */
#ifndef BUDGETLENS_MODELES_H
#define BUDGETLENS_MODELES_H

#include "app.h"
#include "operations.h"

typedef struct {
    Nature nature;
    char nom[NOM_MAX + 1];
    char libelle[LIBELLE_MAX + 1];
    long long montant_total_ct;
    Id id_compte_source;
    Id id_compte_destination;
    Id id_tiers;
} ModeleSaisie;

ModeleOperation *regles_modele(const Donnees *d, Id id);
size_t mod_nombre_lignes(const Donnees *d, Id id_modele);
/* Copie au plus max lignes (ordre d'enregistrement) ; renvoie le nombre copié. */
size_t mod_lignes_copier(const Donnees *d, Id id_modele, LigneSaisie *sortie, size_t max);
/* Nombre de récurrences qui utilisent le modèle. */
size_t mod_nombre_recurrences(const Donnees *d, Id id_modele);

Resultat mod_creer(Application *app, const ModeleSaisie *s, const LigneSaisie *lignes, size_t nb,
                   Id *id_cree, RapportErreurs *r);
/* La nature ne change pas tant qu'une récurrence utilise le modèle. */
Resultat mod_modifier(Application *app, Id id, const ModeleSaisie *s, const LigneSaisie *lignes,
                      size_t nb, RapportErreurs *r);
/* Refusé tant qu'une récurrence utilise le modèle. */
Resultat mod_supprimer(Application *app, Id id, RapportErreurs *r);

/*
 * Crée une opération manuelle confirmée à la date donnée, en copiant les
 * valeurs du modèle (comme op_creer : B3, Q3, G10, seuil).
 */
Resultat op_creer_depuis_modele(Application *app, Id id_modele, Date date, bool accord_seuil,
                                Id *id_cree, RapportErreurs *avert, RapportErreurs *r);

#endif
