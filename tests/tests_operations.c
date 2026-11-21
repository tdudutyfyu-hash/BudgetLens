/*
 * BudgetLens — tests_operations.c
 * Étape 2 : validateur V08–V13, soldes chronologiques (G10), Q3, B3, seuil,
 * opérations manuelles, recherche, tri et statistiques.
 * Les résultats attendus sont calculés à la main à partir des exigences
 * (MLD §6 à §8, audit G2/G7/G10, cahier des charges §6), jamais en relisant
 * la sortie du programme.
 */
#include <stdlib.h>
#include <string.h>

#include "analyses.h"
#include "app.h"
#include "calendrier.h"
#include "modeles.h"
#include "import.h"
#include "operations.h"
#include "persistance.h"
#include "recurrences.h"
#include "referentiel.h"
#include "regles.h"
#include "sauvegardes.h"
#include "soldes.h"
#include "tests.h"
#include "texte.h"
#include "valeurs.h"

static Date D(int a, int m, int j)
{
    return date_creer(a, m, j);
}

typedef struct {
    Id banque;       /* 5 000,00 DH au 2026-01-01 */
    Id especes;      /* 200,00 DH au 2026-01-01 */
    Id neuf;         /* 1 000,00 DH au 2026-06-01 */
    Id seuil;        /* 100,00 DH au 2026-01-01, seuil 50,00 DH */
    Id alim, courses, hygiene, transport, carburant, salaire;
    Id carrefour, afriquia;
    Id clio;
} Jeu;

static Compte compte(const char *nom, TypeCompte type, long long solde, Date date, bool a_seuil,
                     long long seuil)
{
    Compte c;

    memset(&c, 0, sizeof c);
    texte_copier(c.nom, sizeof c.nom, nom);
    c.type = type;
    c.solde_initial_ct = solde;
    c.date_solde_initial = date;
    c.a_seuil = a_seuil;
    c.seuil_minimal_ct = seuil;
    return c;
}

static void preparer(Application *app, Jeu *j)
{
    RapportErreurs r;
    Compte c;

    memset(j, 0, sizeof *j);
    app_init(app, D(2026, 11, 13), true, ".");
    rapport_init(&r);
    c = compte("Banque", COMPTE_BANQUE, 500000, D(2026, 1, 1), false, 0);
    ref_compte_creer(app, &c, &j->banque, &r);
    c = compte("Espèces", COMPTE_ESPECES, 20000, D(2026, 1, 1), false, 0);
    ref_compte_creer(app, &c, &j->especes, &r);
    c = compte("Neuf", COMPTE_BANQUE, 100000, D(2026, 6, 1), false, 0);
    ref_compte_creer(app, &c, &j->neuf, &r);
    c = compte("Seuil", COMPTE_BANQUE, 10000, D(2026, 1, 1), true, 5000);
    ref_compte_creer(app, &c, &j->seuil, &r);
    ref_categorie_creer(app, "Alimentation", ID_AUCUN, CAT_DEPENSE, &j->alim, &r);
    ref_categorie_creer(app, "Courses", j->alim, CAT_DEPENSE, &j->courses, &r);
    ref_categorie_creer(app, "Hygiène", j->alim, CAT_DEPENSE, &j->hygiene, &r);
    ref_categorie_creer(app, "Transport", ID_AUCUN, CAT_DEPENSE, &j->transport, &r);
    ref_categorie_creer(app, "Carburant", j->transport, CAT_DEPENSE, &j->carburant, &r);
    ref_categorie_creer(app, "Salaire", ID_AUCUN, CAT_REVENU, &j->salaire, &r);
    ref_tiers_creer(app, "Carrefour", &j->carrefour, &r);
    ref_tiers_creer(app, "Afriquia", &j->afriquia, &r);
    ref_vehicule_creer(app, "Clio", &j->clio, &r);
    VERIFIER(rapport_nombre(&r) == 0);
    rapport_liberer(&r);
}

static OperationSaisie saisie(Nature n, Date d, const char *libelle, long long total, Id src, Id dst)
{
    OperationSaisie s;

    memset(&s, 0, sizeof s);
    s.nature = n;
    s.date_realisation = d;
    texte_copier(s.libelle, sizeof s.libelle, libelle);
    s.montant_total_ct = total;
    s.id_compte_source = src;
    s.id_compte_destination = dst;
    return s;
}

static LigneSaisie ligne(Id cat, long long montant, Id veh)
{
    LigneSaisie l;

    l.id_categorie = cat;
    l.id_vehicule = veh;
    l.montant_ct = montant;
    return l;
}

/* Création avec accord de seuil donné d'avance (le seuil est testé à part). */
static Resultat depense(Application *app, const char *lib, Date d, long long total, Id compte_id,
                        Id cat, Id *id)
{
    OperationSaisie s = saisie(NATURE_DEPENSE, d, lib, total, compte_id, ID_AUCUN);
    LigneSaisie l = ligne(cat, total, ID_AUCUN);
    RapportErreurs r;
    Resultat res;

    rapport_init(&r);
    res = op_creer(app, &s, &l, 1, true, id, NULL, &r);
    rapport_liberer(&r);
    return res;
}

static Resultat revenu(Application *app, const char *lib, Date d, long long total, Id compte_id,
                       Id cat, Id *id)
{
    OperationSaisie s = saisie(NATURE_REVENU, d, lib, total, ID_AUCUN, compte_id);
    LigneSaisie l = ligne(cat, total, ID_AUCUN);
    RapportErreurs r;
    Resultat res;

    rapport_init(&r);
    res = op_creer(app, &s, &l, 1, true, id, NULL, &r);
    rapport_liberer(&r);
    return res;
}

static bool contient(const RapportErreurs *r, const char *morceau)
{
    size_t i;

    for (i = 0; i < r->messages.taille; i++) {
        if (strstr(rapport_message(r, i), morceau) != NULL) {
            return true;
        }
    }
    return false;
}

/* Accesseurs qui n'échouent pas : une valeur neutre est renvoyée si l'élément manque, de sorte
   qu'un défaut du programme produise un ÉCHEC lisible plutôt qu'un plantage du test. */
static const Operation *op_sure(const Donnees *d, Id id)
{
    static Operation vide;
    const Operation *o = regles_operation(d, id);

    if (o == NULL) {
        memset(&vide, 0, sizeof vide);
        vide.montant_total_ct = -1;
        vide.id_tiers = -1;
        vide.id_compte_source = -1;
        vide.statut = STATUT_ANNULEE;
        vide.id_recurrence = -1;
        return &vide;
    }
    return o;
}

static const ResultatRecherche *res_sure(const Tableau *res, size_t i)
{
    static ResultatRecherche vide;

    if (i >= res->taille) {
        memset(&vide, 0, sizeof vide);
        vide.id = -1;
        vide.attribuable_ct = -1;
        vide.total_ct = -1;
        return &vide;
    }
    return ELEMENT(res, ResultatRecherche, i);
}

/* Insertions « brutes » qui contournent les contrôles : simulent des données chargées. */
static Id brute_op(Application *app, Nature n, Statut st, Date reel, long long total, Id src,
                   Id dst, Id rec, Date prevue)
{
    Operation o;

    memset(&o, 0, sizeof o);
    o.id = donnees_nouvel_id(&app->donnees, ENT_OPERATION);
    o.nature = n;
    o.statut = st;
    o.date_realisation = reel;
    texte_copier(o.libelle, sizeof o.libelle, "Brute");
    o.montant_total_ct = total;
    o.id_compte_source = src;
    o.id_compte_destination = dst;
    o.id_recurrence = rec;
    o.date_prevue_origine = prevue;
    tableau_ajouter(&app->donnees.operations, &o);
    return o.id;
}

static void brute_ligne(Application *app, Id op, Id cat, Id veh, long long montant)
{
    LigneOperation l;

    memset(&l, 0, sizeof l);
    l.id = donnees_nouvel_id(&app->donnees, ENT_LIGNE_OPERATION);
    l.id_operation = op;
    l.id_categorie = cat;
    l.id_vehicule = veh;
    l.montant_ct = montant;
    tableau_ajouter(&app->donnees.lignes_operations, &l);
}

/* Les données sont-elles refusées avec un message contenant « morceau » ? */
static bool refusees_avec(Application *app, const char *morceau)
{
    RapportErreurs r;
    bool ok;
    bool trouve;

    rapport_init(&r);
    ok = regles_valider(&app->donnees, &r);
    trouve = contient(&r, morceau);
    if (ok || !trouve) {
        rapport_afficher(&r, stdout);
    }
    rapport_liberer(&r);
    return !ok && trouve;
}

static bool valides(Application *app)
{
    RapportErreurs r;
    bool ok;

    rapport_init(&r);
    ok = regles_valider(&app->donnees, &r);
    if (!ok) {
        rapport_afficher(&r, stdout);
    }
    rapport_liberer(&r);
    return ok;
}

/* ------------------------------------------------------------------ */
/* V08 à V13 : le validateur refuse les données incohérentes           */
/* ------------------------------------------------------------------ */

void test_validateur_operations(void)
{
    Application app;
    Jeu j;
    Id op;

    printf("Validateur V08-V13 (opérations, ventilation, statuts, Q3, G10)\n");

    preparer(&app, &j);
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 3, 1), 12000, j.banque, ID_AUCUN,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 8000);
    brute_ligne(&app, op, j.hygiene, ID_AUCUN, 4000);
    VERIFIER(valides(&app));                 /* MLD §8 : 12 000 ct = 8 000 + 4 000 */
    app_liberer(&app);

    preparer(&app, &j);                      /* somme différente du total (MLD §8) */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 3, 1), 12000, j.banque, ID_AUCUN,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 8000);
    brute_ligne(&app, op, j.hygiene, ID_AUCUN, 3000);
    VERIFIER(refusees_avec(&app, "diffère du montant total"));
    app_liberer(&app);

    preparer(&app, &j);                      /* dépense avec un compte destination */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 3, 1), 1000, j.banque, j.especes,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "pas de compte destination"));
    app_liberer(&app);

    preparer(&app, &j);                      /* revenu avec un compte source */
    op = brute_op(&app, NATURE_REVENU, STATUT_CONFIRMEE, D(2026, 3, 1), 1000, j.banque, j.especes,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.salaire, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "pas de compte source"));
    app_liberer(&app);

    preparer(&app, &j);                      /* virement entre un compte et lui-même */
    brute_op(&app, NATURE_VIREMENT, STATUT_CONFIRMEE, D(2026, 3, 1), 1000, j.banque, j.banque,
             ID_AUCUN, date_aucune());
    VERIFIER(refusees_avec(&app, "doivent être différents"));
    app_liberer(&app);

    preparer(&app, &j);                      /* virement avec une ligne de catégorie */
    op = brute_op(&app, NATURE_VIREMENT, STATUT_CONFIRMEE, D(2026, 3, 1), 1000, j.banque, j.especes,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "ne comporte aucune ligne"));
    app_liberer(&app);

    preparer(&app, &j);                      /* dépense sans ligne */
    brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 3, 1), 1000, j.banque, ID_AUCUN,
             ID_AUCUN, date_aucune());
    VERIFIER(refusees_avec(&app, "au moins une ligne"));
    app_liberer(&app);

    preparer(&app, &j);                      /* ligne sur une catégorie non terminale */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 3, 1), 1000, j.banque, ID_AUCUN,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.alim, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "n'est pas terminale"));
    app_liberer(&app);

    preparer(&app, &j);                      /* dépense classée en catégorie de revenu */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 3, 1), 1000, j.banque, ID_AUCUN,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.salaire, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "n'est pas compatible"));
    app_liberer(&app);

    preparer(&app, &j);                      /* véhicule sur une ligne de revenu */
    op = brute_op(&app, NATURE_REVENU, STATUT_CONFIRMEE, D(2026, 3, 1), 1000, ID_AUCUN, j.banque,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.salaire, j.clio, 1000);
    VERIFIER(refusees_avec(&app, "véhicule n'est admis que sur une ligne de dépense"));
    app_liberer(&app);

    preparer(&app, &j);                      /* G2 : une opération manuelle est toujours confirmée */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_ANNULEE, D(2026, 3, 1), 1000, j.banque, ID_AUCUN,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "toujours confirmée"));
    app_liberer(&app);

    preparer(&app, &j);                      /* CONFIRMEE sans date réelle (Q7) */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, date_aucune(), 1000, j.banque, ID_AUCUN,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "date de réalisation est obligatoire"));
    app_liberer(&app);

    preparer(&app, &j);                      /* identifiant de récurrence sans date prévue */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, date_aucune(), 1000, j.banque,
                  ID_AUCUN, 7, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "ensemble ou absents ensemble"));
    app_liberer(&app);

    preparer(&app, &j);                      /* A_CONFIRMER avec date réelle */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, D(2026, 3, 1), 1000, j.banque,
                  ID_AUCUN, 7, D(2026, 3, 1));
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "n'a pas de date de réalisation"));
    app_liberer(&app);

    preparer(&app, &j);                      /* clé (récurrence, date prévue) en double, tous statuts */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, date_aucune(), 1000, j.banque,
                  ID_AUCUN, 7, D(2026, 3, 10));
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    op = brute_op(&app, NATURE_DEPENSE, STATUT_ANNULEE, date_aucune(), 1000, j.banque, ID_AUCUN,
                  7, D(2026, 3, 10));
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "a déjà une occurrence"));
    app_liberer(&app);

    preparer(&app, &j);                      /* montant au-delà de la limite */
    op = brute_op(&app, NATURE_REVENU, STATUT_CONFIRMEE, D(2026, 3, 1), MONTANT_MAX_CT + 1,
                  ID_AUCUN, j.banque, ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.salaire, ID_AUCUN, MONTANT_MAX_CT + 1);
    VERIFIER(refusees_avec(&app, "hors limites"));
    app_liberer(&app);

    preparer(&app, &j);                      /* Q3 : date antérieure à l'ouverture du compte */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 5, 31), 1000, j.neuf, ID_AUCUN,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(refusees_avec(&app, "précède la date du solde initial"));
    app_liberer(&app);

    preparer(&app, &j);                      /* G10 : solde de fin de journée négatif */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 3, 1), 20001, j.especes,
                  ID_AUCUN, ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 20001);
    VERIFIER(refusees_avec(&app, "solde de fin de journée"));
    app_liberer(&app);

    preparer(&app, &j);                      /* les opérations non confirmées n'ont aucun effet */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, date_aucune(), 99999999, j.especes,
                  ID_AUCUN, 7, D(2026, 3, 1));
    brute_ligne(&app, op, j.courses, ID_AUCUN, 99999999);
    op = brute_op(&app, NATURE_DEPENSE, STATUT_ANNULEE, D(2026, 3, 2), 99999999, j.especes,
                  ID_AUCUN, 7, D(2026, 3, 2));
    brute_ligne(&app, op, j.courses, ID_AUCUN, 99999999);
    {
        RapportErreurs r;
        bool ok;
        rapport_init(&r);
        ok = regles_valider(&app.donnees, &r);
        VERIFIER(!contient(&r, "solde de fin de journée"));   /* seule la récurrence 7 est inconnue */
        VERIFIER(!ok);                                        /* V02 : récurrence 7 inexistante */
        rapport_liberer(&r);
    }
    app_liberer(&app);

    preparer(&app, &j);                      /* B3 n'est pas une règle du validateur (chargement) */
    op = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2027, 1, 15), 1000, j.banque, ID_AUCUN,
                  ID_AUCUN, date_aucune());
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(valides(&app));
    app_liberer(&app);
}

/* ------------------------------------------------------------------ */
/* Soldes chronologiques, Q3, B3, G10, seuil                           */
/* ------------------------------------------------------------------ */

static size_t nb_ops(const Application *app)
{
    return app->donnees.operations.taille;
}

void test_soldes_et_seuils(void)
{
    Application app;
    Jeu j;
    RapportErreurs r;
    RapportErreurs w;
    OperationSaisie s;
    LigneSaisie l;
    Id op_revenu = ID_AUCUN;
    Id op_depense = ID_AUCUN;
    Id id = ID_AUCUN;
    long long solde;

    printf("Soldes de fin de journée (G10), Q3, B3 et seuil minimal\n");
    rapport_init(&r);
    rapport_init(&w);

    /* --- G10 : une dépense seule dépassant le solde est refusée, avec compte, date, manque --- */
    preparer(&app, &j);
    s = saisie(NATURE_DEPENSE, D(2026, 2, 1), "Trop cher", 25000, j.especes, ID_AUCUN);
    l = ligne(j.courses, 25000, ID_AUCUN);
    app.modifie = false;
    VERIFIER(op_creer(&app, &s, &l, 1, true, &id, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "Espèces") && contient(&r, "2026-02-01") && contient(&r, "50,00 DH"));
    VERIFIER(nb_ops(&app) == 0 && !app.modifie && app.donnees.prochain_id[ENT_OPERATION] == 1
             && app.donnees.prochain_id[ENT_LIGNE_OPERATION] == 1);   /* rien n'a changé, compteurs compris */
    rapport_vider(&r);

    /* --- Même jour : le revenu et la dépense sont agrégés (200 + 100 - 250 = 50 >= 0) --- */
    VERIFIER(revenu(&app, "Prime", D(2026, 2, 1), 10000, j.especes, j.salaire, &op_revenu) == RES_OK);
    VERIFIER(depense(&app, "Gros achat", D(2026, 2, 1), 25000, j.especes, j.courses, &op_depense)
             == RES_OK);
    VERIFIER(solde_compte_a_date(&app.donnees, j.especes, D(2026, 2, 1), &solde) && solde == 5000);
    VERIFIER(solde_compte_a_date(&app.donnees, j.especes, D(2026, 1, 31), &solde) && solde == 20000);

    /* --- Supprimer le revenu rendrait le solde négatif : refusé, rien n'est modifié --- */
    {
        size_t avant = nb_ops(&app);
        long modifs = app.nb_modifications;
        VERIFIER(op_supprimer(&app, op_revenu, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "solde de fin de journée"));
        VERIFIER(nb_ops(&app) == avant && app.nb_modifications == modifs
                 && regles_operation(&app.donnees, op_revenu) != NULL);
        rapport_vider(&r);
    }
    /* --- Reporter le revenu au lendemain : le solde du 2026-02-01 devient négatif : refusé --- */
    s = saisie(NATURE_REVENU, D(2026, 2, 2), "Prime", 10000, ID_AUCUN, j.especes);
    l = ligne(j.salaire, 10000, ID_AUCUN);
    VERIFIER(op_corriger(&app, op_revenu, &s, &l, 1, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "2026-02-01"));
    rapport_vider(&r);
    /* --- Réduire le revenu à 49,99 DH : 200 + 49,99 - 250 = -0,01 DH, refusé (manque 0,01 DH) --- */
    s = saisie(NATURE_REVENU, D(2026, 2, 1), "Prime", 4999, ID_AUCUN, j.especes);
    l = ligne(j.salaire, 4999, ID_AUCUN);
    VERIFIER(op_corriger(&app, op_revenu, &s, &l, 1, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "0,01 DH"));
    VERIFIER(op_sure(&app.donnees, op_revenu)->montant_total_ct == 10000);
    rapport_vider(&r);
    /* --- À 50,00 DH : 200 + 50 - 250 = 0, un solde nul est accepté --- */
    s = saisie(NATURE_REVENU, D(2026, 2, 1), "Prime", 5000, ID_AUCUN, j.especes);
    l = ligne(j.salaire, 5000, ID_AUCUN);
    VERIFIER(op_corriger(&app, op_revenu, &s, &l, 1, true, NULL, &r) == RES_OK);
    VERIFIER(solde_compte_final(&app.donnees, j.especes, &solde) && solde == 0);
    rapport_vider(&r);
    app_liberer(&app);

    /* --- Solde nul accepté, négatif refusé (frontière exacte) --- */
    preparer(&app, &j);
    VERIFIER(depense(&app, "Tout le solde", D(2026, 2, 1), 20000, j.especes, j.courses, &id) == RES_OK);
    VERIFIER(depense(&app, "Un centime de trop", D(2026, 2, 1), 1, j.especes, j.courses, &id)
             == RES_ERR_VALIDATION);
    VERIFIER(solde_compte_final(&app.donnees, j.especes, &solde) && solde == 0);
    app_liberer(&app);

    /* --- Q3 : l'ouverture du compte « Neuf » est le 2026-06-01 --- */
    preparer(&app, &j);
    s = saisie(NATURE_DEPENSE, D(2026, 5, 31), "Avant l'ouverture", 1000, j.neuf, ID_AUCUN);
    l = ligne(j.courses, 1000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, &l, 1, true, &id, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "précède la date du solde initial") && contient(&r, "Neuf"));
    rapport_vider(&r);
    VERIFIER(depense(&app, "Le jour de l'ouverture", D(2026, 6, 1), 1000, j.neuf, j.courses, &op_depense)
             == RES_OK);
    /* Virement : les deux comptes sont contrôlés (la banque existe depuis janvier, « Neuf » non). */
    s = saisie(NATURE_VIREMENT, D(2026, 5, 15), "Virement trop tôt", 5000, j.banque, j.neuf);
    VERIFIER(op_creer(&app, &s, NULL, 0, true, &id, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "Neuf"));
    rapport_vider(&r);
    s = saisie(NATURE_VIREMENT, D(2026, 6, 1), "Virement le jour J", 5000, j.banque, j.neuf);
    VERIFIER(op_creer(&app, &s, NULL, 0, true, &id, NULL, &r) == RES_OK);
    /* Correction de la date vers une date antérieure à l'ouverture : refus. */
    s = saisie(NATURE_DEPENSE, D(2026, 5, 31), "Le jour de l'ouverture", 1000, j.neuf, ID_AUCUN);
    l = ligne(j.courses, 1000, ID_AUCUN);
    VERIFIER(op_corriger(&app, op_depense, &s, &l, 1, true, NULL, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    /* Changer la date du solde initial de « Neuf » au 2026-06-02 : l'opération du 06-01 devient interdite. */
    {
        Compte c = *regles_compte(&app.donnees, j.neuf);
        c.date_solde_initial = D(2026, 6, 2);
        VERIFIER(ref_compte_modifier(&app, &c, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "précède la date du solde initial"));
        rapport_vider(&r);
    }
    app_liberer(&app);

    /* --- B3 : date réelle future refusée à la saisie, pas au chargement --- */
    preparer(&app, &j);                          /* date de référence : 2026-11-13 */
    s = saisie(NATURE_DEPENSE, D(2026, 11, 14), "Demain", 1000, j.banque, ID_AUCUN);
    l = ligne(j.courses, 1000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, &l, 1, true, &id, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "postérieure à la date de référence"));
    rapport_vider(&r);
    VERIFIER(depense(&app, "Aujourd'hui", D(2026, 11, 13), 1000, j.banque, j.courses, &op_depense)
             == RES_OK);
    s = saisie(NATURE_DEPENSE, D(2026, 11, 14), "Aujourd'hui", 1000, j.banque, ID_AUCUN);
    VERIFIER(op_corriger(&app, op_depense, &s, &l, 1, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "postérieure à la date de référence"));
    rapport_vider(&r);
    {
        /* Les données enregistrées sont rechargées sous une date simulée antérieure : acceptées. */
        Application autre;
        RapportErreurs infos;
        char p[CHEMIN_MAX + 64];

        rapport_init(&infos);
        strcpy(app.dossier_donnees, dossier_test);
        VERIFIER(persistance_enregistrer(&app, &infos) == RES_OK);
        app_init(&autre, D(2026, 11, 1), true, dossier_test);
        VERIFIER(persistance_charger(&autre, &infos) == CHARGEMENT_OK);
        VERIFIER(nb_ops(&autre) == 1
                 && date_comparer(ELEMENT(&autre.donnees.operations, Operation, 0)->date_realisation,
                                  D(2026, 11, 13)) == 0);
        app_liberer(&autre);
        jeu_supprimer(dossier_test, "A_");
        jeu_supprimer(dossier_test, "B_");
        snprintf(p, sizeof p, "%s/A_manifest.tmp", dossier_test);
        remove(p);
        rapport_liberer(&infos);
    }
    app_liberer(&app);

    /* --- Seuil minimal : simple avertissement, jamais une autorisation de solde négatif --- */
    preparer(&app, &j);                          /* « Seuil » : 100,00 DH, seuil 50,00 DH */
    VERIFIER(depense(&app, "Petit achat", D(2026, 2, 1), 3000, j.seuil, j.courses, &id) == RES_OK);
    s = saisie(NATURE_DEPENSE, D(2026, 2, 2), "Passe sous le seuil", 3000, j.seuil, ID_AUCUN);
    l = ligne(j.courses, 3000, ID_AUCUN);
    {
        size_t avant = nb_ops(&app);
        long modifs = app.nb_modifications;
        VERIFIER(op_creer(&app, &s, &l, 1, false, &id, &w, &r) == RES_CONFIRMATION_REQUISE);
        VERIFIER(contient(&w, "seuil minimal") && contient(&w, "40,00 DH")
                 && contient(&w, "2026-02-02"));
        VERIFIER(nb_ops(&app) == avant && app.nb_modifications == modifs
                 && rapport_nombre(&r) == 0);
        VERIFIER(op_creer(&app, &s, &l, 1, true, &id, NULL, &r) == RES_OK);     /* accord donné */
        VERIFIER(nb_ops(&app) == avant + 1);
    }
    rapport_vider(&w);
    /* Le seuil n'autorise jamais un solde négatif, même avec accord. */
    s = saisie(NATURE_DEPENSE, D(2026, 2, 3), "Négatif", 5000, j.seuil, ID_AUCUN);
    l = ligne(j.courses, 5000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, &l, 1, true, &id, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "solde de fin de journée"));
    rapport_vider(&r);
    /* Un revenu relève le solde : pas d'avertissement, même sous le seuil. */
    VERIFIER(solde_compte_final(&app.donnees, j.seuil, &solde) && solde == 4000);
    s = saisie(NATURE_REVENU, D(2026, 2, 4), "Petit revenu", 500, ID_AUCUN, j.seuil);
    l = ligne(j.salaire, 500, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, &l, 1, false, &id, &w, &r) == RES_OK);
    VERIFIER(rapport_nombre(&w) == 0);
    /* Une nouvelle dépense qui abaisse encore le solde déjà sous le seuil avertit à nouveau. */
    s = saisie(NATURE_DEPENSE, D(2026, 2, 5), "Encore un peu", 100, j.seuil, ID_AUCUN);
    l = ligne(j.courses, 100, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, &l, 1, false, &id, &w, &r) == RES_CONFIRMATION_REQUISE);
    rapport_vider(&w);
    /* Modifier le solde initial à la baisse : avertissement ; sans conséquence sur les autres comptes. */
    {
        Compte c = *regles_compte(&app.donnees, j.banque);
        c.a_seuil = true;
        c.seuil_minimal_ct = 100000;
        VERIFIER(ref_compte_modifier(&app, &c, false, &w, &r) == RES_OK);   /* relever un seuil n'abaisse rien */
        VERIFIER(rapport_nombre(&w) == 0);
        c.solde_initial_ct = 400000;                                         /* 4 000 DH < seuil 1 000 DH ? non */
        VERIFIER(ref_compte_modifier(&app, &c, false, &w, &r) == RES_OK);
        c.solde_initial_ct = 50000;                                          /* 500 DH < seuil 1 000 DH : avertir */
        VERIFIER(ref_compte_modifier(&app, &c, false, &w, &r) == RES_CONFIRMATION_REQUISE);
        VERIFIER(contient(&w, "Banque"));
        VERIFIER(regles_compte(&app.donnees, j.banque)->solde_initial_ct == 400000);
    }
    rapport_vider(&w);
    app_liberer(&app);

    /* --- Seuil et suppression : retirer un revenu qui garde le solde au-dessus du seuil --- */
    preparer(&app, &j);
    VERIFIER(revenu(&app, "Revenu", D(2026, 2, 1), 10000, j.seuil, j.salaire, &op_revenu) == RES_OK);
    VERIFIER(depense(&app, "Dépense", D(2026, 2, 2), 13000, j.seuil, j.courses, &id) == RES_OK);
    VERIFIER(solde_compte_final(&app.donnees, j.seuil, &solde) && solde == 7000);
    /* Supprimer le revenu rendrait le solde négatif (100 - 130) : refus, pas un simple avertissement. */
    VERIFIER(op_supprimer(&app, op_revenu, false, &w, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    /* Réduire le revenu à 85,00 DH : solde final 55 DH >= seuil : aucun avertissement. */
    s = saisie(NATURE_REVENU, D(2026, 2, 1), "Revenu", 8500, ID_AUCUN, j.seuil);
    l = ligne(j.salaire, 8500, ID_AUCUN);
    VERIFIER(op_corriger(&app, op_revenu, &s, &l, 1, false, &w, &r) == RES_OK);
    VERIFIER(rapport_nombre(&w) == 0);
    /* À 75,00 DH : solde 45 DH, sous le seuil : avertissement, état inchangé tant qu'il n'est pas accepté. */
    s = saisie(NATURE_REVENU, D(2026, 2, 1), "Revenu", 7500, ID_AUCUN, j.seuil);
    l = ligne(j.salaire, 7500, ID_AUCUN);
    VERIFIER(op_corriger(&app, op_revenu, &s, &l, 1, false, &w, &r) == RES_CONFIRMATION_REQUISE);
    VERIFIER(op_sure(&app.donnees, op_revenu)->montant_total_ct == 8500);
    VERIFIER(op_corriger(&app, op_revenu, &s, &l, 1, true, NULL, &r) == RES_OK);
    VERIFIER(op_sure(&app.donnees, op_revenu)->montant_total_ct == 7500);
    rapport_vider(&w);
    app_liberer(&app);

    /* --- Soldes affichés : date de référence et solde après toutes les opérations --- */
    preparer(&app, &j);
    VERIFIER(depense(&app, "D1", D(2026, 3, 1), 1000, j.banque, j.courses, &id) == RES_OK);
    VERIFIER(revenu(&app, "R1", D(2026, 3, 5), 20000, j.banque, j.salaire, &id) == RES_OK);
    s = saisie(NATURE_VIREMENT, D(2026, 3, 5), "V1", 50000, j.banque, j.especes);
    VERIFIER(op_creer(&app, &s, NULL, 0, true, &id, NULL, &r) == RES_OK);
    VERIFIER(solde_compte_a_date(&app.donnees, j.banque, D(2026, 3, 4), &solde) && solde == 499000);
    /* 5 000 - 10 + 200 - 500 = 4 690 DH ; virement : Espèces 200 + 500 = 700 DH */
    VERIFIER(solde_compte_a_date(&app.donnees, j.banque, D(2026, 3, 5), &solde) && solde == 469000);
    VERIFIER(solde_compte_a_date(&app.donnees, j.especes, D(2026, 3, 5), &solde) && solde == 70000);
    VERIFIER(!solde_compte_a_date(&app.donnees, j.banque, D(2025, 12, 31), &solde));  /* avant l'ouverture */
    VERIFIER(!solde_compte_a_date(&app.donnees, 999, D(2026, 3, 5), &solde));
    VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 469000);
    app_liberer(&app);

    rapport_liberer(&w);
    rapport_liberer(&r);
}

/* ------------------------------------------------------------------ */
/* Opérations manuelles                                                */
/* ------------------------------------------------------------------ */

void test_operations_manuelles(void)
{
    Application app;
    Jeu j;
    RapportErreurs r;
    OperationSaisie s;
    LigneSaisie l[3];
    Id id = ID_AUCUN;
    Id id2 = ID_AUCUN;
    long long solde;
    size_t lignes_avant;

    printf("Opérations manuelles (création, ventilation, correction, suppression)\n");
    rapport_init(&r);

    preparer(&app, &j);
    /* MLD §8 : 12 000 ct en deux lignes de 8 000 et 4 000 : acceptée, créée CONFIRMEE (G2). */
    s = saisie(NATURE_DEPENSE, D(2026, 3, 1), "Courses du mois", 12000, j.banque, ID_AUCUN);
    s.id_tiers = j.carrefour;
    l[0] = ligne(j.courses, 8000, ID_AUCUN);
    l[1] = ligne(j.hygiene, 4000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 2, true, &id, NULL, &r) == RES_OK);
    VERIFIER(nb_ops(&app) == 1 && app.donnees.lignes_operations.taille == 2);
    VERIFIER(op_sure(&app.donnees, id)->statut == STATUT_CONFIRMEE
             && op_sure(&app.donnees, id)->id_recurrence == ID_AUCUN
             && op_sure(&app.donnees, id)->id_tiers == j.carrefour);
    VERIFIER(op_nombre_lignes(&app.donnees, id) == 2);
    VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 488000);

    /* Lignes 8 000 + 3 000 pour 12 000 : refusée, avec le reste à répartir. */
    {
        size_t avant = nb_ops(&app);
        long modifs = app.nb_modifications;
        Id compteur = app.donnees.prochain_id[ENT_OPERATION];
        l[1].montant_ct = 3000;
        VERIFIER(op_creer(&app, &s, l, 2, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "diffère du montant total") && contient(&r, "10,00 DH"));
        VERIFIER(nb_ops(&app) == avant && app.nb_modifications == modifs
                 && app.donnees.prochain_id[ENT_OPERATION] == compteur);
        rapport_vider(&r);
    }
    /* Pré-contrôles : libellé vide, montant nul, nature/comptes, ligne non terminale, mauvais type. */
    l[1].montant_ct = 4000;
    s.libelle[0] = '\0';
    VERIFIER(op_creer(&app, &s, l, 2, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "Libellé"));
    rapport_vider(&r);
    s = saisie(NATURE_DEPENSE, D(2026, 3, 1), "x;y", 1000, j.banque, ID_AUCUN);   /* « ; » interdit (T1) */
    l[0] = ligne(j.courses, 1000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    s = saisie(NATURE_DEPENSE, D(2026, 3, 1), "Zéro", 0, j.banque, ID_AUCUN);
    l[0] = ligne(j.courses, 0, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    s = saisie(NATURE_DEPENSE, D(2026, 3, 1), "Sans compte", 1000, ID_AUCUN, ID_AUCUN);
    l[0] = ligne(j.courses, 1000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "compte source"));
    rapport_vider(&r);
    s = saisie(NATURE_VIREMENT, D(2026, 3, 1), "Même compte", 1000, j.banque, j.banque);
    VERIFIER(op_creer(&app, &s, NULL, 0, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "distincts"));
    rapport_vider(&r);
    s = saisie(NATURE_VIREMENT, D(2026, 3, 1), "Virement ventilé", 1000, j.banque, j.especes);
    l[0] = ligne(j.courses, 1000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "aucune ligne de catégorie"));
    rapport_vider(&r);
    s = saisie(NATURE_DEPENSE, D(2026, 3, 1), "Parent", 1000, j.banque, ID_AUCUN);
    l[0] = ligne(j.alim, 1000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "n'est pas terminale"));
    rapport_vider(&r);
    l[0] = ligne(j.salaire, 1000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "ne convient pas"));
    rapport_vider(&r);
    l[0] = ligne(j.courses, 1000, 99);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "véhicule 99 introuvable"));
    rapport_vider(&r);
    s = saisie(NATURE_REVENU, D(2026, 3, 1), "Revenu avec véhicule", 1000, ID_AUCUN, j.banque);
    l[0] = ligne(j.salaire, 1000, j.clio);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "véhicule n'est admis que sur une dépense"));
    rapport_vider(&r);
    s = saisie(NATURE_DEPENSE, D(2026, 3, 1), "Tiers inconnu", 1000, j.banque, ID_AUCUN);
    s.id_tiers = 99;
    l[0] = ligne(j.courses, 1000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 1, true, &id2, NULL, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    VERIFIER(nb_ops(&app) == 1);

    /* Carburant + courses : seule la ligne carburant est rattachée au véhicule (Q2). */
    s = saisie(NATURE_DEPENSE, D(2026, 3, 2), "Station + épicerie", 10000, j.banque, ID_AUCUN);
    l[0] = ligne(j.carburant, 6000, j.clio);
    l[1] = ligne(j.courses, 4000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 2, true, &id2, NULL, &r) == RES_OK);
    VERIFIER(regles_usages_vehicule(&app.donnees, j.clio).lignes_operations == 1);

    /* Virement MLD §8 : 50 000 ct Banque -> Espèces, aucune ligne, ni dépense ni revenu. */
    s = saisie(NATURE_VIREMENT, D(2026, 3, 3), "Retrait", 50000, j.banque, j.especes);
    VERIFIER(op_creer(&app, &s, NULL, 0, true, &id2, NULL, &r) == RES_OK);
    VERIFIER(op_nombre_lignes(&app.donnees, id2) == 0);
    VERIFIER(solde_compte_final(&app.donnees, j.especes, &solde) && solde == 70000);

    /* Identifiants monotones : supprimer la dernière opération ne libère pas son identifiant. */
    VERIFIER(op_supprimer(&app, id2, true, NULL, &r) == RES_OK);
    VERIFIER(regles_operation(&app.donnees, id2) == NULL);
    s = saisie(NATURE_VIREMENT, D(2026, 3, 3), "Retrait bis", 50000, j.banque, j.especes);
    {
        Id id3 = ID_AUCUN;
        VERIFIER(op_creer(&app, &s, NULL, 0, true, &id3, NULL, &r) == RES_OK);
        VERIFIER(id3 > id2);
    }

    /* Correction : date, libellé, montant, ventilation, comptes ; la nature ne change pas. */
    s = saisie(NATURE_DEPENSE, D(2026, 3, 4), "Courses corrigées", 15000, j.especes, ID_AUCUN);
    l[0] = ligne(j.courses, 9000, ID_AUCUN);
    l[1] = ligne(j.hygiene, 6000, ID_AUCUN);
    VERIFIER(op_corriger(&app, id, &s, l, 2, true, NULL, &r) == RES_OK);
    VERIFIER(op_sure(&app.donnees, id)->montant_total_ct == 15000
             && op_sure(&app.donnees, id)->id_compte_source == j.especes
             && date_comparer(op_sure(&app.donnees, id)->date_realisation, D(2026, 3, 4)) == 0
             && op_nombre_lignes(&app.donnees, id) == 2);
    {
        LigneSaisie copie[4];
        VERIFIER(op_lignes_copier(&app.donnees, id, copie, 4) == 2
                 && copie[0].montant_ct + copie[1].montant_ct == 15000);
    }
    s.nature = NATURE_REVENU;                                      /* changement de nature interdit */
    s.id_compte_source = ID_AUCUN;
    s.id_compte_destination = j.especes;
    l[0] = ligne(j.salaire, 15000, ID_AUCUN);
    VERIFIER(op_corriger(&app, id, &s, l, 1, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "nature"));
    rapport_vider(&r);
    VERIFIER(op_corriger(&app, 999, &s, l, 1, true, NULL, &r) == RES_ERR_INTROUVABLE);
    rapport_vider(&r);

    /* Suppression physique : l'opération et toutes ses lignes disparaissent, l'état reste valide. */
    lignes_avant = app.donnees.lignes_operations.taille;
    VERIFIER(op_supprimer(&app, id, true, NULL, &r) == RES_OK);
    VERIFIER(app.donnees.lignes_operations.taille == lignes_avant - 2);
    VERIFIER(op_nombre_lignes(&app.donnees, id) == 0);
    {
        RapportErreurs v;
        rapport_init(&v);
        VERIFIER(regles_valider(&app.donnees, &v));
        rapport_liberer(&v);
    }
    VERIFIER(op_supprimer(&app, 999, true, NULL, &r) == RES_ERR_INTROUVABLE);
    rapport_vider(&r);
    app_liberer(&app);

    /* Occurrences : jamais de suppression physique ; correction seulement avant confirmation (G3). */
    preparer(&app, &j);
    {
        Id a_confirmer = brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, date_aucune(), 5000,
                                  j.banque, ID_AUCUN, 7, D(2026, 3, 10));
        Id confirmee = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 3, 11), 5000,
                                j.banque, ID_AUCUN, 7, D(2026, 3, 11));
        Id annulee = brute_op(&app, NATURE_DEPENSE, STATUT_ANNULEE, date_aucune(), 5000, j.banque,
                              ID_AUCUN, 7, D(2026, 3, 12));
        size_t avant;

        brute_ligne(&app, a_confirmer, j.courses, ID_AUCUN, 5000);
        brute_ligne(&app, confirmee, j.courses, ID_AUCUN, 5000);
        brute_ligne(&app, annulee, j.courses, ID_AUCUN, 5000);
        /* La récurrence 7 est inconnue pour le validateur : les actions sont donc refusées pour V02
           — on vérifie ici uniquement la règle propre aux occurrences, détectée avant toute copie. */
        avant = nb_ops(&app);
        VERIFIER(op_supprimer(&app, a_confirmer, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "jamais supprimée physiquement"));
        rapport_vider(&r);
        VERIFIER(op_supprimer(&app, confirmee, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(op_supprimer(&app, annulee, true, NULL, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
        s = saisie(NATURE_DEPENSE, D(2026, 3, 11), "Modif", 4000, j.banque, ID_AUCUN);
        l[0] = ligne(j.courses, 4000, ID_AUCUN);
        VERIFIER(op_corriger(&app, confirmee, &s, l, 1, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "plus modifiables"));
        rapport_vider(&r);
        VERIFIER(op_corriger(&app, annulee, &s, l, 1, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "plus modifiables"));
        rapport_vider(&r);
        VERIFIER(nb_ops(&app) == avant);
    }
    app_liberer(&app);

    /* Aller-retour disque : opérations, lignes, véhicule, tiers et compteurs conservés. */
    preparer(&app, &j);
    s = saisie(NATURE_DEPENSE, D(2026, 3, 2), "Station + épicerie", 10000, j.banque, ID_AUCUN);
    s.id_tiers = j.afriquia;
    l[0] = ligne(j.carburant, 6000, j.clio);
    l[1] = ligne(j.courses, 4000, ID_AUCUN);
    VERIFIER(op_creer(&app, &s, l, 2, true, &id, NULL, &r) == RES_OK);
    s = saisie(NATURE_VIREMENT, D(2026, 3, 3), "Retrait", 50000, j.banque, j.especes);
    VERIFIER(op_creer(&app, &s, NULL, 0, true, &id2, NULL, &r) == RES_OK);
    {
        Application autre;
        RapportErreurs infos;
        char p[CHEMIN_MAX + 64];

        rapport_init(&infos);
        strcpy(app.dossier_donnees, dossier_test);
        VERIFIER(persistance_enregistrer(&app, &infos) == RES_OK);
        app_init(&autre, D(2026, 11, 13), true, dossier_test);
        VERIFIER(persistance_charger(&autre, &infos) == CHARGEMENT_OK);
        VERIFIER(nb_ops(&autre) == 2 && autre.donnees.lignes_operations.taille == 2);
        VERIFIER(autre.donnees.prochain_id[ENT_OPERATION] == app.donnees.prochain_id[ENT_OPERATION]
                 && autre.donnees.prochain_id[ENT_LIGNE_OPERATION]
                        == app.donnees.prochain_id[ENT_LIGNE_OPERATION]);
        VERIFIER(regles_operation(&autre.donnees, id) != NULL
                 && op_sure(&autre.donnees, id)->id_tiers == j.afriquia
                 && op_sure(&autre.donnees, id)->montant_total_ct == 10000
                 && op_nombre_lignes(&autre.donnees, id) == 2
                 && regles_usages_vehicule(&autre.donnees, j.clio).lignes_operations == 1);
        VERIFIER(strcmp(op_sure(&autre.donnees, id)->libelle, "Station + épicerie") == 0);
        app_liberer(&autre);
        jeu_supprimer(dossier_test, "A_");
        jeu_supprimer(dossier_test, "B_");
        snprintf(p, sizeof p, "%s/A_manifest.tmp", dossier_test);
        remove(p);
        snprintf(p, sizeof p, "%s/%s", dossier_test, NOM_VERROU);
        remove(p);
        rapport_liberer(&infos);
    }
    app_liberer(&app);
    rapport_liberer(&r);
}

/* ------------------------------------------------------------------ */
/* Recherche, tri, statistiques                                        */
/* ------------------------------------------------------------------ */

static Id ajouter(Application *app, Jeu *j, Nature n, Date d, const char *lib, long long total,
                  Id compte_id, const LigneSaisie *lignes, size_t nb, Id tiers)
{
    OperationSaisie s;
    RapportErreurs r;
    Id id = ID_AUCUN;

    (void)j;
    s = saisie(n, d, lib, total, n == NATURE_REVENU ? ID_AUCUN : compte_id,
               n == NATURE_REVENU ? compte_id : ID_AUCUN);
    s.id_tiers = tiers;
    rapport_init(&r);
    VERIFIER(op_creer(app, &s, lignes, nb, true, &id, NULL, &r) == RES_OK);
    rapport_liberer(&r);
    return id;
}

/* Les identifiants des résultats, dans l'ordre, sont-ils exactement ceux attendus ? */
static bool ordre_egal(const Tableau *res, const Id *attendus, size_t n)
{
    size_t i;

    if (res->taille != n) {
        return false;
    }
    for (i = 0; i < n; i++) {
        if (ELEMENT(res, ResultatRecherche, i)->id != attendus[i]) {
            return false;
        }
    }
    return true;
}

static bool recherche(Application *app, const FiltreOperations *f, Tableau *res)
{
    return analyses_rechercher(&app->donnees, f, res);
}

void test_analyses(void)
{
    Application app;
    Jeu j;
    Tableau res;
    FiltreOperations f;
    LigneSaisie l[3];
    RapportErreurs r;
    StatistiquesDepenses st;
    Id o[10];

    printf("Recherche (F03), tri (F04) et statistiques (F06)\n");
    rapport_init(&r);
    tableau_init(&res, sizeof(ResultatRecherche));
    preparer(&app, &j);
    app.date_reference = D(2026, 12, 31);

    l[0] = ligne(j.courses, 15000, ID_AUCUN);
    l[1] = ligne(j.hygiene, 5000, ID_AUCUN);
    o[1] = ajouter(&app, &j, NATURE_DEPENSE, D(2026, 11, 1), "Courses Marjane", 20000, j.banque, l, 2,
                   j.carrefour);
    l[0] = ligne(j.carburant, 30000, j.clio);
    o[2] = ajouter(&app, &j, NATURE_DEPENSE, D(2026, 11, 5), "Carburant", 30000, j.banque, l, 1, ID_AUCUN);
    l[0] = ligne(j.courses, 4000, ID_AUCUN);
    o[3] = ajouter(&app, &j, NATURE_DEPENSE, D(2026, 11, 5), "Café", 4000, j.banque, l, 1, ID_AUCUN);
    l[0] = ligne(j.salaire, 400000, ID_AUCUN);
    o[4] = ajouter(&app, &j, NATURE_REVENU, D(2026, 11, 12), "Salaire novembre", 400000, j.banque, l,
                   1, ID_AUCUN);
    {
        OperationSaisie s = saisie(NATURE_VIREMENT, D(2026, 11, 10), "Retrait", 50000, j.banque,
                                   j.especes);
        VERIFIER(op_creer(&app, &s, NULL, 0, true, &o[5], NULL, &r) == RES_OK);
    }
    l[0] = ligne(j.courses, 10000, ID_AUCUN);
    o[6] = ajouter(&app, &j, NATURE_DEPENSE, D(2026, 10, 31), "courses MARJANE", 10000, j.banque, l, 1,
                   ID_AUCUN);
    l[0] = ligne(j.hygiene, 6000, ID_AUCUN);
    o[7] = ajouter(&app, &j, NATURE_DEPENSE, D(2026, 11, 30), "Pharmacie", 6000, j.banque, l, 1, ID_AUCUN);
    l[0] = ligne(j.courses, 2500, ID_AUCUN);
    o[8] = ajouter(&app, &j, NATURE_DEPENSE, D(2026, 12, 1), "Épicerie fine", 2500, j.banque, l, 1,
                   ID_AUCUN);
    o[9] = ajouter(&app, &j, NATURE_DEPENSE, D(2026, 12, 2), "Test A", 2500, j.banque, l, 1, ID_AUCUN);
    VERIFIER(o[1] == 1 && o[9] == 9);                 /* identifiants attribués dans l'ordre */

    /* Sans filtre : 9 opérations (toutes natures, tous statuts). */
    filtre_init(&f);
    VERIFIER(recherche(&app, &f, &res) && res.taille == 9);

    /* Nature. */
    f.par_nature = true;
    f.nature = NATURE_DEPENSE;
    VERIFIER(recherche(&app, &f, &res) && res.taille == 7);
    f.nature = NATURE_REVENU;
    VERIFIER(recherche(&app, &f, &res) && res.taille == 1 && res_sure(&res, 0)->id == o[4]);
    f.nature = NATURE_VIREMENT;
    VERIFIER(recherche(&app, &f, &res) && res.taille == 1 && res_sure(&res, 0)->id == o[5]);

    /* Période du 2026-11-01 au 2026-11-30, bornes incluses : exclut le 10-31 et le 12-01. */
    filtre_init(&f);
    f.par_periode = true;
    f.debut = D(2026, 11, 1);
    f.fin = D(2026, 11, 30);
    {
        const Id attendus[] = {o[1], o[2], o[3], o[4], o[5], o[7]};
        VERIFIER(recherche(&app, &f, &res) && res.taille == 6);
        analyses_trier(&res, TRI_DATE, true);
        VERIFIER(ordre_egal(&res, (const Id[]){o[1], o[2], o[3], o[5], o[4], o[7]}, 6));
        (void)attendus;
    }

    /* Catégorie avec descendants (Alimentation) : montant attribuable, virements et revenus exclus. */
    filtre_init(&f);
    f.par_periode = true;
    f.debut = D(2026, 11, 1);
    f.fin = D(2026, 11, 30);
    f.id_categorie = j.alim;
    VERIFIER(recherche(&app, &f, &res) && res.taille == 3);
    analyses_trier(&res, TRI_DATE, true);
    VERIFIER(ordre_egal(&res, (const Id[]){o[1], o[3], o[7]}, 3));
    VERIFIER(res_sure(&res, 0)->attribuable_ct == 20000    /* 150 + 50 */
             && res_sure(&res, 0)->total_ct == 20000
             && res_sure(&res, 1)->attribuable_ct == 4000
             && res_sure(&res, 2)->attribuable_ct == 6000);
    f.id_categorie = j.hygiene;                        /* feuille : seule la part Hygiène */
    VERIFIER(recherche(&app, &f, &res) && res.taille == 2);
    analyses_trier(&res, TRI_DATE, true);
    VERIFIER(ordre_egal(&res, (const Id[]){o[1], o[7]}, 2));
    VERIFIER(res_sure(&res, 0)->attribuable_ct == 5000
             && res_sure(&res, 0)->total_ct == 20000);   /* 50 DH sur 200 DH */
    f.id_categorie = j.courses;
    VERIFIER(recherche(&app, &f, &res) && res.taille == 2);
    analyses_trier(&res, TRI_DATE, true);
    VERIFIER(res_sure(&res, 0)->attribuable_ct == 15000);
    f.id_categorie = j.salaire;                        /* un revenu a ses lignes dans le sous-arbre REVENU */
    VERIFIER(recherche(&app, &f, &res) && res.taille == 1);

    /* Texte du libellé : sans distinction de casse ni d'accent majuscule ; compte ; tiers. */
    filtre_init(&f);
    texte_copier(f.texte, sizeof f.texte, "marjane");
    VERIFIER(recherche(&app, &f, &res) && res.taille == 2);
    texte_copier(f.texte, sizeof f.texte, "ÉPICERIE");
    VERIFIER(recherche(&app, &f, &res) && res.taille == 1 && res_sure(&res, 0)->id == o[8]);
    filtre_init(&f);
    f.id_compte = j.especes;                           /* destination d'un virement */
    VERIFIER(recherche(&app, &f, &res) && res.taille == 1 && res_sure(&res, 0)->id == o[5]);
    f.id_compte = j.banque;
    VERIFIER(recherche(&app, &f, &res) && res.taille == 9);
    filtre_init(&f);
    f.id_tiers = j.carrefour;
    VERIFIER(recherche(&app, &f, &res) && res.taille == 1 && res_sure(&res, 0)->id == o[1]);
    filtre_init(&f);                                   /* aucun résultat : liste vide, pas d'erreur */
    texte_copier(f.texte, sizeof f.texte, "introuvable");
    VERIFIER(recherche(&app, &f, &res) && res.taille == 0);

    /* Tri par date : égalités (11-05) départagées par identifiant croissant, dans les deux sens. */
    filtre_init(&f);
    VERIFIER(recherche(&app, &f, &res));
    analyses_trier(&res, TRI_DATE, true);
    VERIFIER(ordre_egal(&res, (const Id[]){o[6], o[1], o[2], o[3], o[5], o[4], o[7], o[8], o[9]}, 9));
    analyses_trier(&res, TRI_DATE, false);
    VERIFIER(ordre_egal(&res, (const Id[]){o[9], o[8], o[7], o[4], o[5], o[2], o[3], o[1], o[6]}, 9));
    /* Tri par montant : 25 DH (ids 8 et 9) à égalité, toujours id croissant. */
    analyses_trier(&res, TRI_MONTANT, true);
    VERIFIER(ordre_egal(&res, (const Id[]){o[8], o[9], o[3], o[7], o[6], o[1], o[2], o[5], o[4]}, 9));
    analyses_trier(&res, TRI_MONTANT, false);
    VERIFIER(ordre_egal(&res, (const Id[]){o[4], o[5], o[2], o[1], o[6], o[7], o[3], o[8], o[9]}, 9));
    /* Tri par montant attribuable avec filtre de catégorie (Alimentation, toutes dates). */
    f.id_categorie = j.alim;
    VERIFIER(recherche(&app, &f, &res) && res.taille == 6);
    analyses_trier(&res, TRI_ATTRIBUABLE, true);
    VERIFIER(ordre_egal(&res, (const Id[]){o[8], o[9], o[3], o[7], o[6], o[1]}, 6));

    /* Statistiques de novembre : dépenses confirmées 200 + 300 + 40 + 60 = 600 DH. */
    VERIFIER(analyses_statistiques(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), ID_AUCUN, &st));
    VERIFIER(st.nombre == 4 && st.total_ct == 60000 && st.moyenne_ct == 15000
             && st.minimum_ct == 4000 && st.maximum_ct == 30000);
    /* Avec catégorie Alimentation : l'opération ventilée compte une fois, pour sa part attribuable. */
    VERIFIER(analyses_statistiques(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), j.alim, &st));
    VERIFIER(st.nombre == 3 && st.total_ct == 30000 && st.moyenne_ct == 10000
             && st.minimum_ct == 4000 && st.maximum_ct == 20000);
    VERIFIER(analyses_statistiques(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), j.hygiene, &st));
    VERIFIER(st.nombre == 2 && st.total_ct == 11000 && st.moyenne_ct == 5500
             && st.minimum_ct == 5000 && st.maximum_ct == 6000);
    /* Période sans dépense : nombre nul, aucune division. */
    VERIFIER(analyses_statistiques(&app.donnees, D(2026, 1, 1), D(2026, 1, 31), ID_AUCUN, &st));
    VERIFIER(st.nombre == 0 && st.total_ct == 0 && st.moyenne_ct == 0);
    /* Les occurrences non confirmées ou annulées n'alimentent pas les statistiques réalisées. */
    {
        Id a = brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, date_aucune(), 99999, j.banque,
                        ID_AUCUN, 7, D(2026, 11, 20));
        Id b = brute_op(&app, NATURE_DEPENSE, STATUT_ANNULEE, D(2026, 11, 21), 99999, j.banque,
                        ID_AUCUN, 7, D(2026, 11, 21));
        brute_ligne(&app, a, j.courses, ID_AUCUN, 99999);
        brute_ligne(&app, b, j.courses, ID_AUCUN, 99999);
        VERIFIER(analyses_statistiques(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), ID_AUCUN, &st));
        VERIFIER(st.nombre == 4 && st.total_ct == 60000);
        /* La recherche, elle, les affiche (statut « tous » par défaut) avec leur date prévue. */
        filtre_init(&f);
        f.par_periode = true;
        f.debut = D(2026, 11, 20);
        f.fin = D(2026, 11, 20);
        VERIFIER(recherche(&app, &f, &res) && res.taille == 1 && res_sure(&res, 0)->id == a);
        f.par_statut = true;
        f.statut = STATUT_CONFIRMEE;
        VERIFIER(recherche(&app, &f, &res) && res.taille == 0);
    }
    app_liberer(&app);

    /* Critère de recette : 40, 10, 30 et 20 DH dans le même mois. */
    preparer(&app, &j);
    {
        const long long montants[4] = {4000, 1000, 3000, 2000};
        int k;
        for (k = 0; k < 4; k++) {
            VERIFIER(depense(&app, "Dépense", D(2026, 11, 2 + k), montants[k], j.banque, j.courses,
                             &o[0]) == RES_OK);
        }
        VERIFIER(analyses_statistiques(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), ID_AUCUN, &st));
        VERIFIER(st.nombre == 4 && st.total_ct == 10000 && st.moyenne_ct == 2500
                 && st.minimum_ct == 1000 && st.maximum_ct == 4000);
    }
    app_liberer(&app);

    /* Moyenne arrondie au plus proche, demi au-dessus. */
    preparer(&app, &j);
    VERIFIER(depense(&app, "A", D(2026, 11, 2), 1000, j.banque, j.courses, &o[0]) == RES_OK);
    VERIFIER(depense(&app, "B", D(2026, 11, 3), 1001, j.banque, j.courses, &o[0]) == RES_OK);
    VERIFIER(analyses_statistiques(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), ID_AUCUN, &st));
    VERIFIER(st.total_ct == 2001 && st.moyenne_ct == 1001);          /* 10,005 -> 10,01 */
    app_liberer(&app);
    preparer(&app, &j);
    VERIFIER(depense(&app, "A", D(2026, 11, 2), 1, j.banque, j.courses, &o[0]) == RES_OK);
    VERIFIER(depense(&app, "B", D(2026, 11, 3), 2, j.banque, j.courses, &o[0]) == RES_OK);
    VERIFIER(analyses_statistiques(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), ID_AUCUN, &st));
    VERIFIER(st.total_ct == 3 && st.moyenne_ct == 2);                /* 1,5 ct -> 2 ct */
    VERIFIER(depense(&app, "C", D(2026, 11, 4), 3, j.banque, j.courses, &o[0]) == RES_OK);
    VERIFIER(analyses_statistiques(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), ID_AUCUN, &st));
    VERIFIER(st.total_ct == 6 && st.moyenne_ct == 2);                /* 2 ct exactement */
    app_liberer(&app);

    tableau_liberer(&res);
    rapport_liberer(&r);
}

/* ================================================================== */
/* Étape 3 : doublons I6, bilan G13, véhicule G11, I3, 1A              */
/* ================================================================== */

static Id creer_op(Application *app, Nature n, Date d, const char *lib, long long total, Id src,
                   Id dst, Id tiers, const LigneSaisie *l, size_t nb)
{
    OperationSaisie s = saisie(n, d, lib, total, src, dst);
    RapportErreurs r;
    Id id = ID_AUCUN;

    s.id_tiers = tiers;
    rapport_init(&r);
    VERIFIER(op_creer(app, &s, l, nb, true, &id, NULL, &r) == RES_OK);
    if (rapport_nombre(&r) > 0) {
        rapport_afficher(&r, stdout);
    }
    rapport_liberer(&r);
    return id;
}

/* Modèle de dépense et récurrence minimaux (insertion brute) pour porter des occurrences. */
static Id brute_modele(Application *app, Id compte_id, Id cat)
{
    ModeleOperation m;
    LigneModele l;

    memset(&m, 0, sizeof m);
    m.id = donnees_nouvel_id(&app->donnees, ENT_MODELE);
    texte_copier(m.nom, sizeof m.nom, "Modèle");
    m.nature = NATURE_DEPENSE;
    texte_copier(m.libelle, sizeof m.libelle, "Modèle");
    m.montant_total_ct = 1000;
    m.id_compte_source = compte_id;
    tableau_ajouter(&app->donnees.modeles, &m);
    memset(&l, 0, sizeof l);
    l.id = donnees_nouvel_id(&app->donnees, ENT_LIGNE_MODELE);
    l.id_modele = m.id;
    l.id_categorie = cat;
    l.montant_ct = 1000;
    tableau_ajouter(&app->donnees.lignes_modeles, &l);
    return m.id;
}

static Id brute_recurrence(Application *app, Id modele)
{
    Recurrence x;

    memset(&x, 0, sizeof x);
    x.id = donnees_nouvel_id(&app->donnees, ENT_RECURRENCE);
    x.id_modele = modele;
    x.frequence = FREQ_HEBDOMADAIRE;
    x.date_debut = D(2026, 1, 5);
    x.active = true;
    x.date_prochaine_echeance = D(2027, 1, 4);   /* lundi, postérieur aux occurrences des tests */
    tableau_ajouter(&app->donnees.recurrences, &x);
    return x.id;
}

static bool groupe_exact(const Tableau *groupes, const Tableau *membres, const Id *ids, size_t n)
{
    size_t g;

    for (g = 0; g < groupes->taille; g++) {
        const GroupeDoublons *x = ELEMENT(groupes, GroupeDoublons, g);
        size_t k;
        bool egal = (x->nombre == n);

        for (k = 0; egal && k < n; k++) {
            egal = (*ELEMENT(membres, Id, x->debut + k) == ids[k]);
        }
        if (egal) {
            return true;
        }
    }
    return false;
}

static bool dans_un_groupe(const Tableau *membres, Id id)
{
    size_t i;

    for (i = 0; i < membres->taille; i++) {
        if (*ELEMENT(membres, Id, i) == id) {
            return true;
        }
    }
    return false;
}

static long long total_cat(const Donnees *d, Date deb, Date fin, Nature n, Id cat, long long *glob)
{
    long long *t = calloc(d->categories.taille + 1, sizeof *t);
    long long g = 0;
    long long v = -1;
    size_t idx;

    if (t != NULL && analyses_totaux_categories(d, deb, fin, n, t, &g)
        && tableau_trouver_id(&d->categories, cat, &idx) != NULL) {
        v = t[idx];
    }
    if (glob != NULL) {
        *glob = g;
    }
    free(t);
    return v;
}

void test_utilitaires_etape3(void)
{
    char s[LIBELLE_MAX + 1];
    Id ids[4];
    size_t n;

    printf("Libellé normalisé (I6) et liste d'identifiants\n");
    texte_normaliser("  Achat   MARJANE  ", s, sizeof s);
    VERIFIER(strcmp(s, "achat marjane") == 0);
    texte_normaliser("ÉPICERIE  Fine", s, sizeof s);
    VERIFIER(strcmp(s, "épicerie fine") == 0);
    texte_normaliser("   ", s, sizeof s);
    VERIFIER(s[0] == '\0');
    texte_normaliser("a b", s, 3);               /* tronqué sans dépasser la taille */
    VERIFIER(strcmp(s, "a ") != 0 && strlen(s) < 3);
    VERIFIER(liste_ids_lire("12,15,18", ids, 4, &n) && n == 3 && ids[0] == 12 && ids[2] == 18);
    VERIFIER(liste_ids_lire(" 3 , 4 ", ids, 4, &n) && n == 2 && ids[1] == 4);
    VERIFIER(!liste_ids_lire("1,,2", ids, 4, &n));
    VERIFIER(!liste_ids_lire("1,1", ids, 4, &n));
    VERIFIER(!liste_ids_lire("1,a", ids, 4, &n));
    VERIFIER(!liste_ids_lire("0", ids, 4, &n));
    VERIFIER(!liste_ids_lire("", ids, 4, &n));
    VERIFIER(!liste_ids_lire("1,", ids, 4, &n));
    VERIFIER(!liste_ids_lire("1,2,3,4,5", ids, 4, &n));
    VERIFIER(!liste_ids_lire("99999999999", ids, 4, &n));
}

void test_doublons(void)
{
    Application app;
    Jeu j;
    RapportErreurs r;
    LigneSaisie a[3];
    LigneSaisie b[3];
    Tableau groupes;
    Tableau membres;
    Id o[20];

    printf("Doublons I6 (ventilation en multiensemble, tiers, comptes, nature) et leur traitement\n");
    rapport_init(&r);
    tableau_init(&groupes, sizeof(GroupeDoublons));
    tableau_init(&membres, sizeof(Id));
    preparer(&app, &j);

    /* A : trois opérations identiques (libellé en casse/espaces différents, lignes dans un autre ordre). */
    a[0] = ligne(j.courses, 15000, ID_AUCUN);
    a[1] = ligne(j.hygiene, 5000, ID_AUCUN);
    b[0] = ligne(j.hygiene, 5000, ID_AUCUN);
    b[1] = ligne(j.courses, 15000, ID_AUCUN);
    o[1] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 8), "Marjane", 20000, j.banque, ID_AUCUN, ID_AUCUN, a, 2);
    o[2] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 8), "  MARJANE ", 20000, j.banque, ID_AUCUN, ID_AUCUN, b, 2);
    o[3] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 8), "marjane", 20000, j.banque, ID_AUCUN, ID_AUCUN, a, 2);
    o[4] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 9), "Marjane", 20000, j.banque, ID_AUCUN, ID_AUCUN, a, 2);
    /* B : un véhicule différent sur une ligne empêche le regroupement. */
    a[0] = ligne(j.carburant, 10000, j.clio);
    a[1] = ligne(j.courses, 10000, ID_AUCUN);
    b[0] = ligne(j.carburant, 10000, ID_AUCUN);
    b[1] = ligne(j.courses, 10000, ID_AUCUN);
    o[5] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Plein", 20000, j.banque, ID_AUCUN, ID_AUCUN, a, 2);
    o[6] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Plein", 20000, j.banque, ID_AUCUN, ID_AUCUN, b, 2);
    /* C : le tiers doit être le même (ou absent des deux). */
    a[0] = ligne(j.courses, 5000, ID_AUCUN);
    o[7] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Achat", 5000, j.banque, ID_AUCUN, j.carrefour, a, 1);
    o[8] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Achat", 5000, j.banque, ID_AUCUN, j.afriquia, a, 1);
    o[9] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Achat", 5000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    o[10] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Achat", 5000, j.banque, ID_AUCUN, j.carrefour, a, 1);
    /* D : le nombre de lignes identiques compte. */
    a[0] = ligne(j.courses, 5000, ID_AUCUN);
    a[1] = ligne(j.courses, 5000, ID_AUCUN);
    b[0] = ligne(j.courses, 10000, ID_AUCUN);
    o[11] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Lot", 10000, j.banque, ID_AUCUN, ID_AUCUN, a, 2);
    o[12] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Lot", 10000, j.banque, ID_AUCUN, ID_AUCUN, b, 1);
    o[13] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Lot", 10000, j.banque, ID_AUCUN, ID_AUCUN, a, 2);
    a[0] = ligne(j.courses, 5000, ID_AUCUN);
    a[1] = ligne(j.courses, 5000, ID_AUCUN);
    a[2] = ligne(j.hygiene, 5000, ID_AUCUN);
    b[0] = ligne(j.courses, 5000, ID_AUCUN);
    b[1] = ligne(j.hygiene, 5000, ID_AUCUN);
    b[2] = ligne(j.hygiene, 5000, ID_AUCUN);
    o[14] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Panier", 15000, j.banque, ID_AUCUN, ID_AUCUN, a, 3);
    o[15] = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 10), "Panier", 15000, j.banque, ID_AUCUN, ID_AUCUN, b, 3);
    /* E : rôles des comptes d'un virement. */
    o[16] = creer_op(&app, NATURE_VIREMENT, D(2026, 11, 10), "Retrait", 10000, j.banque, j.especes, ID_AUCUN, NULL, 0);
    o[17] = creer_op(&app, NATURE_VIREMENT, D(2026, 11, 10), "Retrait", 10000, j.especes, j.banque, ID_AUCUN, NULL, 0);
    o[18] = creer_op(&app, NATURE_VIREMENT, D(2026, 11, 10), "Retrait", 10000, j.banque, j.especes, ID_AUCUN, NULL, 0);
    /* F : la nature différencie (revenu de même libellé, date et montant). */
    a[0] = ligne(j.salaire, 20000, ID_AUCUN);
    o[19] = creer_op(&app, NATURE_REVENU, D(2026, 11, 8), "Marjane", 20000, ID_AUCUN, j.banque, ID_AUCUN, a, 1);

    VERIFIER(analyses_doublons(&app.donnees, &groupes, &membres));
    VERIFIER(groupes.taille == 4);
    VERIFIER(groupe_exact(&groupes, &membres, (const Id[]){o[1], o[2], o[3]}, 3));
    VERIFIER(groupe_exact(&groupes, &membres, (const Id[]){o[7], o[10]}, 2));
    VERIFIER(groupe_exact(&groupes, &membres, (const Id[]){o[11], o[13]}, 2));
    VERIFIER(groupe_exact(&groupes, &membres, (const Id[]){o[16], o[18]}, 2));
    VERIFIER(!dans_un_groupe(&membres, o[4]) && !dans_un_groupe(&membres, o[5])
             && !dans_un_groupe(&membres, o[6]) && !dans_un_groupe(&membres, o[8])
             && !dans_un_groupe(&membres, o[9]) && !dans_un_groupe(&membres, o[12])
             && !dans_un_groupe(&membres, o[14]) && !dans_un_groupe(&membres, o[15])
             && !dans_un_groupe(&membres, o[17]) && !dans_un_groupe(&membres, o[19]));

    /* Une occurrence non confirmée ou annulée n'est jamais comparée. */
    {
        Id modele = brute_modele(&app, j.banque, j.courses);
        Id rec = brute_recurrence(&app, modele);
        Id x = brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, date_aucune(), 5000, j.banque,
                        ID_AUCUN, rec, D(2026, 11, 9));
        Id y = brute_op(&app, NATURE_DEPENSE, STATUT_ANNULEE, D(2026, 11, 10), 5000, j.banque,
                        ID_AUCUN, rec, D(2026, 11, 16));
        brute_ligne(&app, x, j.courses, ID_AUCUN, 5000);
        brute_ligne(&app, y, j.courses, ID_AUCUN, 5000);
        VERIFIER(valides(&app));
        VERIFIER(analyses_doublons(&app.donnees, &groupes, &membres) && groupes.taille == 4);
        VERIFIER(!dans_un_groupe(&membres, x) && !dans_un_groupe(&membres, y));
    }

    /* Traitement : supprimer deux copies du groupe A en une transaction. */
    {
        size_t lignes_avant = app.donnees.lignes_operations.taille;
        Id choisis[2];
        choisis[0] = o[2];
        choisis[1] = o[3];
        VERIFIER(op_supprimer_ou_annuler(&app, choisis, 2, true, NULL, &r) == RES_OK);
        VERIFIER(regles_operation(&app.donnees, o[1]) != NULL
                 && regles_operation(&app.donnees, o[2]) == NULL
                 && regles_operation(&app.donnees, o[3]) == NULL);
        VERIFIER(app.donnees.lignes_operations.taille == lignes_avant - 4);
        VERIFIER(valides(&app));
        /* Tout ou rien : une opération inconnue dans la sélection annule tout. */
        choisis[0] = o[7];
        choisis[1] = 9999;
        VERIFIER(op_supprimer_ou_annuler(&app, choisis, 2, true, NULL, &r) == RES_ERR_INTROUVABLE);
        VERIFIER(regles_operation(&app.donnees, o[7]) != NULL);
        rapport_vider(&r);
        choisis[1] = o[7];                                        /* doublon dans la sélection */
        VERIFIER(op_supprimer_ou_annuler(&app, choisis, 2, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(op_supprimer_ou_annuler(&app, choisis, 0, true, NULL, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
    }
    app_liberer(&app);

    /* Occurrence confirmée en doublon d'une opération manuelle : protégée (annulée, jamais supprimée). */
    preparer(&app, &j);
    {
        Id modele = brute_modele(&app, j.banque, j.courses);
        Id rec = brute_recurrence(&app, modele);
        Id occ;
        Id manuelle;
        Id ids[1];
        const Operation *op;

        a[0] = ligne(j.courses, 20000, ID_AUCUN);
        manuelle = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 8), "Loyer", 20000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
        occ = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 11, 8), 20000, j.banque,
                       ID_AUCUN, rec, D(2026, 11, 2));
        texte_copier(ELEMENT(&app.donnees.operations, Operation, 1)->libelle, LIBELLE_MAX + 1, "loyer");
        brute_ligne(&app, occ, j.courses, ID_AUCUN, 20000);
        VERIFIER(valides(&app));
        VERIFIER(analyses_doublons(&app.donnees, &groupes, &membres));
        VERIFIER(groupe_exact(&groupes, &membres, (const Id[]){manuelle, occ}, 2));
        ids[0] = occ;
        VERIFIER(op_supprimer(&app, occ, true, NULL, &r) == RES_ERR_VALIDATION);   /* jamais supprimée */
        rapport_vider(&r);
        VERIFIER(op_supprimer_ou_annuler(&app, ids, 1, true, NULL, &r) == RES_OK);
        op = regles_operation(&app.donnees, occ);
        VERIFIER(op != NULL && op->statut == STATUT_ANNULEE && op->id_recurrence == rec
                 && date_comparer(op->date_realisation, D(2026, 11, 8)) == 0
                 && date_comparer(op->date_prevue_origine, D(2026, 11, 2)) == 0
                 && op_nombre_lignes(&app.donnees, occ) == 1);
        {
            long long solde;
            VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 480000);  /* effet retiré */
        }
        VERIFIER(analyses_doublons(&app.donnees, &groupes, &membres) && groupes.taille == 0);
        /* ANNULEE est un état final. */
        VERIFIER(op_annuler(&app, occ, true, NULL, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
        VERIFIER(op_annuler(&app, manuelle, true, NULL, &r) == RES_ERR_VALIDATION);   /* manuelle */
        rapport_vider(&r);
    }
    app_liberer(&app);

    /* G10 à l'annulation : annuler un revenu récurrent qui finance une dépense est refusé. */
    preparer(&app, &j);
    {
        Id modele = brute_modele(&app, j.banque, j.courses);
        Id rec = brute_recurrence(&app, modele);
        Id occ = brute_op(&app, NATURE_REVENU, STATUT_CONFIRMEE, D(2026, 11, 1), 10000, ID_AUCUN,
                          j.especes, rec, D(2026, 11, 2));
        Id dep;
        long modifs;

        brute_ligne(&app, occ, j.salaire, ID_AUCUN, 10000);
        a[0] = ligne(j.courses, 25000, ID_AUCUN);
        dep = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 2), "Gros achat", 25000, j.especes, ID_AUCUN, ID_AUCUN, a, 1);
        (void)dep;
        modifs = app.nb_modifications;
        VERIFIER(op_annuler(&app, occ, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "solde de fin de journée"));
        VERIFIER(regles_operation(&app.donnees, occ)->statut == STATUT_CONFIRMEE
                 && app.nb_modifications == modifs);
        rapport_vider(&r);
    }
    app_liberer(&app);

    tableau_liberer(&groupes);
    tableau_liberer(&membres);
    rapport_liberer(&r);
}

void test_bilan_vehicule(void)
{
    Application app;
    Jeu j;
    RapportErreurs r;
    LigneSaisie a[2];
    Tableau lignes;
    Tableau par_cat;
    long long glob;
    long long total;
    Id id = ID_AUCUN;
    SituationBudget sb;
    const Date deb = D(2026, 11, 1);
    const Date fin = D(2026, 11, 30);

    printf("Totaux par catégorie sans double comptage (G13), budgets, véhicule (G11)\n");
    rapport_init(&r);
    tableau_init(&lignes, sizeof(LigneVehicule));
    tableau_init(&par_cat, sizeof(TotalCategorie));
    preparer(&app, &j);
    app.date_reference = D(2026, 12, 31);

    a[0] = ligne(j.courses, 4000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 2), "e1", 4000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.courses, 3000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 3), "e2", 3000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.hygiene, 2000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 4), "e3", 2000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.carburant, 6000, j.clio);
    a[1] = ligne(j.courses, 4000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 5), "mixte", 10000, j.banque, ID_AUCUN, ID_AUCUN, a, 2);
    a[0] = ligne(j.courses, 50000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 10, 31), "hors mois avant", 50000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.courses, 70000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 12, 1), "hors mois apres", 70000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.salaire, 300000, ID_AUCUN);
    creer_op(&app, NATURE_REVENU, D(2026, 11, 10), "salaire", 300000, ID_AUCUN, j.banque, ID_AUCUN, a, 1);
    creer_op(&app, NATURE_VIREMENT, D(2026, 11, 11), "virement", 20000, j.banque, j.especes, ID_AUCUN, NULL, 0);
    a[0] = ligne(j.carburant, 5000, j.clio);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 20), "plein", 5000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.hygiene, 1500, j.clio);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 22), "lavage", 1500, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.carburant, 8000, j.clio);
    creer_op(&app, NATURE_DEPENSE, D(2026, 10, 15), "plein octobre", 8000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);

    /* Dépenses de novembre : Courses 40+30+40 = 110 ; Hygiène 20+15 = 35 ; Carburant 60+50 = 110. */
    VERIFIER(total_cat(&app.donnees, deb, fin, NATURE_DEPENSE, j.courses, &glob) == 11000);
    VERIFIER(total_cat(&app.donnees, deb, fin, NATURE_DEPENSE, j.hygiene, NULL) == 3500);
    VERIFIER(total_cat(&app.donnees, deb, fin, NATURE_DEPENSE, j.alim, NULL) == 14500);   /* 110 + 35 */
    VERIFIER(total_cat(&app.donnees, deb, fin, NATURE_DEPENSE, j.carburant, NULL) == 11000);
    VERIFIER(total_cat(&app.donnees, deb, fin, NATURE_DEPENSE, j.transport, NULL) == 11000);
    /* Total global : chaque opération une fois (40+30+20+100+50+15 = 255 DH), pas 145+110+... */
    VERIFIER(glob == 25500);
    VERIFIER(total_cat(&app.donnees, deb, fin, NATURE_DEPENSE, j.alim, NULL)
             + total_cat(&app.donnees, deb, fin, NATURE_DEPENSE, j.transport, NULL) == glob);
    /* Revenus et virements : séparés. */
    VERIFIER(total_cat(&app.donnees, deb, fin, NATURE_REVENU, j.salaire, &glob) == 300000 && glob == 300000);
    VERIFIER(total_cat(&app.donnees, deb, fin, NATURE_REVENU, j.alim, NULL) == 0);
    /* Un mois sans opération. */
    VERIFIER(total_cat(&app.donnees, D(2026, 3, 1), D(2026, 3, 31), NATURE_DEPENSE, j.alim, &glob) == 0 && glob == 0);

    /* Budgets : plafond zéro = budget défini ; absence = non défini ; dépassement et reste. */
    VERIFIER(ref_budget_creer(&app, 2026, 11, j.alim, 20000, &id, &r) == RES_OK);
    VERIFIER(ref_budget_creer(&app, 2026, 11, j.courses, 10000, &id, &r) == RES_OK);
    VERIFIER(ref_budget_creer(&app, 2026, 11, ID_AUCUN, 0, &id, &r) == RES_OK);
    sb = analyses_situation_budget(&app.donnees, 2026, 11, j.alim, 14500);
    VERIFIER(sb.defini && sb.plafond_ct == 20000 && sb.reste_ct == 5500 && sb.depassement_ct == 0);
    sb = analyses_situation_budget(&app.donnees, 2026, 11, j.courses, 11000);
    VERIFIER(sb.defini && sb.reste_ct == 0 && sb.depassement_ct == 1000);
    sb = analyses_situation_budget(&app.donnees, 2026, 11, ID_AUCUN, 25500);
    VERIFIER(sb.defini && sb.plafond_ct == 0 && sb.depassement_ct == 25500);
    sb = analyses_situation_budget(&app.donnees, 2026, 11, j.transport, 11000);
    VERIFIER(!sb.defini && sb.reste_ct == 0 && sb.depassement_ct == 0);
    sb = analyses_situation_budget(&app.donnees, 2026, 10, j.alim, 0);
    VERIFIER(!sb.defini);

    /* Véhicule : seules les lignes rattachées comptent ; l'opération mixte ne contribue que par sa ligne. */
    VERIFIER(analyses_vehicule(&app.donnees, j.clio, deb, fin, &lignes, &par_cat, &total));
    VERIFIER(total == 12500);                                  /* 60 + 50 + 15 */
    VERIFIER(lignes.taille == 3 && par_cat.taille == 2);
    VERIFIER(ELEMENT(&lignes, LigneVehicule, 0)->montant_ct == 6000
             && ELEMENT(&lignes, LigneVehicule, 1)->montant_ct == 5000
             && ELEMENT(&lignes, LigneVehicule, 2)->montant_ct == 1500);
    VERIFIER(strcmp(ELEMENT(&par_cat, TotalCategorie, 0)->chemin, "Alimentation > Hygiène") == 0
             && ELEMENT(&par_cat, TotalCategorie, 0)->total_ct == 1500
             && strcmp(ELEMENT(&par_cat, TotalCategorie, 1)->chemin, "Transport > Carburant") == 0
             && ELEMENT(&par_cat, TotalCategorie, 1)->total_ct == 11000);
    VERIFIER(analyses_vehicule(&app.donnees, j.clio, D(2026, 10, 1), D(2026, 10, 31), &lignes, &par_cat, &total)
             && total == 8000 && lignes.taille == 1);
    VERIFIER(analyses_vehicule(&app.donnees, j.clio, D(2026, 1, 1), D(2026, 1, 31), &lignes, &par_cat, &total)
             && total == 0 && lignes.taille == 0 && par_cat.taille == 0);
    app_liberer(&app);

    /* Critère de recette : 40 et 30 DH dans une catégorie plafonnée à 100 DH -> 70 dépensés, 30 restants. */
    preparer(&app, &j);
    VERIFIER(ref_budget_creer(&app, 2026, 11, j.courses, 10000, &id, &r) == RES_OK);
    a[0] = ligne(j.courses, 4000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 2), "d40", 4000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.courses, 3000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 3), "d30", 3000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    total = total_cat(&app.donnees, deb, fin, NATURE_DEPENSE, j.courses, NULL);
    sb = analyses_situation_budget(&app.donnees, 2026, 11, j.courses, total);
    VERIFIER(total == 7000 && sb.depense_ct == 7000 && sb.reste_ct == 3000 && sb.depassement_ct == 0);
    app_liberer(&app);

    tableau_liberer(&lignes);
    tableau_liberer(&par_cat);
    rapport_liberer(&r);
}

void test_categories_avancees(void)
{
    Application app;
    Jeu j;
    RapportErreurs r;
    RapportErreurs w;
    LigneSaisie a[2];
    Id id = ID_AUCUN;
    Id autres = ID_AUCUN;
    Id budget = ID_AUCUN;
    Id e1;

    printf("Catégories avancées : migration I3, déplacement\n");
    rapport_init(&r);
    rapport_init(&w);

    /* --- I3 : créer un enfant sous une feuille utilisée --- */
    preparer(&app, &j);
    a[0] = ligne(j.courses, 4000, ID_AUCUN);
    e1 = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 2), "e1", 4000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    {
        Id modele = brute_modele(&app, j.banque, j.courses);
        Id rec = brute_recurrence(&app, modele);
        Id occ = brute_op(&app, NATURE_DEPENSE, STATUT_ANNULEE, date_aucune(), 1000, j.banque,
                          ID_AUCUN, rec, D(2026, 11, 9));
        brute_ligne(&app, occ, j.courses, ID_AUCUN, 1000);
    }
    VERIFIER(ref_budget_creer(&app, 2026, 11, j.courses, 10000, &budget, &r) == RES_OK);
    VERIFIER(valides(&app));
    {
        size_t cats = app.donnees.categories.taille;
        Id compteur = app.donnees.prochain_id[ENT_CATEGORIE];
        long modifs = app.nb_modifications;
        long long avant = total_cat(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), NATURE_DEPENSE, j.courses, NULL);

        /* L'ancienne API refuse toujours sans accord explicite. */
        VERIFIER(ref_categorie_creer(&app, "Bio", j.courses, CAT_DEPENSE, &id, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "Autres"));
        rapport_vider(&r);
        VERIFIER(ref_categorie_creer_i3(&app, "Bio", j.courses, CAT_DEPENSE, false, &id, &w, &r)
                 == RES_CONFIRMATION_REQUISE);
        VERIFIER(contient(&w, "2 ligne(s) d'opération") && contient(&w, "1 ligne(s) de modèle"));
        VERIFIER(app.donnees.categories.taille == cats && app.donnees.prochain_id[ENT_CATEGORIE] == compteur
                 && app.nb_modifications == modifs);
        rapport_vider(&w);
        /* Échec en bloc : un nom invalide ne laisse aucune migration partielle. */
        VERIFIER(ref_categorie_creer_i3(&app, "A > B", j.courses, CAT_DEPENSE, true, &id, NULL, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
        VERIFIER(app.donnees.categories.taille == cats && regles_usages_categorie(&app.donnees, j.courses).lignes_operations == 2);
        /* Échec de validation globale (données invalides sans rapport) : annulation complète. */
        {
            Id mauvais = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 11, 3), 500, j.banque,
                                  ID_AUCUN, ID_AUCUN, date_aucune());      /* sans ligne : invalide */
            VERIFIER(ref_categorie_creer_i3(&app, "Bio", j.courses, CAT_DEPENSE, true, &id, NULL, &r)
                     == RES_ERR_VALIDATION);
            VERIFIER(app.donnees.categories.taille == cats
                     && regles_usages_categorie(&app.donnees, j.courses).lignes_operations == 2
                     && regles_usages_categorie(&app.donnees, j.courses).lignes_modeles == 1);
            rapport_vider(&r);
            tableau_retirer(&app.donnees.operations, app.donnees.operations.taille - 1);
            (void)mauvais;
        }
        VERIFIER(valides(&app));
        /* Succès : « Autres » créée, lignes (opérations de tous statuts et modèle) déplacées, puis « Bio ». */
        VERIFIER(ref_categorie_creer_i3(&app, "Bio", j.courses, CAT_DEPENSE, true, &id, NULL, &r) == RES_OK);
        VERIFIER(app.donnees.categories.taille == cats + 2);
        autres = ID_AUCUN;
        {
            size_t k;
            for (k = 0; k < app.donnees.categories.taille; k++) {
                const Categorie *c = ELEMENT(&app.donnees.categories, Categorie, k);
                if (c->id_parent == j.courses && strcmp(c->nom, "Autres") == 0) {
                    autres = c->id;
                }
            }
        }
        VERIFIER(autres != ID_AUCUN && id != autres && regles_categorie(&app.donnees, id) != NULL
                 && regles_categorie(&app.donnees, id)->id_parent == j.courses);
        VERIFIER(regles_usages_categorie(&app.donnees, j.courses).lignes_operations == 0
                 && regles_usages_categorie(&app.donnees, j.courses).lignes_modeles == 0
                 && regles_usages_categorie(&app.donnees, autres).lignes_operations == 2
                 && regles_usages_categorie(&app.donnees, autres).lignes_modeles == 1);
        VERIFIER(regles_budget(&app.donnees, budget) != NULL
                 && regles_budget(&app.donnees, budget)->id_categorie == j.courses);   /* budget sur le parent */
        VERIFIER(total_cat(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), NATURE_DEPENSE, j.courses, NULL) == avant);
        VERIFIER(op_nombre_lignes(&app.donnees, e1) == 1 && regles_operation(&app.donnees, e1)->montant_total_ct == 4000);
        VERIFIER(valides(&app));
    }
    app_liberer(&app);

    /* L'enfant demandé s'appelle « Autres » (casse ignorée) : il reçoit directement les lignes. */
    preparer(&app, &j);
    a[0] = ligne(j.courses, 4000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 2), "e1", 4000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    {
        size_t cats = app.donnees.categories.taille;
        VERIFIER(ref_categorie_creer_i3(&app, "AUTRES", j.courses, CAT_DEPENSE, true, &id, NULL, &r) == RES_OK);
        VERIFIER(app.donnees.categories.taille == cats + 1
                 && regles_usages_categorie(&app.donnees, id).lignes_operations == 1);
    }
    /* Une feuille inutilisée reçoit un enfant sans migration ni confirmation. */
    VERIFIER(ref_categorie_creer_i3(&app, "Taxi", j.carburant, CAT_DEPENSE, false, &id, &w, &r) == RES_OK);
    VERIFIER(rapport_nombre(&w) == 0);
    app_liberer(&app);

    /* --- Déplacement --- */
    preparer(&app, &j);
    app.date_reference = D(2026, 12, 31);
    a[0] = ligne(j.courses, 4000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 2), "e1", 4000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.hygiene, 2000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 4), "e3", 2000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.salaire, 100000, ID_AUCUN);
    creer_op(&app, NATURE_REVENU, D(2026, 11, 5), "salaire", 100000, ID_AUCUN, j.banque, ID_AUCUN, a, 1);
    VERIFIER(ref_budget_creer(&app, 2026, 11, j.alim, 15000, &budget, &r) == RES_OK);

    VERIFIER(ref_categorie_deplacer(&app, j.alim, j.courses, CAT_DEPENSE, true, NULL, &r) == RES_ERR_VALIDATION);   /* sous un descendant */
    VERIFIER(contient(&r, "descendants"));
    rapport_vider(&r);
    VERIFIER(ref_categorie_deplacer(&app, j.alim, j.alim, CAT_DEPENSE, true, NULL, &r) == RES_ERR_VALIDATION);     /* sous elle-même */
    rapport_vider(&r);
    VERIFIER(ref_categorie_deplacer(&app, j.courses, j.alim, CAT_DEPENSE, true, NULL, &r) == RES_ERR_VALIDATION);  /* déjà là */
    rapport_vider(&r);
    VERIFIER(ref_categorie_deplacer(&app, 999, j.alim, CAT_DEPENSE, true, NULL, &r) == RES_ERR_INTROUVABLE);
    rapport_vider(&r);
    /* Changement de type d'une branche utilisée : bloqué. */
    VERIFIER(ref_categorie_deplacer(&app, j.salaire, j.alim, CAT_DEPENSE, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "Changement de type impossible"));
    rapport_vider(&r);
    VERIFIER(regles_categorie(&app.donnees, j.salaire)->type == CAT_REVENU);
    /* Conflit de nom au nouvel emplacement. */
    VERIFIER(ref_categorie_creer(&app, "Hygiène", j.transport, CAT_DEPENSE, &id, &r) == RES_OK);
    VERIFIER(ref_categorie_deplacer(&app, j.hygiene, j.transport, CAT_DEPENSE, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "existe déjà"));
    rapport_vider(&r);
    VERIFIER(ref_categorie_renommer(&app, id, "Entretien", &r) == RES_OK);
    /* Hygiène (20 DH) quitte Alimentation (budget 150 DH) : aperçu et accord requis. */
    {
        size_t cats = app.donnees.categories.taille;
        long modifs = app.nb_modifications;
        VERIFIER(ref_categorie_deplacer(&app, j.hygiene, j.transport, CAT_DEPENSE, false, &w, &r)
                 == RES_CONFIRMATION_REQUISE);
        VERIFIER(contient(&w, "Budget « Alimentation » 2026-11") && contient(&w, "60,00 DH avant")
                 && contient(&w, "40,00 DH après"));
        VERIFIER(regles_categorie(&app.donnees, j.hygiene)->id_parent == j.alim
                 && app.nb_modifications == modifs && app.donnees.categories.taille == cats);
        rapport_vider(&w);
        VERIFIER(ref_categorie_deplacer(&app, j.hygiene, j.transport, CAT_DEPENSE, true, NULL, &r) == RES_OK);
        VERIFIER(regles_categorie(&app.donnees, j.hygiene)->id_parent == j.transport);
        VERIFIER(total_cat(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), NATURE_DEPENSE, j.alim, NULL) == 4000);
        VERIFIER(total_cat(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), NATURE_DEPENSE, j.transport, NULL) == 2000);
        VERIFIER(regles_budget(&app.donnees, budget)->plafond_ct == 15000);          /* plafonds inchangés */
        VERIFIER(valides(&app));
    }
    /* Une branche inutilisée dont aucun budget d'ancêtre ne change se déplace sans confirmation. */
    VERIFIER(ref_categorie_deplacer(&app, j.carburant, j.alim, CAT_DEPENSE, false, &w, &r) == RES_OK);
    VERIFIER(rapport_nombre(&w) == 0 && regles_categorie(&app.donnees, j.carburant)->id_parent == j.alim);
    /* Changement de type d'une branche inutilisée : tout le sous-arbre change de type. */
    {
        Id primes = ID_AUCUN;
        Id sous = ID_AUCUN;
        VERIFIER(ref_categorie_creer(&app, "Primes", ID_AUCUN, CAT_REVENU, &primes, &r) == RES_OK);
        VERIFIER(ref_categorie_creer(&app, "Prime annuelle", primes, CAT_REVENU, &sous, &r) == RES_OK);
        VERIFIER(ref_categorie_deplacer(&app, primes, j.alim, CAT_DEPENSE, true, NULL, &r) == RES_OK);
        VERIFIER(regles_categorie(&app.donnees, primes)->type == CAT_DEPENSE
                 && regles_categorie(&app.donnees, sous)->type == CAT_DEPENSE);
        VERIFIER(valides(&app));
        /* Vers la racine, avec choix du type. */
        VERIFIER(ref_categorie_deplacer(&app, primes, ID_AUCUN, CAT_REVENU, true, NULL, &r) == RES_OK);
        VERIFIER(regles_categorie(&app.donnees, primes)->id_parent == ID_AUCUN
                 && regles_categorie(&app.donnees, sous)->type == CAT_REVENU);
    }
    /* Déplacer sous une feuille utilisée : migration I3 préalable, avec accord. */
    {
        Id tr = j.transport;
        VERIFIER(ref_categorie_deplacer(&app, tr, j.courses, CAT_DEPENSE, false, &w, &r) == RES_CONFIRMATION_REQUISE);
        VERIFIER(contient(&w, "Autres"));
        rapport_vider(&w);
        VERIFIER(regles_categorie(&app.donnees, tr)->id_parent == ID_AUCUN);
        VERIFIER(ref_categorie_deplacer(&app, tr, j.courses, CAT_DEPENSE, true, NULL, &r) == RES_OK);
        VERIFIER(regles_categorie(&app.donnees, tr)->id_parent == j.courses
                 && regles_usages_categorie(&app.donnees, j.courses).lignes_operations == 0);
        VERIFIER(valides(&app));
        VERIFIER(total_cat(&app.donnees, D(2026, 11, 1), D(2026, 11, 30), NATURE_DEPENSE, j.alim, NULL) == 6000);
    }
    app_liberer(&app);

    rapport_liberer(&w);
    rapport_liberer(&r);
}

void test_reaffectations(void)
{
    Application app;
    Jeu j;
    RapportErreurs r;
    RapportErreurs w;
    LigneSaisie a[2];
    Id o1;
    Id o2;
    Id o3;
    Id id = ID_AUCUN;

    printf("Réaffectation 1A (catégorie, compte, tiers) : tout ou rien\n");
    rapport_init(&r);
    rapport_init(&w);

    /* --- Catégorie --- */
    preparer(&app, &j);
    a[0] = ligne(j.courses, 4000, ID_AUCUN);
    o1 = creer_op(&app, NATURE_DEPENSE, D(2026, 11, 2), "o1", 4000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    {
        Id modele = brute_modele(&app, j.banque, j.courses);
        Id rec = brute_recurrence(&app, modele);
        Id occ = brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 11, 3), 1000, j.banque,
                          ID_AUCUN, rec, D(2026, 11, 2));
        Id ann = brute_op(&app, NATURE_DEPENSE, STATUT_ANNULEE, date_aucune(), 1000, j.banque,
                          ID_AUCUN, rec, D(2026, 11, 9));
        brute_ligne(&app, occ, j.courses, ID_AUCUN, 1000);
        brute_ligne(&app, ann, j.courses, ID_AUCUN, 1000);
        VERIFIER(valides(&app));
        {
            size_t cats = app.donnees.categories.taille;
            long modifs = app.nb_modifications;
            /* Réaffectations invalides : rien ne change. */
            VERIFIER(ref_categorie_reaffecter(&app, j.courses, j.courses, &r) == RES_ERR_VALIDATION);
            VERIFIER(ref_categorie_reaffecter(&app, j.courses, j.alim, &r) == RES_ERR_VALIDATION);    /* non terminale */
            VERIFIER(ref_categorie_reaffecter(&app, j.courses, j.salaire, &r) == RES_ERR_VALIDATION); /* autre type */
            VERIFIER(ref_categorie_reaffecter(&app, j.courses, 999, &r) == RES_ERR_INTROUVABLE);
            VERIFIER(ref_categorie_reaffecter(&app, j.hygiene, j.courses, &r) == RES_ERR_VALIDATION); /* rien à réaffecter */
            rapport_vider(&r);
            VERIFIER(regles_usages_categorie(&app.donnees, j.courses).lignes_operations == 3
                     && app.nb_modifications == modifs && app.donnees.categories.taille == cats);
        }
        VERIFIER(ref_categorie_reaffecter(&app, j.courses, j.hygiene, &r) == RES_OK);
        VERIFIER(regles_usages_categorie(&app.donnees, j.courses).lignes_operations == 0
                 && regles_usages_categorie(&app.donnees, j.courses).lignes_modeles == 0
                 && regles_usages_categorie(&app.donnees, j.hygiene).lignes_operations == 3
                 && regles_usages_categorie(&app.donnees, j.hygiene).lignes_modeles == 1);
        /* Seule la référence change : montants, statuts, dates et clés d'origine intacts. */
        VERIFIER(regles_operation(&app.donnees, occ)->statut == STATUT_CONFIRMEE
                 && regles_operation(&app.donnees, occ)->montant_total_ct == 1000
                 && regles_operation(&app.donnees, ann)->statut == STATUT_ANNULEE
                 && date_comparer(regles_operation(&app.donnees, ann)->date_prevue_origine, D(2026, 11, 9)) == 0
                 && regles_operation(&app.donnees, ann)->id_recurrence == rec);
        VERIFIER(valides(&app));
        /* La source peut ensuite être supprimée. */
        VERIFIER(ref_categorie_supprimer(&app, j.courses, true, &r) == RES_OK);
    }
    app_liberer(&app);

    /* --- Compte : conflit de virement, G10, Q3, seuil : annulation complète --- */
    preparer(&app, &j);
    a[0] = ligne(j.courses, 1000, ID_AUCUN);
    o1 = creer_op(&app, NATURE_DEPENSE, D(2026, 3, 1), "petit 1", 1000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.courses, 2000, ID_AUCUN);
    o2 = creer_op(&app, NATURE_DEPENSE, D(2026, 3, 2), "petit 2", 2000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    a[0] = ligne(j.courses, 100000, ID_AUCUN);
    o3 = creer_op(&app, NATURE_DEPENSE, D(2026, 3, 3), "gros", 100000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    {
        long modifs = app.nb_modifications;
        Id compteur = app.donnees.prochain_id[ENT_OPERATION];

        /* G10 : le compte « Espèces » (200 DH) ne peut pas recevoir la dépense de 1 000 DH. */
        VERIFIER(ref_compte_reaffecter(&app, j.banque, j.especes, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "solde de fin de journée"));
        rapport_vider(&r);
        VERIFIER(regles_operation(&app.donnees, o1)->id_compte_source == j.banque
                 && regles_operation(&app.donnees, o2)->id_compte_source == j.banque
                 && regles_operation(&app.donnees, o3)->id_compte_source == j.banque
                 && app.nb_modifications == modifs && app.donnees.prochain_id[ENT_OPERATION] == compteur);
        /* Q3 : « Neuf » ouvre le 2026-06-01, les opérations datent de mars. */
        VERIFIER(ref_compte_reaffecter(&app, j.banque, j.neuf, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "précède la date du solde initial"));
        rapport_vider(&r);
        VERIFIER(regles_operation(&app.donnees, o1)->id_compte_source == j.banque);
        /* Compte source inutilisé, compte identique, compte inconnu. */
        VERIFIER(ref_compte_reaffecter(&app, j.neuf, j.banque, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(ref_compte_reaffecter(&app, j.banque, j.banque, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(ref_compte_reaffecter(&app, j.banque, 999, true, NULL, &r) == RES_ERR_INTROUVABLE);
        rapport_vider(&r);
    }
    /* Conflit : un virement Banque -> Espèces interdit la réaffectation Espèces -> Banque. */
    creer_op(&app, NATURE_VIREMENT, D(2026, 3, 4), "retrait", 5000, j.banque, j.especes, ID_AUCUN, NULL, 0);
    {
        long modifs = app.nb_modifications;
        VERIFIER(ref_compte_reaffecter(&app, j.especes, j.banque, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "virement entre ces deux comptes"));
        rapport_vider(&r);
        VERIFIER(app.nb_modifications == modifs);
    }
    app_liberer(&app);

    /* Réaffectation de compte réussie, avec avertissement de seuil. */
    preparer(&app, &j);
    a[0] = ligne(j.courses, 6000, ID_AUCUN);
    o1 = creer_op(&app, NATURE_DEPENSE, D(2026, 3, 1), "o1", 6000, j.banque, ID_AUCUN, ID_AUCUN, a, 1);
    {
        long long solde;
        long modifs = app.nb_modifications;
        /* « Seuil » : 100 DH, seuil 50 DH ; recevoir 60 DH la ramène à 40 DH. */
        VERIFIER(ref_compte_reaffecter(&app, j.banque, j.seuil, false, &w, &r) == RES_CONFIRMATION_REQUISE);
        VERIFIER(contient(&w, "Seuil") && app.nb_modifications == modifs
                 && regles_operation(&app.donnees, o1)->id_compte_source == j.banque);
        rapport_vider(&w);
        VERIFIER(ref_compte_reaffecter(&app, j.banque, j.seuil, true, NULL, &r) == RES_OK);
        VERIFIER(regles_operation(&app.donnees, o1)->id_compte_source == j.seuil);
        VERIFIER(solde_compte_final(&app.donnees, j.seuil, &solde) && solde == 4000);
        VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 500000);
        VERIFIER(valides(&app));
        VERIFIER(ref_compte_supprimer(&app, j.banque, &r) == RES_OK);
    }
    app_liberer(&app);

    /* --- Tiers --- */
    preparer(&app, &j);
    a[0] = ligne(j.courses, 1000, ID_AUCUN);
    o1 = creer_op(&app, NATURE_DEPENSE, D(2026, 3, 1), "o1", 1000, j.banque, ID_AUCUN, j.carrefour, a, 1);
    o2 = creer_op(&app, NATURE_DEPENSE, D(2026, 3, 2), "o2", 1000, j.banque, ID_AUCUN, j.carrefour, a, 1);
    VERIFIER(ref_tiers_supprimer(&app, j.carrefour, &r) == RES_ERR_VALIDATION);       /* utilisé : bloqué */
    rapport_vider(&r);
    VERIFIER(ref_tiers_reaffecter(&app, j.carrefour, j.carrefour, &r) == RES_ERR_VALIDATION);
    VERIFIER(ref_tiers_reaffecter(&app, j.carrefour, 999, &r) == RES_ERR_INTROUVABLE);
    VERIFIER(ref_tiers_reaffecter(&app, j.afriquia, j.carrefour, &r) == RES_ERR_VALIDATION);   /* source inutilisée */
    rapport_vider(&r);
    VERIFIER(ref_tiers_reaffecter(&app, j.carrefour, j.afriquia, &r) == RES_OK);
    VERIFIER(regles_operation(&app.donnees, o1)->id_tiers == j.afriquia
             && regles_operation(&app.donnees, o2)->id_tiers == j.afriquia);
    VERIFIER(ref_tiers_supprimer(&app, j.carrefour, &r) == RES_OK);
    (void)id;
    app_liberer(&app);

    rapport_liberer(&w);
    rapport_liberer(&r);
}

/* ================================================================== */
/* Étape 4 : calendrier, modèles, récurrences, échéances, lots H5      */
/* ================================================================== */

static bool date_egale(Date a, Date b)
{
    return date_comparer(a, b) == 0;
}

void test_calendrier(void)
{
    Date d;
    Recurrence r;

    printf("Calendrier des récurrences (G5, Q5, Q6, jour 31, années bissextiles)\n");
    /* Première échéance mensuelle : exemples de l'audit (G5). */
    VERIFIER(date_egale(cal_premiere_echeance(FREQ_MENSUELLE, D(2026, 11, 20), 10), D(2026, 12, 10)));
    VERIFIER(date_egale(cal_premiere_echeance(FREQ_MENSUELLE, D(2026, 11, 5), 31), D(2026, 11, 30)));
    VERIFIER(date_egale(cal_premiere_echeance(FREQ_MENSUELLE, D(2026, 11, 10), 10), D(2026, 11, 10)));
    VERIFIER(date_egale(cal_premiere_echeance(FREQ_MENSUELLE, D(2026, 11, 11), 10), D(2026, 12, 10)));
    VERIFIER(date_egale(cal_premiere_echeance(FREQ_MENSUELLE, D(2026, 12, 20), 10), D(2027, 1, 10)));
    VERIFIER(date_egale(cal_premiere_echeance(FREQ_HEBDOMADAIRE, D(2026, 11, 20), 0), D(2026, 11, 20)));
    /* Jour souhaité 31 en 2027 : 31 janvier, 28 février, 31 mars, 30 avril, puis 31 mai (MLD §8). */
    d = D(2027, 1, 31);
    d = cal_echeance_suivante(FREQ_MENSUELLE, d, 31);
    VERIFIER(date_egale(d, D(2027, 2, 28)));
    d = cal_echeance_suivante(FREQ_MENSUELLE, d, 31);
    VERIFIER(date_egale(d, D(2027, 3, 31)));
    d = cal_echeance_suivante(FREQ_MENSUELLE, d, 31);
    VERIFIER(date_egale(d, D(2027, 4, 30)));
    d = cal_echeance_suivante(FREQ_MENSUELLE, d, 31);
    VERIFIER(date_egale(d, D(2027, 5, 31)));
    /* Année bissextile et changement d'année. */
    VERIFIER(date_egale(cal_echeance_suivante(FREQ_MENSUELLE, D(2028, 1, 31), 31), D(2028, 2, 29)));
    VERIFIER(date_egale(cal_echeance_suivante(FREQ_MENSUELLE, D(2028, 2, 29), 31), D(2028, 3, 31)));
    VERIFIER(date_egale(cal_echeance_suivante(FREQ_MENSUELLE, D(2026, 12, 15), 15), D(2027, 1, 15)));
    VERIFIER(date_absente(cal_echeance_suivante(FREQ_MENSUELLE, D(9999, 12, 15), 15)));
    /* Hebdomadaire : sept jours, y compris en fin d'année et en année bissextile. */
    VERIFIER(date_egale(cal_echeance_suivante(FREQ_HEBDOMADAIRE, D(2026, 12, 28), 0), D(2027, 1, 4)));
    VERIFIER(date_egale(cal_echeance_suivante(FREQ_HEBDOMADAIRE, D(2028, 2, 24), 0), D(2028, 3, 2)));
    VERIFIER(date_absente(cal_echeance_suivante(FREQ_HEBDOMADAIRE, D(9999, 12, 29), 0)));
    /* Date de fin inclusive ; absente = sans limite ; échéance absente reste absente. */
    VERIFIER(date_egale(cal_borner(D(2026, 12, 10), D(2026, 12, 10)), D(2026, 12, 10)));
    VERIFIER(date_absente(cal_borner(D(2026, 12, 11), D(2026, 12, 10))));
    VERIFIER(date_egale(cal_borner(D(2030, 1, 1), date_aucune()), D(2030, 1, 1)));
    VERIFIER(date_absente(cal_borner(date_aucune(), D(2026, 12, 10))));
    /* Appartenance au calendrier. */
    memset(&r, 0, sizeof r);
    r.frequence = FREQ_HEBDOMADAIRE;
    r.date_debut = D(2026, 11, 2);
    r.date_fin = D(2026, 11, 30);
    VERIFIER(cal_sur_calendrier(&r, D(2026, 11, 2)) && cal_sur_calendrier(&r, D(2026, 11, 16))
             && cal_sur_calendrier(&r, D(2026, 11, 30)));
    VERIFIER(!cal_sur_calendrier(&r, D(2026, 11, 3)) && !cal_sur_calendrier(&r, D(2026, 10, 26))
             && !cal_sur_calendrier(&r, D(2026, 12, 7)));
    r.frequence = FREQ_MENSUELLE;
    r.jour_mensuel_souhaite = 31;
    r.date_debut = D(2027, 1, 31);
    r.date_fin = date_aucune();
    VERIFIER(cal_sur_calendrier(&r, D(2027, 2, 28)) && cal_sur_calendrier(&r, D(2027, 3, 31))
             && cal_sur_calendrier(&r, D(2027, 4, 30)));
    VERIFIER(!cal_sur_calendrier(&r, D(2027, 2, 27)) && !cal_sur_calendrier(&r, D(2027, 3, 30))
             && !cal_sur_calendrier(&r, D(2026, 12, 31)));
}

/* Récurrence brute (contourne les contrôles) avec calendrier et curseur choisis. */
static Id brute_rec_custom(Application *app, Id modele, Frequence f, Date debut, Date fin, int jour,
                           bool active, Date curseur)
{
    Recurrence x;

    memset(&x, 0, sizeof x);
    x.id = donnees_nouvel_id(&app->donnees, ENT_RECURRENCE);
    x.id_modele = modele;
    x.frequence = f;
    x.date_debut = debut;
    x.date_fin = fin;
    x.jour_mensuel_souhaite = jour;
    x.active = active;
    x.date_prochaine_echeance = curseur;
    tableau_ajouter(&app->donnees.recurrences, &x);
    return x.id;
}

void test_validateur_recurrences(void)
{
    Application app;
    Jeu j;
    Id modele;
    Id rec;
    Id op;

    printf("Validateur V09/V10 : curseur et occurrences sur le calendrier\n");
    preparer(&app, &j);
    modele = brute_modele(&app, j.banque, j.courses);
    rec = brute_rec_custom(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, true,
                           D(2026, 11, 16));
    op = brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, date_aucune(), 1000, j.banque, ID_AUCUN,
                  rec, D(2026, 11, 9));
    brute_ligne(&app, op, j.courses, ID_AUCUN, 1000);
    VERIFIER(valides(&app));                                   /* occurrence 11-09 < curseur 11-16 */
    app.donnees.recurrences.taille = 0;
    rec = brute_rec_custom(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, true,
                           D(2026, 11, 17));                   /* mardi : hors calendrier */
    ELEMENT(&app.donnees.operations, Operation, 0)->id_recurrence = rec;
    VERIFIER(refusees_avec(&app, "n'est pas sur le calendrier"));
    app.donnees.recurrences.taille = 0;
    rec = brute_rec_custom(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, true,
                           D(2026, 11, 9));                    /* curseur sur une occurrence existante */
    ELEMENT(&app.donnees.operations, Operation, 0)->id_recurrence = rec;
    VERIFIER(refusees_avec(&app, "doit être postérieure à l'occurrence"));
    app.donnees.recurrences.taille = 0;
    rec = brute_rec_custom(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, true,
                           date_aucune());                     /* NULL sans date de fin */
    ELEMENT(&app.donnees.operations, Operation, 0)->id_recurrence = rec;
    VERIFIER(refusees_avec(&app, "sans date de fin"));
    app.donnees.recurrences.taille = 0;
    rec = brute_rec_custom(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), D(2026, 11, 30), 0, true,
                           date_aucune());                     /* NULL avec date de fin : terminée */
    ELEMENT(&app.donnees.operations, Operation, 0)->id_recurrence = rec;
    VERIFIER(valides(&app));
    /* Occurrence hors calendrier (mardi) ou au-delà de la date de fin. */
    ELEMENT(&app.donnees.operations, Operation, 0)->date_prevue_origine = D(2026, 11, 10);
    VERIFIER(refusees_avec(&app, "n'est pas une échéance du calendrier"));
    ELEMENT(&app.donnees.operations, Operation, 0)->date_prevue_origine = D(2026, 12, 7);
    VERIFIER(refusees_avec(&app, "n'est pas une échéance du calendrier"));
    ELEMENT(&app.donnees.operations, Operation, 0)->date_prevue_origine = D(2026, 10, 26);
    VERIFIER(refusees_avec(&app, "n'est pas une échéance du calendrier"));      /* avant le début */
    app_liberer(&app);
}

static Id modele_simple(Application *app, Id compte_id, long long total, bool deux_lignes)
{
    ModeleSaisie m;
    LigneSaisie l[2];
    RapportErreurs r;
    Id id = ID_AUCUN;
    size_t nb;

    memset(&m, 0, sizeof m);
    m.nature = NATURE_DEPENSE;
    texte_copier(m.nom, sizeof m.nom, "Abonnement");
    texte_copier(m.libelle, sizeof m.libelle, "Abonnement mensuel");
    m.montant_total_ct = total;
    m.id_compte_source = compte_id;
    /* Catégories du jeu de test : 2 = Courses, 3 = Hygiène. */
    if (deux_lignes) {
        l[0] = ligne(2, total * 2 / 3, ID_AUCUN);
        l[1] = ligne(3, total - total * 2 / 3, ID_AUCUN);
        nb = 2;
    } else {
        l[0] = ligne(2, total, ID_AUCUN);
        nb = 1;
    }
    rapport_init(&r);
    VERIFIER(mod_creer(app, &m, l, nb, &id, &r) == RES_OK);
    if (rapport_nombre(&r) > 0) {
        rapport_afficher(&r, stdout);
    }
    rapport_liberer(&r);
    return id;
}

static Id rec_nouvelle(Application *app, Id modele, Frequence f, Date debut, Date fin, int jour,
                       ChoixRetro choix, size_t *nb)
{
    RecurrenceSaisie s;
    RapportErreurs r;
    Id id = ID_AUCUN;
    size_t n = 0;

    s.id_modele = modele;
    s.frequence = f;
    s.date_debut = debut;
    s.date_fin = fin;
    s.jour_mensuel_souhaite = jour;
    rapport_init(&r);
    VERIFIER(rec_creer(app, &s, choix, &id, &n, &r) == RES_OK);
    if (rapport_nombre(&r) > 0) {
        rapport_afficher(&r, stdout);
    }
    rapport_liberer(&r);
    if (nb != NULL) {
        *nb = n;
    }
    return id;
}

/* Date prévue de la k-ième occurrence (ordre de création) d'une récurrence. */
static Date prevue_occ(const Application *app, Id rec, size_t k)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < app->donnees.operations.taille; i++) {
        const Operation *o = ELEMENT(&app->donnees.operations, Operation, i);
        if (o->id_recurrence == rec) {
            if (n == k) {
                return o->date_prevue_origine;
            }
            n++;
        }
    }
    return date_aucune();
}

void test_modeles(void)
{
    Application app;
    Jeu j;
    ModeleSaisie m;
    LigneSaisie l[2];
    RapportErreurs r;
    RapportErreurs w;
    Id mod = ID_AUCUN;
    Id op = ID_AUCUN;

    printf("Modèles d'opération et opération créée depuis un modèle\n");
    rapport_init(&r);
    rapport_init(&w);
    preparer(&app, &j);
    memset(&m, 0, sizeof m);
    m.nature = NATURE_DEPENSE;
    texte_copier(m.nom, sizeof m.nom, "Loyer");
    texte_copier(m.libelle, sizeof m.libelle, "Loyer du mois");
    m.montant_total_ct = 12000;
    m.id_compte_source = j.banque;
    m.id_tiers = j.afriquia;
    l[0] = ligne(j.courses, 8000, ID_AUCUN);
    l[1] = ligne(j.hygiene, 3000, ID_AUCUN);
    VERIFIER(mod_creer(&app, &m, l, 2, &mod, &r) == RES_ERR_VALIDATION);       /* 11 000 ≠ 12 000 */
    VERIFIER(contient(&r, "diffère du montant total") && app.donnees.modeles.taille == 0);
    rapport_vider(&r);
    l[1].montant_ct = 4000;
    VERIFIER(mod_creer(&app, &m, l, 2, &mod, &r) == RES_OK);
    VERIFIER(regles_modele(&app.donnees, mod) != NULL && mod_nombre_lignes(&app.donnees, mod) == 2);
    m.nature = NATURE_VIREMENT;                    /* un virement n'a pas de ligne de catégorie */
    VERIFIER(mod_creer(&app, &m, l, 2, &mod, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    m.nature = NATURE_DEPENSE;
    m.nom[0] = '\0';
    VERIFIER(mod_creer(&app, &m, l, 2, &mod, &r) == RES_ERR_VALIDATION);       /* nom obligatoire */
    rapport_vider(&r);
    texte_copier(m.nom, sizeof m.nom, "Loyer");
    l[0] = ligne(j.alim, 8000, ID_AUCUN);                                        /* catégorie non terminale */
    VERIFIER(mod_creer(&app, &m, l, 2, &mod, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);

    /* Opération depuis le modèle : valeurs et lignes copiées, confirmée (G2), B3/G10 appliqués. */
    VERIFIER(op_creer_depuis_modele(&app, 1, D(2026, 11, 10), true, &op, NULL, &r) == RES_OK);
    VERIFIER(op_sure(&app.donnees, op)->statut == STATUT_CONFIRMEE
             && op_sure(&app.donnees, op)->montant_total_ct == 12000
             && op_sure(&app.donnees, op)->id_tiers == j.afriquia
             && op_sure(&app.donnees, op)->id_recurrence == ID_AUCUN
             && op_nombre_lignes(&app.donnees, op) == 2
             && strcmp(op_sure(&app.donnees, op)->libelle, "Loyer du mois") == 0);
    VERIFIER(op_creer_depuis_modele(&app, 1, D(2026, 11, 14), true, &op, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "postérieure"));
    rapport_vider(&r);
    VERIFIER(op_creer_depuis_modele(&app, 99, D(2026, 11, 10), true, &op, NULL, &r) == RES_ERR_INTROUVABLE);
    rapport_vider(&r);

    /* Modifier un modèle ne réécrit pas les opérations déjà créées. */
    memset(&m, 0, sizeof m);
    m.nature = NATURE_DEPENSE;
    texte_copier(m.nom, sizeof m.nom, "Loyer");
    texte_copier(m.libelle, sizeof m.libelle, "Loyer révisé");
    m.montant_total_ct = 15000;
    m.id_compte_source = j.banque;
    l[0] = ligne(j.courses, 15000, ID_AUCUN);
    VERIFIER(mod_modifier(&app, 1, &m, l, 1, &r) == RES_OK);
    VERIFIER(mod_nombre_lignes(&app.donnees, 1) == 1 && regles_modele(&app.donnees, 1)->montant_total_ct == 15000);
    VERIFIER(op_sure(&app.donnees, op)->montant_total_ct == 12000 && op_nombre_lignes(&app.donnees, op) == 2);
    VERIFIER(mod_modifier(&app, 99, &m, l, 1, &r) == RES_ERR_INTROUVABLE);
    rapport_vider(&r);

    /* Utilisé par une récurrence : nature figée, suppression refusée. */
    {
        Id rec = rec_nouvelle(&app, 1, FREQ_HEBDOMADAIRE, D(2026, 11, 16), date_aucune(), 0, RETRO_GENERER, NULL);
        (void)rec;
        m.nature = NATURE_REVENU;
        m.id_compte_source = ID_AUCUN;
        m.id_compte_destination = j.banque;
        l[0] = ligne(j.salaire, 15000, ID_AUCUN);
        VERIFIER(mod_modifier(&app, 1, &m, l, 1, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "nature"));
        rapport_vider(&r);
        VERIFIER(mod_supprimer(&app, 1, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "récurrence"));
        rapport_vider(&r);
    }
    app_liberer(&app);

    /* Un modèle libre se supprime avec ses lignes. */
    preparer(&app, &j);
    mod = modele_simple(&app, j.banque, 3000, false);
    VERIFIER(app.donnees.lignes_modeles.taille == 1);
    VERIFIER(mod_supprimer(&app, mod, &r) == RES_OK);
    VERIFIER(app.donnees.modeles.taille == 0 && app.donnees.lignes_modeles.taille == 0);
    app_liberer(&app);
    rapport_liberer(&w);
    rapport_liberer(&r);
}

void test_recurrences(void)
{
    Application app;
    Jeu j;
    RapportErreurs r;
    ApercuEcheances ap;
    RecurrenceSaisie s;
    Id modele;
    Id rec;
    Id id = ID_AUCUN;
    size_t nb = 0;

    printf("Récurrences : création rétroactive G5, rattrapage, pause Q4, clôture G6\n");
    rapport_init(&r);

    /* --- Première échéance future : rien à générer --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    s.id_modele = modele;
    s.frequence = FREQ_MENSUELLE;
    s.date_debut = D(2026, 11, 5);
    s.date_fin = date_aucune();
    s.jour_mensuel_souhaite = 31;
    VERIFIER(rec_apercu(&s, app.date_reference, &ap) && ap.nombre == 0 && date_egale(ap.curseur, D(2026, 11, 30)));
    rec = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2026, 11, 5), date_aucune(), 31, RETRO_GENERER, &nb);
    VERIFIER(nb == 0 && rec_nombre_occurrences(&app.donnees, rec) == 0
             && date_egale(regles_recurrence(&app.donnees, rec)->date_prochaine_echeance, D(2026, 11, 30))
             && regles_recurrence(&app.donnees, rec)->active);
    app_liberer(&app);

    /* --- G5 rétroactif : 4 échéances dues (19/10, 26/10, 2/11, 9/11) --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    s.frequence = FREQ_HEBDOMADAIRE;
    s.date_debut = D(2026, 10, 19);
    s.jour_mensuel_souhaite = 0;
    s.id_modele = modele;
    VERIFIER(rec_apercu(&s, app.date_reference, &ap) && ap.nombre == 4 && date_egale(ap.premiere, D(2026, 10, 19))
             && date_egale(ap.derniere, D(2026, 11, 9)) && date_egale(ap.curseur, D(2026, 10, 19)));
    {
        long modifs = app.nb_modifications;
        Id compteur = app.donnees.prochain_id[ENT_RECURRENCE];
        VERIFIER(rec_creer(&app, &s, RETRO_ANNULER, &id, &nb, &r) == RES_ABANDON);      /* Annuler */
        VERIFIER(app.donnees.recurrences.taille == 0 && app.nb_modifications == modifs
                 && app.donnees.prochain_id[ENT_RECURRENCE] == compteur);
        rapport_vider(&r);
    }
    VERIFIER(rec_creer(&app, &s, RETRO_GENERER, &id, &nb, &r) == RES_OK && nb == 4);   /* Générer */
    VERIFIER(rec_nombre_occurrences(&app.donnees, id) == 4);
    VERIFIER(date_egale(prevue_occ(&app, id, 0), D(2026, 10, 19)) && date_egale(prevue_occ(&app, id, 3), D(2026, 11, 9)));
    VERIFIER(date_egale(regles_recurrence(&app.donnees, id)->date_prochaine_echeance, D(2026, 11, 16)));
    {
        const Operation *o = op_sure(&app.donnees, 1);
        VERIFIER(o->statut == STATUT_A_CONFIRMER && date_absente(o->date_realisation)
                 && o->id_recurrence == id && o->montant_total_ct == 12000
                 && op_nombre_lignes(&app.donnees, 1) == 2
                 && strcmp(o->libelle, "Abonnement mensuel") == 0);
        VERIFIER(valides(&app));
    }
    /* Les occurrences A_CONFIRMER n'ont aucun effet financier. */
    {
        long long solde;
        VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 500000);
    }
    app_liberer(&app);

    /* --- G5 Ignorer : rien n'est créé et les échéances ignorées ne reviennent jamais --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 10, 19), date_aucune(), 0, RETRO_IGNORER, &nb);
    VERIFIER(nb == 0 && rec_nombre_occurrences(&app.donnees, id) == 0
             && date_egale(regles_recurrence(&app.donnees, id)->date_prochaine_echeance, D(2026, 11, 16)));
    VERIFIER(rec_rattraper(&app, &nb, &r) == RES_OK && nb == 0);
    app.date_reference = D(2026, 11, 20);
    VERIFIER(rec_rattraper(&app, &nb, &r) == RES_OK && nb == 1);                    /* seulement 11-16 */
    VERIFIER(date_egale(prevue_occ(&app, id, 0), D(2026, 11, 16)));
    app_liberer(&app);

    /* --- Une échéance le jour même est due (<=) --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 13), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(nb == 1 && date_egale(regles_recurrence(&app.donnees, id)->date_prochaine_echeance, D(2026, 11, 20)));
    app_liberer(&app);

    /* --- Date de fin inclusive puis curseur NULL, sans nouvelle génération --- */
    preparer(&app, &j);
    app.date_reference = D(2026, 12, 31);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2026, 10, 10), D(2026, 12, 10), 10, RETRO_GENERER, &nb);
    VERIFIER(nb == 3 && date_egale(prevue_occ(&app, id, 2), D(2026, 12, 10)));
    VERIFIER(date_absente(regles_recurrence(&app.donnees, id)->date_prochaine_echeance)
             && regles_recurrence(&app.donnees, id)->active);                       /* terminée ≠ en pause */
    VERIFIER(valides(&app));
    app.date_reference = D(2027, 6, 1);
    VERIFIER(rec_rattraper(&app, &nb, &r) == RES_OK && nb == 0 && rec_nombre_occurrences(&app.donnees, id) == 3);
    /* Fin entre deux échéances (9 décembre) : la dernière est le 10 novembre. */
    app.date_reference = D(2026, 12, 31);
    id = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2026, 10, 10), D(2026, 12, 9), 10, RETRO_GENERER, &nb);
    VERIFIER(nb == 2 && date_absente(regles_recurrence(&app.donnees, id)->date_prochaine_echeance));
    /* Fin avant la première échéance : acceptée, curseur NULL. */
    id = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2027, 2, 20), D(2027, 2, 28), 10, RETRO_GENERER, &nb);
    VERIFIER(nb == 0 && date_absente(regles_recurrence(&app.donnees, id)->date_prochaine_echeance));
    VERIFIER(valides(&app));
    app_liberer(&app);

    /* --- Jour 31 : 31/01, 28/02, 31/03, 30/04 en 2027 ; 29/02 en 2028 --- */
    preparer(&app, &j);
    app.date_reference = D(2027, 5, 1);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2027, 1, 31), date_aucune(), 31, RETRO_GENERER, &nb);
    VERIFIER(nb == 4 && date_egale(prevue_occ(&app, id, 0), D(2027, 1, 31))
             && date_egale(prevue_occ(&app, id, 1), D(2027, 2, 28))
             && date_egale(prevue_occ(&app, id, 2), D(2027, 3, 31))
             && date_egale(prevue_occ(&app, id, 3), D(2027, 4, 30))
             && date_egale(regles_recurrence(&app.donnees, id)->date_prochaine_echeance, D(2027, 5, 31))
             && regles_recurrence(&app.donnees, id)->jour_mensuel_souhaite == 31);
    app_liberer(&app);
    preparer(&app, &j);
    app.date_reference = D(2028, 3, 15);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2028, 1, 31), date_aucune(), 31, RETRO_GENERER, &nb);
    VERIFIER(nb == 2 && date_egale(prevue_occ(&app, id, 1), D(2028, 2, 29)));
    app_liberer(&app);

    /* --- Rattrapage au démarrage : une seule fois, sans doublon --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 16), date_aucune(), 0, RETRO_GENERER, &nb);
    app.date_reference = D(2026, 12, 1);
    app.modifie = false;
    VERIFIER(rec_rattraper(&app, &nb, &r) == RES_OK && nb == 3 && app.modifie);     /* 16/11, 23/11, 30/11 */
    app.modifie = false;
    VERIFIER(rec_rattraper(&app, &nb, &r) == RES_OK && nb == 0 && !app.modifie);
    VERIFIER(rec_nombre_occurrences(&app.donnees, id) == 3 && valides(&app));

    /* --- Pause Q4 : rattrapage avant la pause, rien pendant la pause, reprise strictement future --- */
    app.date_reference = D(2026, 12, 8);                       /* 7/12 est due */
    VERIFIER(rec_desactiver(&app, id, &nb, &r) == RES_OK && nb == 1);
    VERIFIER(!regles_recurrence(&app.donnees, id)->active && rec_nombre_occurrences(&app.donnees, id) == 4);
    VERIFIER(rec_desactiver(&app, id, &nb, &r) == RES_ERR_VALIDATION);              /* déjà en pause */
    rapport_vider(&r);
    app.date_reference = D(2026, 12, 28);                      /* échéances 14/12, 21/12, 28/12 en pause */
    VERIFIER(rec_rattraper(&app, &nb, &r) == RES_OK && nb == 0 && rec_nombre_occurrences(&app.donnees, id) == 4);
    VERIFIER(rec_reactiver(&app, id, &r) == RES_OK);           /* le 28/12 est un jour d'échéance */
    VERIFIER(regles_recurrence(&app.donnees, id)->active && rec_nombre_occurrences(&app.donnees, id) == 4
             && date_egale(regles_recurrence(&app.donnees, id)->date_prochaine_echeance, D(2027, 1, 4)));
    VERIFIER(rec_reactiver(&app, id, &r) == RES_ERR_VALIDATION);                    /* déjà active */
    rapport_vider(&r);
    VERIFIER(valides(&app));
    app.date_reference = D(2027, 1, 4);                        /* la nouvelle échéance, due le jour même */
    VERIFIER(rec_rattraper(&app, &nb, &r) == RES_OK && nb == 1);
    app_liberer(&app);

    /* Réactivation d'une récurrence terminée : le curseur reste NULL, rien n'est rattrapé. */
    preparer(&app, &j);
    app.date_reference = D(2026, 12, 31);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2026, 10, 10), D(2026, 12, 10), 10, RETRO_GENERER, &nb);
    VERIFIER(rec_desactiver(&app, id, &nb, &r) == RES_OK && nb == 0);
    app.date_reference = D(2027, 3, 1);
    VERIFIER(rec_reactiver(&app, id, &r) == RES_OK);
    VERIFIER(date_absente(regles_recurrence(&app.donnees, id)->date_prochaine_echeance)
             && rec_nombre_occurrences(&app.donnees, id) == 3);
    app_liberer(&app);

    /* --- Clôture G6 --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(nb == 2);                                             /* 2/11 et 9/11 ; curseur 16/11 */
    {
        long modifs = app.nb_modifications;
        VERIFIER(rec_cloturer(&app, id, D(2026, 11, 8), &r) == RES_ERR_VALIDATION);          /* avant l'occurrence du 9 */
        VERIFIER(contient(&r, "Conflit") && contient(&r, "2026-11-09"));
        rapport_vider(&r);
        VERIFIER(rec_cloturer(&app, id, D(2026, 10, 1), &r) == RES_ERR_VALIDATION);          /* avant le début */
        rapport_vider(&r);
        VERIFIER(app.nb_modifications == modifs
                 && date_absente(regles_recurrence(&app.donnees, id)->date_fin));
    }
    VERIFIER(rec_cloturer(&app, id, D(2026, 11, 12), &r) == RES_OK);                        /* entre deux échéances */
    VERIFIER(date_egale(regles_recurrence(&app.donnees, id)->date_fin, D(2026, 11, 12))
             && date_absente(regles_recurrence(&app.donnees, id)->date_prochaine_echeance)
             && rec_nombre_occurrences(&app.donnees, id) == 2 && valides(&app));
    VERIFIER(rec_cloturer(&app, id, D(2026, 11, 30), &r) == RES_ERR_VALIDATION);            /* pas d'allongement */
    VERIFIER(contient(&r, "raccourcir"));
    rapport_vider(&r);
    VERIFIER(rec_cloturer(&app, id, D(2026, 11, 9), &r) == RES_OK);                         /* fin = dernière occurrence */
    /* Calendrier figé après la première occurrence. */
    s.id_modele = modele;
    s.frequence = FREQ_MENSUELLE;
    s.date_debut = D(2026, 11, 2);
    s.date_fin = date_aucune();
    s.jour_mensuel_souhaite = 2;
    VERIFIER(rec_modifier_calendrier(&app, id, &s, RETRO_GENERER, &nb, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "figé"));
    rapport_vider(&r);
    /* Suppression : refusée avec des occurrences. */
    VERIFIER(rec_supprimer(&app, id, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    VERIFIER(rec_cloturer(&app, 99, D(2026, 11, 9), &r) == RES_ERR_INTROUVABLE);
    rapport_vider(&r);
    app_liberer(&app);

    /* Avant la première occurrence : calendrier modifiable (G5 relancé), sinon suppression libre. */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2026, 11, 20), date_aucune(), 10, RETRO_GENERER, &nb);
    VERIFIER(nb == 0 && date_egale(regles_recurrence(&app.donnees, id)->date_prochaine_echeance, D(2026, 12, 10)));
    s.id_modele = modele;
    s.frequence = FREQ_HEBDOMADAIRE;
    s.date_debut = D(2026, 11, 2);
    s.date_fin = date_aucune();
    s.jour_mensuel_souhaite = 0;
    VERIFIER(rec_modifier_calendrier(&app, id, &s, RETRO_ANNULER, &nb, &r) == RES_ABANDON);
    rapport_vider(&r);
    VERIFIER(rec_modifier_calendrier(&app, id, &s, RETRO_GENERER, &nb, &r) == RES_OK && nb == 2);
    VERIFIER(regles_recurrence(&app.donnees, id)->frequence == FREQ_HEBDOMADAIRE);
    s.id_modele = 99;
    VERIFIER(rec_modifier_calendrier(&app, id, &s, RETRO_GENERER, &nb, &r) == RES_ERR_VALIDATION);   /* modèle immuable */
    rapport_vider(&r);
    app_liberer(&app);
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    id = rec_nouvelle(&app, modele, FREQ_MENSUELLE, D(2026, 11, 20), date_aucune(), 10, RETRO_GENERER, &nb);
    VERIFIER(rec_supprimer(&app, id, &r) == RES_OK && app.donnees.recurrences.taille == 0);
    /* Saisies invalides et excès d'échéances dues. */
    s.id_modele = modele;
    s.frequence = FREQ_MENSUELLE;
    s.date_debut = D(2026, 11, 5);
    s.date_fin = D(2026, 11, 1);
    s.jour_mensuel_souhaite = 5;
    VERIFIER(rec_creer(&app, &s, RETRO_GENERER, &id, &nb, &r) == RES_ERR_VALIDATION);   /* fin < début */
    rapport_vider(&r);
    s.date_fin = date_aucune();
    s.jour_mensuel_souhaite = 32;
    VERIFIER(rec_creer(&app, &s, RETRO_GENERER, &id, &nb, &r) == RES_ERR_VALIDATION);   /* jour hors 1..31 */
    rapport_vider(&r);
    s.id_modele = 99;
    s.jour_mensuel_souhaite = 5;
    VERIFIER(rec_creer(&app, &s, RETRO_GENERER, &id, &nb, &r) == RES_ERR_VALIDATION);   /* modèle inconnu */
    rapport_vider(&r);
    s.id_modele = modele;
    s.frequence = FREQ_HEBDOMADAIRE;
    s.date_debut = D(1900, 1, 8);                                                       /* ~6 700 échéances dues */
    VERIFIER(rec_creer(&app, &s, RETRO_GENERER, &id, &nb, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "500") && app.donnees.recurrences.taille == 0 && app.donnees.operations.taille == 0);
    rapport_vider(&r);
    app_liberer(&app);

    rapport_liberer(&r);
}

void test_echeances_et_lots(void)
{
    Application app;
    Jeu j;
    RapportErreurs r;
    RapportErreurs w;
    LigneSaisie pool[4];
    ConfirmationLot lot[3];
    Id modele;
    Id rec;
    size_t nb = 0;
    long modifs;

    printf("Échéances : confirmation (date explicite), refus, annulation, lots tout ou rien (H5)\n");
    rapport_init(&r);
    rapport_init(&w);

    /* --- Confirmation individuelle --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    rec = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(nb == 2);                                                  /* occurrences 1 (2/11) et 2 (9/11) */
    {
        long long solde;
        VERIFIER(op_confirmer(&app, 1, date_aucune(), 0, NULL, 0, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "obligatoire"));
        rapport_vider(&r);
        VERIFIER(op_confirmer(&app, 1, D(2026, 11, 14), 0, NULL, 0, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "postérieure"));
        rapport_vider(&r);
        VERIFIER(op_sure(&app.donnees, 1)->statut == STATUT_A_CONFIRMER);
        VERIFIER(op_confirmer(&app, 99, D(2026, 11, 3), 0, NULL, 0, true, NULL, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
        /* La date réelle peut différer de la date prévue (corrigée à la confirmation) ; la clé reste. */
        VERIFIER(op_confirmer(&app, 1, D(2026, 11, 4), 0, NULL, 0, true, NULL, &r) == RES_OK);
        VERIFIER(op_sure(&app.donnees, 1)->statut == STATUT_CONFIRMEE
                 && date_egale(op_sure(&app.donnees, 1)->date_realisation, D(2026, 11, 4))
                 && date_egale(op_sure(&app.donnees, 1)->date_prevue_origine, D(2026, 11, 2))
                 && op_sure(&app.donnees, 1)->id_recurrence == rec);
        VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 488000);
        VERIFIER(op_confirmer(&app, 1, D(2026, 11, 4), 0, NULL, 0, true, NULL, &r) == RES_ERR_VALIDATION);  /* déjà confirmée */
        rapport_vider(&r);
        /* Annulation après confirmation : date conservée, effet financier retiré, état final. */
        VERIFIER(op_annuler(&app, 1, true, NULL, &r) == RES_OK);
        VERIFIER(op_sure(&app.donnees, 1)->statut == STATUT_ANNULEE
                 && date_egale(op_sure(&app.donnees, 1)->date_realisation, D(2026, 11, 4)));
        VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 500000);
        VERIFIER(op_confirmer(&app, 1, D(2026, 11, 5), 0, NULL, 0, true, NULL, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
    }
    /* Correction avant confirmation (G3) puis ajustement du montant à la confirmation. */
    {
        OperationSaisie os = saisie(NATURE_DEPENSE, date_aucune(), "Abonnement ajusté", 15000, j.banque, ID_AUCUN);
        LigneSaisie nl[2];
        nl[0] = ligne(j.courses, 10000, ID_AUCUN);
        nl[1] = ligne(j.hygiene, 5000, ID_AUCUN);
        VERIFIER(op_corriger(&app, 2, &os, nl, 2, true, NULL, &r) == RES_OK);
        VERIFIER(op_sure(&app.donnees, 2)->statut == STATUT_A_CONFIRMER && op_sure(&app.donnees, 2)->montant_total_ct == 15000
                 && date_absente(op_sure(&app.donnees, 2)->date_realisation));
        /* Montant changé, deux lignes, sans ventilation : refusé. */
        VERIFIER(op_confirmer(&app, 2, D(2026, 11, 9), 16000, NULL, 0, true, NULL, &r) == RES_ERR_VALIDATION);
        VERIFIER(contient(&r, "ventilation"));
        rapport_vider(&r);
        /* Ventilation dont la somme diffère du total : refusée. */
        nl[0] = ligne(j.courses, 10000, ID_AUCUN);
        nl[1] = ligne(j.hygiene, 5500, ID_AUCUN);
        VERIFIER(op_confirmer(&app, 2, D(2026, 11, 9), 16000, nl, 2, true, NULL, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
        nl[1] = ligne(j.hygiene, 6000, ID_AUCUN);
        VERIFIER(op_confirmer(&app, 2, D(2026, 11, 9), 16000, nl, 2, true, NULL, &r) == RES_OK);
        VERIFIER(op_sure(&app.donnees, 2)->montant_total_ct == 16000 && op_nombre_lignes(&app.donnees, 2) == 2);
    }
    app_liberer(&app);

    /* Ligne unique : le montant est ajusté automatiquement ; Q3 et G10 à la confirmation. */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 3000, false);
    rec = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(op_confirmer(&app, 1, D(2026, 11, 2), 3500, NULL, 0, true, NULL, &r) == RES_OK);
    {
        LigneSaisie l1[1];
        VERIFIER(op_lignes_copier(&app.donnees, 1, l1, 1) == 1 && l1[0].montant_ct == 3500
                 && op_sure(&app.donnees, 1)->montant_total_ct == 3500);
    }
    app_liberer(&app);
    preparer(&app, &j);
    modele = modele_simple(&app, j.neuf, 3000, false);            /* compte ouvert le 2026-06-01 */
    rec = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(op_confirmer(&app, 1, D(2026, 5, 31), 0, NULL, 0, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "précède la date du solde initial"));
    rapport_vider(&r);
    VERIFIER(op_confirmer(&app, 1, D(2026, 6, 1), 0, NULL, 0, true, NULL, &r) == RES_OK);
    app_liberer(&app);
    preparer(&app, &j);
    modele = modele_simple(&app, j.especes, 25000, false);       /* Espèces : 200 DH */
    rec = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(op_confirmer(&app, 1, D(2026, 11, 2), 0, NULL, 0, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "solde de fin de journée") && op_sure(&app.donnees, 1)->statut == STATUT_A_CONFIRMER);
    rapport_vider(&r);
    app_liberer(&app);
    /* Seuil minimal à la confirmation. */
    preparer(&app, &j);
    modele = modele_simple(&app, j.seuil, 6000, false);          /* 100 DH, seuil 50 DH */
    rec = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(op_confirmer(&app, 1, D(2026, 11, 2), 0, NULL, 0, false, &w, &r) == RES_CONFIRMATION_REQUISE);
    VERIFIER(op_sure(&app.donnees, 1)->statut == STATUT_A_CONFIRMER && contient(&w, "seuil minimal"));
    VERIFIER(op_confirmer(&app, 1, D(2026, 11, 2), 0, NULL, 0, true, NULL, &r) == RES_OK);
    rapport_vider(&w);
    app_liberer(&app);

    /* --- Refus : ANNULEE sans date réelle, clé conservée, lot en une transaction --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    rec = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 10, 26), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(nb == 3);                                           /* 26/10, 2/11, 9/11 : ids 1, 2, 3 */
    VERIFIER(op_refuser(&app, 1, &r) == RES_OK);
    VERIFIER(op_sure(&app.donnees, 1)->statut == STATUT_ANNULEE && date_absente(op_sure(&app.donnees, 1)->date_realisation)
             && op_sure(&app.donnees, 1)->id_recurrence == rec
             && date_egale(op_sure(&app.donnees, 1)->date_prevue_origine, D(2026, 10, 26)));
    VERIFIER(op_refuser(&app, 1, &r) == RES_ERR_VALIDATION);                   /* ANNULEE est final */
    rapport_vider(&r);
    modifs = app.nb_modifications;
    {
        const Id mauvais[3] = {2, 3, 1};                         /* 1 est déjà annulée : tout est refusé */
        VERIFIER(op_lot_refuser(&app, mauvais, 3, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
        VERIFIER(op_sure(&app.donnees, 2)->statut == STATUT_A_CONFIRMER && op_sure(&app.donnees, 3)->statut == STATUT_A_CONFIRMER
                 && app.nb_modifications == modifs);
    }
    {
        const Id bons[2] = {2, 3};
        VERIFIER(op_lot_refuser(&app, bons, 2, &r) == RES_OK && app.nb_modifications == modifs + 1);
        VERIFIER(op_sure(&app.donnees, 2)->statut == STATUT_ANNULEE && op_sure(&app.donnees, 3)->statut == STATUT_ANNULEE);
        VERIFIER(valides(&app));
        /* Une clé annulée reste réservée : le rattrapage ne recrée rien. */
        app.date_reference = D(2026, 11, 13);
        VERIFIER(rec_rattraper(&app, &nb, &r) == RES_OK && nb == 0 && rec_nombre_occurrences(&app.donnees, rec) == 3);
    }
    app_liberer(&app);

    /* --- Lots de confirmation : tout ou rien, G10 cumulé entre les échéances du lot --- */
    preparer(&app, &j);
    modele = modele_simple(&app, j.especes, 15000, false);       /* Espèces : 200 DH */
    rec = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(nb == 2);                                           /* occurrences 1 (2/11) et 2 (9/11) */
    memset(lot, 0, sizeof lot);
    lot[0].id = 1;
    lot[0].date_realisation = D(2026, 11, 2);
    lot[1].id = 2;
    lot[1].date_realisation = D(2026, 11, 9);
    modifs = app.nb_modifications;
    {
        Id compteur = app.donnees.prochain_id[ENT_OPERATION];
        /* Chaque confirmation passe seule (200 - 150 = 50) mais pas les deux (200 - 300 < 0). */
        VERIFIER(op_lot_confirmer(&app, &lot[0], 1, pool, true, true, NULL, &r) == RES_OK);
        VERIFIER(op_lot_confirmer(&app, &lot[1], 1, pool, true, true, NULL, &r) == RES_OK);
        VERIFIER(op_lot_confirmer(&app, lot, 2, pool, true, true, NULL, &r) == RES_ERR_VALIDATION);   /* simulation */
        VERIFIER(contient(&r, "solde de fin de journée"));
        rapport_vider(&r);
        VERIFIER(op_lot_confirmer(&app, lot, 2, pool, false, true, NULL, &r) == RES_ERR_VALIDATION);
        rapport_vider(&r);
        VERIFIER(op_sure(&app.donnees, 1)->statut == STATUT_A_CONFIRMER && op_sure(&app.donnees, 2)->statut == STATUT_A_CONFIRMER
                 && app.nb_modifications == modifs && app.donnees.prochain_id[ENT_OPERATION] == compteur);
        /* La simulation réussie ne modifie rien non plus. */
        VERIFIER(op_lot_confirmer(&app, &lot[0], 1, pool, true, true, NULL, &r) == RES_OK
                 && op_sure(&app.donnees, 1)->statut == STATUT_A_CONFIRMER && app.nb_modifications == modifs);
    }
    /* Le dernier membre échoue (date future) : le premier, valide, n'est pas validé non plus. */
    lot[1].date_realisation = D(2026, 11, 20);
    VERIFIER(op_lot_confirmer(&app, lot, 2, pool, false, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "Échéance 2"));
    rapport_vider(&r);
    VERIFIER(op_sure(&app.donnees, 1)->statut == STATUT_A_CONFIRMER && app.nb_modifications == modifs);
    /* Plusieurs erreurs : toutes sont listées. */
    lot[0].date_realisation = date_aucune();
    VERIFIER(op_lot_confirmer(&app, lot, 2, pool, false, true, NULL, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "Échéance 1") && contient(&r, "Échéance 2"));
    rapport_vider(&r);
    /* Sélection en double. */
    lot[0].date_realisation = D(2026, 11, 2);
    lot[1].id = 1;
    VERIFIER(op_lot_confirmer(&app, lot, 2, pool, false, true, NULL, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    lot[1].id = 2;
    lot[1].date_realisation = D(2026, 11, 9);
    /* Lot valide en une seule transaction : une seule modification comptée. */
    lot[1].nouveau_total_ct = 5000;                              /* l'échéance 2 passe à 50 DH */
    VERIFIER(op_lot_confirmer(&app, lot, 2, pool, false, true, NULL, &r) == RES_OK);
    VERIFIER(app.nb_modifications == modifs + 1);
    VERIFIER(op_sure(&app.donnees, 1)->statut == STATUT_CONFIRMEE && op_sure(&app.donnees, 2)->statut == STATUT_CONFIRMEE
             && op_sure(&app.donnees, 2)->montant_total_ct == 5000);
    {
        long long solde;
        VERIFIER(solde_compte_final(&app.donnees, j.especes, &solde) && solde == 0);   /* 200 - 150 - 50 */
        VERIFIER(valides(&app));
    }
    VERIFIER(op_lot_confirmer(&app, lot, 0, pool, false, true, NULL, &r) == RES_ERR_VALIDATION);   /* lot vide */
    rapport_vider(&r);
    app_liberer(&app);

    /* Lot de trois avec nouvelle ventilation pour l'une des échéances (pool partagé). */
    preparer(&app, &j);
    modele = modele_simple(&app, j.banque, 12000, true);
    rec = rec_nouvelle(&app, modele, FREQ_HEBDOMADAIRE, D(2026, 10, 26), date_aucune(), 0, RETRO_GENERER, &nb);
    VERIFIER(nb == 3);
    memset(lot, 0, sizeof lot);
    pool[0] = ligne(j.courses, 9000, ID_AUCUN);
    pool[1] = ligne(j.hygiene, 6000, ID_AUCUN);
    lot[0].id = 1;
    lot[0].date_realisation = D(2026, 10, 26);
    lot[1].id = 2;
    lot[1].date_realisation = D(2026, 11, 2);
    lot[1].nouveau_total_ct = 15000;
    lot[1].debut_lignes = 0;
    lot[1].nb_lignes = 2;
    lot[2].id = 3;
    lot[2].date_realisation = D(2026, 11, 9);
    VERIFIER(op_lot_confirmer(&app, lot, 3, pool, false, true, NULL, &r) == RES_OK);
    VERIFIER(op_sure(&app.donnees, 2)->montant_total_ct == 15000 && op_sure(&app.donnees, 1)->montant_total_ct == 12000);
    {
        long long solde;
        VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 500000 - 12000 - 15000 - 12000);
    }
    app_liberer(&app);

    /* Virement récurrent : aucune ligne, le montant peut changer à la confirmation. */
    preparer(&app, &j);
    {
        ModeleSaisie ms;
        Id mv = ID_AUCUN;
        long long solde;

        memset(&ms, 0, sizeof ms);
        ms.nature = NATURE_VIREMENT;
        texte_copier(ms.nom, sizeof ms.nom, "Épargne mensuelle");
        texte_copier(ms.libelle, sizeof ms.libelle, "Virement épargne");
        ms.montant_total_ct = 5000;
        ms.id_compte_source = j.banque;
        ms.id_compte_destination = j.especes;
        VERIFIER(mod_creer(&app, &ms, NULL, 0, &mv, &r) == RES_OK);
        rec = rec_nouvelle(&app, mv, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, &nb);
        VERIFIER(nb == 2 && op_nombre_lignes(&app.donnees, 1) == 0);
        VERIFIER(op_confirmer(&app, 1, D(2026, 11, 2), 6000, NULL, 0, true, NULL, &r) == RES_OK);
        VERIFIER(op_sure(&app.donnees, 1)->montant_total_ct == 6000);
        VERIFIER(solde_compte_final(&app.donnees, j.especes, &solde) && solde == 20000 + 6000);
        VERIFIER(solde_compte_final(&app.donnees, j.banque, &solde) && solde == 500000 - 6000);
    }
    app_liberer(&app);

    rapport_liberer(&w);
    rapport_liberer(&r);
}

/* ================================================================== */
/* Étape 5 : export, sauvegarde, restauration, import additif          */
/* ================================================================== */

static void ecrire_fichier(const char *nom, const char *contenu)
{
    char chemin[CHEMIN_MAX + 64];
    FILE *f;

    snprintf(chemin, sizeof chemin, "%s/%s", dossier_test, nom);
    f = fopen(chemin, "wb");
    if (f != NULL) {
        fputs(contenu, f);
        fclose(f);
    }
}

static void supprimer_fichier(const char *nom)
{
    char chemin[CHEMIN_MAX + 64];

    snprintf(chemin, sizeof chemin, "%s/%s", dossier_test, nom);
    remove(chemin);
}

static void nettoyer_dossier_test(void)
{
    jeu_supprimer(dossier_test, "");
    jeu_supprimer(dossier_test, "A_");
    jeu_supprimer(dossier_test, "B_");
    jeu_supprimer(dossier_test, "20261113-120000_");
    jeu_supprimer(dossier_test, "20261113-120000-2_");
    supprimer_fichier("manifest.tmp");
    supprimer_fichier(NOM_INDEX_SAUVEGARDES);
    supprimer_fichier(NOM_IMPORT_OPERATIONS);
    supprimer_fichier(NOM_IMPORT_LIGNES);
    supprimer_fichier(NOM_VERROU);
}

/* Jeu riche : comptes, catégories, tiers, véhicule, budget, opérations, modèle, récurrence + occurrences. */
static void jeu_riche(Application *app, Jeu *j)
{
    LigneSaisie l[2];
    Id modele;
    RapportErreurs r;
    Id id = ID_AUCUN;

    preparer(app, j);
    rapport_init(&r);
    ref_budget_creer(app, 2026, 11, j->alim, 20000, &id, &r);
    l[0] = ligne(j->courses, 15000, ID_AUCUN);
    l[1] = ligne(j->hygiene, 5000, ID_AUCUN);
    creer_op(app, NATURE_DEPENSE, D(2026, 11, 2), "Courses Marjane", 20000, j->banque, ID_AUCUN, j->carrefour, l, 2);
    l[0] = ligne(j->carburant, 6000, j->clio);
    creer_op(app, NATURE_DEPENSE, D(2026, 11, 5), "Plein", 6000, j->banque, ID_AUCUN, ID_AUCUN, l, 1);
    l[0] = ligne(j->salaire, 300000, ID_AUCUN);
    creer_op(app, NATURE_REVENU, D(2026, 11, 10), "Salaire", 300000, ID_AUCUN, j->banque, ID_AUCUN, l, 1);
    creer_op(app, NATURE_VIREMENT, D(2026, 11, 11), "Retrait", 5000, j->banque, j->especes, ID_AUCUN, NULL, 0);
    modele = modele_simple(app, j->banque, 12000, true);
    rec_nouvelle(app, modele, FREQ_HEBDOMADAIRE, D(2026, 11, 2), date_aucune(), 0, RETRO_GENERER, NULL);
    rapport_liberer(&r);
}

static bool memes_comptages(const Donnees *a, const Donnees *b)
{
    int e;

    for (e = 0; e < NB_ENTITES; e++) {
        if (donnees_table_const(a, (Entite)e)->taille != donnees_table_const(b, (Entite)e)->taille) {
            return false;
        }
    }
    return true;
}

void test_export_sauvegarde_restauration(void)
{
    Application app;
    Application autre;
    Jeu j;
    RapportErreurs r;
    RapportErreurs w;
    Donnees restaure;
    ApercuRestauration ap;
    Tableau liste;
    char prefixe[48];
    size_t nb = 0;
    char chemin[CHEMIN_MAX + 64];

    printf("Export, sauvegarde horodatée avec index, restauration\n");
    rapport_init(&r);
    rapport_init(&w);
    nettoyer_dossier_test();
    jeu_riche(&app, &j);
    VERIFIER(valides(&app));

    /* --- Export : restaurable, sans effet sur la mémoire ni sur l'indicateur --- */
    app.modifie = false;
    {
        long modifs = app.nb_modifications;
        VERIFIER(!export_manifeste_present(dossier_test));
        VERIFIER(fich_exporter(&app, dossier_test, &r) == RES_OK);
        VERIFIER(export_manifeste_present(dossier_test));
        VERIFIER(!app.modifie && app.nb_modifications == modifs);
        VERIFIER(fich_exporter(&app, dossier_test, &r) == RES_OK);           /* écrasement d'un export */
        VERIFIER(fich_exporter(&app, "/dossier/inexistant", &r) == RES_ERR_FICHIER);
        rapport_vider(&r);
        VERIFIER(fich_exporter(&app, "", &r) == RES_ERR_FICHIER);
        rapport_vider(&r);
    }
    /* Restauration dans une application vide : mêmes enregistrements, mêmes liens. */
    app_init(&autre, D(2026, 11, 13), true, dossier_test);
    donnees_init(&restaure);
    VERIFIER(fich_restaurer_charger(&autre, dossier_test, "", &restaure, &ap, &r) == RES_OK);
    VERIFIER(memes_comptages(&app.donnees, &restaure) && valides(&autre) && regles_valider(&restaure, &r));
    VERIFIER(ap.courant[ENT_OPERATION] == 0 && ap.restaure[ENT_OPERATION] == app.donnees.operations.taille);
    VERIFIER(restaure.operations.taille > 0
             && memcmp(restaure.operations.donnees, app.donnees.operations.donnees,
                       app.donnees.operations.taille * sizeof(Operation)) == 0);
    VERIFIER(memcmp(restaure.lignes_operations.donnees, app.donnees.lignes_operations.donnees,
                    app.donnees.lignes_operations.taille * sizeof(LigneOperation)) == 0);
    {
        int e;
        bool compteurs_egaux = true;
        for (e = 0; e < NB_ENTITES; e++) {
            compteurs_egaux = compteurs_egaux && restaure.prochain_id[e] == app.donnees.prochain_id[e];
        }
        VERIFIER(compteurs_egaux);
    }
    donnees_liberer(&restaure);
    /* Compteurs : max(courant, sauvegarde, plus grand id + 1) — un compteur courant plus haut est conservé. */
    autre.donnees.prochain_id[ENT_OPERATION] = 500;
    donnees_init(&restaure);
    VERIFIER(fich_restaurer_charger(&autre, dossier_test, "", &restaure, &ap, &r) == RES_OK);
    VERIFIER(restaure.prochain_id[ENT_OPERATION] == 500 && ap.compteurs_releves);
    VERIFIER(restaure.prochain_id[ENT_COMPTE] == app.donnees.prochain_id[ENT_COMPTE]);
    donnees_liberer(&restaure);

    /* Appliquer : remplacement en mémoire seulement, indicateur modifié, rattrapage des récurrences. */
    autre.date_reference = D(2026, 12, 1);
    autre.donnees.prochain_id[ENT_OPERATION] = 1;
    donnees_init(&restaure);
    VERIFIER(fich_restaurer_charger(&autre, dossier_test, "", &restaure, &ap, &r) == RES_OK);
    {
        size_t ops_avant = restaure.operations.taille;
        VERIFIER(fich_restaurer_appliquer(&autre, &restaure, &nb, &w) == RES_OK);
        VERIFIER(autre.modifie && autre.donnees.operations.taille == ops_avant + 3);     /* 16/11, 23/11, 30/11 */
        VERIFIER(nb == 3 && restaure.operations.taille == 0);                            /* restaure consommé */
        VERIFIER(valides(&autre));
    }
    snprintf(chemin, sizeof chemin, "%s/A_manifest.csv", dossier_test);
    {
        FILE *f = fopen(chemin, "rb");
        VERIFIER(f == NULL);               /* les fichiers de data ne sont touchés que par « Enregistrer » */
        if (f != NULL) {
            fclose(f);
        }
    }
    app_liberer(&autre);

    /* Export corrompu (octet altéré, même taille) : refus, état courant intact. */
    {
        FILE *f;
        int c;
        long pos = 0;
        size_t avant = app.donnees.operations.taille;

        snprintf(chemin, sizeof chemin, "%s/operations.csv", dossier_test);
        f = fopen(chemin, "r+b");
        VERIFIER(f != NULL);
        if (f != NULL) {
            while ((c = fgetc(f)) != EOF && c != '\n') {
                pos++;
            }
            pos++;
            while ((c = fgetc(f)) != EOF) {
                if (c >= '0' && c <= '8') {
                    fseek(f, pos, SEEK_SET);
                    fputc(c + 1, f);
                    break;
                }
                pos++;
            }
            fclose(f);
        }
        app_init(&autre, D(2026, 11, 13), true, dossier_test);
        donnees_init(&restaure);
        VERIFIER(fich_restaurer_charger(&autre, dossier_test, "", &restaure, &ap, &r) == RES_ERR_FICHIER);
        VERIFIER(contient(&r, "somme de contrôle") && restaure.operations.taille == 0
                 && autre.donnees.operations.taille == 0 && app.donnees.operations.taille == avant);
        rapport_vider(&r);
        donnees_liberer(&restaure);
        app_liberer(&autre);
    }
    jeu_supprimer(dossier_test, "");
    app_init(&autre, D(2026, 11, 13), true, dossier_test);
    donnees_init(&restaure);
    VERIFIER(fich_restaurer_charger(&autre, dossier_test, "", &restaure, &ap, &r) == RES_ERR_FICHIER);   /* dossier sans jeu */
    rapport_vider(&r);
    donnees_liberer(&restaure);
    app_liberer(&autre);

    /* --- Sauvegarde : enregistrer d'abord, préfixe horodaté, index, collisions --- */
    texte_copier(app.dossier_sauvegardes, sizeof app.dossier_sauvegardes, dossier_test);
    app.modifie = true;
    VERIFIER(fich_sauvegarder(&app, "20261113-120000", prefixe, sizeof prefixe, &r) == RES_ERR_VALIDATION);
    VERIFIER(contient(&r, "enregistrez d'abord"));
    rapport_vider(&r);
    app.modifie = false;
    app.generation_chargee = 7;
    VERIFIER(fich_sauvegarder(&app, "2026-11-13", prefixe, sizeof prefixe, &r) == RES_ERR_FICHIER);   /* horodatage invalide */
    rapport_vider(&r);
    VERIFIER(fich_sauvegarder(&app, "20261113-120000", prefixe, sizeof prefixe, &r) == RES_OK);
    VERIFIER(strcmp(prefixe, "20261113-120000_") == 0);
    VERIFIER(fich_sauvegarder(&app, "20261113-120000", prefixe, sizeof prefixe, &r) == RES_OK);
    VERIFIER(strcmp(prefixe, "20261113-120000-2_") == 0);                       /* même seconde : « -2 » */
    tableau_init(&liste, sizeof(EntreeSauvegarde));
    VERIFIER(fich_lister_sauvegardes(dossier_test, &liste, &r) == RES_OK && liste.taille == 2);
    VERIFIER(strcmp(ELEMENT(&liste, EntreeSauvegarde, 0)->prefixe, "20261113-120000_") == 0
             && ELEMENT(&liste, EntreeSauvegarde, 0)->generation == 7
             && ELEMENT(&liste, EntreeSauvegarde, 0)->nb_operations == (long)app.donnees.operations.taille
             && strcmp(ELEMENT(&liste, EntreeSauvegarde, 0)->horodatage, "2026-11-13 12:00:00") == 0
             && strcmp(ELEMENT(&liste, EntreeSauvegarde, 1)->prefixe, "20261113-120000-2_") == 0);
    /* La sauvegarde est restaurable. */
    app_init(&autre, D(2026, 11, 13), true, dossier_test);
    donnees_init(&restaure);
    VERIFIER(fich_restaurer_charger(&autre, dossier_test, ELEMENT(&liste, EntreeSauvegarde, 1)->prefixe, &restaure, &ap, &r)
             == RES_OK);
    VERIFIER(memes_comptages(&app.donnees, &restaure));
    donnees_liberer(&restaure);
    app_liberer(&autre);
    /* Sans index : liste vide. Index à l'en-tête invalide : erreur. */
    supprimer_fichier(NOM_INDEX_SAUVEGARDES);
    VERIFIER(fich_lister_sauvegardes(dossier_test, &liste, &r) == RES_OK && liste.taille == 0);
    ecrire_fichier(NOM_INDEX_SAUVEGARDES, "mauvais;en;tete\n");
    VERIFIER(fich_lister_sauvegardes(dossier_test, &liste, &r) == RES_ERR_FICHIER);
    rapport_vider(&r);
    ecrire_fichier(NOM_INDEX_SAUVEGARDES, ENTETE_INDEX "\n20261113-1_;2026-11-13 12:00:00;abc;3\n");
    VERIFIER(fich_lister_sauvegardes(dossier_test, &liste, &r) == RES_ERR_FICHIER && contient(&r, "ligne 2"));
    rapport_vider(&r);
    tableau_liberer(&liste);
    /* Dossier de sauvegarde absent. */
    texte_copier(app.dossier_sauvegardes, sizeof app.dossier_sauvegardes, "/dossier/inexistant");
    VERIFIER(fich_sauvegarder(&app, "20261113-130000", prefixe, sizeof prefixe, &r) == RES_ERR_FICHIER);
    rapport_vider(&r);

    nettoyer_dossier_test();
    app_liberer(&app);
    rapport_liberer(&w);
    rapport_liberer(&r);
}

/* En-têtes d'import. */
#define H_OPS ENTETE_IMPORT_OPERATIONS "\n"
#define H_LIG ENTETE_IMPORT_LIGNES "\n"

static Resultat analyser(const Application *app, ImportPrepare *p, RapportErreurs *e)
{
    return imp_analyser(app, dossier_test, p, e);
}

void test_import_additif(void)
{
    Application app;
    Jeu j;
    ImportPrepare p;
    RapportErreurs e;
    RapportErreurs w;
    LigneSaisie l[2];
    Tableau groupes;
    Tableau membres;
    Tableau exclus;
    size_t ops_avant;
    long modifs;
    Id compteur;
    size_t i;

    printf("Import additif tout ou rien : clés naturelles, erreurs détaillées, doublons, G10\n");
    rapport_init(&e);
    rapport_init(&w);
    tableau_init(&groupes, sizeof(GroupeDoublons));
    tableau_init(&membres, sizeof(Id));
    tableau_init(&exclus, sizeof(Id));
    nettoyer_dossier_test();
    preparer(&app, &j);
    imp_init(&p);
    l[0] = ligne(j.courses, 4000, ID_AUCUN);
    creer_op(&app, NATURE_DEPENSE, D(2026, 11, 2), "Existante", 4000, j.banque, ID_AUCUN, ID_AUCUN, l, 1);
    ops_avant = app.donnees.operations.taille;
    app.modifie = false;
    modifs = app.nb_modifications;
    compteur = app.donnees.prochain_id[ENT_OPERATION];

    /* --- Import valide : trois natures, chemins insensibles à la casse, BOM toléré --- */
    ecrire_fichier(NOM_IMPORT_OPERATIONS,
                   "\xEF\xBB\xBF" H_OPS
                   "1;DEPENSE;2026-11-03;Courses importées;20000;BANQUE;;Carrefour\n"
                   "2;REVENU;2026-11-04;Prime;50000;;Banque;\n"
                   "3;VIREMENT;2026-11-05;Retrait importé;10000;Banque;Espèces;\n");
    ecrire_fichier(NOM_IMPORT_LIGNES,
                   H_LIG
                   "1;Alimentation > Courses;;15000\n"
                   "1;alimentation>HYGIÈNE;;5000\n"
                   "2;Salaire;;50000\n");
    VERIFIER(analyser(&app, &p, &e) == RES_OK && p.pret && p.nb_operations == 3 && p.nb_lignes == 3);
    VERIFIER(rapport_nombre(&e) == 0);
    VERIFIER(app.donnees.operations.taille == ops_avant && app.nb_modifications == modifs
             && !app.modifie && app.donnees.prochain_id[ENT_OPERATION] == compteur);   /* rien tant que non appliqué */
    VERIFIER(imp_doublons(&p, &groupes, &membres, &exclus) && groupes.taille == 0);
    VERIFIER(imp_appliquer(&app, &p, false, &w, &e) == RES_OK);
    VERIFIER(app.donnees.operations.taille == ops_avant + 3 && app.modifie && app.nb_modifications == modifs + 1);
    VERIFIER(app.donnees.prochain_id[ENT_OPERATION] >= compteur + 3);
    VERIFIER(op_sure(&app.donnees, 2)->statut == STATUT_CONFIRMEE && op_sure(&app.donnees, 2)->id_recurrence == ID_AUCUN
             && op_sure(&app.donnees, 2)->id_compte_source == j.banque
             && op_sure(&app.donnees, 2)->id_tiers == j.carrefour
             && op_nombre_lignes(&app.donnees, 2) == 2 && op_nombre_lignes(&app.donnees, 3) == 1
             && op_nombre_lignes(&app.donnees, 4) == 0 && op_sure(&app.donnees, 4)->id_compte_destination == j.especes);
    VERIFIER(op_sure(&app.donnees, 1)->montant_total_ct == 4000 && valides(&app));         /* l'existant est conservé */
    imp_liberer(&p);
    imp_init(&p);

    /* --- Rejet complet : toutes les erreurs sont listées avec fichier, ligne et motif --- */
    app.modifie = false;
    ops_avant = app.donnees.operations.taille;
    modifs = app.nb_modifications;
    compteur = app.donnees.prochain_id[ENT_OPERATION];
    {
        size_t lignes_avant = app.donnees.lignes_operations.taille;
        Id compteur_lignes = app.donnees.prochain_id[ENT_LIGNE_OPERATION];
        Id tiers2 = ID_AUCUN;

        VERIFIER(ref_tiers_creer(&app, "carrefour", &tiers2, &e) == RES_OK);                /* tiers ambigu (nom nonunique) */
        app.modifie = false;
        modifs = app.nb_modifications;
        ecrire_fichier(NOM_IMPORT_OPERATIONS,
                       H_OPS
                       "1;DEPENSE;2026-11-03;Catégorie inconnue;1000;Banque;;\n"
                       "2;DEPENSE;2026-11-03;Compte inconnu;1000;Fantôme;;\n"
                       "3;DEPENSE;2026-11-03;Tiers ambigu;1000;Banque;;Carrefour\n"
                       "4;DEPENSE;2027-01-01;Date future;1000;Banque;;\n"
                       "5;DEPENSE;2026-11-03;Somme fausse;1000;Banque;;\n"
                       "5;DEPENSE;2026-11-03;Ref en double;1000;Banque;;\n"
                       "6;DEPENSE;2026-11-03;Sans ligne;1000;Banque;;\n"
                       "7;VIREMENT;2026-11-03;Virement ventilé;1000;Banque;Espèces;\n"
                       "8;DEPENSE;2026-11-03;Montant nul;0;Banque;;\n"
                       "9;DEPENSE;2026-13-03;Date invalide;1000;Banque;;\n"
                       "10;REVENU;2026-11-03;Revenu avec source;1000;Banque;Espèces;\n");
        ecrire_fichier(NOM_IMPORT_LIGNES,
                       H_LIG
                       "1;Alimentation > Inconnue;;1000\n"
                       "2;Alimentation > Courses;;1000\n"
                       "3;Alimentation > Courses;;1000\n"
                       "4;Alimentation > Courses;;1000\n"
                       "5;Alimentation > Courses;;900\n"
                       "7;Alimentation > Courses;;1000\n"
                       "99;Alimentation > Courses;;1000\n"
                       "5;Alimentation;;100\n"
                       "5;Salaire;;100\n"
                       "5;Alimentation > Courses;2;100\n"
                       "5;Alimentation > Courses;;abc\n"
                       "10;Salaire;;1000\n");
        VERIFIER(analyser(&app, &p, &e) == RES_ERR_VALIDATION && !p.pret);
        VERIFIER(contient(&e, "import_lignes.csv, ligne 2") && contient(&e, "« Alimentation > Inconnue » introuvable"));
        VERIFIER(contient(&e, "import_operations.csv, ligne 3") && contient(&e, "compte source « Fantôme » introuvable"));
        VERIFIER(contient(&e, "import_operations.csv, ligne 4") && contient(&e, "ambigu"));
        VERIFIER(contient(&e, "import_operations.csv, ligne 5") && contient(&e, "postérieure à la date de référence"));
        VERIFIER(contient(&e, "ref déjà utilisée"));
        VERIFIER(contient(&e, "au moins une ligne"));
        VERIFIER(contient(&e, "import_lignes.csv, ligne 8") && contient(&e, "ref 99 absente"));
        VERIFIER(contient(&e, "n'est pas terminale"));                            /* « Alimentation » seule */
        VERIFIER(contient(&e, "ne convient pas à une dépense"));                  /* « Salaire » sur une dépense */
        VERIFIER(contient(&e, "champ « vehicule »"));
        VERIFIER(contient(&e, "champ « montant_ct »"));
        VERIFIER(contient(&e, "un virement ne comporte aucune ligne de catégorie"));
        VERIFIER(contient(&e, "un revenu exige un compte destination et n'a pas de compte source"));
        VERIFIER(contient(&e, "champ « montant_total_ct »") && contient(&e, "champ « date_realisation »"));
        /* Données, compteurs et indicateur intacts. */
        VERIFIER(app.donnees.operations.taille == ops_avant && app.donnees.lignes_operations.taille == lignes_avant
                 && app.donnees.prochain_id[ENT_OPERATION] == compteur
                 && app.donnees.prochain_id[ENT_LIGNE_OPERATION] == compteur_lignes
                 && app.nb_modifications == modifs && !app.modifie);
        VERIFIER(imp_appliquer(&app, &p, true, NULL, &e) == RES_ERR_VALIDATION);       /* rien à appliquer */
        rapport_vider(&e);
        VERIFIER(ref_tiers_supprimer(&app, tiers2, &e) == RES_OK);
        app.modifie = false;
        modifs = app.nb_modifications;
    }
    /* Fichier manquant, en-tête faux, fichier vide d'opérations. */
    supprimer_fichier(NOM_IMPORT_LIGNES);
    ecrire_fichier(NOM_IMPORT_OPERATIONS, H_OPS "1;DEPENSE;2026-11-03;x;1000;Banque;;\n");
    VERIFIER(analyser(&app, &p, &e) == RES_ERR_FICHIER && contient(&e, "import_lignes.csv"));
    rapport_vider(&e);
    ecrire_fichier(NOM_IMPORT_LIGNES, "ref;mauvais\n");
    VERIFIER(analyser(&app, &p, &e) == RES_ERR_FICHIER && contient(&e, "en-tête attendu"));
    rapport_vider(&e);
    ecrire_fichier(NOM_IMPORT_LIGNES, H_LIG);
    ecrire_fichier(NOM_IMPORT_OPERATIONS, H_OPS);
    VERIFIER(analyser(&app, &p, &e) == RES_ERR_VALIDATION && contient(&e, "aucune opération"));
    rapport_vider(&e);
    ecrire_fichier(NOM_IMPORT_OPERATIONS, H_OPS "1;DEPENSE;2026-11-03;Trop;champs\n");
    VERIFIER(analyser(&app, &p, &e) == RES_ERR_VALIDATION && contient(&e, "8 champs attendus"));
    rapport_vider(&e);
    ecrire_fichier(NOM_IMPORT_OPERATIONS, H_OPS "1;DEPENSE;2026-11-03;Libellé;interdit;1000;Banque;;\n");
    VERIFIER(analyser(&app, &p, &e) == RES_ERR_VALIDATION);
    rapport_vider(&e);

    /* --- G10 : l'erreur de solde est rattachée à la ligne de l'opération importée en cause --- */
    ecrire_fichier(NOM_IMPORT_OPERATIONS,
                   H_OPS
                   "1;DEPENSE;2026-11-03;Petit achat;20000;Espèces;;\n"
                   "2;DEPENSE;2026-11-03;Achat fatal;25000;Espèces;;\n"
                   "3;DEPENSE;2026-11-03;Autre compte;1000;Banque;;\n");
    ecrire_fichier(NOM_IMPORT_LIGNES,
                   H_LIG
                   "1;Alimentation > Courses;;20000\n"
                   "2;Alimentation > Courses;;25000\n"
                   "3;Alimentation > Courses;;1000\n");
    VERIFIER(analyser(&app, &p, &e) == RES_ERR_VALIDATION && !p.pret);
    VERIFIER(contient(&e, "import_operations.csv, ligne 3 (ref 2)") && contient(&e, "solde de fin de journée"));
    VERIFIER(!contient(&e, "(ref 1)") && !contient(&e, "(ref 3)"));
    rapport_vider(&e);
    VERIFIER(app.donnees.operations.taille == ops_avant && app.nb_modifications == modifs);
    /* Q3 : date antérieure à l'ouverture du compte « Neuf ». */
    ecrire_fichier(NOM_IMPORT_OPERATIONS, H_OPS "1;DEPENSE;2026-05-31;Avant l'ouverture;1000;Neuf;;\n");
    ecrire_fichier(NOM_IMPORT_LIGNES, H_LIG "1;Alimentation > Courses;;1000\n");
    VERIFIER(analyser(&app, &p, &e) == RES_ERR_VALIDATION && contient(&e, "précède la date du solde initial")
             && contient(&e, "ligne 2 (ref 1)"));
    rapport_vider(&e);

    /* --- Doublons I6 : détectés sur existant + importé ; exclusion puis revalidation --- */
    ecrire_fichier(NOM_IMPORT_OPERATIONS,
                   H_OPS
                   "1;DEPENSE;2026-11-02;existante;4000;Banque;;\n"
                   "2;DEPENSE;2026-11-09;Nouveau doublon;3000;Banque;;\n"
                   "3;DEPENSE;2026-11-09;nouveau DOUBLON;3000;Banque;;\n"
                   "4;DEPENSE;2026-11-10;Unique;1500;Banque;;\n");
    ecrire_fichier(NOM_IMPORT_LIGNES,
                   H_LIG
                   "1;Alimentation > Courses;;4000\n"
                   "2;Alimentation > Courses;;3000\n"
                   "3;Alimentation > Courses;;3000\n"
                   "4;Alimentation > Courses;;1500\n");
    VERIFIER(analyser(&app, &p, &e) == RES_OK && p.nb_operations == 4);
    VERIFIER(imp_doublons(&p, &groupes, &membres, &exclus) && groupes.taille == 2 && exclus.taille == 2);
    {
        /* Groupe 1 : l'existante (id 1) + l'importée → l'importée est exclue ; groupe 2 : deux importées → une seule exclue. */
        Id premier_importe = *ELEMENT(&p.ids_importes, Id, 0);
        Id second = *ELEMENT(&p.ids_importes, Id, 1);
        Id troisieme = *ELEMENT(&p.ids_importes, Id, 2);
        bool a_premier = false;
        bool a_second = false;
        bool a_troisieme = false;
        for (i = 0; i < exclus.taille; i++) {
            Id x = *ELEMENT(&exclus, Id, i);
            a_premier = a_premier || x == premier_importe;
            a_second = a_second || x == second;
            a_troisieme = a_troisieme || x == troisieme;
        }
        VERIFIER(a_premier && !a_second && a_troisieme);                 /* on garde le plus petit des deux importés */
        VERIFIER(imp_exclure(&p, &exclus, &e) == RES_OK && p.nb_operations == 2 && p.pret);
        VERIFIER(imp_doublons(&p, &groupes, &membres, &exclus) && groupes.taille == 0);
        ops_avant = app.donnees.operations.taille;
        VERIFIER(imp_appliquer(&app, &p, true, NULL, &e) == RES_OK && app.donnees.operations.taille == ops_avant + 2);
        VERIFIER(valides(&app));
    }
    imp_liberer(&p);
    imp_init(&p);

    /* L'exclusion est revalidée : un revenu exclu peut rendre un solde négatif → exclusion refusée. */
    ecrire_fichier(NOM_IMPORT_OPERATIONS,
                   H_OPS
                   "1;REVENU;2026-11-04;Prime;10000;;Espèces;\n"
                   "2;DEPENSE;2026-11-04;Achat;35000;Espèces;;\n");
    ecrire_fichier(NOM_IMPORT_LIGNES, H_LIG "1;Salaire;;10000\n" "2;Alimentation > Courses;;35000\n");
    l[0] = ligne(j.salaire, 10000, ID_AUCUN);
    creer_op(&app, NATURE_REVENU, D(2026, 11, 4), "Prime", 10000, ID_AUCUN, j.especes, ID_AUCUN, l, 1);
    VERIFIER(analyser(&app, &p, &e) == RES_OK);                                   /* 200 + 100 + 100 - 350 = 50 */
    VERIFIER(imp_doublons(&p, &groupes, &membres, &exclus) && groupes.taille == 1 && exclus.taille == 1);
    {
        size_t importes = p.nb_operations;
        VERIFIER(imp_exclure(&p, &exclus, &e) == RES_ERR_VALIDATION);              /* 200 + 100 - 350 < 0 */
        VERIFIER(contient(&e, "solde de fin de journée") && p.nb_operations == importes && p.pret);
        rapport_vider(&e);
        VERIFIER(imp_appliquer(&app, &p, true, NULL, &e) == RES_OK);               /* « Tout importer » reste possible */
    }
    imp_liberer(&p);
    imp_init(&p);

    /* --- Seuil minimal à l'application : p reste réutilisable après la demande d'accord --- */
    ecrire_fichier(NOM_IMPORT_OPERATIONS, H_OPS "1;DEPENSE;2026-11-06;Sous le seuil;6000;Seuil;;\n");
    ecrire_fichier(NOM_IMPORT_LIGNES, H_LIG "1;Alimentation > Courses;;6000\n");
    VERIFIER(analyser(&app, &p, &e) == RES_OK);
    ops_avant = app.donnees.operations.taille;
    VERIFIER(imp_appliquer(&app, &p, false, &w, &e) == RES_CONFIRMATION_REQUISE && contient(&w, "seuil minimal"));
    VERIFIER(app.donnees.operations.taille == ops_avant && p.pret);
    VERIFIER(imp_appliquer(&app, &p, true, NULL, &e) == RES_OK && app.donnees.operations.taille == ops_avant + 1);
    imp_liberer(&p);
    app_liberer(&app);

    /* --- Recette : 20 opérations admissibles ajoutées à 30 existantes (identifiants sans collision) --- */
    preparer(&app, &j);
    for (i = 0; i < 30; i++) {
        char lib[LIBELLE_MAX + 1];
        snprintf(lib, sizeof lib, "Existante %lu", (unsigned long)i);
        l[0] = ligne(j.courses, 1000 + (long long)i, ID_AUCUN);
        creer_op(&app, NATURE_DEPENSE, D(2026, 11, 1 + (int)(i % 12)), lib, 1000 + (long long)i, j.banque, ID_AUCUN, ID_AUCUN, l, 1);
    }
    VERIFIER(app.donnees.operations.taille == 30);
    {
        char *ops_txt = malloc(4096);
        char *lig_txt = malloc(4096);
        size_t no = 0;
        size_t nl = 0;

        VERIFIER(ops_txt != NULL && lig_txt != NULL);
        if (ops_txt != NULL && lig_txt != NULL) {
            no += (size_t)snprintf(ops_txt + no, 4096 - no, "%s", H_OPS);
            nl += (size_t)snprintf(lig_txt + nl, 4096 - nl, "%s", H_LIG);
            for (i = 1; i <= 20; i++) {
                no += (size_t)snprintf(ops_txt + no, 4096 - no, "%lu;DEPENSE;2026-11-%02lu;Importée %lu;%lu;Banque;;\n",
                                       (unsigned long)i, (unsigned long)(1 + i % 12), (unsigned long)i,
                                       (unsigned long)(5000 + i));
                nl += (size_t)snprintf(lig_txt + nl, 4096 - nl, "%lu;Alimentation > Courses;;%lu\n",
                                       (unsigned long)i, (unsigned long)(5000 + i));
            }
            ecrire_fichier(NOM_IMPORT_OPERATIONS, ops_txt);
            ecrire_fichier(NOM_IMPORT_LIGNES, lig_txt);
        }
        free(ops_txt);
        free(lig_txt);
    }
    imp_init(&p);
    VERIFIER(analyser(&app, &p, &e) == RES_OK && p.nb_operations == 20);
    VERIFIER(imp_appliquer(&app, &p, true, NULL, &e) == RES_OK);
    VERIFIER(app.donnees.operations.taille == 50 && valides(&app));
    {
        size_t k;
        bool ids_uniques = true;
        for (k = 0; k < 50; k++) {
            ids_uniques = ids_uniques && ELEMENT(&app.donnees.operations, Operation, k)->id == (Id)(k + 1);
        }
        VERIFIER(ids_uniques);                                         /* 1..30 conservés, 31..50 nouveaux */
    }
    imp_liberer(&p);
    app_liberer(&app);

    nettoyer_dossier_test();
    tableau_liberer(&groupes);
    tableau_liberer(&membres);
    tableau_liberer(&exclus);
    rapport_liberer(&w);
    rapport_liberer(&e);
}

/* ================================================================== */
/* Ergonomie : catégorie par nom, dernières opérations                  */
/* ================================================================== */

static Id trouvee(const Tableau *t, size_t i)
{
    return i < t->taille ? ELEMENT(t, CategorieTrouvee, i)->id : -1;
}

void test_ergonomie(void)
{
    Application app;
    Jeu j;
    Tableau cats;
    Tableau res;
    RapportErreurs r;
    Id taxi = ID_AUCUN;
    Id racine_courses = ID_AUCUN;
    size_t total = 99;

    printf("Ergonomie : choix de catégorie par nom, dernières opérations\n");
    tableau_init(&cats, sizeof(CategorieTrouvee));
    tableau_init(&res, sizeof(ResultatRecherche));
    rapport_init(&r);
    preparer(&app, &j);

    /* Liste vide : rien à afficher, total nul. */
    VERIFIER(analyses_dernieres_operations(&app.donnees, 10, &res, &total));
    VERIFIER(res.taille == 0 && total == 0);

    VERIFIER(ref_categorie_creer(&app, "Courses taxi", j.transport, CAT_DEPENSE, &taxi, &r) == RES_OK);
    VERIFIER(ref_categorie_creer(&app, "Courses", ID_AUCUN, CAT_DEPENSE, &racine_courses, &r)
             == RES_OK);

    /* Motif vide : toutes les feuilles de dépense, triées par chemin (Alimentation et Transport,
       parents, exclus ; Salaire, revenu, exclu). */
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "", &cats));
    VERIFIER(cats.taille == 5 && trouvee(&cats, 0) == j.courses && trouvee(&cats, 1) == j.hygiene
             && trouvee(&cats, 2) == racine_courses && trouvee(&cats, 3) == j.carburant
             && trouvee(&cats, 4) == taxi);
    /* Nom unique, sans distinction de casse (majuscule accentuée comprise). */
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "HYGIÈNE", &cats));
    VERIFIER(cats.taille == 1 && trouvee(&cats, 0) == j.hygiene);
    VERIFIER(strcmp(ELEMENT(&cats, CategorieTrouvee, 0)->chemin, "Alimentation > Hygiène") == 0);
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "carbu", &cats));
    VERIFIER(cats.taille == 1 && trouvee(&cats, 0) == j.carburant);
    /* Partie du chemin : le nom d'un parent désigne ses feuilles. */
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "alimentation", &cats));
    VERIFIER(cats.taille == 2 && trouvee(&cats, 0) == j.courses && trouvee(&cats, 1) == j.hygiene);
    /* Nom ambigu : trois candidats, triés par chemin. */
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "cours", &cats));
    VERIFIER(cats.taille == 3 && trouvee(&cats, 0) == j.courses && trouvee(&cats, 1) == racine_courses
             && trouvee(&cats, 2) == taxi);
    /* Chemin complet exact : il l'emporte (« Courses » racine contre deux correspondances partielles). */
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "courses", &cats));
    VERIFIER(cats.taille == 1 && trouvee(&cats, 0) == racine_courses);
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "alimentation > COURSES", &cats));
    VERIFIER(cats.taille == 1 && trouvee(&cats, 0) == j.courses);
    /* Aucun résultat ; type incompatible exclu. */
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "loisirs", &cats));
    VERIFIER(cats.taille == 0);
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_DEPENSE, "salaire", &cats));
    VERIFIER(cats.taille == 0);
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_REVENU, "sal", &cats));
    VERIFIER(cats.taille == 1 && trouvee(&cats, 0) == j.salaire);
    VERIFIER(analyses_categories_par_nom(&app.donnees, CAT_REVENU, "courses", &cats));
    VERIFIER(cats.taille == 0);

    /* Dernières opérations : 12 opérations confirmées + 1 échéance (date prévue 2026-11-15). */
    {
        const int jours[12] = {1, 2, 3, 4, 10, 10, 10, 5, 6, 7, 8, 9};
        const Id attendus[10] = {13, 5, 6, 7, 12, 11, 10, 9, 8, 4};
        int k;

        for (k = 0; k < 12; k++) {
            brute_op(&app, NATURE_DEPENSE, STATUT_CONFIRMEE, D(2026, 11, jours[k]), 1000, j.banque,
                     ID_AUCUN, ID_AUCUN, date_aucune());
        }
        brute_op(&app, NATURE_DEPENSE, STATUT_A_CONFIRMER, date_aucune(), 2000, j.banque, ID_AUCUN,
                 99, D(2026, 11, 15));
        VERIFIER(analyses_dernieres_operations(&app.donnees, 10, &res, &total));
        /* Plus de dix : dix affichées, le stockage garde les 13 ; même date : id croissant. */
        VERIFIER(total == 13 && app.donnees.operations.taille == 13);
        VERIFIER(ordre_egal(&res, attendus, 10));
        /* Ordre stable : un second appel donne le même résultat. */
        VERIFIER(analyses_dernieres_operations(&app.donnees, 10, &res, &total));
        VERIFIER(ordre_egal(&res, attendus, 10));
        /* Limite plus grande que le nombre d'opérations : toutes. */
        VERIFIER(analyses_dernieres_operations(&app.donnees, 50, &res, &total));
        VERIFIER(res.taille == 13 && total == 13);
    }

    app_liberer(&app);
    rapport_liberer(&r);
    tableau_liberer(&res);
    tableau_liberer(&cats);
}
