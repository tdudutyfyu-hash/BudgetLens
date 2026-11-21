/*
 * BudgetLens — import.c
 */
#include "import.h"

#include <stdio.h>
#include <string.h>

#include "csv.h"
#include "regles.h"
#include "texte.h"
#include "valeurs.h"

typedef struct {
    long ref;
    unsigned long ligne;            /* numéro de ligne dans import_operations.csv */
    Nature nature;
    Date date;
    char libelle[LIBELLE_MAX + 1];
    long long total;
    Id source;
    Id destination;
    Id tiers;
    bool ok;                        /* tous les champs sont valides et résolus */
} OpImport;

typedef struct {
    long ref;
    unsigned long ligne;            /* numéro de ligne dans import_lignes.csv */
    Id categorie;
    Id vehicule;
    long long montant;
    bool ok;
} LigneImport;

void imp_init(ImportPrepare *p)
{
    donnees_init(&p->candidat);
    tableau_init(&p->ids_importes, sizeof(Id));
    p->nb_operations = 0;
    p->nb_lignes = 0;
    p->pret = false;
}

void imp_liberer(ImportPrepare *p)
{
    donnees_liberer(&p->candidat);
    tableau_liberer(&p->ids_importes);
    p->nb_operations = 0;
    p->nb_lignes = 0;
    p->pret = false;
}

/* ------------------------------------------------------------------ */
/* Résolution des clés naturelles                                      */
/* ------------------------------------------------------------------ */

/* Nombre de comptes portant ce nom (sans casse) ; id reçoit le premier. */
static size_t trouver_compte(const Donnees *d, const char *nom, Id *id)
{
    size_t i;
    size_t n = 0;

    *id = ID_AUCUN;
    for (i = 0; i < d->comptes.taille; i++) {
        const Compte *c = ELEMENT(&d->comptes, Compte, i);
        if (texte_egal_sans_casse(c->nom, nom)) {
            if (n == 0) {
                *id = c->id;
            }
            n++;
        }
    }
    return n;
}

static size_t trouver_tiers(const Donnees *d, const char *nom, Id *id)
{
    size_t i;
    size_t n = 0;

    *id = ID_AUCUN;
    for (i = 0; i < d->tiers.taille; i++) {
        const Tiers *t = ELEMENT(&d->tiers, Tiers, i);
        if (texte_egal_sans_casse(t->nom, nom)) {
            if (n == 0) {
                *id = t->id;
            }
            n++;
        }
    }
    return n;
}

/* Chemin complet « A > B > C » : chaque niveau est cherché parmi les enfants du précédent. */
static bool trouver_categorie(const Donnees *d, const char *chemin, Id *id, char *motif, size_t taille)
{
    char copie[CHEMIN_MAX + 1];
    char *segment;
    char *suite;
    Id parent = ID_AUCUN;

    *id = ID_AUCUN;
    if (strlen(chemin) > CHEMIN_MAX) {
        snprintf(motif, taille, "chemin de catégorie trop long");
        return false;
    }
    texte_copier(copie, sizeof copie, chemin);
    segment = copie;
    for (;;) {
        size_t i;
        bool trouve = false;

        suite = strchr(segment, '>');
        if (suite != NULL) {
            *suite = '\0';
        }
        texte_rogner(segment);
        if (segment[0] == '\0') {
            snprintf(motif, taille, "chemin de catégorie « %s » invalide (niveau vide)", chemin);
            return false;
        }
        for (i = 0; i < d->categories.taille; i++) {
            const Categorie *c = ELEMENT(&d->categories, Categorie, i);
            if (c->id_parent == parent && texte_egal_sans_casse(c->nom, segment)) {
                parent = c->id;
                trouve = true;
                break;
            }
        }
        if (!trouve) {
            snprintf(motif, taille, "catégorie « %s » introuvable", chemin);
            return false;
        }
        if (suite == NULL) {
            break;
        }
        segment = suite + 1;
    }
    *id = parent;
    return true;
}

/* ------------------------------------------------------------------ */
/* Lecture des fichiers                                                */
/* ------------------------------------------------------------------ */

static void erreur_ligne(RapportErreurs *err, const char *fichier, unsigned long ligne, const char *motif)
{
    rapport_ajouter(err, "%s, ligne %lu : %s", fichier, ligne, motif);
}

static FILE *ouvrir(const char *dossier, const char *nom, const char *entete, RapportErreurs *err)
{
    char chemin[CHEMIN_MAX + 64];
    char ligne[CSV_LIGNE_MAX];
    FILE *f;

    if (snprintf(chemin, sizeof chemin, "%s/%s", dossier, nom) >= (int)sizeof chemin) {
        rapport_ajouter(err, "%s : chemin trop long", nom);
        return NULL;
    }
    f = fopen(chemin, "rb");
    if (f == NULL) {
        rapport_ajouter(err, "%s : fichier absent ou illisible dans « %s »", nom, dossier);
        return NULL;
    }
    if (csv_lire_ligne(f, ligne, sizeof ligne, true) != LECTURE_OK || strcmp(ligne, entete) != 0) {
        rapport_ajouter(err, "%s, ligne 1 : en-tête attendu « %s »", nom, entete);
        fclose(f);
        return NULL;
    }
    return f;
}

static const OpImport *chercher_op(const Tableau *ops, long ref)
{
    size_t i;

    for (i = 0; i < ops->taille; i++) {
        if (ELEMENT(ops, OpImport, i)->ref == ref) {
            return ELEMENT(ops, OpImport, i);
        }
    }
    return NULL;
}

static bool lire_operations(const Application *app, const char *dossier, Tableau *ops, RapportErreurs *err)
{
    const Donnees *d = &app->donnees;
    FILE *f = ouvrir(dossier, NOM_IMPORT_OPERATIONS, ENTETE_IMPORT_OPERATIONS, err);
    char ligne[CSV_LIGNE_MAX];
    char *c[CSV_CHAMPS_MAX];
    unsigned long numero = 1;
    EtatLecture etat;

    if (f == NULL) {
        return false;
    }
    while ((etat = csv_lire_ligne(f, ligne, sizeof ligne, false)) == LECTURE_OK) {
        AnalyseCsv a;
        OpImport o;
        char motif[MESSAGE_MAX];
        size_t n;

        numero++;
        memset(&o, 0, sizeof o);
        o.ligne = numero;
        n = csv_decouper(ligne, c, CSV_CHAMPS_MAX);
        if (n != 8) {
            erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero, "8 champs attendus");
            continue;
        }
        csv_analyse_init(&a);
        o.ref = csv_entier(&a, c[0], "ref", 1, 2000000000L, false);
        if (!nature_lire(c[1], &o.nature)) {
            csv_echec(&a, "nature", "DEPENSE, REVENU ou VIREMENT attendu");
        }
        o.date = csv_date(&a, c[2], "date_realisation", false);
        csv_texte(&a, c[3], "libelle", o.libelle, LIBELLE_MAX, true);
        o.total = csv_centimes(&a, c[4], "montant_total_ct", 1, MONTANT_MAX_CT, false, NULL);
        if (!a.ok) {
            erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero, a.message);
            /* la ref reste connue pour ne pas multiplier les erreurs sur les lignes de ventilation */
            if (a.ok == false && o.ref > 0 && chercher_op(ops, o.ref) == NULL) {
                if (!tableau_ajouter(ops, &o)) {
                    rapport_ajouter(err, "Mémoire insuffisante.");
                    fclose(f);
                    return false;
                }
            }
            continue;
        }
        o.ok = true;
        if (chercher_op(ops, o.ref) != NULL) {
            erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero, "ref déjà utilisée dans ce fichier");
            o.ok = false;
        }
        if (date_comparer(o.date, app->date_reference) > 0) {
            /* B3 : jamais de date réelle future à la saisie, import compris. */
            char d1[DATE_TEXTE];
            char d2[DATE_TEXTE];
            date_formater(o.date, d1);
            date_formater(app->date_reference, d2);
            snprintf(motif, sizeof motif, "la date %s est postérieure à la date de référence (%s)", d1, d2);
            erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero, motif);
            o.ok = false;
        }
        if (c[5][0] != '\0') {
            size_t k = trouver_compte(d, c[5], &o.source);
            if (k != 1) {
                snprintf(motif, sizeof motif, "compte source « %s » %s", c[5], k == 0 ? "introuvable" : "ambigu");
                erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero, motif);
                o.ok = false;
            }
        }
        if (c[6][0] != '\0') {
            size_t k = trouver_compte(d, c[6], &o.destination);
            if (k != 1) {
                snprintf(motif, sizeof motif, "compte destination « %s » %s", c[6], k == 0 ? "introuvable" : "ambigu");
                erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero, motif);
                o.ok = false;
            }
        }
        if (c[7][0] != '\0') {
            size_t k = trouver_tiers(d, c[7], &o.tiers);
            if (k != 1) {
                snprintf(motif, sizeof motif, "tiers « %s » %s", c[7],
                         k == 0 ? "introuvable" : "ambigu (plusieurs tiers portent ce nom)");
                erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero, motif);
                o.ok = false;
            }
        }
        if (o.ok) {
            const bool a_src = (c[5][0] != '\0');
            const bool a_dst = (c[6][0] != '\0');
            switch (o.nature) {
            case NATURE_DEPENSE:
                if (!a_src || a_dst) {
                    erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero,
                                 "une dépense exige un compte source et n'a pas de compte destination");
                    o.ok = false;
                }
                break;
            case NATURE_REVENU:
                if (!a_dst || a_src) {
                    erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero,
                                 "un revenu exige un compte destination et n'a pas de compte source");
                    o.ok = false;
                }
                break;
            default:
                if (!a_src || !a_dst || o.source == o.destination) {
                    erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero,
                                 "un virement exige deux comptes distincts");
                    o.ok = false;
                }
                break;
            }
        }
        if (!tableau_ajouter(ops, &o)) {
            rapport_ajouter(err, "Mémoire insuffisante.");
            fclose(f);
            return false;
        }
    }
    if (etat == LECTURE_TROP_LONGUE || etat == LECTURE_ERREUR) {
        erreur_ligne(err, NOM_IMPORT_OPERATIONS, numero + 1, "ligne illisible ou trop longue");
    }
    fclose(f);
    return true;
}

static bool lire_lignes(const Application *app, const char *dossier, const Tableau *ops,
                        Tableau *lignes, RapportErreurs *err)
{
    const Donnees *d = &app->donnees;
    FILE *f = ouvrir(dossier, NOM_IMPORT_LIGNES, ENTETE_IMPORT_LIGNES, err);
    char tampon[CSV_LIGNE_MAX];
    char *c[CSV_CHAMPS_MAX];
    unsigned long numero = 1;
    EtatLecture etat;

    if (f == NULL) {
        return false;
    }
    while ((etat = csv_lire_ligne(f, tampon, sizeof tampon, false)) == LECTURE_OK) {
        AnalyseCsv a;
        LigneImport l;
        const OpImport *o;
        char motif[MESSAGE_MAX];

        numero++;
        memset(&l, 0, sizeof l);
        l.ligne = numero;
        if (csv_decouper(tampon, c, CSV_CHAMPS_MAX) != 4) {
            erreur_ligne(err, NOM_IMPORT_LIGNES, numero, "4 champs attendus");
            continue;
        }
        csv_analyse_init(&a);
        l.ref = csv_entier(&a, c[0], "ref", 1, 2000000000L, false);
        l.montant = csv_centimes(&a, c[3], "montant_ct", 1, MONTANT_MAX_CT, false, NULL);
        if (!a.ok) {
            erreur_ligne(err, NOM_IMPORT_LIGNES, numero, a.message);
            continue;
        }
        o = chercher_op(ops, l.ref);
        if (o == NULL) {
            snprintf(motif, sizeof motif, "ref %ld absente de %s", l.ref, NOM_IMPORT_OPERATIONS);
            erreur_ligne(err, NOM_IMPORT_LIGNES, numero, motif);
            continue;
        }
        l.ok = true;
        if (!trouver_categorie(d, c[1], &l.categorie, motif, sizeof motif)) {
            erreur_ligne(err, NOM_IMPORT_LIGNES, numero, motif);
            l.ok = false;
        } else {
            const Categorie *cat = regles_categorie(d, l.categorie);
            if (!regles_categorie_est_feuille(d, l.categorie)) {
                snprintf(motif, sizeof motif, "la catégorie « %s » n'est pas terminale", c[1]);
                erreur_ligne(err, NOM_IMPORT_LIGNES, numero, motif);
                l.ok = false;
            } else if (o->nature == NATURE_VIREMENT) {
                erreur_ligne(err, NOM_IMPORT_LIGNES, numero, "un virement ne comporte aucune ligne de catégorie");
                l.ok = false;
            } else if ((o->nature == NATURE_DEPENSE) != (cat->type == CAT_DEPENSE)) {
                snprintf(motif, sizeof motif, "la catégorie « %s » (%s) ne convient pas à %s", c[1],
                         type_categorie_libelle(cat->type),
                         o->nature == NATURE_DEPENSE ? "une dépense" : "un revenu");
                erreur_ligne(err, NOM_IMPORT_LIGNES, numero, motif);
                l.ok = false;
            }
        }
        if (c[2][0] != '\0') {
            if (strcmp(c[2], "1") != 0) {
                erreur_ligne(err, NOM_IMPORT_LIGNES, numero, "champ « vehicule » : vide ou 1 attendu");
                l.ok = false;
            } else if (d->vehicules.taille == 0) {
                erreur_ligne(err, NOM_IMPORT_LIGNES, numero, "aucun véhicule n'existe dans les données courantes");
                l.ok = false;
            } else if (o->nature != NATURE_DEPENSE) {
                erreur_ligne(err, NOM_IMPORT_LIGNES, numero, "le véhicule n'est admis que sur une ligne de dépense");
                l.ok = false;
            } else {
                l.vehicule = ELEMENT(&d->vehicules, Vehicule, 0)->id;
            }
        }
        if (!tableau_ajouter(lignes, &l)) {
            rapport_ajouter(err, "Mémoire insuffisante.");
            fclose(f);
            return false;
        }
    }
    if (etat == LECTURE_TROP_LONGUE || etat == LECTURE_ERREUR) {
        erreur_ligne(err, NOM_IMPORT_LIGNES, numero + 1, "ligne illisible ou trop longue");
    }
    fclose(f);
    return true;
}

/* Cohérence de chaque opération avec ses lignes (nombre, somme). */
static void controler_ventilations(const Tableau *ops, const Tableau *lignes, RapportErreurs *err)
{
    size_t i;
    size_t j;

    for (i = 0; i < ops->taille; i++) {
        const OpImport *o = ELEMENT(ops, OpImport, i);
        size_t n = 0;
        long long somme = 0;
        bool debord = false;
        bool lignes_ok = true;

        if (!o->ok) {
            continue;
        }
        for (j = 0; j < lignes->taille; j++) {
            const LigneImport *l = ELEMENT(lignes, LigneImport, j);
            if (l->ref == o->ref) {
                n++;
                lignes_ok = lignes_ok && l->ok;
                if (!montant_additionner(somme, l->montant, &somme)) {
                    debord = true;
                }
            }
        }
        if (o->nature == NATURE_VIREMENT) {
            continue;       /* les lignes d'un virement sont déjà signalées une à une */
        }
        if (n == 0) {
            erreur_ligne(err, NOM_IMPORT_OPERATIONS, o->ligne,
                         "une dépense ou un revenu exige au moins une ligne dans import_lignes.csv");
        } else if (lignes_ok && (debord || somme != o->total)) {
            char a[MONTANT_TEXTE];
            char b[MONTANT_TEXTE];
            char motif[MESSAGE_MAX];
            montant_formater(somme, a, sizeof a);
            montant_formater(o->total, b, sizeof b);
            snprintf(motif, sizeof motif, "la somme des lignes (%s) diffère du montant total (%s)", a, b);
            erreur_ligne(err, NOM_IMPORT_OPERATIONS, o->ligne, motif);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Assemblage et validation                                            */
/* ------------------------------------------------------------------ */

/* Ajoute l'opération importée (et ses lignes) à d avec de nouveaux identifiants. */
static bool ajouter_import(Donnees *d, const OpImport *o, const Tableau *lignes, Id *id_cree)
{
    Operation op;
    size_t j;

    memset(&op, 0, sizeof op);
    op.id = donnees_nouvel_id(d, ENT_OPERATION);
    if (op.id == ID_AUCUN) {
        return false;
    }
    op.nature = o->nature;
    op.statut = STATUT_CONFIRMEE;
    op.date_realisation = o->date;
    texte_copier(op.libelle, sizeof op.libelle, o->libelle);
    op.montant_total_ct = o->total;
    op.id_compte_source = o->source;
    op.id_compte_destination = o->destination;
    op.id_tiers = o->tiers;
    op.id_recurrence = ID_AUCUN;
    op.date_prevue_origine = date_aucune();
    if (!tableau_ajouter(&d->operations, &op)) {
        return false;
    }
    for (j = 0; j < lignes->taille; j++) {
        const LigneImport *l = ELEMENT(lignes, LigneImport, j);
        LigneOperation x;

        if (l->ref != o->ref) {
            continue;
        }
        memset(&x, 0, sizeof x);
        x.id = donnees_nouvel_id(d, ENT_LIGNE_OPERATION);
        if (x.id == ID_AUCUN) {
            return false;
        }
        x.id_operation = op.id;
        x.id_categorie = l->categorie;
        x.id_vehicule = l->vehicule;
        x.montant_ct = l->montant;
        if (!tableau_ajouter(&d->lignes_operations, &x)) {
            return false;
        }
    }
    *id_cree = op.id;
    return true;
}

/* Validation globale ayant échoué : on cherche, une à une, les opérations importées qui la causent. */
static void diagnostiquer(const Application *app, const Tableau *ops, const Tableau *lignes,
                          RapportErreurs *err)
{
    Donnees travail;
    size_t i;

    if (!donnees_copier(&travail, &app->donnees)) {
        rapport_ajouter(err, "Mémoire insuffisante pour localiser l'erreur.");
        return;
    }
    for (i = 0; i < ops->taille; i++) {
        const OpImport *o = ELEMENT(ops, OpImport, i);
        Donnees essai;
        RapportErreurs v;
        Id id;
        size_t k;

        if (!donnees_copier(&essai, &travail)) {
            rapport_ajouter(err, "Mémoire insuffisante pour localiser l'erreur.");
            break;
        }
        rapport_init(&v);
        if (ajouter_import(&essai, o, lignes, &id) && regles_valider(&essai, &v)) {
            donnees_liberer(&travail);
            travail = essai;
        } else {
            char entete[96];
            snprintf(entete, sizeof entete, "%s, ligne %lu (ref %ld) : ", NOM_IMPORT_OPERATIONS,
                     o->ligne, o->ref);
            for (k = 0; k < v.messages.taille; k++) {
                rapport_ajouter(err, "%s%s", entete, rapport_message(&v, k));
            }
            if (v.messages.taille == 0) {
                rapport_ajouter(err, "%srefusée par la validation", entete);
            }
            donnees_liberer(&essai);
        }
        rapport_liberer(&v);
    }
    donnees_liberer(&travail);
}

Resultat imp_analyser(const Application *app, const char *dossier, ImportPrepare *p, RapportErreurs *erreurs)
{
    Tableau ops;
    Tableau lignes;
    size_t i;
    size_t avant;
    Resultat res = RES_ERR_VALIDATION;

    imp_liberer(p);
    imp_init(p);
    tableau_init(&ops, sizeof(OpImport));
    tableau_init(&lignes, sizeof(LigneImport));
    avant = rapport_nombre(erreurs);
    if (!lire_operations(app, dossier, &ops, erreurs)) {
        res = RES_ERR_FICHIER;
        goto fin;
    }
    if (!lire_lignes(app, dossier, &ops, &lignes, erreurs)) {
        res = RES_ERR_FICHIER;
        goto fin;
    }
    controler_ventilations(&ops, &lignes, erreurs);
    if (ops.taille == 0 && rapport_nombre(erreurs) == avant) {
        rapport_ajouter(erreurs, "%s : aucune opération à importer", NOM_IMPORT_OPERATIONS);
    }
    if (rapport_nombre(erreurs) > avant) {
        goto fin;           /* erreurs de fichier : rejet complet, rien n'est assemblé */
    }
    if (!donnees_copier(&p->candidat, &app->donnees)) {
        rapport_ajouter(erreurs, "Mémoire insuffisante : import impossible.");
        res = RES_ERR_MEMOIRE;
        goto fin;
    }
    for (i = 0; i < ops.taille; i++) {
        Id id;
        if (!ajouter_import(&p->candidat, ELEMENT(&ops, OpImport, i), &lignes, &id)
            || !tableau_ajouter(&p->ids_importes, &id)) {
            rapport_ajouter(erreurs, "Mémoire insuffisante ou identifiants épuisés : import impossible.");
            res = RES_ERR_MEMOIRE;
            goto fin;
        }
    }
    {
        RapportErreurs v;

        rapport_init(&v);
        if (!regles_valider(&p->candidat, &v)) {
            diagnostiquer(app, &ops, &lignes, erreurs);
            if (rapport_nombre(erreurs) == avant) {
                rapport_fusionner(erreurs, &v, "");     /* cause globale non attribuable à une ligne */
            }
            rapport_liberer(&v);
            goto fin;
        }
        rapport_liberer(&v);
    }
    p->nb_operations = ops.taille;
    p->nb_lignes = lignes.taille;
    p->pret = true;
    res = RES_OK;
fin:
    if (res != RES_OK) {
        imp_liberer(p);
        imp_init(p);
    }
    tableau_liberer(&ops);
    tableau_liberer(&lignes);
    return res;
}

/* ------------------------------------------------------------------ */
/* Doublons, exclusion, application                                    */
/* ------------------------------------------------------------------ */

static bool est_importe(const ImportPrepare *p, Id id)
{
    size_t i;

    for (i = 0; i < p->ids_importes.taille; i++) {
        if (*ELEMENT(&p->ids_importes, Id, i) == id) {
            return true;
        }
    }
    return false;
}

bool imp_doublons(const ImportPrepare *p, Tableau *groupes, Tableau *membres, Tableau *a_exclure)
{
    Tableau tous_g;
    Tableau tous_m;
    size_t g;
    bool ok;

    tableau_vider(groupes);
    tableau_vider(membres);
    tableau_vider(a_exclure);
    tableau_init(&tous_g, sizeof(GroupeDoublons));
    tableau_init(&tous_m, sizeof(Id));
    ok = analyses_doublons(&p->candidat, &tous_g, &tous_m);
    for (g = 0; ok && g < tous_g.taille; g++) {
        const GroupeDoublons *gr = ELEMENT(&tous_g, GroupeDoublons, g);
        GroupeDoublons sortie;
        size_t k;
        size_t nb_importes = 0;

        for (k = 0; k < gr->nombre; k++) {
            if (est_importe(p, *ELEMENT(&tous_m, Id, gr->debut + k))) {
                nb_importes++;
            }
        }
        if (nb_importes == 0) {
            continue;       /* doublon entre opérations déjà présentes : hors sujet */
        }
        sortie.debut = membres->taille;
        sortie.nombre = gr->nombre;
        {
            bool existant = (nb_importes < gr->nombre);
            bool garde = false;
            for (k = 0; ok && k < gr->nombre; k++) {
                Id id = *ELEMENT(&tous_m, Id, gr->debut + k);
                ok = tableau_ajouter(membres, &id);
                if (ok && est_importe(p, id)) {
                    if (existant || garde) {
                        ok = tableau_ajouter(a_exclure, &id);
                    } else {
                        garde = true;       /* aucun membre existant : on garde le plus petit importé */
                    }
                }
            }
        }
        ok = ok && tableau_ajouter(groupes, &sortie);
    }
    tableau_liberer(&tous_g);
    tableau_liberer(&tous_m);
    if (!ok) {
        tableau_vider(groupes);
        tableau_vider(membres);
        tableau_vider(a_exclure);
    }
    return ok;
}

Resultat imp_exclure(ImportPrepare *p, const Tableau *ids, RapportErreurs *r)
{
    Donnees essai;
    Tableau restants;
    size_t i;

    if (!donnees_copier(&essai, &p->candidat)) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return RES_ERR_MEMOIRE;
    }
    for (i = 0; i < ids->taille; i++) {
        Id id = *ELEMENT(ids, Id, i);
        size_t indice;
        size_t k;

        for (k = essai.lignes_operations.taille; k > 0; k--) {
            if (ELEMENT(&essai.lignes_operations, LigneOperation, k - 1)->id_operation == id) {
                tableau_retirer(&essai.lignes_operations, k - 1);
            }
        }
        if (tableau_trouver_id(&essai.operations, id, &indice) != NULL) {
            tableau_retirer(&essai.operations, indice);
        }
    }
    if (!regles_valider(&essai, r)) {
        rapport_ajouter(r, "Exclure ces opérations rendrait l'ensemble invalide : l'import reste inchangé.");
        donnees_liberer(&essai);
        return RES_ERR_VALIDATION;
    }
    tableau_init(&restants, sizeof(Id));
    for (i = 0; i < p->ids_importes.taille; i++) {
        Id id = *ELEMENT(&p->ids_importes, Id, i);
        size_t k;
        bool exclu = false;
        for (k = 0; k < ids->taille; k++) {
            if (*ELEMENT(ids, Id, k) == id) {
                exclu = true;
            }
        }
        if (!exclu && !tableau_ajouter(&restants, &id)) {
            tableau_liberer(&restants);
            donnees_liberer(&essai);
            rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
            return RES_ERR_MEMOIRE;
        }
    }
    donnees_liberer(&p->candidat);
    p->candidat = essai;
    tableau_liberer(&p->ids_importes);
    p->ids_importes = restants;
    p->nb_operations = restants.taille;
    p->nb_lignes = 0;
    for (i = 0; i < p->candidat.lignes_operations.taille; i++) {
        if (est_importe(p, ELEMENT(&p->candidat.lignes_operations, LigneOperation, i)->id_operation)) {
            p->nb_lignes++;
        }
    }
    return RES_OK;
}

Resultat imp_appliquer(Application *app, ImportPrepare *p, bool accord_seuil, RapportErreurs *avert,
                       RapportErreurs *r)
{
    Donnees copie;

    if (!p->pret) {
        rapport_ajouter(r, "Aucun import analysé.");
        return RES_ERR_VALIDATION;
    }
    if (p->nb_operations == 0) {
        rapport_ajouter(r, "Aucune opération à importer.");
        return RES_ERR_VALIDATION;
    }
    if (!donnees_copier(&copie, &p->candidat)) {      /* le candidat reste réutilisable */
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return RES_ERR_MEMOIRE;
    }
    return transaction_valider_seuil(app, &copie, accord_seuil, avert, r);
}
