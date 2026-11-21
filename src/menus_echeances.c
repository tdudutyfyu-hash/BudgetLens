/*
 * BudgetLens — menus_echeances.c
 * Échéances à confirmer : consultation, confirmation (date réelle explicite,
 * « = » recopie la date prévue), refus, annulation d'une occurrence confirmée
 * et traitements par lot tout ou rien (H5).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analyses.h"
#include "menus.h"
#include "operations.h"
#include "regles.h"
#include "saisie.h"
#include "texte.h"
#include "valeurs.h"

/* Échéances A_CONFIRMER triées par date prévue ; res (Tableau de ResultatRecherche) est rempli. */
static bool charger_echeances(const Application *app, Tableau *res)
{
    FiltreOperations f;

    filtre_init(&f);
    f.par_statut = true;
    f.statut = STATUT_A_CONFIRMER;
    if (!analyses_rechercher(&app->donnees, &f, res)) {
        printf("Mémoire insuffisante : recherche impossible.\n");
        return false;
    }
    analyses_trier(res, TRI_DATE, true);
    return true;
}

/* Nombre d'échéances que « Lister » présenterait (même sélection) ; 0 si la mémoire manque. */
size_t menus_nombre_echeances(const Application *app)
{
    FiltreOperations f;
    Tableau res;
    size_t n = 0;

    filtre_init(&f);
    f.par_statut = true;
    f.statut = STATUT_A_CONFIRMER;
    tableau_init(&res, sizeof(ResultatRecherche));
    if (analyses_rechercher(&app->donnees, &f, &res)) {
        n = res.taille;
    }
    tableau_liberer(&res);
    return n;
}

static void afficher_echeances(const Application *app, const Tableau *res)
{
    size_t i;

    printf("%-5s ", "Id");
    menus_colonne("Prévue le", 11);
    menus_colonne("Nature", 9);
    menus_colonne("Libellé", 26);
    menus_colonne("Compte(s)", 26);
    menus_colonne("Montant", 16);
    printf("Récurrence\n");
    for (i = 0; i < res->taille; i++) {
        const ResultatRecherche *x = ELEMENT(res, ResultatRecherche, i);
        const Operation *o = ELEMENT(&app->donnees.operations, Operation, x->indice);
        char date[DATE_TEXTE];
        char montant[MONTANT_TEXTE];
        char comptes[2 * NOM_MAX + 8];

        date_formater(o->date_prevue_origine, date);
        montant_formater(o->montant_total_ct, montant, sizeof montant);
        op_description_comptes(&app->donnees, o, comptes, sizeof comptes);
        printf("%-5d ", o->id);
        menus_colonne(date, 11);
        menus_colonne(nature_libelle(o->nature), 9);
        menus_colonne_tronquee(o->libelle, 26);
        menus_colonne_tronquee(comptes, 26);
        menus_colonne(montant, 16);
        printf("%d\n", o->id_recurrence);
    }
}

static void lister_echeances(const Application *app)
{
    Tableau res;

    tableau_init(&res, sizeof(ResultatRecherche));
    if (charger_echeances(app, &res)) {
        if (res.taille == 0) {
            printf("Aucune échéance à confirmer.\n");
        } else {
            afficher_echeances(app, &res);
            printf("%lu échéance(s) à confirmer (aucun effet sur les soldes avant confirmation).\n",
                   (unsigned long)res.taille);
        }
    }
    tableau_liberer(&res);
}

/* Occurrence A_CONFIRMER par identifiant ; NULL avec message sinon. */
static const Operation *echeance_par_id(const Application *app, Id id)
{
    const Operation *o = regles_operation(&app->donnees, id);

    if (o == NULL) {
        printf("Opération %d introuvable.\n", id);
        return NULL;
    }
    if (o->id_recurrence == ID_AUCUN || o->statut != STATUT_A_CONFIRMER) {
        printf("L'opération %d n'est pas une échéance à confirmer (statut : %s).\n", id,
               statut_libelle(o->statut));
        return NULL;
    }
    return o;
}

/* Date réelle explicite ; « = » recopie la date prévue. */
static EtatSaisie saisir_date_reelle(const Operation *o, Date *d)
{
    char invite[200];
    char prevue[DATE_TEXTE];
    char tampon[SAISIE_LIGNE_MAX];

    date_formater(o->date_prevue_origine, prevue);
    snprintf(invite, sizeof invite, "Échéance %d, date de réalisation (AAAA-MM-JJ, = pour la date prévue %s) : ",
             o->id, prevue);
    for (;;) {
        EtatSaisie s = saisie_ligne(invite, tampon, sizeof tampon);

        if (s != SAISIE_OK) {
            return s;
        }
        if (strcmp(tampon, "=") == 0) {
            *d = o->date_prevue_origine;
            return SAISIE_OK;
        }
        if (date_lire(tampon, d)) {
            return SAISIE_OK;
        }
        printf("Date invalide : format AAAA-MM-JJ, ou « = » pour la date prévue.\n");
    }
}

/*
 * Saisit date, montant et ventilation éventuelle d'une confirmation. Les nouvelles lignes sont
 * ajoutées au réservoir « pool ». SAISIE_OK, SAISIE_VIDE (échéance abandonnée) ou SAISIE_FIN.
 */
static EtatSaisie collecter_confirmation(const Application *app, const Operation *o, ConfirmationLot *it,
                                         Tableau *pool)
{
    char invite[160];
    char montant[MONTANT_TEXTE];
    long long nouveau;
    EtatSaisie s;
    Tableau tmp;
    size_t k;

    memset(it, 0, sizeof *it);
    it->id = o->id;
    montant_formater(o->montant_total_ct, montant, sizeof montant);
    printf("Échéance %d : %s, montant prévu %s\n", o->id, o->libelle, montant);
    s = saisir_date_reelle(o, &it->date_realisation);
    if (s != SAISIE_OK) {
        return s;
    }
    snprintf(invite, sizeof invite, "Montant réel [%s] (Entrée = inchangé) : ", montant);
    s = saisie_montant(invite, false, false, &nouveau);
    if (s == SAISIE_FIN) {
        return s;
    }
    if (s == SAISIE_VIDE || nouveau == o->montant_total_ct) {
        return SAISIE_OK;
    }
    it->nouveau_total_ct = nouveau;
    if (o->nature == NATURE_VIREMENT || op_nombre_lignes(&app->donnees, o->id) == 1) {
        return SAISIE_OK;       /* ligne unique ajustée automatiquement ; virement sans ligne */
    }
    printf("Le montant change et la ventilation compte plusieurs lignes : saisissez-la de nouveau.\n");
    tableau_init(&tmp, sizeof(LigneSaisie));
    if (!menus_saisir_ventilation(app, o->nature, nouveau, &tmp)) {
        tableau_liberer(&tmp);
        return saisie_etat_entree() == ENTREE_DISPONIBLE ? SAISIE_VIDE : SAISIE_FIN;
    }
    it->debut_lignes = pool->taille;
    for (k = 0; k < tmp.taille; k++) {
        if (!tableau_ajouter(pool, ELEMENT(&tmp, LigneSaisie, k))) {
            tableau_liberer(&tmp);
            printf("Mémoire insuffisante.\n");
            return SAISIE_VIDE;
        }
    }
    it->nb_lignes = tmp.taille;
    tableau_liberer(&tmp);
    return SAISIE_OK;
}

/* Exécute une confirmation (ou un lot) avec la gestion des avertissements de seuil. */
static void executer_lot(Application *app, const ConfirmationLot *items, size_t nb, const LigneSaisie *pool,
                         const char *succes)
{
    RapportErreurs r;
    RapportErreurs w;
    Resultat res;

    rapport_init(&r);
    rapport_init(&w);
    res = op_lot_confirmer(app, items, nb, pool, false, false, &w, &r);
    if (res == RES_CONFIRMATION_REQUISE) {
        if (menus_confirmer_avertissements(&w)) {
            rapport_vider(&r);
            res = op_lot_confirmer(app, items, nb, pool, false, true, NULL, &r);
        } else {
            rapport_ajouter(&r, "Aucune modification n'a été faite.");
            res = RES_ABANDON;
        }
    }
    menus_resultat(res, &r, succes);
    rapport_liberer(&w);
    rapport_liberer(&r);
}

static void confirmer_echeance(Application *app)
{
    Tableau res;
    Tableau pool;
    ConfirmationLot it;
    const Operation *o;
    Id id;
    EtatSaisie s;

    tableau_init(&res, sizeof(ResultatRecherche));
    if (charger_echeances(app, &res) && res.taille > 0) {
        afficher_echeances(app, &res);
    } else if (res.taille == 0) {
        printf("Aucune échéance à confirmer.\n");
        tableau_liberer(&res);
        return;
    }
    tableau_liberer(&res);
    if (saisie_id("Identifiant de l'échéance à confirmer : ", &id) != SAISIE_OK) {
        return;
    }
    o = echeance_par_id(app, id);
    if (o == NULL) {
        return;
    }
    tableau_init(&pool, sizeof(LigneSaisie));
    s = collecter_confirmation(app, o, &it, &pool);
    if (s == SAISIE_OK) {
        executer_lot(app, &it, 1, (const LigneSaisie *)pool.donnees, "Échéance confirmée.");
    } else if (s == SAISIE_VIDE) {
        menus_abandon();
    }
    tableau_liberer(&pool);
}

/* Lit une liste d'échéances (« 12,15,18 » ou « * » = toutes celles qui sont affichées). */
static bool saisir_selection(const Application *app, const Tableau *res, Id *ids, size_t max, size_t *n)
{
    char tampon[SAISIE_LIGNE_MAX];
    EtatSaisie s;
    size_t i;

    *n = 0;
    s = saisie_ligne("Identifiants des échéances (ex. 12,15,18) ou * pour toutes : ", tampon, sizeof tampon);
    if (s != SAISIE_OK) {
        return false;
    }
    if (strcmp(tampon, "*") == 0) {
        for (i = 0; i < res->taille && *n < max; i++) {
            ids[(*n)++] = ELEMENT(res, ResultatRecherche, i)->id;
        }
        return *n > 0;
    }
    if (!liste_ids_lire(tampon, ids, max, n)) {
        printf("Liste invalide (exemple : 12,15,18).\n");
        return false;
    }
    for (i = 0; i < *n; i++) {
        if (echeance_par_id(app, ids[i]) == NULL) {
            return false;
        }
    }
    return true;
}

#define LOT_MAX 200

static void confirmer_lot(Application *app)
{
    Tableau res;
    Tableau pool;
    Tableau items;
    Id ids[LOT_MAX];
    size_t n = 0;
    size_t k;
    bool abandon = false;
    bool oui;

    tableau_init(&res, sizeof(ResultatRecherche));
    tableau_init(&pool, sizeof(LigneSaisie));
    tableau_init(&items, sizeof(ConfirmationLot));
    if (!charger_echeances(app, &res)) {
        goto fin;
    }
    if (res.taille == 0) {
        printf("Aucune échéance à confirmer.\n");
        goto fin;
    }
    afficher_echeances(app, &res);
    if (!saisir_selection(app, &res, ids, LOT_MAX, &n)) {
        if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            menus_abandon();
        }
        goto fin;
    }
    printf("Lot de %lu échéance(s) : saisie de chaque date réelle (« = » recopie la date prévue). "
           "Tout est validé ensemble ; au moindre échec rien n'est enregistré.\n", (unsigned long)n);
    for (k = 0; k < n && !abandon; k++) {
        for (;;) {
            const Operation *o = regles_operation(&app->donnees, ids[k]);
            ConfirmationLot it;
            size_t pool_avant = pool.taille;
            RapportErreurs r;
            EtatSaisie s = collecter_confirmation(app, o, &it, &pool);

            if (s != SAISIE_OK) {
                abandon = true;     /* échéance abandonnée ou fin d'entrée : le lot entier est abandonné */
                break;
            }
            if (!tableau_ajouter(&items, &it)) {
                printf("Mémoire insuffisante.\n");
                abandon = true;
                break;
            }
            /* Vérification immédiate du lot déjà saisi (G10 cumulé entre les échéances). */
            rapport_init(&r);
            if (op_lot_confirmer(app, (const ConfirmationLot *)items.donnees, items.taille,
                                 (const LigneSaisie *)pool.donnees, true, true, NULL, &r) == RES_OK) {
                rapport_liberer(&r);
                break;
            }
            printf("Cette saisie est refusée :\n");
            rapport_afficher(&r, stdout);
            rapport_liberer(&r);
            items.taille--;
            pool.taille = pool_avant;
            if (saisie_oui_non("Ressaisir cette échéance ? (o = oui, n = abandonner tout le lot) : ", &oui)
                    != SAISIE_OK || !oui) {
                abandon = true;
                break;
            }
        }
    }
    if (abandon) {
        if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            printf("Lot abandonné : aucune modification.\n");
        }
        goto fin;
    }
    printf("\nRécapitulatif du lot :\n");
    for (k = 0; k < items.taille; k++) {
        const ConfirmationLot *it = ELEMENT(&items, ConfirmationLot, k);
        const Operation *o = regles_operation(&app->donnees, it->id);
        char date[DATE_TEXTE];
        char montant[MONTANT_TEXTE];

        date_formater(it->date_realisation, date);
        montant_formater(it->nouveau_total_ct > 0 ? it->nouveau_total_ct : o->montant_total_ct, montant,
                         sizeof montant);
        printf("  [%d] %s | réalisée le %s | %s\n", it->id, o->libelle, date, montant);
    }
    if (saisie_oui_non("Confirmer ce lot ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
        printf("Lot abandonné : aucune modification.\n");
        goto fin;
    }
    executer_lot(app, (const ConfirmationLot *)items.donnees, items.taille,
                 (const LigneSaisie *)pool.donnees, "Lot confirmé.");
fin:
    tableau_liberer(&res);
    tableau_liberer(&pool);
    tableau_liberer(&items);
}

static void refuser_echeance(Application *app)
{
    RapportErreurs r;
    Id id;
    bool oui;

    lister_echeances(app);
    if (saisie_id("Identifiant de l'échéance à refuser : ", &id) != SAISIE_OK) {
        return;
    }
    if (echeance_par_id(app, id) == NULL) {
        return;
    }
    if (saisie_oui_non("Refuser cette échéance (elle sera annulée, sans date réelle) ? (o/n) : ", &oui)
            != SAISIE_OK || !oui) {
        printf("Refus annulé.\n");
        return;
    }
    rapport_init(&r);
    menus_resultat(op_refuser(app, id, &r), &r, "Échéance refusée (statut Annulée).");
    rapport_liberer(&r);
}

static void refuser_lot(Application *app)
{
    Tableau res;
    Id ids[LOT_MAX];
    size_t n = 0;
    RapportErreurs r;
    bool oui;

    tableau_init(&res, sizeof(ResultatRecherche));
    if (charger_echeances(app, &res) && res.taille > 0) {
        afficher_echeances(app, &res);
        if (saisir_selection(app, &res, ids, LOT_MAX, &n)) {
            printf("%lu échéance(s) seront refusées en une seule opération (statut Annulée, sans date réelle).\n",
                   (unsigned long)n);
            if (saisie_oui_non("Confirmer le refus du lot ? (o/n) : ", &oui) == SAISIE_OK && oui) {
                rapport_init(&r);
                menus_resultat(op_lot_refuser(app, ids, n, &r), &r, "Lot refusé.");
                rapport_liberer(&r);
            } else {
                printf("Refus annulé.\n");
            }
        } else if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            menus_abandon();
        }
    } else if (res.taille == 0) {
        printf("Aucune échéance à confirmer.\n");
    }
    tableau_liberer(&res);
}

static void annuler_occurrence(Application *app)
{
    RapportErreurs r;
    RapportErreurs w;
    const Operation *o;
    Id id;
    bool oui;
    Resultat res;

    if (saisie_id("Identifiant de l'occurrence confirmée à annuler : ", &id) != SAISIE_OK) {
        return;
    }
    o = regles_operation(&app->donnees, id);
    if (o == NULL || o->id_recurrence == ID_AUCUN || o->statut != STATUT_CONFIRMEE) {
        printf("L'opération %d n'est pas une occurrence de récurrence confirmée.\n", id);
        return;
    }
    menus_afficher_operation(app, o);
    if (saisie_oui_non("Annuler cette occurrence (état final, date réelle conservée, effet financier "
                       "retiré) ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
        printf("Annulation abandonnée.\n");
        return;
    }
    rapport_init(&r);
    rapport_init(&w);
    res = op_annuler(app, id, false, &w, &r);
    if (res == RES_CONFIRMATION_REQUISE) {
        if (menus_confirmer_avertissements(&w)) {
            rapport_vider(&r);
            res = op_annuler(app, id, true, NULL, &r);
        } else {
            rapport_ajouter(&r, "Aucune modification n'a été faite.");
            res = RES_ABANDON;
        }
    }
    menus_resultat(res, &r, "Occurrence annulée.");
    rapport_liberer(&w);
    rapport_liberer(&r);
}

void menu_echeances(Application *app)
{
    for (;;) {
        int choix;

        menus_bandeau(app, "Échéances à confirmer");
        printf("1. Lister les échéances à confirmer\n2. Confirmer une échéance\n3. Refuser une échéance\n"
               "4. Corriger une échéance (avant confirmation)\n5. Confirmer plusieurs échéances (lot)\n"
               "6. Refuser plusieurs échéances (lot)\n7. Annuler une occurrence confirmée\n0. Retour\n");
        choix = saisie_menu(7);
        if (choix <= 0) {
            return;
        }
        switch (choix) {
        case 1:
            lister_echeances(app);
            break;
        case 2:
            confirmer_echeance(app);
            break;
        case 3:
            refuser_echeance(app);
            break;
        case 4:
            menus_corriger_operation(app);
            break;
        case 5:
            confirmer_lot(app);
            break;
        case 6:
            refuser_lot(app);
            break;
        default:
            annuler_occurrence(app);
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}
