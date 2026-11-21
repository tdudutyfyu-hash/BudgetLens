/*
 * BudgetLens — menus_modeles.c
 * Modèles d'opération et récurrences.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calendrier.h"
#include "menus.h"
#include "modeles.h"
#include "recurrences.h"
#include "regles.h"
#include "saisie.h"
#include "texte.h"
#include "valeurs.h"

/* ------------------------------------------------------------------ */
/* Modèles                                                             */
/* ------------------------------------------------------------------ */

static void lister_modeles(const Application *app)
{
    const Tableau *t = &app->donnees.modeles;
    size_t i;

    if (t->taille == 0) {
        printf("Aucun modèle.\n");
        return;
    }
    printf("%-5s ", "Id");
    menus_colonne("Nom", 24);
    menus_colonne("Nature", 9);
    menus_colonne("Montant", 16);
    menus_colonne("Libellé", 26);
    printf("Récurrences\n");
    for (i = 0; i < t->taille; i++) {
        const ModeleOperation *m = ELEMENT(t, ModeleOperation, i);
        char montant[MONTANT_TEXTE];

        montant_formater(m->montant_total_ct, montant, sizeof montant);
        printf("%-5d ", m->id);
        menus_colonne_tronquee(m->nom, 24);
        menus_colonne(nature_libelle(m->nature), 9);
        menus_colonne(montant, 16);
        menus_colonne_tronquee(m->libelle, 26);
        printf("%lu\n", (unsigned long)mod_nombre_recurrences(&app->donnees, m->id));
    }
}

static const ModeleOperation *choisir_modele(const Application *app)
{
    Id id;
    const ModeleOperation *m;

    if (app->donnees.modeles.taille == 0) {
        printf("Aucun modèle : créez-en un d'abord (menu Modèles).\n");
        return NULL;
    }
    lister_modeles(app);
    if (saisie_id("Identifiant du modèle : ", &id) != SAISIE_OK) {
        return NULL;
    }
    m = regles_modele(&app->donnees, id);
    if (m == NULL) {
        printf("Modèle %d introuvable.\n", id);
    }
    return m;
}

static void afficher_modele(const Application *app, const ModeleOperation *m)
{
    const Donnees *d = &app->donnees;
    char montant[MONTANT_TEXTE];
    Operation fictive;
    char comptes[2 * NOM_MAX + 8];
    size_t i;

    memset(&fictive, 0, sizeof fictive);
    fictive.nature = m->nature;
    fictive.id_compte_source = m->id_compte_source;
    fictive.id_compte_destination = m->id_compte_destination;
    op_description_comptes(d, &fictive, comptes, sizeof comptes);
    montant_formater(m->montant_total_ct, montant, sizeof montant);
    printf("Modèle %d : %s\n", m->id, m->nom);
    printf("  Nature        : %s\n", nature_libelle(m->nature));
    printf("  Libellé       : %s\n", m->libelle);
    printf("  Montant total : %s\n", montant);
    printf("  Compte(s)     : %s\n", comptes);
    if (m->id_tiers != ID_AUCUN) {
        const Tiers *t = regles_tiers(d, m->id_tiers);
        printf("  Tiers         : %s\n", t != NULL ? t->nom : "?");
    }
    printf("  Utilisé par   : %lu récurrence(s)\n", (unsigned long)mod_nombre_recurrences(d, m->id));
    if (m->nature == NATURE_VIREMENT) {
        return;
    }
    printf("  Ventilation   :\n");
    for (i = 0; i < d->lignes_modeles.taille; i++) {
        const LigneModele *l = ELEMENT(&d->lignes_modeles, LigneModele, i);
        char chemin[CHEMIN_MAX + 1];
        char ml[MONTANT_TEXTE];

        if (l->id_modele != m->id) {
            continue;
        }
        regles_chemin_categorie(d, l->id_categorie, chemin, sizeof chemin);
        montant_formater(l->montant_ct, ml, sizeof ml);
        printf("    - ");
        menus_colonne_tronquee(chemin, 40);
        printf(" %s", ml);
        if (l->id_vehicule != ID_AUCUN) {
            const Vehicule *v = regles_vehicule(d, l->id_vehicule);
            printf("  [véhicule : %s]", v != NULL ? v->nom : "?");
        }
        printf("\n");
    }
}

static void creer_modele(Application *app)
{
    ModeleSaisie ms;
    OperationSaisie os;
    Tableau lignes;
    RapportErreurs r;
    long nature;
    Id id = ID_AUCUN;
    char message[64];

    if (app->donnees.comptes.taille == 0) {
        printf("Aucun compte : créez d'abord un compte (menu Comptes).\n");
        return;
    }
    memset(&ms, 0, sizeof ms);
    memset(&os, 0, sizeof os);
    printf("(Entrée sur un champ obligatoire : abandon)\n");
    if (saisie_texte("Nom du modèle : ", ms.nom, NOM_MAX, false) != SAISIE_OK
        || saisie_entier("Nature (1 = Dépense, 2 = Revenu, 3 = Virement) : ", 1, 3, &nature)
               != SAISIE_OK) {
        menus_abandon();
        return;
    }
    os.nature = (Nature)(nature - 1);
    tableau_init(&lignes, sizeof(LigneSaisie));
    if (!menus_collecter_saisie(app, &os, &lignes, false)) {
        if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            menus_abandon();
        }
        tableau_liberer(&lignes);
        return;
    }
    ms.nature = os.nature;
    texte_copier(ms.libelle, sizeof ms.libelle, os.libelle);
    ms.montant_total_ct = os.montant_total_ct;
    ms.id_compte_source = os.id_compte_source;
    ms.id_compte_destination = os.id_compte_destination;
    ms.id_tiers = os.id_tiers;
    rapport_init(&r);
    snprintf(message, sizeof message, "Modèle créé.");
    if (mod_creer(app, &ms, (const LigneSaisie *)lignes.donnees, lignes.taille, &id, &r) == RES_OK) {
        snprintf(message, sizeof message, "Modèle créé (identifiant %d).", id);
        menus_resultat(RES_OK, &r, message);
    } else {
        menus_resultat(RES_ERR_VALIDATION, &r, message);
    }
    rapport_liberer(&r);
    tableau_liberer(&lignes);
}

static void modifier_modele(Application *app)
{
    const ModeleOperation *m = choisir_modele(app);
    ModeleOperation actuel;
    Operation fictive;
    OperationSaisie os;
    ModeleSaisie ms;
    LigneSaisie *existantes;
    size_t nb_existantes;
    Tableau nouvelles;
    bool remplacer = false;
    char nom[NOM_MAX + 1];
    char invite[NOM_MAX + 32];
    EtatSaisie st;
    RapportErreurs r;
    bool complet;

    if (m == NULL) {
        return;
    }
    actuel = *m;
    nb_existantes = mod_nombre_lignes(&app->donnees, actuel.id);
    existantes = malloc((nb_existantes > 0 ? nb_existantes : 1) * sizeof *existantes);
    if (existantes == NULL) {
        printf("Mémoire insuffisante.\n");
        return;
    }
    mod_lignes_copier(&app->donnees, actuel.id, existantes, nb_existantes);
    snprintf(invite, sizeof invite, "Nom [%s] (Entrée = inchangé) : ", actuel.nom);
    st = saisie_texte(invite, nom, NOM_MAX, false);
    if (st == SAISIE_FIN) {
        free(existantes);
        return;
    }
    if (st == SAISIE_VIDE) {
        texte_copier(nom, sizeof nom, actuel.nom);
    }
    /* La saisie de correction d'une opération sert aussi aux modèles (sans date : id_recurrence factice). */
    memset(&fictive, 0, sizeof fictive);
    fictive.nature = actuel.nature;
    texte_copier(fictive.libelle, sizeof fictive.libelle, actuel.libelle);
    fictive.montant_total_ct = actuel.montant_total_ct;
    fictive.id_compte_source = actuel.id_compte_source;
    fictive.id_compte_destination = actuel.id_compte_destination;
    fictive.id_tiers = actuel.id_tiers;
    fictive.id_recurrence = 1;
    tableau_init(&nouvelles, sizeof(LigneSaisie));
    complet = menus_collecter_correction(app, &fictive, &os, &existantes, &nb_existantes, &nouvelles,
                                         &remplacer);
    if (complet) {
        const LigneSaisie *l = remplacer ? (const LigneSaisie *)nouvelles.donnees : existantes;
        size_t n = remplacer ? nouvelles.taille : nb_existantes;

        memset(&ms, 0, sizeof ms);
        ms.nature = actuel.nature;
        texte_copier(ms.nom, sizeof ms.nom, nom);
        texte_copier(ms.libelle, sizeof ms.libelle, os.libelle);
        ms.montant_total_ct = os.montant_total_ct;
        ms.id_compte_source = os.id_compte_source;
        ms.id_compte_destination = os.id_compte_destination;
        ms.id_tiers = os.id_tiers;
        rapport_init(&r);
        menus_resultat(mod_modifier(app, actuel.id, &ms, l, n, &r), &r,
                       "Modèle modifié (les opérations déjà générées ne changent pas).");
        rapport_liberer(&r);
    } else if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
        menus_abandon();
    }
    tableau_liberer(&nouvelles);
    free(existantes);
}

void menu_modeles(Application *app)
{
    for (;;) {
        const ModeleOperation *m;
        RapportErreurs r;
        bool oui;
        Id id;
        int choix;

        menus_bandeau(app, "Modèles");
        printf("1. Lister\n2. Créer\n3. Consulter\n4. Modifier\n5. Supprimer\n0. Retour\n");
        choix = saisie_menu(5);
        if (choix <= 0) {
            return;
        }
        switch (choix) {
        case 1:
            lister_modeles(app);
            break;
        case 2:
            creer_modele(app);
            break;
        case 3:
            m = choisir_modele(app);
            if (m != NULL) {
                afficher_modele(app, m);
            }
            break;
        case 4:
            modifier_modele(app);
            break;
        default:
            m = choisir_modele(app);
            if (m == NULL) {
                break;
            }
            id = m->id;
            if (saisie_oui_non("Supprimer ce modèle et ses lignes ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
                printf("Suppression annulée.\n");
                break;
            }
            rapport_init(&r);
            menus_resultat(mod_supprimer(app, id, &r), &r, "Modèle supprimé.");
            rapport_liberer(&r);
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}

void menus_operation_depuis_modele(Application *app)
{
    const ModeleOperation *m = choisir_modele(app);
    char invite[96];
    char ref[DATE_TEXTE];
    Date date;
    EtatSaisie st;
    RapportErreurs r;
    RapportErreurs w;
    Resultat res;
    Id id_modele;
    Id id = ID_AUCUN;
    char message[96];

    if (m == NULL) {
        return;
    }
    id_modele = m->id;
    date_formater(app->date_reference, ref);
    snprintf(invite, sizeof invite, "Date de réalisation [Entrée = %s] : ", ref);
    st = saisie_date(invite, false, &date);
    if (st == SAISIE_FIN) {
        return;
    }
    if (st == SAISIE_VIDE) {
        date = app->date_reference;
    }
    rapport_init(&r);
    rapport_init(&w);
    res = op_creer_depuis_modele(app, id_modele, date, false, &id, &w, &r);
    if (res == RES_CONFIRMATION_REQUISE) {
        if (menus_confirmer_avertissements(&w)) {
            rapport_vider(&r);
            res = op_creer_depuis_modele(app, id_modele, date, true, &id, NULL, &r);
        } else {
            rapport_ajouter(&r, "Aucune modification n'a été faite.");
            res = RES_ABANDON;
        }
    }
    snprintf(message, sizeof message, "Opération créée et confirmée (identifiant %d).", id);
    menus_resultat(res, &r, message);
    rapport_liberer(&w);
    rapport_liberer(&r);
}

/* ------------------------------------------------------------------ */
/* Récurrences                                                         */
/* ------------------------------------------------------------------ */

static const char *etat_recurrence(const Recurrence *x)
{
    if (!x->active) {
        return "en pause";
    }
    return date_absente(x->date_prochaine_echeance) ? "terminée" : "active";
}

static void lister_recurrences(const Application *app)
{
    const Tableau *t = &app->donnees.recurrences;
    size_t i;

    if (t->taille == 0) {
        printf("Aucune récurrence.\n");
        return;
    }
    printf("%-5s ", "Id");
    menus_colonne("État", 10);
    menus_colonne("Modèle", 22);
    menus_colonne("Fréquence", 14);
    menus_colonne("Début", 12);
    menus_colonne("Fin", 12);
    menus_colonne("Prochaine", 12);
    printf("Occ.\n");
    for (i = 0; i < t->taille; i++) {
        const Recurrence *x = ELEMENT(t, Recurrence, i);
        const ModeleOperation *m = regles_modele(&app->donnees, x->id_modele);
        char debut[DATE_TEXTE];
        char fin[DATE_TEXTE];
        char suivante[DATE_TEXTE];
        char freq[32];

        date_formater(x->date_debut, debut);
        date_formater(x->date_fin, fin);
        date_formater(x->date_prochaine_echeance, suivante);
        if (x->frequence == FREQ_MENSUELLE) {
            snprintf(freq, sizeof freq, "Mensuelle (%d)", x->jour_mensuel_souhaite);
        } else {
            texte_copier(freq, sizeof freq, "Hebdomadaire");
        }
        printf("%-5d ", x->id);
        menus_colonne(etat_recurrence(x), 10);
        menus_colonne_tronquee(m != NULL ? m->nom : "?", 22);
        menus_colonne(freq, 14);
        menus_colonne(debut, 12);
        menus_colonne(fin[0] != '\0' ? fin : "-", 12);
        menus_colonne(suivante[0] != '\0' ? suivante : "-", 12);
        printf("%lu\n", (unsigned long)rec_nombre_occurrences(&app->donnees, x->id));
    }
}

static const Recurrence *choisir_recurrence(const Application *app)
{
    Id id;
    const Recurrence *x;

    if (app->donnees.recurrences.taille == 0) {
        printf("Aucune récurrence.\n");
        return NULL;
    }
    lister_recurrences(app);
    if (saisie_id("Identifiant de la récurrence : ", &id) != SAISIE_OK) {
        return NULL;
    }
    x = regles_recurrence(&app->donnees, id);
    if (x == NULL) {
        printf("Récurrence %d introuvable.\n", id);
    }
    return x;
}

/* Calendrier d'une récurrence (le modèle est donné) ; faux si abandon ou fin d'entrée. */
static bool saisir_calendrier(const Application *app, Id id_modele, RecurrenceSaisie *s)
{
    long v;
    EtatSaisie st;
    char invite[96];

    (void)app;
    memset(s, 0, sizeof *s);
    s->id_modele = id_modele;
    if (saisie_entier("Fréquence (1 = Hebdomadaire, 2 = Mensuelle) : ", 1, 2, &v) != SAISIE_OK) {
        return false;
    }
    s->frequence = (v == 1) ? FREQ_HEBDOMADAIRE : FREQ_MENSUELLE;
    if (saisie_date("Date de début (AAAA-MM-JJ) : ", false, &s->date_debut) != SAISIE_OK) {
        return false;
    }
    if (s->frequence == FREQ_MENSUELLE) {
        snprintf(invite, sizeof invite, "Jour du mois souhaité (1-31) [Entrée = %d] : ", s->date_debut.jour);
        st = saisie_entier(invite, 1, 31, &v);
        if (st == SAISIE_FIN) {
            return false;
        }
        s->jour_mensuel_souhaite = (st == SAISIE_OK) ? (int)v : s->date_debut.jour;
    }
    for (;;) {
        st = saisie_date("Date de fin, incluse (facultative, Entrée = sans fin) : ", false, &s->date_fin);
        if (st == SAISIE_FIN) {
            return false;
        }
        if (st == SAISIE_VIDE) {
            s->date_fin = date_aucune();
            break;
        }
        if (date_comparer(s->date_fin, s->date_debut) >= 0) {
            break;
        }
        printf("La date de fin doit être postérieure ou égale à la date de début.\n");
    }
    return true;
}

/* Affiche les échéances dues (G5) et demande le choix. Renvoie faux si fin d'entrée. */
static bool choisir_retroactif(const RecurrenceSaisie *s, const ApercuEcheances *ap, ChoixRetro *choix)
{
    char d1[DATE_TEXTE];
    char d2[DATE_TEXTE];
    int c;

    *choix = RETRO_GENERER;
    if (ap->nombre == 0) {
        return true;
    }
    date_formater(ap->premiere, d1);
    date_formater(ap->derniere, d2);
    printf("%lu échéance(s) déjà due(s) (du %s au %s).\n", (unsigned long)ap->nombre, d1, d2);
    if (ap->nombre <= 20) {
        Date c0 = ap->premiere;
        size_t k;
        printf("  Échéances :");
        for (k = 0; k < ap->nombre && !date_absente(c0); k++) {
            date_formater(c0, d1);
            printf(" %s", d1);
            c0 = cal_borner(cal_echeance_suivante(s->frequence, c0,
                                                  s->frequence == FREQ_MENSUELLE ? s->jour_mensuel_souhaite : 0),
                            s->date_fin);
        }
        printf("\n");
    }
    printf("1. Générer ces occurrences (à confirmer)\n2. Ignorer ces échéances (jamais générées)\n"
           "0. Annuler\n");
    c = saisie_menu(2);
    if (c < 0) {
        return false;
    }
    *choix = c == 1 ? RETRO_GENERER : (c == 2 ? RETRO_IGNORER : RETRO_ANNULER);
    return true;
}

static void creer_recurrence(Application *app)
{
    const ModeleOperation *m = choisir_modele(app);
    RecurrenceSaisie s;
    ApercuEcheances ap;
    ChoixRetro choix;
    RapportErreurs r;
    Id id = ID_AUCUN;
    size_t nb = 0;
    Resultat res;
    char message[160];

    if (m == NULL) {
        return;
    }
    if (!saisir_calendrier(app, m->id, &s)) {
        if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            menus_abandon();
        }
        return;
    }
    rapport_init(&r);
    if (!rec_controler_saisie(&app->donnees, &s, &r)) {
        menus_resultat(RES_ERR_VALIDATION, &r, "");
        rapport_liberer(&r);
        return;
    }
    if (!rec_apercu(&s, app->date_reference, &ap)) {
        printf("Plus de %d échéances sont déjà dues : choisissez une date de début plus proche.\n",
               ECHEANCES_DUES_MAX);
        rapport_liberer(&r);
        return;
    }
    if (!choisir_retroactif(&s, &ap, &choix)) {
        rapport_liberer(&r);
        return;
    }
    res = rec_creer(app, &s, choix, &id, &nb, &r);
    snprintf(message, sizeof message, "Récurrence créée (identifiant %d), %lu occurrence(s) générée(s).%s",
             id, (unsigned long)nb,
             date_absente(ap.curseur) ? " Aucune échéance possible avant la date de fin." : "");
    menus_resultat(res, &r, message);
    rapport_liberer(&r);
}

static void modifier_calendrier(Application *app)
{
    const Recurrence *x = choisir_recurrence(app);
    RecurrenceSaisie s;
    ApercuEcheances ap;
    ChoixRetro choix;
    RapportErreurs r;
    Id id;
    size_t nb = 0;
    char message[96];
    Resultat res;

    if (x == NULL) {
        return;
    }
    id = x->id;
    if (rec_nombre_occurrences(&app->donnees, id) > 0) {
        printf("Le calendrier est figé : la récurrence a déjà des occurrences. Utilisez la clôture ; "
               "pour changer le calendrier, clôturez-la et créez-en une nouvelle.\n");
        return;
    }
    if (!saisir_calendrier(app, x->id_modele, &s)) {
        if (saisie_etat_entree() == ENTREE_DISPONIBLE) {
            menus_abandon();
        }
        return;
    }
    rapport_init(&r);
    if (!rec_controler_saisie(&app->donnees, &s, &r)) {
        menus_resultat(RES_ERR_VALIDATION, &r, "");
        rapport_liberer(&r);
        return;
    }
    if (!rec_apercu(&s, app->date_reference, &ap)) {
        printf("Plus de %d échéances sont déjà dues : choisissez une date de début plus proche.\n",
               ECHEANCES_DUES_MAX);
        rapport_liberer(&r);
        return;
    }
    if (!choisir_retroactif(&s, &ap, &choix)) {
        rapport_liberer(&r);
        return;
    }
    res = rec_modifier_calendrier(app, id, &s, choix, &nb, &r);
    snprintf(message, sizeof message, "Calendrier modifié, %lu occurrence(s) générée(s).",
             (unsigned long)nb);
    menus_resultat(res, &r, message);
    rapport_liberer(&r);
}

void menu_recurrences(Application *app)
{
    for (;;) {
        const Recurrence *x;
        RapportErreurs r;
        Date fin;
        size_t nb;
        bool oui;
        Id id;
        int choix;
        char message[96];

        menus_bandeau(app, "Récurrences");
        printf("1. Lister\n2. Créer\n3. Consulter\n4. Désactiver (pause)\n5. Réactiver\n"
               "6. Modifier le calendrier (avant la première occurrence)\n7. Clôturer\n"
               "8. Supprimer (sans occurrence)\n0. Retour\n");
        choix = saisie_menu(8);
        if (choix <= 0) {
            return;
        }
        switch (choix) {
        case 1:
            lister_recurrences(app);
            break;
        case 2:
            creer_recurrence(app);
            break;
        case 3:
            x = choisir_recurrence(app);
            if (x != NULL) {
                char d[DATE_TEXTE];
                const ModeleOperation *m = regles_modele(&app->donnees, x->id_modele);
                printf("Récurrence %d (%s) : modèle %d « %s », %s", x->id, etat_recurrence(x), x->id_modele,
                       m != NULL ? m->nom : "?", frequence_libelle(x->frequence));
                if (x->frequence == FREQ_MENSUELLE) {
                    printf(" au jour %d", x->jour_mensuel_souhaite);
                }
                date_formater(x->date_debut, d);
                printf(", du %s", d);
                date_formater(x->date_fin, d);
                printf(" au %s", d[0] != '\0' ? d : "(sans fin)");
                date_formater(x->date_prochaine_echeance, d);
                printf("\n  Prochaine échéance : %s ; %lu occurrence(s).\n",
                       d[0] != '\0' ? d : "aucune", (unsigned long)rec_nombre_occurrences(&app->donnees, x->id));
            }
            break;
        case 4:
        case 5:
        case 7:
        case 8:
            x = choisir_recurrence(app);
            if (x == NULL) {
                break;
            }
            id = x->id;
            rapport_init(&r);
            nb = 0;
            if (choix == 4) {
                menus_resultat(rec_desactiver(app, id, &nb, &r), &r, "Récurrence mise en pause.");
                if (nb > 0) {
                    printf("%lu échéance(s) due(s) rattrapée(s) avant la pause.\n", (unsigned long)nb);
                }
            } else if (choix == 5) {
                menus_resultat(rec_reactiver(app, id, &r), &r,
                               "Récurrence réactivée (les échéances de la pause ne sont pas rattrapées).");
            } else if (choix == 7) {
                if (saisie_date("Nouvelle date de fin, incluse (AAAA-MM-JJ) : ", false, &fin) != SAISIE_OK) {
                    menus_abandon();
                } else {
                    menus_resultat(rec_cloturer(app, id, fin, &r), &r, "Récurrence clôturée.");
                }
            } else if (saisie_oui_non("Supprimer cette récurrence ? (o/n) : ", &oui) != SAISIE_OK || !oui) {
                printf("Suppression annulée.\n");
            } else {
                snprintf(message, sizeof message, "Récurrence supprimée.");
                menus_resultat(rec_supprimer(app, id, &r), &r, message);
            }
            rapport_liberer(&r);
            break;
        default:
            modifier_calendrier(app);
            break;
        }
        if (saisie_etat_entree() != ENTREE_DISPONIBLE) {
            return;
        }
    }
}
