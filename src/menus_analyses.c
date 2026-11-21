/*
 * BudgetLens — menus_analyses.c
 * Bilan mensuel (G13), doublons potentiels (I6) et coûts du véhicule (G11).
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

/* ------------------------------------------------------------------ */
/* Bilan mensuel                                                       */
/* ------------------------------------------------------------------ */

static int comparer_noeuds(const void *a, const void *b)
{
    const Categorie *x = *(const Categorie *const *)a;
    const Categorie *y = *(const Categorie *const *)b;
    int c = texte_comparer_sans_casse(x->nom, y->nom);

    return c != 0 ? c : (x->id > y->id) - (x->id < y->id);
}

typedef struct {
    const Application *app;
    TypeCategorie type;
    const long long *totaux;     /* indexés par position dans Donnees.categories */
    int annee;
    int mois;
} ContexteBilan;

static void afficher_noeud_bilan(const ContexteBilan *c, Id id_parent, int profondeur)
{
    const Tableau *t = &c->app->donnees.categories;
    const Categorie **enfants;
    size_t n = 0;
    size_t i;

    if (profondeur > 32) {
        return;
    }
    enfants = malloc((t->taille > 0 ? t->taille : 1) * sizeof *enfants);
    if (enfants == NULL) {
        printf("Mémoire insuffisante pour afficher le bilan.\n");
        return;
    }
    for (i = 0; i < t->taille; i++) {
        const Categorie *cat = ELEMENT(t, Categorie, i);
        if (cat->id_parent == id_parent && cat->type == c->type) {
            enfants[n++] = cat;
        }
    }
    qsort(enfants, n, sizeof *enfants, comparer_noeuds);
    for (i = 0; i < n; i++) {
        size_t indice;
        char libelle[NOM_MAX + 8];
        char montant[MONTANT_TEXTE];
        long long total;

        tableau_trouver_id(t, enfants[i]->id, &indice);
        total = c->totaux[indice];
        snprintf(libelle, sizeof libelle, "%*s%s", profondeur * 2, "", enfants[i]->nom);
        menus_colonne_tronquee(libelle, 34);
        montant_formater(total, montant, sizeof montant);
        menus_colonne(montant, 18);
        if (c->type == CAT_DEPENSE) {
            SituationBudget sb = analyses_situation_budget(&c->app->donnees, c->annee, c->mois,
                                                           enfants[i]->id, total);
            if (!sb.defini) {
                printf("budget non défini");
            } else {
                char plafond[MONTANT_TEXTE];
                char autre[MONTANT_TEXTE];
                montant_formater(sb.plafond_ct, plafond, sizeof plafond);
                if (sb.depassement_ct > 0) {
                    montant_formater(sb.depassement_ct, autre, sizeof autre);
                    printf("plafond %s | DÉPASSEMENT %s", plafond, autre);
                } else {
                    montant_formater(sb.reste_ct, autre, sizeof autre);
                    printf("plafond %s | reste %s", plafond, autre);
                }
            }
        }
        printf("\n");
        afficher_noeud_bilan(c, enfants[i]->id, profondeur + 1);
    }
    free(enfants);
}

void menu_bilan(Application *app)
{
    int annee;
    int mois;
    Date debut;
    Date fin;
    long long *dep;
    long long *rev;
    long long total_dep = 0;
    long long total_rev = 0;
    long long net;
    char texte[MONTANT_TEXTE];
    ContexteBilan c;
    SituationBudget sb;

    menus_bandeau(app, "Bilan mensuel");
    if (!menus_saisir_mois(app, &annee, &mois)) {
        return;
    }
    debut = date_creer(annee, mois, 1);
    fin = date_creer(annee, mois, jours_dans_mois(annee, mois));
    dep = calloc(app->donnees.categories.taille + 1, sizeof *dep);
    rev = calloc(app->donnees.categories.taille + 1, sizeof *rev);
    if (dep == NULL || rev == NULL
        || !analyses_totaux_categories(&app->donnees, debut, fin, NATURE_DEPENSE, dep, &total_dep)
        || !analyses_totaux_categories(&app->donnees, debut, fin, NATURE_REVENU, rev, &total_rev)) {
        printf("Calcul du bilan impossible (mémoire insuffisante ou débordement).\n");
        free(dep);
        free(rev);
        return;
    }
    printf("\nBilan de %04d-%02d (opérations confirmées ; virements exclus)\n", annee, mois);
    printf("\n-- Budget global --\n");
    montant_formater(total_dep, texte, sizeof texte);
    printf("  Dépenses confirmées : %s\n", texte);
    sb = analyses_situation_budget(&app->donnees, annee, mois, ID_AUCUN, total_dep);
    if (!sb.defini) {
        printf("  Budget global       : budget non défini\n");
    } else {
        montant_formater(sb.plafond_ct, texte, sizeof texte);
        printf("  Plafond             : %s\n", texte);
        if (sb.depassement_ct > 0) {
            montant_formater(sb.depassement_ct, texte, sizeof texte);
            printf("  Dépassement         : %s\n", texte);
        } else {
            montant_formater(sb.reste_ct, texte, sizeof texte);
            printf("  Restant             : %s\n", texte);
        }
    }
    printf("\n-- Dépenses par catégorie (total du sous-arbre, chaque ligne comptée une fois) --\n");
    c.app = app;
    c.annee = annee;
    c.mois = mois;
    c.type = CAT_DEPENSE;
    c.totaux = dep;
    if (app->donnees.categories.taille == 0) {
        printf("  Aucune catégorie.\n");
    } else {
        afficher_noeud_bilan(&c, ID_AUCUN, 0);
    }
    printf("\n-- Revenus --\n");
    montant_formater(total_rev, texte, sizeof texte);
    printf("  Revenus confirmés   : %s\n", texte);
    c.type = CAT_REVENU;
    c.totaux = rev;
    afficher_noeud_bilan(&c, ID_AUCUN, 0);
    net = total_rev - total_dep;
    montant_formater(net, texte, sizeof texte);
    printf("\nRésultat net (revenus - dépenses) : %s\n", texte);
    free(dep);
    free(rev);
}

/* ------------------------------------------------------------------ */
/* Doublons                                                            */
/* ------------------------------------------------------------------ */

static void afficher_membre_doublon(const Application *app, const Operation *o, bool propose)
{
    char montant[MONTANT_TEXTE];
    char date[DATE_TEXTE];
    char comptes[2 * NOM_MAX + 8];
    const Tiers *t = o->id_tiers != ID_AUCUN ? regles_tiers(&app->donnees, o->id_tiers) : NULL;

    montant_formater(o->montant_total_ct, montant, sizeof montant);
    date_formater(o->date_realisation, date);
    op_description_comptes(&app->donnees, o, comptes, sizeof comptes);
    printf("  [%d] %s | %s | ", o->id, date, o->libelle);
    printf("%s | %s | tiers : %s", montant, comptes, t != NULL ? t->nom : "-");
    if (o->id_recurrence != ID_AUCUN) {
        printf(" | occurrence de la récurrence %d (protégée : annulation seulement)",
               o->id_recurrence);
    }
    if (propose) {
        printf(" <- proposée à conserver");
    }
    printf("\n");
}

static Resultat lancer_doublons(Application *app, const Id *ids, size_t n, bool accord,
                                RapportErreurs *w, RapportErreurs *r)
{
    return op_supprimer_ou_annuler(app, ids, n, accord, w, r);
}

void menu_doublons(Application *app)
{
    Tableau groupes;
    Tableau membres;
    size_t g;

    menus_bandeau(app, "Doublons potentiels");
    tableau_init(&groupes, sizeof(GroupeDoublons));
    tableau_init(&membres, sizeof(Id));
    if (!analyses_doublons(&app->donnees, &groupes, &membres)) {
        printf("Détection impossible (mémoire insuffisante).\n");
        tableau_liberer(&groupes);
        tableau_liberer(&membres);
        return;
    }
    if (groupes.taille == 0) {
        printf("Aucun doublon potentiel parmi les opérations confirmées.\n");
    } else {
        printf("%lu groupe(s) de doublons potentiels. La détection ne supprime rien toute seule : "
               "vous décidez, groupe par groupe.\n", (unsigned long)groupes.taille);
    }
    for (g = 0; g < groupes.taille; g++) {
        const GroupeDoublons *gr = ELEMENT(&groupes, GroupeDoublons, g);
        const Id *ids = ELEMENT(&membres, Id, gr->debut);
        Id choisis[64];
        size_t nb = 0;
        char tampon[SAISIE_LIGNE_MAX];
        EtatSaisie s;
        size_t k;
        bool valide = true;
        bool arreter = false;

        printf("\nGroupe %lu/%lu (%lu opérations identiques) :\n", (unsigned long)(g + 1),
               (unsigned long)groupes.taille, (unsigned long)gr->nombre);
        for (k = 0; k < gr->nombre; k++) {
            const Operation *o = regles_operation(&app->donnees, ids[k]);
            if (o != NULL) {
                afficher_membre_doublon(app, o, k == 0);
            }
        }
        if (gr->nombre > 64) {
            printf("Groupe trop grand pour un traitement interactif.\n");
            continue;
        }
        s = saisie_ligne("Identifiants à supprimer (ex. 12,15), * = tous sauf l'opération proposée, "
                         "Entrée = tout conserver, 0 = arrêter : ", tampon, sizeof tampon);
        if (s == SAISIE_FIN) {
            break;
        }
        if (s == SAISIE_VIDE) {
            continue;
        }
        if (strcmp(tampon, "0") == 0) {
            arreter = true;
        } else if (strcmp(tampon, "*") == 0) {
            for (k = 1; k < gr->nombre; k++) {
                choisis[nb++] = ids[k];
            }
        } else if (!liste_ids_lire(tampon, choisis, 64, &nb)) {
            printf("Liste d'identifiants invalide (exemple : 12,15).\n");
            valide = false;
        } else {
            for (k = 0; k < nb && valide; k++) {
                size_t m;
                bool membre = false;
                for (m = 0; m < gr->nombre; m++) {
                    if (ids[m] == choisis[k]) {
                        membre = true;
                    }
                }
                if (!membre) {
                    printf("L'opération %d n'appartient pas à ce groupe.\n", choisis[k]);
                    valide = false;
                }
            }
            if (valide && nb >= gr->nombre) {
                printf("Au moins une opération du groupe doit être conservée.\n");
                valide = false;
            }
        }
        if (arreter) {
            break;
        }
        if (!valide || nb == 0) {
            continue;
        }
        {
            bool oui;
            RapportErreurs r;
            RapportErreurs w;
            Resultat res;
            char message[96];

            printf("Seront traitées :");
            for (k = 0; k < nb; k++) {
                const Operation *o = regles_operation(&app->donnees, choisis[k]);
                printf(" %d (%s)", choisis[k],
                       o != NULL && o->id_recurrence != ID_AUCUN ? "annulée" : "supprimée");
            }
            printf("\n");
            if (saisie_oui_non("Confirmer ? (o/n) : ", &oui) != SAISIE_OK) {
                break;
            }
            if (!oui) {
                printf("Groupe laissé tel quel.\n");
                continue;
            }
            rapport_init(&r);
            rapport_init(&w);
            res = lancer_doublons(app, choisis, nb, false, &w, &r);
            if (res == RES_CONFIRMATION_REQUISE) {
                if (menus_confirmer_avertissements(&w)) {
                    rapport_vider(&r);
                    res = lancer_doublons(app, choisis, nb, true, NULL, &r);
                } else {
                    rapport_ajouter(&r, "Aucune modification n'a été faite.");
                    res = RES_ABANDON;
                }
            }
            snprintf(message, sizeof message, "%lu opération(s) du groupe traitée(s).",
                     (unsigned long)nb);
            menus_resultat(res, &r, message);
            rapport_liberer(&w);
            rapport_liberer(&r);
        }
    }
    tableau_liberer(&groupes);
    tableau_liberer(&membres);
}

/* ------------------------------------------------------------------ */
/* Coûts du véhicule                                                   */
/* ------------------------------------------------------------------ */

void menus_cout_vehicule(Application *app)
{
    const Vehicule *v;
    bool toutes;
    Date debut;
    Date fin;
    Tableau lignes;
    Tableau par_cat;
    long long total;
    char d1[DATE_TEXTE];
    char d2[DATE_TEXTE];
    char texte[MONTANT_TEXTE];
    size_t i;

    if (app->donnees.vehicules.taille == 0) {
        printf("Aucun véhicule enregistré : créez-le d'abord.\n");
        return;
    }
    v = ELEMENT(&app->donnees.vehicules, Vehicule, 0);
    if (!menus_saisir_periode(app, false, &toutes, &debut, &fin)) {
        if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            menus_abandon();
        }
        return;
    }
    tableau_init(&lignes, sizeof(LigneVehicule));
    tableau_init(&par_cat, sizeof(TotalCategorie));
    if (!analyses_vehicule(&app->donnees, v->id, debut, fin, &lignes, &par_cat, &total)) {
        printf("Calcul impossible (mémoire insuffisante ou débordement).\n");
    } else {
        date_formater(debut, d1);
        date_formater(fin, d2);
        printf("\nVéhicule « %s » du %s au %s (lignes de dépense confirmées rattachées au véhicule) :\n",
               v->nom, d1, d2);
        if (lignes.taille == 0) {
            printf("  Aucune dépense rattachée au véhicule sur cette période.\n");
        } else {
            montant_formater(total, texte, sizeof texte);
            printf("  Total : %s\n\n  Répartition par catégorie :\n", texte);
            for (i = 0; i < par_cat.taille; i++) {
                const TotalCategorie *t = ELEMENT(&par_cat, TotalCategorie, i);
                montant_formater(t->total_ct, texte, sizeof texte);
                printf("    ");
                menus_colonne_tronquee(t->chemin, 40);
                printf(" %s\n", texte);
            }
            printf("\n  Détail :\n");
            for (i = 0; i < lignes.taille; i++) {
                const LigneVehicule *l = ELEMENT(&lignes, LigneVehicule, i);
                const Operation *o = regles_operation(&app->donnees, l->id_operation);
                char chemin[CHEMIN_MAX + 1];

                date_formater(jours_vers_date(l->jours), d1);
                regles_chemin_categorie(&app->donnees, l->id_categorie, chemin, sizeof chemin);
                montant_formater(l->montant_ct, texte, sizeof texte);
                printf("    [%d] %s | ", l->id_operation, d1);
                menus_colonne_tronquee(o != NULL ? o->libelle : "?", 26);
                printf(" | ");
                menus_colonne_tronquee(chemin, 30);
                printf(" | %s\n", texte);
            }
        }
    }
    tableau_liberer(&lignes);
    tableau_liberer(&par_cat);
}
