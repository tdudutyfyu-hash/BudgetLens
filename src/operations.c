/*
 * BudgetLens — operations.c
 */
#include "operations.h"

#include <stdio.h>
#include <string.h>

#include "regles.h"
#include "texte.h"
#include "valeurs.h"

/* ------------------------------------------------------------------ */
/* Consultation                                                        */
/* ------------------------------------------------------------------ */

Operation *regles_operation(const Donnees *d, Id id)
{
    return (Operation *)tableau_trouver_id(&d->operations, id, NULL);
}

size_t op_nombre_lignes(const Donnees *d, Id id_operation)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < d->lignes_operations.taille; i++) {
        if (ELEMENT(&d->lignes_operations, LigneOperation, i)->id_operation == id_operation) {
            n++;
        }
    }
    return n;
}

size_t op_lignes_copier(const Donnees *d, Id id_operation, LigneSaisie *sortie, size_t max)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < d->lignes_operations.taille && n < max; i++) {
        const LigneOperation *l = ELEMENT(&d->lignes_operations, LigneOperation, i);
        if (l->id_operation == id_operation) {
            sortie[n].id_categorie = l->id_categorie;
            sortie[n].id_vehicule = l->id_vehicule;
            sortie[n].montant_ct = l->montant_ct;
            n++;
        }
    }
    return n;
}

Date op_date_effective(const Operation *o)
{
    return date_absente(o->date_realisation) ? o->date_prevue_origine : o->date_realisation;
}

void op_description_comptes(const Donnees *d, const Operation *o, char *sortie, size_t taille)
{
    const Compte *s = o->id_compte_source != ID_AUCUN ? regles_compte(d, o->id_compte_source) : NULL;
    const Compte *t = o->id_compte_destination != ID_AUCUN
                          ? regles_compte(d, o->id_compte_destination) : NULL;
    const char *ns = s != NULL ? s->nom : "?";
    const char *nt = t != NULL ? t->nom : "?";

    if (taille == 0) {
        return;
    }
    switch (o->nature) {
    case NATURE_DEPENSE:
        texte_copier(sortie, taille, ns);
        break;
    case NATURE_REVENU:
        texte_copier(sortie, taille, nt);
        break;
    default:
        snprintf(sortie, taille, "%s -> %s", ns, nt);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Contrôles de saisie (messages précis avant la validation globale)   */
/* ------------------------------------------------------------------ */

static bool controler_comptes(const Application *app, const OperationSaisie *s, bool avec_date,
                              RapportErreurs *r)
{
    const Donnees *d = &app->donnees;
    const Compte *src = NULL;
    const Compte *dst = NULL;
    bool ok = true;
    int k;

    if (s->id_compte_source != ID_AUCUN) {
        src = regles_compte(d, s->id_compte_source);
        if (src == NULL) {
            rapport_ajouter(r, "Compte source %d introuvable.", s->id_compte_source);
            ok = false;
        }
    }
    if (s->id_compte_destination != ID_AUCUN) {
        dst = regles_compte(d, s->id_compte_destination);
        if (dst == NULL) {
            rapport_ajouter(r, "Compte destination %d introuvable.", s->id_compte_destination);
            ok = false;
        }
    }
    switch (s->nature) {
    case NATURE_DEPENSE:
        if (s->id_compte_source == ID_AUCUN) {
            rapport_ajouter(r, "Une dépense exige un compte source.");
            ok = false;
        }
        if (s->id_compte_destination != ID_AUCUN) {
            rapport_ajouter(r, "Une dépense n'a pas de compte destination.");
            ok = false;
        }
        break;
    case NATURE_REVENU:
        if (s->id_compte_destination == ID_AUCUN) {
            rapport_ajouter(r, "Un revenu exige un compte destination.");
            ok = false;
        }
        if (s->id_compte_source != ID_AUCUN) {
            rapport_ajouter(r, "Un revenu n'a pas de compte source.");
            ok = false;
        }
        break;
    default:
        if (s->id_compte_source == ID_AUCUN || s->id_compte_destination == ID_AUCUN) {
            rapport_ajouter(r, "Un virement exige un compte source et un compte destination.");
            ok = false;
        } else if (s->id_compte_source == s->id_compte_destination) {
            rapport_ajouter(r, "Un virement relie deux comptes distincts.");
            ok = false;
        }
        break;
    }
    /* Q3 : pas de date antérieure à la date du solde initial d'un compte concerné. */
    if (avec_date && date_valide(s->date_realisation)) {
        const Compte *cpts[2];
        cpts[0] = src;
        cpts[1] = dst;
        for (k = 0; k < 2; k++) {
            if (cpts[k] != NULL && date_valide(cpts[k]->date_solde_initial)
                && date_comparer(s->date_realisation, cpts[k]->date_solde_initial) < 0) {
                char d1[DATE_TEXTE];
                char d2[DATE_TEXTE];
                date_formater(s->date_realisation, d1);
                date_formater(cpts[k]->date_solde_initial, d2);
                rapport_ajouter(r, "La date %s précède la date du solde initial (%s) du compte « %s ».",
                                d1, d2, cpts[k]->nom);
                ok = false;
            }
        }
    }
    return ok;
}

static bool controler_lignes(const Donnees *d, const OperationSaisie *s, const LigneSaisie *l,
                             size_t nb, RapportErreurs *r)
{
    bool ok = true;
    long long somme = 0;
    bool deborde = false;
    size_t i;

    if (s->nature == NATURE_VIREMENT) {
        if (nb > 0) {
            rapport_ajouter(r, "Un virement ne comporte aucune ligne de catégorie.");
            return false;
        }
        return true;
    }
    if (nb == 0) {
        rapport_ajouter(r, "Au moins une ligne de ventilation est obligatoire.");
        return false;
    }
    for (i = 0; i < nb; i++) {
        const Categorie *cat = regles_categorie(d, l[i].id_categorie);

        if (l[i].montant_ct <= 0 || l[i].montant_ct > MONTANT_MAX_CT) {
            rapport_ajouter(r, "Ligne %lu : montant hors limites (strictement positif).",
                            (unsigned long)(i + 1));
            ok = false;
        }
        if (cat == NULL) {
            rapport_ajouter(r, "Ligne %lu : catégorie %d introuvable.", (unsigned long)(i + 1),
                            l[i].id_categorie);
            ok = false;
        } else {
            char chemin[CHEMIN_MAX + 1];
            regles_chemin_categorie(d, cat->id, chemin, sizeof chemin);
            if (!regles_categorie_est_feuille(d, cat->id)) {
                rapport_ajouter(r, "Ligne %lu : la catégorie « %s » n'est pas terminale.",
                                (unsigned long)(i + 1), chemin);
                ok = false;
            }
            if ((s->nature == NATURE_DEPENSE) != (cat->type == CAT_DEPENSE)) {
                rapport_ajouter(r, "Ligne %lu : la catégorie « %s » (%s) ne convient pas à %s.",
                                (unsigned long)(i + 1), chemin, type_categorie_libelle(cat->type),
                                s->nature == NATURE_DEPENSE ? "une dépense" : "un revenu");
                ok = false;
            }
        }
        if (l[i].id_vehicule != ID_AUCUN) {
            if (s->nature != NATURE_DEPENSE) {
                rapport_ajouter(r, "Ligne %lu : le véhicule n'est admis que sur une dépense.",
                                (unsigned long)(i + 1));
                ok = false;
            } else if (regles_vehicule(d, l[i].id_vehicule) == NULL) {
                rapport_ajouter(r, "Ligne %lu : véhicule %d introuvable.", (unsigned long)(i + 1),
                                l[i].id_vehicule);
                ok = false;
            }
        }
        if (!deborde && !montant_additionner(somme, l[i].montant_ct, &somme)) {
            deborde = true;
        }
    }
    if (deborde) {
        rapport_ajouter(r, "La somme des lignes déborde la capacité des montants.");
        ok = false;
    } else if (s->montant_total_ct > 0 && somme != s->montant_total_ct) {
        char a[MONTANT_TEXTE];
        char b[MONTANT_TEXTE];
        char c[MONTANT_TEXTE];
        long long ecart = s->montant_total_ct - somme;

        montant_formater(somme, a, sizeof a);
        montant_formater(s->montant_total_ct, b, sizeof b);
        montant_formater(ecart < 0 ? -ecart : ecart, c, sizeof c);
        rapport_ajouter(r, "La somme des lignes (%s) diffère du montant total (%s) : %s %s.", a, b,
                        ecart > 0 ? "reste à répartir" : "excédent de", c);
        ok = false;
    }
    return ok;
}

bool op_controler_saisie(const Application *app, const OperationSaisie *s,
                         const LigneSaisie *lignes, size_t nb, bool avec_date, RapportErreurs *r)
{
    const Donnees *d = &app->donnees;
    bool ok = true;
    EtatTexte etat;

    if (s->nature != NATURE_DEPENSE && s->nature != NATURE_REVENU && s->nature != NATURE_VIREMENT) {
        rapport_ajouter(r, "Nature d'opération invalide.");
        return false;
    }
    etat = texte_verifier(s->libelle, LIBELLE_MAX, true);
    if (etat != TEXTE_OK) {
        rapport_ajouter(r, "Libellé refusé : %s.", texte_message(etat));
        ok = false;
    }
    if (avec_date) {
        if (!date_valide(s->date_realisation)) {
            rapport_ajouter(r, "Date de réalisation invalide.");
            ok = false;
        } else if (date_comparer(s->date_realisation, app->date_reference) > 0) {
            /* B3 : refus à la saisie d'une date réelle postérieure à la date de référence. */
            char d1[DATE_TEXTE];
            char d2[DATE_TEXTE];
            date_formater(s->date_realisation, d1);
            date_formater(app->date_reference, d2);
            rapport_ajouter(r, "La date de réalisation (%s) est postérieure à la date de référence "
                               "(%s) : une opération confirmée ne peut pas être datée dans le futur.",
                            d1, d2);
            ok = false;
        }
    }
    if (s->montant_total_ct <= 0 || s->montant_total_ct > MONTANT_MAX_CT) {
        rapport_ajouter(r, "Le montant total doit être strictement positif (au plus %lld ct).",
                        MONTANT_MAX_CT);
        ok = false;
    }
    if (!controler_comptes(app, s, avec_date, r)) {
        ok = false;
    }
    if (s->id_tiers != ID_AUCUN && regles_tiers(d, s->id_tiers) == NULL) {
        rapport_ajouter(r, "Tiers %d introuvable.", s->id_tiers);
        ok = false;
    }
    if (!controler_lignes(d, s, lignes, nb, r)) {
        ok = false;
    }
    return ok;
}

/* ------------------------------------------------------------------ */
/* Transactions                                                        */
/* ------------------------------------------------------------------ */

static Resultat poser_lignes(Donnees *copie, Id id_operation, const LigneSaisie *l, size_t nb,
                             RapportErreurs *r)
{
    size_t i;

    for (i = 0; i < nb; i++) {
        LigneOperation x;

        memset(&x, 0, sizeof x);
        x.id = donnees_nouvel_id(copie, ENT_LIGNE_OPERATION);
        if (x.id == ID_AUCUN) {
            rapport_ajouter(r, "Plus aucun identifiant disponible pour les lignes d'opération.");
            return RES_ERR_VALIDATION;
        }
        x.id_operation = id_operation;
        x.id_categorie = l[i].id_categorie;
        x.id_vehicule = l[i].id_vehicule;
        x.montant_ct = l[i].montant_ct;
        if (!tableau_ajouter(&copie->lignes_operations, &x)) {
            rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
            return RES_ERR_MEMOIRE;
        }
    }
    return RES_OK;
}

static void retirer_lignes(Donnees *copie, Id id_operation)
{
    size_t i;

    for (i = copie->lignes_operations.taille; i > 0; i--) {
        if (ELEMENT(&copie->lignes_operations, LigneOperation, i - 1)->id_operation == id_operation) {
            tableau_retirer(&copie->lignes_operations, i - 1);
        }
    }
}

static Resultat debut(const Application *app, Donnees *copie, RapportErreurs *r)
{
    Resultat res = transaction_debut(app, copie);

    if (res != RES_OK) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
    }
    return res;
}

Resultat op_creer(Application *app, const OperationSaisie *s, const LigneSaisie *lignes,
                  size_t nb, bool accord_seuil, Id *id_cree, RapportErreurs *avert,
                  RapportErreurs *r)
{
    Donnees copie;
    Operation op;
    Resultat res;

    if (!op_controler_saisie(app, s, lignes, nb, true, r)) {
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    memset(&op, 0, sizeof op);
    op.id = donnees_nouvel_id(&copie, ENT_OPERATION);
    if (op.id == ID_AUCUN) {
        rapport_ajouter(r, "Plus aucun identifiant disponible pour les opérations.");
        transaction_annuler(&copie);
        return RES_ERR_VALIDATION;
    }
    op.nature = s->nature;
    op.statut = STATUT_CONFIRMEE;            /* G2 : une opération manuelle est créée confirmée */
    op.date_realisation = s->date_realisation;
    texte_copier(op.libelle, sizeof op.libelle, s->libelle);
    op.montant_total_ct = s->montant_total_ct;
    op.id_compte_source = s->id_compte_source;
    op.id_compte_destination = s->id_compte_destination;
    op.id_tiers = s->id_tiers;
    op.id_recurrence = ID_AUCUN;
    op.date_prevue_origine = date_aucune();
    if (!tableau_ajouter(&copie.operations, &op)) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        transaction_annuler(&copie);
        return RES_ERR_MEMOIRE;
    }
    res = poser_lignes(&copie, op.id, lignes, nb, r);
    if (res != RES_OK) {
        transaction_annuler(&copie);
        return res;
    }
    res = transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
    if (res == RES_OK && id_cree != NULL) {
        *id_cree = op.id;
    }
    return res;
}

Resultat op_corriger(Application *app, Id id, const OperationSaisie *s,
                     const LigneSaisie *lignes, size_t nb, bool accord_seuil,
                     RapportErreurs *avert, RapportErreurs *r)
{
    const Operation *actuelle = regles_operation(&app->donnees, id);
    Donnees copie;
    Operation *cible;
    size_t indice;
    Resultat res;
    bool occurrence;

    if (actuelle == NULL) {
        rapport_ajouter(r, "Opération %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    occurrence = (actuelle->id_recurrence != ID_AUCUN);
    if (occurrence && actuelle->statut != STATUT_A_CONFIRMER) {
        rapport_ajouter(r, "L'occurrence %d est %s : ses montants, lignes, comptes et tiers ne sont "
                           "plus modifiables. Seule son annulation reste possible.",
                        id, statut_libelle(actuelle->statut));
        return RES_ERR_VALIDATION;
    }
    if (s->nature != actuelle->nature) {
        rapport_ajouter(r, "La nature d'une opération ne peut pas être modifiée.");
        return RES_ERR_VALIDATION;
    }
    if (!op_controler_saisie(app, s, lignes, nb, !occurrence, r)) {
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    cible = tableau_trouver_id(&copie.operations, id, &indice);
    if (cible == NULL) {
        rapport_ajouter(r, "Opération %d introuvable.", id);
        transaction_annuler(&copie);
        return RES_ERR_INTROUVABLE;
    }
    if (!occurrence) {
        cible->date_realisation = s->date_realisation;
    }
    texte_copier(cible->libelle, sizeof cible->libelle, s->libelle);
    cible->montant_total_ct = s->montant_total_ct;
    cible->id_compte_source = s->id_compte_source;
    cible->id_compte_destination = s->id_compte_destination;
    cible->id_tiers = s->id_tiers;
    retirer_lignes(&copie, id);
    res = poser_lignes(&copie, id, lignes, nb, r);
    if (res != RES_OK) {
        transaction_annuler(&copie);
        return res;
    }
    return transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
}

Resultat op_supprimer(Application *app, Id id, bool accord_seuil, RapportErreurs *avert,
                      RapportErreurs *r)
{
    const Operation *actuelle = regles_operation(&app->donnees, id);
    Donnees copie;
    size_t indice;
    Resultat res;

    if (actuelle == NULL) {
        rapport_ajouter(r, "Opération %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (actuelle->id_recurrence != ID_AUCUN) {
        rapport_ajouter(r, "L'opération %d est une occurrence de récurrence : elle n'est jamais "
                           "supprimée physiquement. Annulez-la depuis le menu des échéances.", id);
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    retirer_lignes(&copie, id);
    if (tableau_trouver_id(&copie.operations, id, &indice) == NULL) {
        transaction_annuler(&copie);
        return RES_ERR_INTROUVABLE;
    }
    tableau_retirer(&copie.operations, indice);
    return transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
}

Resultat op_annuler(Application *app, Id id, bool accord_seuil, RapportErreurs *avert,
                    RapportErreurs *r)
{
    const Operation *actuelle = regles_operation(&app->donnees, id);
    Donnees copie;
    Operation *cible;
    Resultat res;

    if (actuelle == NULL) {
        rapport_ajouter(r, "Opération %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (actuelle->id_recurrence == ID_AUCUN) {
        rapport_ajouter(r, "L'opération %d est manuelle : elle se corrige ou se supprime, "
                           "elle ne s'annule pas.", id);
        return RES_ERR_VALIDATION;
    }
    if (actuelle->statut != STATUT_CONFIRMEE) {
        rapport_ajouter(r, "L'occurrence %d est %s : seule une occurrence confirmée peut être "
                           "annulée.", id, statut_libelle(actuelle->statut));
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    cible = tableau_trouver_id(&copie.operations, id, NULL);
    if (cible == NULL) {
        transaction_annuler(&copie);
        return RES_ERR_INTROUVABLE;
    }
    cible->statut = STATUT_ANNULEE;      /* la date réelle est conservée */
    return transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
}

Resultat op_supprimer_ou_annuler(Application *app, const Id *ids, size_t nb, bool accord_seuil,
                                 RapportErreurs *avert, RapportErreurs *r)
{
    Donnees copie;
    Resultat res;
    size_t i;
    size_t j;

    if (nb == 0) {
        rapport_ajouter(r, "Aucune opération sélectionnée.");
        return RES_ERR_VALIDATION;
    }
    for (i = 0; i < nb; i++) {
        const Operation *o = regles_operation(&app->donnees, ids[i]);

        for (j = 0; j < i; j++) {
            if (ids[j] == ids[i]) {
                rapport_ajouter(r, "Opération %d sélectionnée deux fois.", ids[i]);
                return RES_ERR_VALIDATION;
            }
        }
        if (o == NULL) {
            rapport_ajouter(r, "Opération %d introuvable.", ids[i]);
            return RES_ERR_INTROUVABLE;
        }
        if (o->statut != STATUT_CONFIRMEE) {
            rapport_ajouter(r, "L'opération %d n'est pas confirmée : elle n'est pas concernée.", ids[i]);
            return RES_ERR_VALIDATION;
        }
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    for (i = 0; i < nb; i++) {
        size_t indice;
        Operation *o = tableau_trouver_id(&copie.operations, ids[i], &indice);

        if (o == NULL) {
            transaction_annuler(&copie);
            return RES_ERR_INTROUVABLE;
        }
        if (o->id_recurrence != ID_AUCUN) {
            o->statut = STATUT_ANNULEE;          /* occurrence : jamais supprimée physiquement */
        } else {
            retirer_lignes(&copie, ids[i]);
            if (tableau_trouver_id(&copie.operations, ids[i], &indice) != NULL) {
                tableau_retirer(&copie.operations, indice);
            }
        }
    }
    return transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
}

/* ------------------------------------------------------------------ */
/* Confirmation, refus, lots                                           */
/* ------------------------------------------------------------------ */

/* Applique une confirmation à d (sans validation globale) ; faux avec motif dans r. */
static bool appliquer_confirmation(Donnees *d, Date reference, const ConfirmationLot *it,
                                   const LigneSaisie *pool, RapportErreurs *r)
{
    Operation *o = tableau_trouver_id(&d->operations, it->id, NULL);
    long long total;
    bool ok = true;

    if (o == NULL) {
        rapport_ajouter(r, "Échéance %d introuvable.", it->id);
        return false;
    }
    if (o->id_recurrence == ID_AUCUN || o->statut != STATUT_A_CONFIRMER) {
        rapport_ajouter(r, "Opération %d : seule une occurrence à confirmer peut être confirmée "
                           "(statut actuel : %s).", it->id, statut_libelle(o->statut));
        return false;
    }
    if (!date_valide(it->date_realisation)) {
        rapport_ajouter(r, "Échéance %d : la date de réalisation est obligatoire et doit être valide.",
                        it->id);
        ok = false;
    } else if (date_comparer(it->date_realisation, reference) > 0) {
        char d1[DATE_TEXTE];
        date_formater(it->date_realisation, d1);
        rapport_ajouter(r, "Échéance %d : la date de réalisation (%s) est postérieure à la date de "
                           "référence : une opération confirmée ne peut pas être datée dans le futur.",
                        it->id, d1);
        ok = false;
    }
    total = it->nouveau_total_ct > 0 ? it->nouveau_total_ct : o->montant_total_ct;
    if (it->nouveau_total_ct < 0 || total > MONTANT_MAX_CT) {
        rapport_ajouter(r, "Échéance %d : montant hors limites.", it->id);
        ok = false;
    }
    if (ok && it->nb_lignes > 0) {
        OperationSaisie s;
        RapportErreurs local;
        const LigneSaisie *nouvelles = pool + it->debut_lignes;
        size_t k;

        memset(&s, 0, sizeof s);
        s.nature = o->nature;
        s.montant_total_ct = total;
        rapport_init(&local);
        if (!controler_lignes(d, &s, nouvelles, it->nb_lignes, &local)) {
            for (k = 0; k < rapport_nombre(&local) && k < local.messages.taille; k++) {
                rapport_ajouter(r, "Échéance %d : %s", it->id, rapport_message(&local, k));
            }
            ok = false;
        }
        rapport_liberer(&local);
        if (ok) {
            retirer_lignes(d, it->id);
            if (poser_lignes(d, it->id, nouvelles, it->nb_lignes, r) != RES_OK) {
                return false;
            }
        }
    } else if (ok && total != o->montant_total_ct) {
        /* Montant changé sans nouvelle ventilation : seule une ligne unique est ajustable
           (un virement n'a aucune ligne : son montant change simplement). */
        if (o->nature == NATURE_VIREMENT) {
            (void)total;
        } else if (op_nombre_lignes(d, it->id) == 1) {
            size_t i;
            for (i = 0; i < d->lignes_operations.taille; i++) {
                LigneOperation *l = ELEMENT(&d->lignes_operations, LigneOperation, i);
                if (l->id_operation == it->id) {
                    l->montant_ct = total;
                }
            }
        } else {
            rapport_ajouter(r, "Échéance %d : le montant change et la ventilation compte plusieurs "
                               "lignes : une nouvelle ventilation est obligatoire.", it->id);
            ok = false;
        }
    }
    if (!ok) {
        return false;
    }
    o = tableau_trouver_id(&d->operations, it->id, NULL);   /* relu : le tableau a pu bouger */
    o->montant_total_ct = total;
    o->date_realisation = it->date_realisation;
    o->statut = STATUT_CONFIRMEE;
    return true;
}

Resultat op_lot_confirmer(Application *app, const ConfirmationLot *items, size_t nb,
                          const LigneSaisie *lignes, bool simuler, bool accord_seuil,
                          RapportErreurs *avert, RapportErreurs *r)
{
    Donnees copie;
    size_t i;
    size_t j;
    bool ok = true;
    Resultat res;

    if (nb == 0) {
        rapport_ajouter(r, "Aucune échéance sélectionnée.");
        return RES_ERR_VALIDATION;
    }
    for (i = 0; i < nb; i++) {
        for (j = 0; j < i; j++) {
            if (items[j].id == items[i].id) {
                rapport_ajouter(r, "Échéance %d sélectionnée deux fois.", items[i].id);
                return RES_ERR_VALIDATION;
            }
        }
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    for (i = 0; i < nb; i++) {
        if (!appliquer_confirmation(&copie, app->date_reference, &items[i], lignes, r)) {
            ok = false;     /* on continue : toutes les erreurs du lot sont listées */
        }
    }
    if (!ok) {
        transaction_annuler(&copie);
        return RES_ERR_VALIDATION;
    }
    if (simuler) {
        ok = regles_valider(&copie, r);
        transaction_annuler(&copie);
        return ok ? RES_OK : RES_ERR_VALIDATION;
    }
    return transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
}

Resultat op_confirmer(Application *app, Id id, Date date_realisation, long long nouveau_total_ct,
                      const LigneSaisie *lignes, size_t nb_lignes, bool accord_seuil,
                      RapportErreurs *avert, RapportErreurs *r)
{
    ConfirmationLot it;

    it.id = id;
    it.date_realisation = date_realisation;
    it.nouveau_total_ct = nouveau_total_ct;
    it.debut_lignes = 0;
    it.nb_lignes = nb_lignes;
    return op_lot_confirmer(app, &it, 1, lignes, false, accord_seuil, avert, r);
}

Resultat op_lot_refuser(Application *app, const Id *ids, size_t nb, RapportErreurs *r)
{
    Donnees copie;
    size_t i;
    size_t j;
    bool ok = true;
    Resultat res;

    if (nb == 0) {
        rapport_ajouter(r, "Aucune échéance sélectionnée.");
        return RES_ERR_VALIDATION;
    }
    for (i = 0; i < nb; i++) {
        const Operation *o = regles_operation(&app->donnees, ids[i]);

        for (j = 0; j < i; j++) {
            if (ids[j] == ids[i]) {
                rapport_ajouter(r, "Échéance %d sélectionnée deux fois.", ids[i]);
                ok = false;
            }
        }
        if (o == NULL) {
            rapport_ajouter(r, "Échéance %d introuvable.", ids[i]);
            ok = false;
        } else if (o->id_recurrence == ID_AUCUN || o->statut != STATUT_A_CONFIRMER) {
            rapport_ajouter(r, "Opération %d : seule une occurrence à confirmer peut être refusée "
                               "(statut actuel : %s).", ids[i], statut_libelle(o->statut));
            ok = false;
        }
    }
    if (!ok) {
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    for (i = 0; i < nb; i++) {
        Operation *o = tableau_trouver_id(&copie.operations, ids[i], NULL);
        o->statut = STATUT_ANNULEE;                 /* sans date réelle : jamais confirmée */
        o->date_realisation = date_aucune();
    }
    return transaction_valider(app, &copie, r);
}

Resultat op_refuser(Application *app, Id id, RapportErreurs *r)
{
    return op_lot_refuser(app, &id, 1, r);
}
