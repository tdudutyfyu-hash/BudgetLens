/*
 * BudgetLens — sauvegardes.c
 */
#include "sauvegardes.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "csv.h"
#include "persistance.h"
#include "recurrences.h"
#include "regles.h"
#include "texte.h"

#define NOM_MANIFESTE_FICHIER "manifest.csv"

static bool chemin_jeu(char *sortie, size_t taille, const char *dossier, const char *prefixe,
                       const char *nom)
{
    int n = snprintf(sortie, taille, "%s/%s%s", dossier, prefixe, nom);
    return n >= 0 && (size_t)n < taille;
}

static bool fichier_existe(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");

    if (f == NULL) {
        return false;
    }
    fclose(f);
    return true;
}

static bool manifeste_present(const char *dossier, const char *prefixe)
{
    char chemin[CHEMIN_MAX + 64];

    return chemin_jeu(chemin, sizeof chemin, dossier, prefixe, NOM_MANIFESTE_FICHIER)
           && fichier_existe(chemin);
}

bool export_manifeste_present(const char *dossier)
{
    return manifeste_present(dossier, "");
}

static long generation_a_ecrire(const Application *app)
{
    return app->generation_chargee > 0 ? app->generation_chargee : 1;
}

Resultat fich_exporter(const Application *app, const char *dossier, RapportErreurs *r)
{
    RapportErreurs erreurs;
    bool valide;

    if (dossier[0] == '\0' || strlen(dossier) > CHEMIN_MAX - 40) {
        rapport_ajouter(r, "Dossier de destination invalide ou trop long.");
        return RES_ERR_FICHIER;
    }
    rapport_init(&erreurs);
    valide = regles_valider(&app->donnees, &erreurs);
    if (!valide) {
        rapport_ajouter(r, "Les données en mémoire sont incohérentes : export refusé.");
        rapport_fusionner(r, &erreurs, "");
        rapport_liberer(&erreurs);
        return RES_ERR_VALIDATION;
    }
    rapport_liberer(&erreurs);
    if (!jeu_ecrire(dossier, "", &app->donnees, generation_a_ecrire(app), r)) {
        return RES_ERR_FICHIER;
    }
    return RES_OK;
}

/* Prefixe de base AAAAMMJJ-HHMMSS et forme lisible, d'après l'heure système réelle ou forcée. */
static bool horodater(const char *force, char base[16], char lisible[24])
{
    if (force != NULL) {
        size_t i;
        if (strlen(force) != 15 || force[8] != '-') {
            return false;
        }
        for (i = 0; i < 15; i++) {
            if (i != 8 && (force[i] < '0' || force[i] > '9')) {
                return false;
            }
        }
        memcpy(base, force, 16);
        snprintf(lisible, 24, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s", force, force + 4, force + 6,
                 force + 9, force + 11, force + 13);
        return true;
    } else {
        time_t t = time(NULL);
        struct tm *tm_local = localtime(&t);   /* tampon statique : copié aussitôt */
        struct tm copie;

        if (t == (time_t)-1 || tm_local == NULL) {
            return false;
        }
        unsigned an;
        unsigned mois;
        unsigned jour;
        unsigned h;
        unsigned mn;
        unsigned s;

        copie = *tm_local;
        an = (unsigned)(copie.tm_year + 1900) % 10000u;
        mois = (unsigned)(copie.tm_mon + 1) % 100u;
        jour = (unsigned)copie.tm_mday % 100u;
        h = (unsigned)copie.tm_hour % 100u;
        mn = (unsigned)copie.tm_min % 100u;
        s = (unsigned)copie.tm_sec % 100u;
        snprintf(base, 16, "%04u%02u%02u-%02u%02u%02u", an, mois, jour, h, mn, s);
        snprintf(lisible, 24, "%04u-%02u-%02u %02u:%02u:%02u", an, mois, jour, h, mn, s);
        return true;
    }
}

static bool ajouter_a_l_index(const char *dossier, const char *prefixe, const char *lisible,
                              long generation, long nb_operations)
{
    char chemin[CHEMIN_MAX + 64];
    bool existe;
    FILE *f;
    bool ok;

    if (!chemin_jeu(chemin, sizeof chemin, dossier, "", NOM_INDEX_SAUVEGARDES)) {
        return false;
    }
    existe = fichier_existe(chemin);
    f = fopen(chemin, existe ? "ab" : "wb");
    if (f == NULL) {
        return false;
    }
    ok = true;
    if (!existe) {
        ok = fprintf(f, "%s\n", ENTETE_INDEX) >= 0;
    }
    ok = ok && fprintf(f, "%s;%s;%ld;%ld\n", prefixe, lisible, generation, nb_operations) >= 0;
    if (fclose(f) != 0) {
        ok = false;
    }
    return ok;
}

Resultat fich_sauvegarder(const Application *app, const char *horodatage_force, char *prefixe_cree,
                          size_t taille, RapportErreurs *r)
{
    char base[16];
    char lisible[24];
    char prefixe[40];
    const char *dossier = app->dossier_sauvegardes;
    int essai;

    if (app->modifie) {
        rapport_ajouter(r, "Des modifications ne sont pas enregistrées : enregistrez d'abord.");
        return RES_ERR_VALIDATION;
    }
    if (!horodater(horodatage_force, base, lisible)) {
        rapport_ajouter(r, "Horodatage impossible (heure système illisible).");
        return RES_ERR_FICHIER;
    }
    for (essai = 1; essai <= 99; essai++) {
        if (essai == 1) {
            snprintf(prefixe, sizeof prefixe, "%s_", base);
        } else {
            snprintf(prefixe, sizeof prefixe, "%s-%d_", base, essai);
        }
        if (!manifeste_present(dossier, prefixe)) {
            break;
        }
    }
    if (essai > 99) {
        rapport_ajouter(r, "Trop de sauvegardes dans la même seconde.");
        return RES_ERR_FICHIER;
    }
    if (!jeu_ecrire(dossier, prefixe, &app->donnees, generation_a_ecrire(app), r)) {
        return RES_ERR_FICHIER;
    }
    if (!ajouter_a_l_index(dossier, prefixe, lisible, generation_a_ecrire(app),
                           (long)app->donnees.operations.taille)) {
        rapport_ajouter(r, "La sauvegarde « %s » est écrite mais l'index %s n'a pas pu être mis à jour.",
                        prefixe, NOM_INDEX_SAUVEGARDES);
        return RES_ERR_FICHIER;
    }
    texte_copier(prefixe_cree, taille, prefixe);
    return RES_OK;
}

Resultat fich_lister_sauvegardes(const char *dossier, Tableau *entrees, RapportErreurs *r)
{
    char chemin[CHEMIN_MAX + 64];
    char ligne[CSV_LIGNE_MAX];
    char *champs[CSV_CHAMPS_MAX];
    FILE *f;
    unsigned long numero = 1;
    EtatLecture etat;
    bool ok = true;

    tableau_vider(entrees);
    if (!chemin_jeu(chemin, sizeof chemin, dossier, "", NOM_INDEX_SAUVEGARDES)) {
        rapport_ajouter(r, "Chemin trop long : %s", dossier);
        return RES_ERR_FICHIER;
    }
    f = fopen(chemin, "rb");
    if (f == NULL) {
        return RES_OK;          /* aucune sauvegarde encore */
    }
    etat = csv_lire_ligne(f, ligne, sizeof ligne, true);
    if (etat != LECTURE_OK || strcmp(ligne, ENTETE_INDEX) != 0) {
        rapport_ajouter(r, "%s : en-tête attendu « %s »", chemin, ENTETE_INDEX);
        fclose(f);
        return RES_ERR_FICHIER;
    }
    while ((etat = csv_lire_ligne(f, ligne, sizeof ligne, false)) == LECTURE_OK) {
        AnalyseCsv a;
        EntreeSauvegarde e;

        numero++;
        memset(&e, 0, sizeof e);
        csv_analyse_init(&a);
        if (csv_decouper(ligne, champs, CSV_CHAMPS_MAX) != 4) {
            rapport_ajouter(r, "%s, ligne %lu : 4 champs attendus", chemin, numero);
            ok = false;
            continue;
        }
        csv_texte(&a, champs[0], "prefixe", e.prefixe, sizeof e.prefixe - 1, true);
        csv_texte(&a, champs[1], "horodatage", e.horodatage, sizeof e.horodatage - 1, true);
        e.generation = csv_entier(&a, champs[2], "generation", 1, 2000000000L, false);
        e.nb_operations = csv_entier(&a, champs[3], "nb_operations", 0, 2000000000L, false);
        if (!a.ok) {
            rapport_ajouter(r, "%s, ligne %lu : %s", chemin, numero, a.message);
            ok = false;
        } else if (!tableau_ajouter(entrees, &e)) {
            rapport_ajouter(r, "Mémoire insuffisante.");
            ok = false;
            break;
        }
    }
    if (etat == LECTURE_TROP_LONGUE || etat == LECTURE_ERREUR) {
        rapport_ajouter(r, "%s : lecture impossible après la ligne %lu", chemin, numero);
        ok = false;
    }
    fclose(f);
    return ok ? RES_OK : RES_ERR_FICHIER;
}

Resultat fich_restaurer_charger(const Application *app, const char *dossier, const char *prefixe,
                                Donnees *restaure, ApercuRestauration *apercu, RapportErreurs *r)
{
    long generation = 0;
    RapportErreurs avert;
    int e;

    memset(apercu, 0, sizeof *apercu);
    if (!manifeste_present(dossier, prefixe)) {
        rapport_ajouter(r, "Aucun jeu de données (%s%s) dans « %s ».", prefixe, NOM_MANIFESTE_FICHIER, dossier);
        return RES_ERR_FICHIER;
    }
    if (!jeu_lire(dossier, prefixe, restaure, &generation, r)) {
        donnees_liberer(restaure);
        donnees_init(restaure);
        return RES_ERR_FICHIER;
    }
    /* prochain_id = max(compteur courant, compteur de la sauvegarde, plus grand id restauré + 1). */
    rapport_init(&avert);
    donnees_relever_compteurs(restaure, &avert);
    rapport_liberer(&avert);
    for (e = 0; e < NB_ENTITES; e++) {
        if (app->donnees.prochain_id[e] > restaure->prochain_id[e]) {
            restaure->prochain_id[e] = app->donnees.prochain_id[e];
            apercu->compteurs_releves = true;
        }
    }
    if (!regles_valider(restaure, r)) {
        donnees_liberer(restaure);
        donnees_init(restaure);
        return RES_ERR_VALIDATION;
    }
    for (e = 0; e < NB_ENTITES; e++) {
        apercu->courant[e] = donnees_table_const(&app->donnees, (Entite)e)->taille;
        apercu->restaure[e] = donnees_table_const(restaure, (Entite)e)->taille;
    }
    return RES_OK;
}

Resultat fich_restaurer_appliquer(Application *app, Donnees *restaure, size_t *nb_rattrapees,
                                  RapportErreurs *messages)
{
    *nb_rattrapees = 0;
    donnees_liberer(&app->donnees);
    app->donnees = *restaure;
    donnees_init(restaure);                 /* « restaure » est consommé */
    app->modifie = true;
    app->nb_modifications++;
    /* Rattrapage des récurrences sur l'état restauré (comme au démarrage). */
    if (rec_rattraper(app, nb_rattrapees, messages) != RES_OK) {
        *nb_rattrapees = 0;
        rapport_ajouter(messages, "Le rattrapage des récurrences a échoué ; la restauration reste valable.");
    }
    return RES_OK;
}
