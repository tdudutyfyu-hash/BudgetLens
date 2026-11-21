/*
 * BudgetLens — persistance.c
 *
 * Enregistrer :
 *   1. choisir l'emplacement inactif (B si A est chargé, et inversement) ;
 *   2. supprimer son manifest.csv : l'emplacement devient invalide ;
 *   3. écrire les onze fichiers, chaque écriture étant vérifiée ;
 *   4. écrire manifest.csv en dernier : c'est la validation de l'enregistrement.
 * Charger : l'emplacement valide de plus haute génération est retenu ;
 * sinon l'autre, avec un avertissement.
 * Limite : le C ISO ne fournit pas fsync ; une coupure de courant peut faire
 * perdre le dernier enregistrement, l'emplacement précédent restant valide.
 */
#include "persistance.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "csv.h"
#include "regles.h"
#include "somme.h"
#include "texte.h"
#include "valeurs.h"

#define FICHIER_METADATA 10
#define NOM_MANIFESTE "manifest.csv"
#define NOM_MANIFESTE_TEMP "manifest.tmp"
#define ENTETE_MANIFESTE "element;valeur;fnv1a64"
#define GENERATION_MAX 2000000000L
#define ERREURS_PAR_FICHIER_MAX 50

static const char *const NOMS_FICHIERS[NB_FICHIERS] = {
    "comptes.csv", "categories.csv", "tiers.csv", "vehicules.csv", "budgets_mensuels.csv",
    "modeles_operations.csv", "lignes_modeles.csv", "recurrences.csv", "operations.csv",
    "lignes_operations.csv", "metadata.csv"
};

static const char *const ENTETES[NB_FICHIERS] = {
    "id_compte;nom;type_compte;solde_initial_ct;date_solde_initial;seuil_minimal_ct;nom_banque;reference_compte",
    "id_categorie;nom;type_categorie;id_parent",
    "id_tiers;nom",
    "id_vehicule;nom",
    "id_budget;annee;mois;plafond_ct;id_categorie",
    "id_modele;nom;nature;libelle;montant_total_ct;id_compte_source;id_compte_destination;id_tiers",
    "id_ligne_modele;id_modele;id_categorie;id_vehicule;montant_ct",
    "id_recurrence;id_modele;frequence;date_debut;date_fin;jour_mensuel_souhaite;active;date_prochaine_echeance",
    "id_operation;nature;statut;date_realisation;libelle;montant_total_ct;id_compte_source;id_compte_destination;id_tiers;id_recurrence;date_prevue_origine",
    "id_ligne_operation;id_operation;id_categorie;id_vehicule;montant_ct",
    "entite;prochain_id"
};

static const size_t NB_CHAMPS[NB_FICHIERS] = {8, 4, 2, 2, 5, 8, 5, 8, 11, 5, 2};

const char *persistance_nom_fichier(int indice)
{
    return (indice >= 0 && indice < NB_FICHIERS) ? NOMS_FICHIERS[indice] : "";
}

char persistance_lettre_emplacement(int emplacement)
{
    return emplacement == 2 ? 'B' : 'A';
}

static const char *prefixe_emplacement(int emplacement)
{
    return emplacement == 2 ? "B_" : "A_";
}

static bool construire_chemin(char *sortie, size_t taille, const char *dossier,
                              const char *prefixe, const char *nom)
{
    int n = snprintf(sortie, taille, "%s/%s%s", dossier, prefixe, nom);
    return n >= 0 && (size_t)n < taille;
}

/* ------------------------------------------------------------------ */
/* Lecture d'une ligne d'entité                                        */
/* ------------------------------------------------------------------ */

static void analyser_entite(int fichier, char **c, Donnees *d, AnalyseCsv *a, bool *memoire)
{
    bool ajoute = true;

    switch (fichier) {
    case ENT_COMPTE: {
        Compte x;
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], "id_compte", false);
        csv_texte(a, c[1], "nom", x.nom, NOM_MAX, true);
        if (!type_compte_lire(c[2], &x.type)) {
            csv_echec(a, "type_compte", "BANQUE ou ESPECES attendu");
        }
        x.solde_initial_ct = csv_centimes(a, c[3], "solde_initial_ct", 0, MONTANT_MAX_CT, false, NULL);
        x.date_solde_initial = csv_date(a, c[4], "date_solde_initial", false);
        x.seuil_minimal_ct = csv_centimes(a, c[5], "seuil_minimal_ct", 0, MONTANT_MAX_CT, true,
                                          &x.a_seuil);
        csv_texte(a, c[6], "nom_banque", x.nom_banque, NOM_BANQUE_MAX, false);
        csv_texte(a, c[7], "reference_compte", x.reference_compte, REFERENCE_MAX, false);
        if (a->ok) {
            ajoute = tableau_ajouter(&d->comptes, &x);
        }
        break;
    }
    case ENT_CATEGORIE: {
        Categorie x;
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], "id_categorie", false);
        csv_texte(a, c[1], "nom", x.nom, NOM_MAX, true);
        if (!type_categorie_lire(c[2], &x.type)) {
            csv_echec(a, "type_categorie", "DEPENSE ou REVENU attendu");
        }
        x.id_parent = csv_id(a, c[3], "id_parent", true);
        if (a->ok) {
            ajoute = tableau_ajouter(&d->categories, &x);
        }
        break;
    }
    case ENT_TIERS:
    case ENT_VEHICULE: {
        Tiers x;   /* Tiers et Vehicule ont la même structure (id, nom). */
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], fichier == ENT_TIERS ? "id_tiers" : "id_vehicule", false);
        csv_texte(a, c[1], "nom", x.nom, NOM_MAX, true);
        if (a->ok) {
            if (fichier == ENT_TIERS) {
                ajoute = tableau_ajouter(&d->tiers, &x);
            } else {
                Vehicule v;
                v.id = x.id;
                memcpy(v.nom, x.nom, sizeof v.nom);
                ajoute = tableau_ajouter(&d->vehicules, &v);
            }
        }
        break;
    }
    case ENT_BUDGET: {
        BudgetMensuel x;
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], "id_budget", false);
        x.annee = (int)csv_entier(a, c[1], "annee", ANNEE_MIN, ANNEE_MAX, false);
        x.mois = (int)csv_entier(a, c[2], "mois", 1, 12, false);
        x.plafond_ct = csv_centimes(a, c[3], "plafond_ct", 0, MONTANT_MAX_CT, false, NULL);
        x.id_categorie = csv_id(a, c[4], "id_categorie", true);
        if (a->ok) {
            ajoute = tableau_ajouter(&d->budgets, &x);
        }
        break;
    }
    case ENT_MODELE: {
        ModeleOperation x;
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], "id_modele", false);
        csv_texte(a, c[1], "nom", x.nom, NOM_MAX, true);
        if (!nature_lire(c[2], &x.nature)) {
            csv_echec(a, "nature", "DEPENSE, REVENU ou VIREMENT attendu");
        }
        csv_texte(a, c[3], "libelle", x.libelle, LIBELLE_MAX, true);
        x.montant_total_ct = csv_centimes(a, c[4], "montant_total_ct", 1, MONTANT_MAX_CT, false, NULL);
        x.id_compte_source = csv_id(a, c[5], "id_compte_source", true);
        x.id_compte_destination = csv_id(a, c[6], "id_compte_destination", true);
        x.id_tiers = csv_id(a, c[7], "id_tiers", true);
        if (a->ok) {
            ajoute = tableau_ajouter(&d->modeles, &x);
        }
        break;
    }
    case ENT_LIGNE_MODELE: {
        LigneModele x;
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], "id_ligne_modele", false);
        x.id_modele = csv_id(a, c[1], "id_modele", false);
        x.id_categorie = csv_id(a, c[2], "id_categorie", false);
        x.id_vehicule = csv_id(a, c[3], "id_vehicule", true);
        x.montant_ct = csv_centimes(a, c[4], "montant_ct", 1, MONTANT_MAX_CT, false, NULL);
        if (a->ok) {
            ajoute = tableau_ajouter(&d->lignes_modeles, &x);
        }
        break;
    }
    case ENT_RECURRENCE: {
        Recurrence x;
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], "id_recurrence", false);
        x.id_modele = csv_id(a, c[1], "id_modele", false);
        if (!frequence_lire(c[2], &x.frequence)) {
            csv_echec(a, "frequence", "HEBDOMADAIRE ou MENSUELLE attendu");
        }
        x.date_debut = csv_date(a, c[3], "date_debut", false);
        x.date_fin = csv_date(a, c[4], "date_fin", true);
        x.jour_mensuel_souhaite = (int)csv_entier(a, c[5], "jour_mensuel_souhaite", 1, 31, true);
        x.active = csv_booleen(a, c[6], "active");
        x.date_prochaine_echeance = csv_date(a, c[7], "date_prochaine_echeance", true);
        if (a->ok) {
            ajoute = tableau_ajouter(&d->recurrences, &x);
        }
        break;
    }
    case ENT_OPERATION: {
        Operation x;
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], "id_operation", false);
        if (!nature_lire(c[1], &x.nature)) {
            csv_echec(a, "nature", "DEPENSE, REVENU ou VIREMENT attendu");
        }
        if (!statut_lire(c[2], &x.statut)) {
            csv_echec(a, "statut", "A_CONFIRMER, CONFIRMEE ou ANNULEE attendu");
        }
        x.date_realisation = csv_date(a, c[3], "date_realisation", true);
        csv_texte(a, c[4], "libelle", x.libelle, LIBELLE_MAX, true);
        x.montant_total_ct = csv_centimes(a, c[5], "montant_total_ct", 1, MONTANT_MAX_CT, false, NULL);
        x.id_compte_source = csv_id(a, c[6], "id_compte_source", true);
        x.id_compte_destination = csv_id(a, c[7], "id_compte_destination", true);
        x.id_tiers = csv_id(a, c[8], "id_tiers", true);
        x.id_recurrence = csv_id(a, c[9], "id_recurrence", true);
        x.date_prevue_origine = csv_date(a, c[10], "date_prevue_origine", true);
        if (a->ok) {
            ajoute = tableau_ajouter(&d->operations, &x);
        }
        break;
    }
    case ENT_LIGNE_OPERATION: {
        LigneOperation x;
        memset(&x, 0, sizeof x);
        x.id = csv_id(a, c[0], "id_ligne_operation", false);
        x.id_operation = csv_id(a, c[1], "id_operation", false);
        x.id_categorie = csv_id(a, c[2], "id_categorie", false);
        x.id_vehicule = csv_id(a, c[3], "id_vehicule", true);
        x.montant_ct = csv_centimes(a, c[4], "montant_ct", 1, MONTANT_MAX_CT, false, NULL);
        if (a->ok) {
            ajoute = tableau_ajouter(&d->lignes_operations, &x);
        }
        break;
    }
    default:
        break;
    }
    if (!ajoute) {
        *memoire = false;
    }
}

static void analyser_metadata(char **c, Donnees *d, bool vus[NB_ENTITES], AnalyseCsv *a)
{
    int e;

    for (e = 0; e < NB_ENTITES; e++) {
        if (strcmp(c[0], entite_code((Entite)e)) == 0) {
            if (vus[e]) {
                csv_echec(a, "entite", "entité en double");
                return;
            }
            vus[e] = true;
            d->prochain_id[e] = (Id)csv_entier(a, c[1], "prochain_id", 1, (long)ID_MAX + 1, false);
            return;
        }
    }
    csv_echec(a, "entite", "entité inconnue");
}

/* Lit un fichier du jeu ; nb_lus reçoit le nombre d'enregistrements. */
static bool lire_fichier(const char *dossier, const char *prefixe, int fichier, Donnees *d,
                         long *nb_lus, RapportErreurs *r)
{
    char chemin[CHEMIN_MAX + 64];
    char ligne[CSV_LIGNE_MAX];
    char *champs[CSV_CHAMPS_MAX];
    bool vus[NB_ENTITES] = {false};
    unsigned long numero = 1;
    unsigned long ligne_vide = 0;
    size_t erreurs = 0;
    bool memoire = true;
    EtatLecture etat;
    FILE *f;
    int e;

    *nb_lus = 0;
    if (!construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOMS_FICHIERS[fichier])) {
        rapport_ajouter(r, "Chemin trop long pour %s", NOMS_FICHIERS[fichier]);
        return false;
    }
    f = fopen(chemin, "rb");
    if (f == NULL) {
        rapport_ajouter(r, "%s : fichier absent ou illisible", chemin);
        return false;
    }
    etat = csv_lire_ligne(f, ligne, sizeof ligne, true);
    if (etat != LECTURE_OK || strcmp(ligne, ENTETES[fichier]) != 0) {
        rapport_ajouter(r, "%s, ligne 1 : en-tête absent ou incorrect", chemin);
        fclose(f);
        return false;
    }
    while ((etat = csv_lire_ligne(f, ligne, sizeof ligne, false)) != LECTURE_FIN) {
        AnalyseCsv a;
        size_t n;

        numero++;
        if (etat == LECTURE_ERREUR) {
            rapport_ajouter(r, "%s, ligne %lu : erreur de lecture", chemin, numero);
            erreurs++;
            break;
        }
        if (etat == LECTURE_TROP_LONGUE) {
            rapport_ajouter(r, "%s, ligne %lu : ligne trop longue", chemin, numero);
            erreurs++;
            continue;
        }
        if (ligne[0] == '\0') {
            if (ligne_vide == 0) {
                ligne_vide = numero;
            }
            continue;
        }
        if (ligne_vide != 0) {
            rapport_ajouter(r, "%s, ligne %lu : ligne vide au milieu du fichier", chemin, ligne_vide);
            erreurs++;
            ligne_vide = 0;
        }
        n = csv_decouper(ligne, champs, CSV_CHAMPS_MAX);
        if (n != NB_CHAMPS[fichier]) {
            rapport_ajouter(r, "%s, ligne %lu : %lu champ(s) au lieu de %lu", chemin, numero,
                            (unsigned long)n, (unsigned long)NB_CHAMPS[fichier]);
            erreurs++;
        } else {
            csv_analyse_init(&a);
            if (fichier == FICHIER_METADATA) {
                analyser_metadata(champs, d, vus, &a);
            } else {
                analyser_entite(fichier, champs, d, &a, &memoire);
            }
            if (!memoire) {
                rapport_ajouter(r, "Mémoire insuffisante pendant la lecture de %s", chemin);
                fclose(f);
                return false;
            }
            if (!a.ok) {
                rapport_ajouter(r, "%s, ligne %lu : %s", chemin, numero, a.message);
                erreurs++;
            } else {
                (*nb_lus)++;
            }
        }
        if (erreurs >= ERREURS_PAR_FICHIER_MAX) {
            rapport_ajouter(r, "%s : trop d'erreurs, lecture interrompue", chemin);
            break;
        }
    }
    fclose(f);
    if (fichier == FICHIER_METADATA) {
        for (e = 0; e < NB_ENTITES; e++) {
            if (!vus[e]) {
                rapport_ajouter(r, "%s : compteur %s absent", chemin, entite_code((Entite)e));
                erreurs++;
            }
        }
    }
    return erreurs == 0;
}

/* ------------------------------------------------------------------ */
/* Manifeste                                                           */
/* ------------------------------------------------------------------ */

static bool manifeste_existe(const char *dossier, const char *prefixe)
{
    char chemin[CHEMIN_MAX + 64];
    FILE *f;

    if (!construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOM_MANIFESTE)) {
        return false;
    }
    f = fopen(chemin, "rb");
    if (f == NULL) {
        return false;
    }
    fclose(f);
    return true;
}

typedef struct {
    long generation;
    long nb[NB_FICHIERS];
    uint64_t somme[NB_FICHIERS];
} Manifeste;

/* 16 chiffres hexadécimaux minuscules exactement. */
static bool lire_hex64(const char *s, uint64_t *v)
{
    uint64_t x = 0;
    int i;

    for (i = 0; i < 16; i++) {
        char c = s[i];
        unsigned chiffre;
        if (c >= '0' && c <= '9') {
            chiffre = (unsigned)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            chiffre = (unsigned)(c - 'a' + 10);
        } else {
            return false;
        }
        x = (x << 4) | chiffre;
    }
    if (s[16] != '\0') {
        return false;
    }
    *v = x;
    return true;
}

static bool manifeste_lire(const char *dossier, const char *prefixe, Manifeste *m,
                           RapportErreurs *r)
{
    char chemin[CHEMIN_MAX + 64];
    char ligne[CSV_LIGNE_MAX];
    char *champs[CSV_CHAMPS_MAX];
    bool vus[NB_FICHIERS] = {false};
    bool format_vu = false;
    bool ok = true;
    unsigned long numero = 1;
    EtatLecture etat;
    FILE *f;
    int i;

    memset(m, 0, sizeof *m);
    if (!construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOM_MANIFESTE)) {
        rapport_ajouter(r, "Chemin trop long pour le manifeste");
        return false;
    }
    f = fopen(chemin, "rb");
    if (f == NULL) {
        rapport_ajouter(r, "%s : manifeste absent", chemin);
        return false;
    }
    etat = csv_lire_ligne(f, ligne, sizeof ligne, true);
    if (etat != LECTURE_OK || strcmp(ligne, ENTETE_MANIFESTE) != 0) {
        rapport_ajouter(r, "%s, ligne 1 : en-tête absent ou incorrect", chemin);
        fclose(f);
        return false;
    }
    while ((etat = csv_lire_ligne(f, ligne, sizeof ligne, false)) == LECTURE_OK) {
        AnalyseCsv a;
        bool connu = false;

        numero++;
        if (ligne[0] == '\0') {
            continue;
        }
        if (csv_decouper(ligne, champs, CSV_CHAMPS_MAX) != 3) {
            rapport_ajouter(r, "%s, ligne %lu : trois champs attendus", chemin, numero);
            ok = false;
            continue;
        }
        csv_analyse_init(&a);
        if (strcmp(champs[0], "FORMAT") == 0) {
            connu = true;
            format_vu = csv_entier(&a, champs[1], "valeur", FORMAT_VERSION, FORMAT_VERSION, false)
                        == FORMAT_VERSION;
        } else if (strcmp(champs[0], "GENERATION") == 0) {
            connu = true;
            m->generation = csv_entier(&a, champs[1], "valeur", 1, GENERATION_MAX, false);
        } else {
            for (i = 0; i < NB_FICHIERS; i++) {
                if (strcmp(champs[0], NOMS_FICHIERS[i]) == 0) {
                    connu = true;
                    if (vus[i]) {
                        csv_echec(&a, "element", "fichier cité deux fois");
                    }
                    vus[i] = true;
                    m->nb[i] = csv_entier(&a, champs[1], "valeur", 0, 2000000000L, false);
                    if (!lire_hex64(champs[2], &m->somme[i])) {
                        csv_echec(&a, "fnv1a64", "somme de contrôle invalide (16 chiffres hexadécimaux)");
                    }
                }
            }
        }
        if (!connu) {
            csv_echec(&a, "element", "élément inconnu");
        }
        if (!a.ok) {
            rapport_ajouter(r, "%s, ligne %lu : %s", chemin, numero, a.message);
            ok = false;
        }
    }
    if (etat != LECTURE_FIN) {
        rapport_ajouter(r, "%s : erreur de lecture", chemin);
        ok = false;
    }
    fclose(f);
    if (!format_vu) {
        rapport_ajouter(r, "%s : version de format absente ou non prise en charge", chemin);
        ok = false;
    }
    if (m->generation < 1) {
        rapport_ajouter(r, "%s : génération absente", chemin);
        ok = false;
    }
    for (i = 0; i < NB_FICHIERS; i++) {
        if (!vus[i]) {
            rapport_ajouter(r, "%s : %s non cité", chemin, NOMS_FICHIERS[i]);
            ok = false;
        }
    }
    return ok;
}

/* Lit les onze fichiers et compare chaque nombre d'enregistrements à attendus. */
static bool lire_fichiers(const char *dossier, const char *prefixe, Donnees *d,
                          const long attendus[NB_FICHIERS], RapportErreurs *r)
{
    bool ok = true;
    int i;

    for (i = 0; i < NB_FICHIERS; i++) {
        long lus = 0;
        if (!lire_fichier(dossier, prefixe, i, d, &lus, r)) {
            ok = false;
        } else if (i != FICHIER_METADATA && lus != attendus[i]) {
            rapport_ajouter(r, "%s%s : %ld enregistrement(s) attendu(s), %ld lu(s)", prefixe,
                            NOMS_FICHIERS[i], attendus[i], lus);
            ok = false;
        }
    }
    return ok;
}

bool jeu_lire(const char *dossier, const char *prefixe, Donnees *d, long *generation,
              RapportErreurs *r)
{
    Manifeste m;
    char chemin[CHEMIN_MAX + 64];
    bool ok = true;
    int i;

    *generation = 0;
    if (!manifeste_lire(dossier, prefixe, &m, r)) {
        return false;
    }
    *generation = m.generation;
    /* 1. Sommes de contrôle calculées sur les octets réels de chaque fichier. */
    for (i = 0; i < NB_FICHIERS; i++) {
        uint64_t somme;
        if (!construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOMS_FICHIERS[i])) {
            rapport_ajouter(r, "Chemin trop long pour %s", NOMS_FICHIERS[i]);
            return false;
        }
        if (!fichier_fnv1a64(chemin, &somme)) {
            rapport_ajouter(r, "%s : fichier absent ou illisible", chemin);
            ok = false;
        } else if (somme != m.somme[i]) {
            rapport_ajouter(r, "%s : somme de contrôle incorrecte (fichier modifié, incomplet ou abîmé)",
                            chemin);
            ok = false;
        }
    }
    if (!ok) {
        return false;
    }
    /* 2. Structure et nombre d'enregistrements. */
    return lire_fichiers(dossier, prefixe, d, m.nb, r);
}

/* ------------------------------------------------------------------ */
/* Écriture                                                            */
/* ------------------------------------------------------------------ */

static const char *texte_id(Id id, char tampon[16])
{
    if (id == ID_AUCUN) {
        tampon[0] = '\0';
    } else if (snprintf(tampon, 16, "%d", id) >= 16) {
        tampon[0] = '\0';
    }
    return tampon;
}

static const char *texte_date(Date d, char tampon[DATE_TEXTE])
{
    date_formater(d, tampon);
    return tampon;
}

static bool ecrire_lignes(FILE *f, int fichier, const Donnees *d, long *nb)
{
    char a[16];
    char b[16];
    char c[16];
    char e[16];
    char d1[DATE_TEXTE];
    char d2[DATE_TEXTE];
    char d3[DATE_TEXTE];
    bool ok = true;
    size_t i;
    const Tableau *t = (fichier < NB_ENTITES) ? donnees_table_const(d, (Entite)fichier) : NULL;

    *nb = 0;
    if (fprintf(f, "%s\n", ENTETES[fichier]) < 0) {
        return false;
    }
    if (fichier == FICHIER_METADATA) {
        int k;
        for (k = 0; k < NB_ENTITES && ok; k++) {
            ok = fprintf(f, "%s;%d\n", entite_code((Entite)k), d->prochain_id[k]) >= 0;
        }
        return ok;
    }
    for (i = 0; i < t->taille && ok; i++) {
        int n = 0;
        switch (fichier) {
        case ENT_COMPTE: {
            const Compte *x = ELEMENT(t, Compte, i);
            char seuil[24] = "";
            if (x->a_seuil && snprintf(seuil, sizeof seuil, "%lld", x->seuil_minimal_ct) >= 24) {
                seuil[0] = '\0';
            }
            n = fprintf(f, "%d;%s;%s;%lld;%s;%s;%s;%s\n", x->id, x->nom, type_compte_code(x->type),
                        x->solde_initial_ct, texte_date(x->date_solde_initial, d1), seuil,
                        x->nom_banque, x->reference_compte);
            break;
        }
        case ENT_CATEGORIE: {
            const Categorie *x = ELEMENT(t, Categorie, i);
            n = fprintf(f, "%d;%s;%s;%s\n", x->id, x->nom, type_categorie_code(x->type),
                        texte_id(x->id_parent, a));
            break;
        }
        case ENT_TIERS: {
            const Tiers *x = ELEMENT(t, Tiers, i);
            n = fprintf(f, "%d;%s\n", x->id, x->nom);
            break;
        }
        case ENT_VEHICULE: {
            const Vehicule *x = ELEMENT(t, Vehicule, i);
            n = fprintf(f, "%d;%s\n", x->id, x->nom);
            break;
        }
        case ENT_BUDGET: {
            const BudgetMensuel *x = ELEMENT(t, BudgetMensuel, i);
            n = fprintf(f, "%d;%d;%d;%lld;%s\n", x->id, x->annee, x->mois, x->plafond_ct,
                        texte_id(x->id_categorie, a));
            break;
        }
        case ENT_MODELE: {
            const ModeleOperation *x = ELEMENT(t, ModeleOperation, i);
            n = fprintf(f, "%d;%s;%s;%s;%lld;%s;%s;%s\n", x->id, x->nom, nature_code(x->nature),
                        x->libelle, x->montant_total_ct, texte_id(x->id_compte_source, a),
                        texte_id(x->id_compte_destination, b), texte_id(x->id_tiers, c));
            break;
        }
        case ENT_LIGNE_MODELE: {
            const LigneModele *x = ELEMENT(t, LigneModele, i);
            n = fprintf(f, "%d;%d;%d;%s;%lld\n", x->id, x->id_modele, x->id_categorie,
                        texte_id(x->id_vehicule, a), x->montant_ct);
            break;
        }
        case ENT_RECURRENCE: {
            const Recurrence *x = ELEMENT(t, Recurrence, i);
            char jour[8] = "";
            if (x->jour_mensuel_souhaite > 0
                && snprintf(jour, sizeof jour, "%d", x->jour_mensuel_souhaite) >= 8) {
                jour[0] = '\0';
            }
            n = fprintf(f, "%d;%d;%s;%s;%s;%s;%d;%s\n", x->id, x->id_modele,
                        frequence_code(x->frequence), texte_date(x->date_debut, d1),
                        texte_date(x->date_fin, d2), jour, x->active ? 1 : 0,
                        texte_date(x->date_prochaine_echeance, d3));
            break;
        }
        case ENT_OPERATION: {
            const Operation *x = ELEMENT(t, Operation, i);
            n = fprintf(f, "%d;%s;%s;%s;%s;%lld;%s;%s;%s;%s;%s\n", x->id, nature_code(x->nature),
                        statut_code(x->statut), texte_date(x->date_realisation, d1), x->libelle,
                        x->montant_total_ct, texte_id(x->id_compte_source, a),
                        texte_id(x->id_compte_destination, b), texte_id(x->id_tiers, c),
                        texte_id(x->id_recurrence, e), texte_date(x->date_prevue_origine, d2));
            break;
        }
        case ENT_LIGNE_OPERATION: {
            const LigneOperation *x = ELEMENT(t, LigneOperation, i);
            n = fprintf(f, "%d;%d;%d;%s;%lld\n", x->id, x->id_operation, x->id_categorie,
                        texte_id(x->id_vehicule, a), x->montant_ct);
            break;
        }
        default:
            break;
        }
        ok = n >= 0;
        (*nb)++;
    }
    return ok;
}

static bool fermer(FILE *f, bool ok)
{
    if (ferror(f)) {
        ok = false;
    }
    if (fclose(f) != 0) {
        ok = false;
    }
    return ok;
}

bool jeu_ecrire(const char *dossier, const char *prefixe, const Donnees *d, long generation,
                RapportErreurs *r)
{
    char chemin[CHEMIN_MAX + 64];
    char temporaire[CHEMIN_MAX + 64];
    long comptes[NB_FICHIERS] = {0};
    uint64_t sommes[NB_FICHIERS] = {0};
    FILE *f;
    bool ok;
    int i;

    /* 1. Invalider l'emplacement : le manifeste disparaît en premier. */
    if (!construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOM_MANIFESTE)) {
        rapport_ajouter(r, "Chemin trop long : %s", dossier);
        return false;
    }
    remove(chemin);
    if (manifeste_existe(dossier, prefixe)) {
        rapport_ajouter(r, "Impossible de supprimer l'ancien manifeste %s", chemin);
        return false;
    }
    /* 2. Écrire les onze fichiers. */
    for (i = 0; i < NB_FICHIERS; i++) {
        if (!construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOMS_FICHIERS[i])) {
            rapport_ajouter(r, "Chemin trop long : %s", dossier);
            return false;
        }
        f = fopen(chemin, "wb");
        if (f == NULL) {
            rapport_ajouter(r, "Impossible d'écrire %s (le dossier « %s » existe-t-il ?)", chemin,
                            dossier);
            return false;
        }
        ok = ecrire_lignes(f, i, d, &comptes[i]);
        if (!fermer(f, ok)) {
            rapport_ajouter(r, "Erreur d'écriture dans %s (disque plein ?)", chemin);
            return false;
        }
    }
    /* 3. Relire les fichiers écrits : octets (somme de contrôle), structure, nombres, règles. */
    for (i = 0; i < NB_FICHIERS; i++) {
        if (!construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOMS_FICHIERS[i])
            || !fichier_fnv1a64(chemin, &sommes[i])) {
            rapport_ajouter(r, "Relecture impossible de %s", chemin);
            return false;
        }
    }
    {
        Donnees relue;
        bool relecture;

        donnees_init(&relue);
        relecture = lire_fichiers(dossier, prefixe, &relue, comptes, r)
                    && regles_valider(&relue, r);
        donnees_liberer(&relue);
        if (!relecture) {
            rapport_ajouter(r, "Les fichiers écrits ne sont pas conformes : enregistrement non validé.");
            return false;
        }
    }
    /* 4. Manifeste écrit sous un nom temporaire puis renommé : il n'apparaît que complet. */
    if (!construire_chemin(temporaire, sizeof temporaire, dossier, prefixe, NOM_MANIFESTE_TEMP)
        || !construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOM_MANIFESTE)) {
        return false;
    }
    remove(temporaire);
    f = fopen(temporaire, "wb");
    if (f == NULL) {
        rapport_ajouter(r, "Impossible d'écrire %s", temporaire);
        return false;
    }
    ok = fprintf(f, "%s\nFORMAT;%d;\nGENERATION;%ld;\n", ENTETE_MANIFESTE, FORMAT_VERSION,
                 generation) >= 0;
    for (i = 0; i < NB_FICHIERS && ok; i++) {
        ok = fprintf(f, "%s;%ld;%016" PRIx64 "\n", NOMS_FICHIERS[i], comptes[i], sommes[i]) >= 0;
    }
    if (!fermer(f, ok)) {
        rapport_ajouter(r, "Erreur d'écriture dans %s", temporaire);
        remove(temporaire);
        return false;
    }
    if (rename(temporaire, chemin) != 0) {
        rapport_ajouter(r, "Impossible de renommer %s en %s", temporaire, chemin);
        remove(temporaire);
        return false;
    }
    return true;
}

void jeu_supprimer(const char *dossier, const char *prefixe)
{
    char chemin[CHEMIN_MAX + 64];
    int i;

    if (construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOM_MANIFESTE)) {
        remove(chemin);
    }
    for (i = 0; i < NB_FICHIERS; i++) {
        if (construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOMS_FICHIERS[i])) {
            remove(chemin);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Emplacements A/B                                                    */
/* ------------------------------------------------------------------ */

static bool fichiers_presents(const char *dossier, const char *prefixe)
{
    char chemin[CHEMIN_MAX + 64];
    int i;

    for (i = 0; i < NB_FICHIERS; i++) {
        FILE *f;
        if (!construire_chemin(chemin, sizeof chemin, dossier, prefixe, NOMS_FICHIERS[i])) {
            continue;
        }
        f = fopen(chemin, "rb");
        if (f != NULL) {
            fclose(f);
            return true;
        }
    }
    return false;
}

EtatChargement persistance_charger(Application *app, RapportErreurs *infos)
{
    int candidats[2];
    long generations[2];
    int nb = 0;
    int k;
    int s;
    bool manifeste_vu = false;
    bool manifeste_illisible = false;
    bool partiels = false;

    for (s = 1; s <= 2; s++) {
        const char *prefixe = prefixe_emplacement(s);
        if (manifeste_existe(app->dossier_donnees, prefixe)) {
            RapportErreurs ignore;
            Manifeste m;
            long g = 0;

            manifeste_vu = true;
            rapport_init(&ignore);
            if (manifeste_lire(app->dossier_donnees, prefixe, &m, &ignore)) {
                g = m.generation;
            } else {
                manifeste_illisible = true;
                rapport_ajouter(infos, "Emplacement %c : manifeste illisible ou incomplet, ignoré.",
                                persistance_lettre_emplacement(s));
            }   /* sinon g = 0 : manifeste illisible, candidat de dernier recours */
            rapport_liberer(&ignore);
            candidats[nb] = s;
            generations[nb] = g;
            nb++;
            if (g > app->generation_max) {
                app->generation_max = g;
            }
        } else if (fichiers_presents(app->dossier_donnees, prefixe)) {
            partiels = true;
        }
    }
    if (nb == 2 && generations[1] > generations[0]) {
        int ts = candidats[0];
        long tg = generations[0];
        candidats[0] = candidats[1];
        generations[0] = generations[1];
        candidats[1] = ts;
        generations[1] = tg;
    }
    for (k = 0; k < nb; k++) {
        Donnees d;
        RapportErreurs erreurs;
        RapportErreurs avertissements;
        long generation = 0;
        bool valide;

        donnees_init(&d);
        rapport_init(&erreurs);
        rapport_init(&avertissements);
        valide = jeu_lire(app->dossier_donnees, prefixe_emplacement(candidats[k]), &d, &generation,
                          &erreurs);
        if (valide) {
            donnees_relever_compteurs(&d, &avertissements);
            valide = regles_valider(&d, &erreurs);
        }
        if (valide) {
            donnees_liberer(&app->donnees);
            app->donnees = d;
            app->emplacement_charge = candidats[k];
            app->generation_chargee = generation;
            app->modifie = false;
            app->nb_modifications = 0;
            if (rapport_nombre(&avertissements) > 0) {
                rapport_fusionner(infos, &avertissements, "");
                app->modifie = true;   /* compteurs relevés : à enregistrer */
                app->nb_modifications = 1;
            }
            rapport_liberer(&erreurs);
            rapport_liberer(&avertissements);
            if (partiels) {
                rapport_ajouter(infos, "Des fichiers sans manifeste ont été ignorés.");
            }
            /* Repli si un emplacement plus récent ou au manifeste illisible a été écarté. */
            return (k == 0 && !manifeste_illisible) ? CHARGEMENT_OK : CHARGEMENT_REPLI;
        }
        rapport_ajouter(infos, "Emplacement %c (génération %ld) rejeté :",
                        persistance_lettre_emplacement(candidats[k]), generations[k]);
        rapport_fusionner(infos, &erreurs, "    ");
        donnees_liberer(&d);
        rapport_liberer(&erreurs);
        rapport_liberer(&avertissements);
    }
    if (!manifeste_vu) {
        if (partiels) {
            rapport_ajouter(infos,
                            "Des fichiers d'un enregistrement interrompu (sans manifeste) ont été ignorés.");
        }
        return CHARGEMENT_VIDE;
    }
    return CHARGEMENT_ECHEC;
}

Resultat persistance_enregistrer(Application *app, RapportErreurs *r)
{
    int cible = (app->emplacement_charge == 1) ? 2 : 1;
    long generation;

    if (app->generation_max >= 2000000000L) {
        rapport_ajouter(r, "Numéro de génération épuisé.");
        return RES_ERR_FICHIER;
    }
    if (!regles_valider(&app->donnees, r)) {
        rapport_ajouter(r, "Données incohérentes : enregistrement refusé.");
        return RES_ERR_VALIDATION;
    }
    generation = app->generation_max + 1;
    if (!jeu_ecrire(app->dossier_donnees, prefixe_emplacement(cible), &app->donnees, generation, r)) {
        return RES_ERR_FICHIER;
    }
    app->emplacement_charge = cible;
    app->generation_chargee = generation;
    app->generation_max = generation;
    app->modifie = false;
    app->nb_modifications = 0;
    return RES_OK;
}

/* ------------------------------------------------------------------ */
/* Verrou d'exécution                                                  */
/* ------------------------------------------------------------------ */

EtatVerrou persistance_verrouiller(const char *dossier, char *chemin, size_t taille)
{
    FILE *f;
    bool ok;

    if (!construire_chemin(chemin, taille, dossier, "", NOM_VERROU)) {
        return VERROU_ERREUR;
    }
    f = fopen(chemin, "wx");
    if (f == NULL) {
        /* Échec : distinguer un verrou existant d'une autre erreur (dossier absent, droits). */
        f = fopen(chemin, "rb");
        if (f != NULL) {
            fclose(f);
            return VERROU_PRIS;
        }
        return VERROU_ERREUR;
    }
    ok = fprintf(f, "Verrou BudgetLens : une instance utilise ce dossier de données.\n"
                    "Ne supprimez ce fichier qu'après avoir vérifié qu'aucune instance n'est ouverte.\n")
         >= 0;
    if (!fermer(f, ok)) {
        remove(chemin);
        return VERROU_ERREUR;
    }
    return VERROU_OK;
}

void persistance_deverrouiller(const char *chemin)
{
    remove(chemin);
}
