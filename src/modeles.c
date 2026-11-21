/*
 * BudgetLens — modeles.c
 */
#include "modeles.h"

#include <stdlib.h>
#include <string.h>

#include "regles.h"
#include "texte.h"
#include "valeurs.h"

ModeleOperation *regles_modele(const Donnees *d, Id id)
{
    return (ModeleOperation *)tableau_trouver_id(&d->modeles, id, NULL);
}

size_t mod_nombre_lignes(const Donnees *d, Id id_modele)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < d->lignes_modeles.taille; i++) {
        if (ELEMENT(&d->lignes_modeles, LigneModele, i)->id_modele == id_modele) {
            n++;
        }
    }
    return n;
}

size_t mod_lignes_copier(const Donnees *d, Id id_modele, LigneSaisie *sortie, size_t max)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < d->lignes_modeles.taille && n < max; i++) {
        const LigneModele *l = ELEMENT(&d->lignes_modeles, LigneModele, i);
        if (l->id_modele == id_modele) {
            sortie[n].id_categorie = l->id_categorie;
            sortie[n].id_vehicule = l->id_vehicule;
            sortie[n].montant_ct = l->montant_ct;
            n++;
        }
    }
    return n;
}

size_t mod_nombre_recurrences(const Donnees *d, Id id_modele)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < d->recurrences.taille; i++) {
        if (ELEMENT(&d->recurrences, Recurrence, i)->id_modele == id_modele) {
            n++;
        }
    }
    return n;
}

/* Contrôles de saisie d'un modèle : mêmes règles qu'une opération, sans date. */
static bool controler_modele(const Application *app, const ModeleSaisie *s, const LigneSaisie *l,
                             size_t nb, RapportErreurs *r)
{
    OperationSaisie os;
    EtatTexte etat = texte_verifier(s->nom, NOM_MAX, true);
    bool ok = true;

    if (etat != TEXTE_OK) {
        rapport_ajouter(r, "Nom du modèle refusé : %s.", texte_message(etat));
        ok = false;
    }
    memset(&os, 0, sizeof os);
    os.nature = s->nature;
    os.date_realisation = app->date_reference;
    texte_copier(os.libelle, sizeof os.libelle, s->libelle);
    os.montant_total_ct = s->montant_total_ct;
    os.id_compte_source = s->id_compte_source;
    os.id_compte_destination = s->id_compte_destination;
    os.id_tiers = s->id_tiers;
    if (!op_controler_saisie(app, &os, l, nb, false, r)) {
        ok = false;
    }
    return ok;
}

static void retirer_lignes_modele(Donnees *copie, Id id_modele)
{
    size_t i;

    for (i = copie->lignes_modeles.taille; i > 0; i--) {
        if (ELEMENT(&copie->lignes_modeles, LigneModele, i - 1)->id_modele == id_modele) {
            tableau_retirer(&copie->lignes_modeles, i - 1);
        }
    }
}

static Resultat poser_lignes_modele(Donnees *copie, Id id_modele, const LigneSaisie *l, size_t nb,
                                    RapportErreurs *r)
{
    size_t i;

    for (i = 0; i < nb; i++) {
        LigneModele x;

        memset(&x, 0, sizeof x);
        x.id = donnees_nouvel_id(copie, ENT_LIGNE_MODELE);
        if (x.id == ID_AUCUN) {
            rapport_ajouter(r, "Plus aucun identifiant disponible pour les lignes de modèle.");
            return RES_ERR_VALIDATION;
        }
        x.id_modele = id_modele;
        x.id_categorie = l[i].id_categorie;
        x.id_vehicule = l[i].id_vehicule;
        x.montant_ct = l[i].montant_ct;
        if (!tableau_ajouter(&copie->lignes_modeles, &x)) {
            rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
            return RES_ERR_MEMOIRE;
        }
    }
    return RES_OK;
}

static void remplir_modele(ModeleOperation *m, const ModeleSaisie *s)
{
    texte_copier(m->nom, sizeof m->nom, s->nom);
    m->nature = s->nature;
    texte_copier(m->libelle, sizeof m->libelle, s->libelle);
    m->montant_total_ct = s->montant_total_ct;
    m->id_compte_source = s->id_compte_source;
    m->id_compte_destination = s->id_compte_destination;
    m->id_tiers = s->id_tiers;
}

Resultat mod_creer(Application *app, const ModeleSaisie *s, const LigneSaisie *lignes, size_t nb,
                   Id *id_cree, RapportErreurs *r)
{
    Donnees copie;
    ModeleOperation m;
    Resultat res;

    if (!controler_modele(app, s, lignes, nb, r)) {
        return RES_ERR_VALIDATION;
    }
    res = transaction_debut(app, &copie);
    if (res != RES_OK) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return res;
    }
    memset(&m, 0, sizeof m);
    m.id = donnees_nouvel_id(&copie, ENT_MODELE);
    if (m.id == ID_AUCUN) {
        rapport_ajouter(r, "Plus aucun identifiant disponible pour les modèles.");
        transaction_annuler(&copie);
        return RES_ERR_VALIDATION;
    }
    remplir_modele(&m, s);
    if (!tableau_ajouter(&copie.modeles, &m)) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        transaction_annuler(&copie);
        return RES_ERR_MEMOIRE;
    }
    res = poser_lignes_modele(&copie, m.id, lignes, nb, r);
    if (res != RES_OK) {
        transaction_annuler(&copie);
        return res;
    }
    res = transaction_valider(app, &copie, r);
    if (res == RES_OK && id_cree != NULL) {
        *id_cree = m.id;
    }
    return res;
}

Resultat mod_modifier(Application *app, Id id, const ModeleSaisie *s, const LigneSaisie *lignes,
                      size_t nb, RapportErreurs *r)
{
    const ModeleOperation *actuel = regles_modele(&app->donnees, id);
    Donnees copie;
    ModeleOperation *cible;
    Resultat res;

    if (actuel == NULL) {
        rapport_ajouter(r, "Modèle %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (s->nature != actuel->nature && mod_nombre_recurrences(&app->donnees, id) > 0) {
        rapport_ajouter(r, "La nature du modèle ne peut pas changer tant qu'une récurrence l'utilise.");
        return RES_ERR_VALIDATION;
    }
    if (!controler_modele(app, s, lignes, nb, r)) {
        return RES_ERR_VALIDATION;
    }
    res = transaction_debut(app, &copie);
    if (res != RES_OK) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return res;
    }
    cible = tableau_trouver_id(&copie.modeles, id, NULL);
    if (cible == NULL) {
        transaction_annuler(&copie);
        return RES_ERR_INTROUVABLE;
    }
    remplir_modele(cible, s);
    retirer_lignes_modele(&copie, id);
    res = poser_lignes_modele(&copie, id, lignes, nb, r);
    if (res != RES_OK) {
        transaction_annuler(&copie);
        return res;
    }
    /* Les opérations déjà générées gardent leur copie : seul le modèle change. */
    return transaction_valider(app, &copie, r);
}

Resultat mod_supprimer(Application *app, Id id, RapportErreurs *r)
{
    Donnees copie;
    size_t indice;
    size_t n;
    Resultat res;

    if (regles_modele(&app->donnees, id) == NULL) {
        rapport_ajouter(r, "Modèle %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    n = mod_nombre_recurrences(&app->donnees, id);
    if (n > 0) {
        rapport_ajouter(r, "Le modèle est utilisé par %lu récurrence(s) : suppression impossible.",
                        (unsigned long)n);
        return RES_ERR_VALIDATION;
    }
    res = transaction_debut(app, &copie);
    if (res != RES_OK) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return res;
    }
    retirer_lignes_modele(&copie, id);
    if (tableau_trouver_id(&copie.modeles, id, &indice) != NULL) {
        tableau_retirer(&copie.modeles, indice);
    }
    return transaction_valider(app, &copie, r);
}

Resultat op_creer_depuis_modele(Application *app, Id id_modele, Date date, bool accord_seuil,
                                Id *id_cree, RapportErreurs *avert, RapportErreurs *r)
{
    const ModeleOperation *m = regles_modele(&app->donnees, id_modele);
    OperationSaisie s;
    LigneSaisie *lignes;
    size_t nb;
    Resultat res;

    if (m == NULL) {
        rapport_ajouter(r, "Modèle %d introuvable.", id_modele);
        return RES_ERR_INTROUVABLE;
    }
    nb = mod_nombre_lignes(&app->donnees, id_modele);
    lignes = malloc((nb > 0 ? nb : 1) * sizeof *lignes);
    if (lignes == NULL) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return RES_ERR_MEMOIRE;
    }
    mod_lignes_copier(&app->donnees, id_modele, lignes, nb);
    memset(&s, 0, sizeof s);
    s.nature = m->nature;
    s.date_realisation = date;
    texte_copier(s.libelle, sizeof s.libelle, m->libelle);
    s.montant_total_ct = m->montant_total_ct;
    s.id_compte_source = m->id_compte_source;
    s.id_compte_destination = m->id_compte_destination;
    s.id_tiers = m->id_tiers;
    res = op_creer(app, &s, lignes, nb, accord_seuil, id_cree, avert, r);
    free(lignes);
    return res;
}
