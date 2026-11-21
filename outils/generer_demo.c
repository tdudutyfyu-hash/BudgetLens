/*
 * BudgetLens — outils/generer_demo.c
 * Génère le jeu de démonstration (données fictives, date de référence 2026-11-13).
 *
 * Usage : generer_demo DOSSIER_DONNEES DOSSIER_EXPORT
 *   DOSSIER_DONNEES : dossier existant où le jeu est enregistré (emplacement A, utilisable avec
 *                     « BudgetLens --donnees DOSSIER_DONNEES --date 2026-11-13 »)
 *   DOSSIER_EXPORT  : dossier existant où le même jeu est exporté (restaurable par le menu
 *                     Fichiers > Restaurer)
 *
 * Le jeu passe par les mêmes fonctions que le programme : le validateur global (soldes de fin de
 * journée, ventilation, calendrier des récurrences...) le contrôle à chaque ajout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "modeles.h"
#include "operations.h"
#include "persistance.h"
#include "recurrences.h"
#include "referentiel.h"
#include "regles.h"
#include "sauvegardes.h"
#include "texte.h"
#include "valeurs.h"

static RapportErreurs rapport;

static void echec(const char *quoi)
{
    fprintf(stderr, "ÉCHEC : %s\n", quoi);
    rapport_afficher(&rapport, stderr);
    exit(1);
}

#define OK(expr, quoi)                                                       \
    do {                                                                     \
        rapport_vider(&rapport);                                             \
        if ((expr) != RES_OK) {                                              \
            echec(quoi);                                                     \
        }                                                                    \
    } while (0)

static Application app;

static Id compte(const char *nom, TypeCompte type, long long solde, long long seuil,
                 const char *banque, const char *reference)
{
    Compte c;
    Id id = ID_AUCUN;

    memset(&c, 0, sizeof c);
    texte_copier(c.nom, sizeof c.nom, nom);
    c.type = type;
    c.solde_initial_ct = solde;
    c.date_solde_initial = date_creer(2026, 9, 1);
    c.a_seuil = seuil > 0;
    c.seuil_minimal_ct = seuil;
    texte_copier(c.nom_banque, sizeof c.nom_banque, banque);
    texte_copier(c.reference_compte, sizeof c.reference_compte, reference);
    OK(ref_compte_creer(&app, &c, &id, &rapport), nom);
    return id;
}

static Id categorie(const char *nom, Id parent, TypeCategorie type)
{
    Id id = ID_AUCUN;

    OK(ref_categorie_creer(&app, nom, parent, type, &id, &rapport), nom);
    return id;
}

static Id tiers(const char *nom)
{
    Id id = ID_AUCUN;

    OK(ref_tiers_creer(&app, nom, &id, &rapport), nom);
    return id;
}

static LigneSaisie ligne(Id cat, long long dh, Id vehicule)
{
    LigneSaisie l;

    l.id_categorie = cat;
    l.id_vehicule = vehicule;
    l.montant_ct = dh * 100;
    return l;
}

/* Opération manuelle confirmée ; montants en DH entiers (la démonstration n'a pas besoin de centimes). */
static Id operation(int mois, int jour, const char *libelle, Nature nature, long long dh, Id src, Id dst,
                    Id tiers_id, const LigneSaisie *lignes, size_t nb)
{
    OperationSaisie s;
    Id id = ID_AUCUN;

    memset(&s, 0, sizeof s);
    s.nature = nature;
    s.date_realisation = date_creer(2026, mois, jour);
    texte_copier(s.libelle, sizeof s.libelle, libelle);
    s.montant_total_ct = dh * 100;
    s.id_compte_source = src;
    s.id_compte_destination = dst;
    s.id_tiers = tiers_id;
    OK(op_creer(&app, &s, lignes, nb, true, &id, NULL, &rapport), libelle);
    return id;
}

static Id depense(int mois, int jour, const char *libelle, long long dh, Id src, Id tiers_id, Id cat,
                  Id vehicule)
{
    LigneSaisie l = ligne(cat, dh, vehicule);

    return operation(mois, jour, libelle, NATURE_DEPENSE, dh, src, ID_AUCUN, tiers_id, &l, 1);
}

static void budget(int mois, Id cat, long long dh)
{
    Id id = ID_AUCUN;

    OK(ref_budget_creer(&app, 2026, mois, cat, dh * 100, &id, &rapport), "budget");
}

static Id modele(const char *nom, const char *libelle, Nature nature, long long dh, Id src, Id dst,
                 Id tiers_id, const LigneSaisie *lignes, size_t nb)
{
    ModeleSaisie m;
    Id id = ID_AUCUN;

    memset(&m, 0, sizeof m);
    m.nature = nature;
    texte_copier(m.nom, sizeof m.nom, nom);
    texte_copier(m.libelle, sizeof m.libelle, libelle);
    m.montant_total_ct = dh * 100;
    m.id_compte_source = src;
    m.id_compte_destination = dst;
    m.id_tiers = tiers_id;
    OK(mod_creer(&app, &m, lignes, nb, &id, &rapport), nom);
    return id;
}

static Id recurrence(Id id_modele, Frequence f, int a, int m, int j, int jour_mensuel, size_t *nb)
{
    RecurrenceSaisie s;
    Id id = ID_AUCUN;

    s.id_modele = id_modele;
    s.frequence = f;
    s.date_debut = date_creer(a, m, j);
    s.date_fin = date_aucune();
    s.jour_mensuel_souhaite = jour_mensuel;
    OK(rec_creer(&app, &s, RETRO_GENERER, &id, nb, &rapport), "récurrence");
    return id;
}

int main(int argc, char **argv)
{
    Id banque;
    Id especes;
    Id epargne;
    Id vehicule;
    Id alim;
    Id courses;
    Id restaurants;
    Id transport;
    Id carburant;
    Id entretien;
    Id logement;
    Id loyer;
    Id elec;
    Id internet;
    Id sante;
    Id pharmacie;
    Id consult;
    Id loisirs;
    Id cinema;
    Id abonnements;
    Id etudes;
    Id livres;
    Id salaire;
    Id bourse;
    Id marjane;
    Id carrefour;
    Id afriquia;
    Id atlas;
    Id proprio;
    Id telecom;
    Id employeur;
    Id cine;
    Id mod_loyer;
    Id mod_sport;
    Id rec_loyer;
    Id rec_sport;
    size_t nb = 0;
    LigneSaisie l[2];
    Id id = ID_AUCUN;

    if (argc != 3) {
        fprintf(stderr, "Usage : generer_demo DOSSIER_DONNEES DOSSIER_EXPORT\n");
        return 2;
    }
    rapport_init(&rapport);
    app_init(&app, date_creer(2026, 11, 13), true, argv[1]);

    banque = compte("Banque Populaire", COMPTE_BANQUE, 1200000, 100000, "Banque Populaire", "MA-000-1234");
    especes = compte("Espèces", COMPTE_ESPECES, 80000, 10000, "", "");
    epargne = compte("Épargne", COMPTE_BANQUE, 500000, 0, "CIH Bank", "MA-000-9876");

    alim = categorie("Alimentation", ID_AUCUN, CAT_DEPENSE);
    courses = categorie("Courses", alim, CAT_DEPENSE);
    restaurants = categorie("Restaurants", alim, CAT_DEPENSE);
    transport = categorie("Transport", ID_AUCUN, CAT_DEPENSE);
    carburant = categorie("Carburant", transport, CAT_DEPENSE);
    (void)categorie("Transports en commun", transport, CAT_DEPENSE);
    entretien = categorie("Entretien véhicule", transport, CAT_DEPENSE);
    logement = categorie("Logement", ID_AUCUN, CAT_DEPENSE);
    loyer = categorie("Loyer", logement, CAT_DEPENSE);
    elec = categorie("Électricité et eau", logement, CAT_DEPENSE);
    internet = categorie("Internet", logement, CAT_DEPENSE);
    sante = categorie("Santé", ID_AUCUN, CAT_DEPENSE);
    pharmacie = categorie("Pharmacie", sante, CAT_DEPENSE);
    consult = categorie("Consultations", sante, CAT_DEPENSE);
    loisirs = categorie("Loisirs", ID_AUCUN, CAT_DEPENSE);
    cinema = categorie("Cinéma", loisirs, CAT_DEPENSE);
    abonnements = categorie("Abonnements", loisirs, CAT_DEPENSE);
    etudes = categorie("Études", ID_AUCUN, CAT_DEPENSE);
    livres = categorie("Livres et fournitures", etudes, CAT_DEPENSE);
    salaire = categorie("Salaire", ID_AUCUN, CAT_REVENU);
    bourse = categorie("Bourse", ID_AUCUN, CAT_REVENU);
    (void)categorie("Autres revenus", ID_AUCUN, CAT_REVENU);

    marjane = tiers("Marjane");
    carrefour = tiers("Carrefour");
    afriquia = tiers("Afriquia");
    atlas = tiers("Pharmacie Atlas");
    proprio = tiers("Propriétaire (Mme Alaoui)");
    telecom = tiers("Maroc Telecom");
    employeur = tiers("Société Atlas");
    cine = tiers("Cinéma Rif");
    OK(ref_vehicule_creer(&app, "Dacia Logan", &vehicule, &rapport), "véhicule");

    /* ----- Septembre 2026 ----- */
    operation(9, 3, "Retrait espèces", NATURE_VIREMENT, 1000, banque, especes, ID_AUCUN, NULL, 0);
    l[0] = ligne(salaire, 8500, ID_AUCUN);
    operation(9, 5, "Salaire septembre", NATURE_REVENU, 8500, ID_AUCUN, banque, employeur, l, 1);
    depense(9, 6, "Courses Marjane", 380, banque, marjane, courses, ID_AUCUN);
    depense(9, 8, "Abonnement internet", 199, banque, telecom, internet, ID_AUCUN);
    depense(9, 10, "Plein d'essence", 300, banque, afriquia, carburant, vehicule);
    depense(9, 12, "Restaurant avec des amis", 140, especes, ID_AUCUN, restaurants, ID_AUCUN);
    depense(9, 14, "Facture eau et électricité", 340, banque, ID_AUCUN, elec, ID_AUCUN);
    l[0] = ligne(courses, 410, ID_AUCUN);
    l[1] = ligne(pharmacie, 50, ID_AUCUN);
    operation(9, 18, "Courses Carrefour", NATURE_DEPENSE, 460, banque, ID_AUCUN, carrefour, l, 2);
    depense(9, 20, "Séance de cinéma", 90, especes, cine, cinema, ID_AUCUN);
    depense(9, 24, "Plein d'essence", 280, banque, afriquia, carburant, vehicule);
    depense(9, 27, "Livres de cours", 260, banque, ID_AUCUN, livres, ID_AUCUN);

    /* ----- Octobre 2026 ----- */
    operation(10, 3, "Retrait espèces", NATURE_VIREMENT, 1000, banque, especes, ID_AUCUN, NULL, 0);
    l[0] = ligne(bourse, 1500, ID_AUCUN);
    operation(10, 4, "Bourse d'études", NATURE_REVENU, 1500, ID_AUCUN, banque, ID_AUCUN, l, 1);
    l[0] = ligne(salaire, 8500, ID_AUCUN);
    operation(10, 5, "Salaire octobre", NATURE_REVENU, 8500, ID_AUCUN, banque, employeur, l, 1);
    depense(10, 6, "Courses Marjane", 410, banque, marjane, courses, ID_AUCUN);
    depense(10, 8, "Abonnement internet", 199, banque, telecom, internet, ID_AUCUN);
    depense(10, 8, "Plein d'essence", 300, banque, afriquia, carburant, vehicule);
    depense(10, 12, "Médicaments", 85, banque, atlas, pharmacie, ID_AUCUN);
    depense(10, 14, "Facture eau et électricité", 365, banque, ID_AUCUN, elec, ID_AUCUN);
    depense(10, 16, "Restaurant", 160, especes, ID_AUCUN, restaurants, ID_AUCUN);
    depense(10, 19, "Courses Carrefour", 440, banque, carrefour, courses, ID_AUCUN);
    depense(10, 22, "Plein d'essence", 290, banque, afriquia, carburant, vehicule);
    depense(10, 25, "Abonnement streaming", 60, banque, ID_AUCUN, abonnements, ID_AUCUN);
    depense(10, 28, "Consultation médicale", 200, banque, ID_AUCUN, consult, ID_AUCUN);

    /* ----- Novembre 2026 (jusqu'au 13) ----- */
    operation(11, 3, "Retrait espèces", NATURE_VIREMENT, 1000, banque, especes, ID_AUCUN, NULL, 0);
    l[0] = ligne(courses, 350, ID_AUCUN);
    l[1] = ligne(pharmacie, 70, ID_AUCUN);
    operation(11, 4, "Courses Marjane", NATURE_DEPENSE, 420, banque, ID_AUCUN, marjane, l, 2);
    l[0] = ligne(salaire, 8500, ID_AUCUN);
    operation(11, 5, "Salaire novembre", NATURE_REVENU, 8500, ID_AUCUN, banque, employeur, l, 1);
    depense(11, 5, "Plein d'essence", 310, banque, afriquia, carburant, vehicule);
    operation(11, 6, "Virement vers l'épargne", NATURE_VIREMENT, 500, banque, epargne, ID_AUCUN, NULL, 0);
    depense(11, 8, "Abonnement internet", 199, banque, telecom, internet, ID_AUCUN);
    depense(11, 9, "Médicaments", 85, banque, atlas, pharmacie, ID_AUCUN);
    depense(11, 9, "Médicaments", 85, banque, atlas, pharmacie, ID_AUCUN);       /* doublon potentiel (I6) */
    depense(11, 10, "Restaurant", 175, especes, ID_AUCUN, restaurants, ID_AUCUN);
    l[0] = ligne(carburant, 280, vehicule);
    l[1] = ligne(entretien, 50, vehicule);
    operation(11, 11, "Station + lavage", NATURE_DEPENSE, 330, banque, ID_AUCUN, afriquia, l, 2);
    depense(11, 12, "Séance de cinéma", 110, especes, cine, cinema, ID_AUCUN);
    depense(11, 12, "Courses Carrefour", 395, banque, carrefour, courses, ID_AUCUN);

    /* ----- Budgets ----- */
    budget(10, ID_AUCUN, 6000);
    budget(10, alim, 1800);
    budget(11, ID_AUCUN, 6000);
    budget(11, alim, 1800);
    budget(11, courses, 700);          /* sera dépassé : 350 + 395 = 745 DH */
    budget(11, transport, 800);
    budget(11, sante, 300);
    budget(11, loisirs, 500);

    /* ----- Modèles et récurrences ----- */
    l[0] = ligne(loyer, 3000, ID_AUCUN);
    mod_loyer = modele("Loyer", "Loyer mensuel", NATURE_DEPENSE, 3000, banque, ID_AUCUN, proprio, l, 1);
    l[0] = ligne(abonnements, 150, ID_AUCUN);
    mod_sport = modele("Abonnement sport", "Abonnement salle de sport", NATURE_DEPENSE, 150, especes,
                       ID_AUCUN, ID_AUCUN, l, 1);
    l[0] = ligne(carburant, 300, vehicule);
    (void)modele("Plein d'essence", "Plein d'essence", NATURE_DEPENSE, 300, banque, ID_AUCUN, afriquia, l, 1);
    rec_loyer = recurrence(mod_loyer, FREQ_MENSUELLE, 2026, 9, 1, 1, &nb);       /* 1/9, 1/10, 1/11 */
    if (nb != 3) {
        echec("trois échéances de loyer attendues");
    }
    rec_sport = recurrence(mod_sport, FREQ_HEBDOMADAIRE, 2026, 10, 26, 0, &nb);  /* 26/10, 2/11, 9/11 */
    if (nb != 3) {
        echec("trois échéances de sport attendues");
    }
    (void)rec_sport;
    (void)rec_loyer;
    /* Les loyers de septembre et octobre ont été payés : confirmés avec leur date prévue. Celui de novembre
       et les abonnements de sport restent « à confirmer » pour la démonstration des échéances et des lots. */
    for (id = 1; id <= (Id)app.donnees.operations.taille; id++) {
        const Operation *o = regles_operation(&app.donnees, id);
        if (o != NULL && o->id_recurrence == rec_loyer && o->statut == STATUT_A_CONFIRMER
            && o->date_prevue_origine.mois < 11) {
            OK(op_confirmer(&app, o->id, o->date_prevue_origine, 0, NULL, 0, true, NULL, &rapport), "loyer");
        }
    }

    /* ----- Contrôle final et écriture ----- */
    rapport_vider(&rapport);
    if (!regles_valider(&app.donnees, &rapport)) {
        echec("le jeu de démonstration est invalide");
    }
    app.modifie = true;
    OK(persistance_enregistrer(&app, &rapport), "enregistrement dans le dossier de données");
    OK(fich_exporter(&app, argv[2], &rapport), "export");
    printf("Jeu de démonstration généré : %lu opérations, %lu comptes, %lu catégories, %lu récurrences.\n",
           (unsigned long)app.donnees.operations.taille, (unsigned long)app.donnees.comptes.taille,
           (unsigned long)app.donnees.categories.taille, (unsigned long)app.donnees.recurrences.taille);
    printf("Données : %s (emplacement A) ; export restaurable : %s\n", argv[1], argv[2]);
    app_liberer(&app);
    rapport_liberer(&rapport);
    return 0;
}
