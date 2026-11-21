/*
 * BudgetLens — tests_unitaires.c
 * Tests automatiques de la logique sans saisie console.
 *
 * Usage : tests_unitaires DOSSIER_TEMPORAIRE
 *   Le dossier doit exister (tests.bat le crée) ; son contenu est remplacé.
 * Code de sortie : 0 si tous les tests réussissent, 1 sinon.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "persistance.h"
#include "referentiel.h"
#include "regles.h"
#include "somme.h"
#include "tests.h"
#include "tableau.h"
#include "texte.h"
#include "valeurs.h"

int nb_tests = 0;
int nb_echecs = 0;
const char *dossier_test;

static Date D(int a, int m, int j)
{
    return date_creer(a, m, j);
}

/* ------------------------------------------------------------------ */

static void test_fnv1a64(void)
{
    uint64_t h;

    printf("FNV-1a 64 bits (vecteurs de référence)\n");
    VERIFIER(fnv1a64("", 0) == UINT64_C(0xcbf29ce484222325));
    VERIFIER(fnv1a64("a", 1) == UINT64_C(0xaf63dc4c8601ec8c));
    VERIFIER(fnv1a64("foobar", 6) == UINT64_C(0x85944171f73967e8));
    /* Calcul par morceaux identique au calcul d'un seul tenant. */
    h = fnv1a64_ajouter(FNV1A64_BASE, "foo", 3);
    h = fnv1a64_ajouter(h, "bar", 3);
    VERIFIER(h == UINT64_C(0x85944171f73967e8));
}

static void test_dates(void)
{
    Date d;
    char t[DATE_TEXTE];

    printf("Dates\n");
    VERIFIER(annee_bissextile(2024) && !annee_bissextile(2025));
    VERIFIER(annee_bissextile(2000) && !annee_bissextile(1900));
    VERIFIER(jours_dans_mois(2024, 2) == 29 && jours_dans_mois(2025, 2) == 28);
    VERIFIER(date_lire("2024-02-29", &d) && d.annee == 2024 && d.mois == 2 && d.jour == 29);
    VERIFIER(!date_lire("2025-02-29", &d));
    VERIFIER(!date_lire("2025-13-01", &d));
    VERIFIER(!date_lire("2025-1-01", &d));
    VERIFIER(!date_lire("2025-01-01x", &d));
    VERIFIER(!date_lire("1899-12-31", &d));
    VERIFIER(date_vers_jours(D(1970, 1, 1)) == 0);
    VERIFIER(date_comparer(jours_vers_date(date_vers_jours(D(2026, 11, 19))), D(2026, 11, 19)) == 0);
    VERIFIER(date_comparer(date_ajouter_jours(D(2024, 2, 28), 1), D(2024, 2, 29)) == 0);
    VERIFIER(date_comparer(date_ajouter_jours(D(2025, 12, 31), 1), D(2026, 1, 1)) == 0);
    VERIFIER(date_comparer(date_ajouter_jours(D(2026, 3, 1), -1), D(2026, 2, 28)) == 0);
    date_formater(D(2026, 1, 5), t);
    VERIFIER(strcmp(t, "2026-01-05") == 0);
}

static void test_montants(void)
{
    long long ct;
    long long somme;
    char t[MONTANT_TEXTE];

    printf("Montants (centimes, sans virgule flottante)\n");
    VERIFIER(montant_lire("120,50", &ct) && ct == 12050);
    VERIFIER(montant_lire("120.50", &ct) && ct == 12050);
    VERIFIER(montant_lire("120,5", &ct) && ct == 12050);
    VERIFIER(montant_lire("0,05", &ct) && ct == 5);
    VERIFIER(montant_lire("7", &ct) && ct == 700);
    VERIFIER(!montant_lire("1,234", &ct));
    VERIFIER(!montant_lire("-5", &ct));
    VERIFIER(!montant_lire("abc", &ct));
    VERIFIER(!montant_lire("", &ct));
    VERIFIER(!montant_lire("12,", &ct));
    VERIFIER(montant_lire("1000000000", &ct) && ct == MONTANT_MAX_CT);
    VERIFIER(!montant_lire("1000000000,01", &ct));
    VERIFIER(montant_additionner(MONTANT_MAX_CT, 1, &somme) && somme == MONTANT_MAX_CT + 1);
    VERIFIER(!montant_additionner(9223372036854775807LL, 1, &somme));
    montant_formater(1234567, t, sizeof t);
    VERIFIER(strcmp(t, "12 345,67 DH") == 0);
}

static void test_texte(void)
{
    printf("Texte UTF-8 et comparaison sans casse\n");
    VERIFIER(utf8_valide("Éducation"));
    VERIFIER(!utf8_valide("\xC0\xAF"));            /* forme trop longue */
    VERIFIER(!utf8_valide("\xED\xA0\x80"));        /* demi-code UTF-16 */
    VERIFIER(!utf8_valide("\xF4\x90\x80\x80"));    /* au-delà de U+10FFFF */
    VERIFIER(!utf8_valide("\x80"));                /* octet de suite isolé */
    VERIFIER(!utf8_valide("\xC3"));                /* séquence tronquée */
    VERIFIER(texte_verifier("Loyer", NOM_MAX, true) == TEXTE_OK);
    VERIFIER(texte_verifier("", NOM_MAX, true) == TEXTE_VIDE);
    VERIFIER(texte_verifier("a;b", NOM_MAX, true) == TEXTE_CARACTERE_INTERDIT);
    VERIFIER(texte_verifier("a\nb", NOM_MAX, true) == TEXTE_CARACTERE_INTERDIT);
    VERIFIER(texte_verifier("a\tb", NOM_MAX, true) == TEXTE_CARACTERE_INTERDIT);
    VERIFIER(texte_verifier("a\xC2\x85" "b", NOM_MAX, true) == TEXTE_CARACTERE_INTERDIT); /* C1 */
    VERIFIER(texte_verifier("\xC0\xAF", NOM_MAX, true) == TEXTE_UTF8_INVALIDE);
    VERIFIER(texte_verifier("abcdef", 5, true) == TEXTE_TROP_LONG);
    VERIFIER(texte_egal_sans_casse("ÉDUCATION", "éducation"));
    VERIFIER(texte_egal_sans_casse("ÇA", "ça"));
    VERIFIER(texte_egal_sans_casse("ŒUVRE", "œuvre"));
    VERIFIER(texte_egal_sans_casse("Ÿ", "ÿ"));
    VERIFIER(!texte_egal_sans_casse("é", "e"));          /* pas de suppression des accents */
    VERIFIER(!texte_egal_sans_casse("é", "e\xCC\x81"));  /* pas de normalisation Unicode */
    VERIFIER(!texte_egal_sans_casse("×", "÷"));          /* × n'est pas une lettre */
    VERIFIER(texte_contient_sans_casse("Courses CARREFOUR", "carre"));
    VERIFIER(texte_largeur("Éé") == 2);
    {
        char s[32] = "  Loyer  ";
        char c[4];
        texte_rogner(s);
        VERIFIER(strcmp(s, "Loyer") == 0);
        texte_copier(c, sizeof c, "aéé");   /* "a" + 2 octets + coupe avant le 2e é */
        VERIFIER(strcmp(c, "aé") == 0);
    }
}

static void test_tableau(void)
{
    Tableau t;
    Tableau u;
    int v;
    size_t i;

    printf("Tableau dynamique\n");
    tableau_init(&t, sizeof(int));
    for (v = 1; v <= 100; v++) {
        VERIFIER(tableau_ajouter(&t, &v) || (printf("mémoire\n"), 0));
    }
    VERIFIER(t.taille == 100 && t.capacite >= 100);
    tableau_retirer(&t, 0);
    VERIFIER(t.taille == 99 && *ELEMENT(&t, int, 0) == 2);
    tableau_init(&u, sizeof(int));
    VERIFIER(tableau_copier(&u, &t) && u.taille == 99);
    for (i = 0; i < u.taille; i++) {
        if (*ELEMENT(&u, int, i) != (int)i + 2) {
            break;
        }
    }
    VERIFIER(i == u.taille);
    tableau_liberer(&t);
    tableau_liberer(&u);
    VERIFIER(t.taille == 0 && t.donnees == NULL);
}

/* ------------------------------------------------------------------ */

static Compte compte_type(const char *nom, long long solde)
{
    Compte c;

    memset(&c, 0, sizeof c);
    texte_copier(c.nom, sizeof c.nom, nom);
    c.type = COMPTE_BANQUE;
    c.solde_initial_ct = solde;
    c.date_solde_initial = D(2026, 1, 1);
    return c;
}

/* Petit jeu de données : 2 comptes, catégories, tiers, véhicule, budget. */
static void remplir(Application *app)
{
    RapportErreurs r;
    Compte c;
    Id id;
    Id alim;

    rapport_init(&r);
    c = compte_type("Banque Populaire", 500000);
    VERIFIER(ref_compte_creer(app, &c, &id, &r) == RES_OK);
    c = compte_type("Portefeuille", 20000);
    c.type = COMPTE_ESPECES;
    VERIFIER(ref_compte_creer(app, &c, &id, &r) == RES_OK);
    VERIFIER(ref_categorie_creer(app, "Alimentation", ID_AUCUN, CAT_DEPENSE, &alim, &r) == RES_OK);
    VERIFIER(ref_categorie_creer(app, "Courses", alim, CAT_DEPENSE, &id, &r) == RES_OK);
    VERIFIER(ref_categorie_creer(app, "Salaire", ID_AUCUN, CAT_REVENU, &id, &r) == RES_OK);
    VERIFIER(ref_tiers_creer(app, "Carrefour", &id, &r) == RES_OK);
    VERIFIER(ref_vehicule_creer(app, "Clio", &id, &r) == RES_OK);
    VERIFIER(ref_budget_creer(app, 2026, 11, alim, 150000, &id, &r) == RES_OK);
    if (rapport_nombre(&r) > 0) {
        rapport_afficher(&r, stdout);
    }
    rapport_liberer(&r);
}

static void test_referentiel(void)
{
    Application app;
    RapportErreurs r;
    Compte c;
    Id id;
    Id salaire = ID_AUCUN;
    size_t k;

    printf("Référentiel (comptes, catégories, budgets, tiers, véhicule)\n");
    app_init(&app, D(2026, 11, 13), true, ".");
    rapport_init(&r);
    remplir(&app);
    VERIFIER(app.modifie && app.nb_modifications == 8);

    c = compte_type("BANQUE populaire", 0);         /* B2 : unicité sans casse */
    VERIFIER(ref_compte_creer(&app, &c, &id, &r) == RES_ERR_VALIDATION);
    rapport_vider(&r);
    c = compte_type("Compte négatif", -1);          /* G10 : solde initial >= 0 */
    VERIFIER(ref_compte_creer(&app, &c, &id, &r) != RES_OK);
    rapport_vider(&r);
    c = compte_type("Livret;A", 0);                 /* T1 : « ; » interdit */
    VERIFIER(ref_compte_creer(&app, &c, &id, &r) != RES_OK);
    rapport_vider(&r);
    VERIFIER(ref_categorie_creer(&app, "ALIMENTATION", ID_AUCUN, CAT_DEPENSE, &id, &r) != RES_OK);
    rapport_vider(&r);
    VERIFIER(ref_vehicule_creer(&app, "Second", &id, &r) != RES_OK);   /* véhicule unique */
    rapport_vider(&r);
    for (k = 0; k < app.donnees.categories.taille; k++) {
        const Categorie *cat = ELEMENT(&app.donnees.categories, Categorie, k);
        if (strcmp(cat->nom, "Salaire") == 0) {
            salaire = cat->id;
        }
    }
    VERIFIER(ref_budget_creer(&app, 2026, 11, salaire, 1000, &id, &r) != RES_OK); /* REVENU */
    rapport_vider(&r);
    VERIFIER(ref_budget_creer(&app, 2026, 13, ID_AUCUN, 1000, &id, &r) != RES_OK);
    rapport_vider(&r);
    VERIFIER(ref_budget_creer(&app, 2026, 11, ID_AUCUN, 0, &id, &r) == RES_OK);  /* global, 0 */
    VERIFIER(ref_budget_creer(&app, 2026, 11, ID_AUCUN, 5, &id, &r) != RES_OK);  /* doublon */
    rapport_vider(&r);
    {
        size_t avant = app.donnees.comptes.taille;
        long modifs = app.nb_modifications;
        c = compte_type("Banque Populaire", 0);
        VERIFIER(ref_compte_creer(&app, &c, &id, &r) != RES_OK);
        VERIFIER(app.donnees.comptes.taille == avant && app.nb_modifications == modifs);
        rapport_vider(&r);
    }
    VERIFIER(regles_valider(&app.donnees, &r));
    rapport_liberer(&r);
    app_liberer(&app);
}

/* ------------------------------------------------------------------ */

static void chemin(char *s, size_t n, const char *prefixe, const char *nom)
{
    snprintf(s, n, "%s/%s%s", dossier_test, prefixe, nom);
}

static void nettoyer(void)
{
    char c[CHEMIN_MAX + 64];

    jeu_supprimer(dossier_test, "A_");
    jeu_supprimer(dossier_test, "B_");
    chemin(c, sizeof c, "", NOM_VERROU);
    remove(c);
    chemin(c, sizeof c, "A_", "manifest.tmp");
    remove(c);
    chemin(c, sizeof c, "B_", "manifest.tmp");
    remove(c);
}

/* Modifie un octet d'un chiffre du fichier sans changer sa taille ni son nombre de lignes. */
static bool alterer_chiffre(const char *prefixe, const char *nom)
{
    char c[CHEMIN_MAX + 64];
    FILE *f;
    int octet;
    long position = 0;
    bool fait = false;

    chemin(c, sizeof c, prefixe, nom);
    f = fopen(c, "r+b");
    if (f == NULL) {
        return false;
    }
    /* Premier chiffre après la première fin de ligne (sauter l'en-tête). */
    while ((octet = fgetc(f)) != EOF && octet != '\n') {
        position++;
    }
    position++;
    while ((octet = fgetc(f)) != EOF) {
        if (octet >= '0' && octet <= '8') {
            fseek(f, position, SEEK_SET);
            fputc(octet + 1, f);
            fait = true;
            break;
        }
        position++;
    }
    fclose(f);
    return fait;
}

/* Change le dernier chiffre hexadécimal du fichier (somme de la dernière ligne du manifeste). */
static bool alterer_fin(const char *prefixe, const char *nom)
{
    char c[CHEMIN_MAX + 64];
    FILE *f;
    int octet;
    bool fait = false;

    chemin(c, sizeof c, prefixe, nom);
    f = fopen(c, "r+b");
    if (f == NULL) {
        return false;
    }
    if (fseek(f, -2L, SEEK_END) == 0 && (octet = fgetc(f)) != EOF
        && ((octet >= '0' && octet <= '9') || (octet >= 'a' && octet <= 'f'))) {
        fseek(f, -2L, SEEK_END);
        fputc(octet == '0' ? '1' : '0', f);
        fait = true;
    }
    fclose(f);
    return fait;
}

static EtatChargement recharger(Application *app, RapportErreurs *infos)
{
    app_liberer(app);
    app_init(app, D(2026, 11, 13), true, dossier_test);
    rapport_vider(infos);
    return persistance_charger(app, infos);
}

static void test_persistance(void)
{
    Application app;
    RapportErreurs r;
    Compte c;
    Id id;
    char p[CHEMIN_MAX + 64];
    const char *f_compte = persistance_nom_fichier(ENT_COMPTE);

    printf("Persistance A/B, manifeste, sommes de contrôle, reprise\n");
    nettoyer();
    rapport_init(&r);
    app_init(&app, D(2026, 11, 13), true, dossier_test);
    VERIFIER(persistance_charger(&app, &r) == CHARGEMENT_VIDE);

    remplir(&app);
    VERIFIER(persistance_enregistrer(&app, &r) == RES_OK);          /* génération 1 -> A */
    VERIFIER(app.emplacement_charge == 1 && app.generation_chargee == 1 && !app.modifie);
    c = compte_type("Épargne", 100);
    VERIFIER(ref_compte_creer(&app, &c, &id, &r) == RES_OK);
    VERIFIER(persistance_enregistrer(&app, &r) == RES_OK);          /* génération 2 -> B */
    VERIFIER(app.emplacement_charge == 2 && app.generation_chargee == 2);

    /* Rechargement : génération la plus récente, contenu identique. */
    VERIFIER(recharger(&app, &r) == CHARGEMENT_OK);
    VERIFIER(app.emplacement_charge == 2 && app.generation_chargee == 2);
    VERIFIER(app.donnees.comptes.taille == 3 && app.donnees.categories.taille == 3);
    VERIFIER(app.donnees.prochain_id[ENT_COMPTE] == 4);
    VERIFIER(texte_egal_sans_casse(ELEMENT(&app.donnees.comptes, Compte, 2)->nom, "ÉPARGNE"));

    /* Troisième enregistrement : retour sur A, B (génération 2) intact. */
    VERIFIER(persistance_enregistrer(&app, &r) == RES_OK);
    VERIFIER(app.emplacement_charge == 1 && app.generation_chargee == 3);

    /* Corruption de même taille et même nombre de lignes : seule la somme la détecte. */
    VERIFIER(alterer_chiffre("A_", f_compte));
    VERIFIER(recharger(&app, &r) == CHARGEMENT_REPLI);
    VERIFIER(app.emplacement_charge == 2 && app.generation_chargee == 2);
    VERIFIER(app.generation_max == 3);
    /* Le prochain enregistrement réécrit l'emplacement abîmé (A) avec la génération 4. */
    VERIFIER(persistance_enregistrer(&app, &r) == RES_OK);
    VERIFIER(app.emplacement_charge == 1 && app.generation_chargee == 4);
    VERIFIER(recharger(&app, &r) == CHARGEMENT_OK && app.generation_chargee == 4);

    /* Enregistrement interrompu : fichiers écrits mais manifeste absent -> ignoré. */
    chemin(p, sizeof p, "B_", "manifest.csv");
    VERIFIER(remove(p) == 0);
    VERIFIER(recharger(&app, &r) == CHARGEMENT_OK && app.generation_chargee == 4);

    /* Somme falsifiée dans un manifeste lisible -> génération 5 rejetée, repli sur 4. */
    VERIFIER(persistance_enregistrer(&app, &r) == RES_OK);          /* génération 5 -> B */
    VERIFIER(alterer_fin("B_", "manifest.csv"));
    VERIFIER(recharger(&app, &r) == CHARGEMENT_REPLI && app.generation_chargee == 4);
    VERIFIER(app.generation_max == 5);

    /* Manifeste illisible (version de format altérée) -> signalé comme repli. */
    VERIFIER(alterer_chiffre("B_", "manifest.csv"));
    VERIFIER(recharger(&app, &r) == CHARGEMENT_REPLI && app.generation_chargee == 4);

    /* Les deux emplacements invalides -> ÉCHEC (aucun démarrage vide silencieux). */
    VERIFIER(alterer_chiffre("A_", persistance_nom_fichier(ENT_CATEGORIE)));
    VERIFIER(recharger(&app, &r) == CHARGEMENT_ECHEC);
    VERIFIER(app.donnees.comptes.taille == 0);
    VERIFIER(rapport_nombre(&r) > 0);

    /* Fichier manquant -> rejeté. */
    nettoyer();
    app_liberer(&app);
    app_init(&app, D(2026, 11, 13), true, dossier_test);
    remplir(&app);
    VERIFIER(persistance_enregistrer(&app, &r) == RES_OK);
    chemin(p, sizeof p, "A_", persistance_nom_fichier(ENT_TIERS));
    VERIFIER(remove(p) == 0);
    VERIFIER(recharger(&app, &r) == CHARGEMENT_ECHEC);

    nettoyer();
    rapport_liberer(&r);
    app_liberer(&app);
}

static void test_verrou(void)
{
    char v1[CHEMIN_MAX + 64];
    char v2[CHEMIN_MAX + 64];
    char absent[CHEMIN_MAX + 64];

    printf("Verrou d'exécution (fopen \"wx\")\n");
    nettoyer();
    VERIFIER(persistance_verrouiller(dossier_test, v1, sizeof v1) == VERROU_OK);
    VERIFIER(persistance_verrouiller(dossier_test, v2, sizeof v2) == VERROU_PRIS);
    persistance_deverrouiller(v1);
    VERIFIER(persistance_verrouiller(dossier_test, v2, sizeof v2) == VERROU_OK);
    persistance_deverrouiller(v2);
    snprintf(absent, sizeof absent, "%s/dossier_inexistant", dossier_test);
    VERIFIER(persistance_verrouiller(absent, v1, sizeof v1) == VERROU_ERREUR);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage : tests_unitaires DOSSIER_TEMPORAIRE\n");
        return 2;
    }
    dossier_test = argv[1];
    setvbuf(stdout, NULL, _IONBF, 0);   /* sortie immédiate : visible même en cas de plantage */
    test_fnv1a64();
    test_dates();
    test_montants();
    test_texte();
    test_tableau();
    test_referentiel();
    test_persistance();
    test_verrou();
    test_validateur_operations();
    test_soldes_et_seuils();
    test_operations_manuelles();
    test_analyses();
    test_utilitaires_etape3();
    test_doublons();
    test_bilan_vehicule();
    test_categories_avancees();
    test_reaffectations();
    test_calendrier();
    test_validateur_recurrences();
    test_modeles();
    test_recurrences();
    test_echeances_et_lots();
    test_export_sauvegarde_restauration();
    test_import_additif();
    test_ergonomie();
    printf("\n%d test(s), %d échec(s)\n", nb_tests, nb_echecs);
    return nb_echecs == 0 ? 0 : 1;
}
