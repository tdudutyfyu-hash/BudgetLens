/*
 * BudgetLens — recurrences.c
 */
#include "recurrences.h"

#include <string.h>

#include "calendrier.h"
#include "modeles.h"
#include "regles.h"
#include "texte.h"
#include "valeurs.h"

Recurrence *regles_recurrence(const Donnees *d, Id id)
{
    return (Recurrence *)tableau_trouver_id(&d->recurrences, id, NULL);
}

size_t rec_nombre_occurrences(const Donnees *d, Id id_recurrence)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < d->operations.taille; i++) {
        if (ELEMENT(&d->operations, Operation, i)->id_recurrence == id_recurrence) {
            n++;
        }
    }
    return n;
}

bool rec_controler_saisie(const Donnees *d, const RecurrenceSaisie *s, RapportErreurs *r)
{
    bool ok = true;

    if (regles_modele(d, s->id_modele) == NULL) {
        rapport_ajouter(r, "Modèle %d introuvable.", s->id_modele);
        ok = false;
    }
    if (s->frequence != FREQ_HEBDOMADAIRE && s->frequence != FREQ_MENSUELLE) {
        rapport_ajouter(r, "Fréquence invalide.");
        ok = false;
    }
    if (!date_valide(s->date_debut)) {
        rapport_ajouter(r, "Date de début invalide.");
        ok = false;
    }
    if (!date_absente(s->date_fin)
        && (!date_valide(s->date_fin) || (date_valide(s->date_debut)
                                          && date_comparer(s->date_fin, s->date_debut) < 0))) {
        rapport_ajouter(r, "La date de fin doit être valide et postérieure ou égale à la date de début.");
        ok = false;
    }
    if (s->frequence == FREQ_MENSUELLE && (s->jour_mensuel_souhaite < 1 || s->jour_mensuel_souhaite > 31)) {
        rapport_ajouter(r, "Le jour mensuel souhaité doit être compris entre 1 et 31.");
        ok = false;
    }
    return ok;
}

static int jour_de(const RecurrenceSaisie *s)
{
    return s->frequence == FREQ_MENSUELLE ? s->jour_mensuel_souhaite : 0;
}

static Date premier_curseur(const RecurrenceSaisie *s)
{
    return cal_borner(cal_premiere_echeance(s->frequence, s->date_debut, jour_de(s)), s->date_fin);
}

bool rec_apercu(const RecurrenceSaisie *s, Date reference, ApercuEcheances *a)
{
    Date c = premier_curseur(s);

    a->nombre = 0;
    a->premiere = date_aucune();
    a->derniere = date_aucune();
    a->curseur = c;
    while (!date_absente(c) && date_comparer(c, reference) <= 0) {
        if (a->nombre == 0) {
            a->premiere = c;
        }
        a->derniere = c;
        a->nombre++;
        if (a->nombre > ECHEANCES_DUES_MAX) {
            return false;
        }
        c = cal_borner(cal_echeance_suivante(s->frequence, c, jour_de(s)), s->date_fin);
    }
    return true;
}

/* Nombre d'échéances dues à partir du curseur de x (sans le modifier) ; au plus MAX + 1. */
static size_t compter_dues(const Recurrence *x, Date reference)
{
    Date c = x->date_prochaine_echeance;
    size_t n = 0;

    while (!date_absente(c) && date_comparer(c, reference) <= 0 && n <= ECHEANCES_DUES_MAX) {
        n++;
        c = cal_borner(cal_echeance_suivante(x->frequence, c, x->jour_mensuel_souhaite), x->date_fin);
    }
    return n;
}

static bool cle_existe(const Donnees *d, Id id_recurrence, Date prevue)
{
    size_t i;

    for (i = 0; i < d->operations.taille; i++) {
        const Operation *o = ELEMENT(&d->operations, Operation, i);
        if (o->id_recurrence == id_recurrence && date_comparer(o->date_prevue_origine, prevue) == 0) {
            return true;
        }
    }
    return false;
}

/* Occurrence A_CONFIRMER copiée du modèle (valeurs et lignes) à la date prévue. */
static Resultat generer_occurrence(Donnees *copie, Id id_recurrence, Id id_modele, Date prevue,
                                   RapportErreurs *r)
{
    const ModeleOperation *m = regles_modele(copie, id_modele);
    Operation o;
    size_t i;

    if (m == NULL) {
        rapport_ajouter(r, "Modèle %d introuvable.", id_modele);
        return RES_ERR_INTROUVABLE;
    }
    memset(&o, 0, sizeof o);
    o.id = donnees_nouvel_id(copie, ENT_OPERATION);
    if (o.id == ID_AUCUN) {
        rapport_ajouter(r, "Plus aucun identifiant disponible pour les opérations.");
        return RES_ERR_VALIDATION;
    }
    o.nature = m->nature;
    o.statut = STATUT_A_CONFIRMER;
    o.date_realisation = date_aucune();
    texte_copier(o.libelle, sizeof o.libelle, m->libelle);
    o.montant_total_ct = m->montant_total_ct;
    o.id_compte_source = m->id_compte_source;
    o.id_compte_destination = m->id_compte_destination;
    o.id_tiers = m->id_tiers;
    o.id_recurrence = id_recurrence;
    o.date_prevue_origine = prevue;
    if (!tableau_ajouter(&copie->operations, &o)) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        return RES_ERR_MEMOIRE;
    }
    for (i = 0; i < copie->lignes_modeles.taille; i++) {
        const LigneModele *lm = ELEMENT(&copie->lignes_modeles, LigneModele, i);
        LigneOperation x;

        if (lm->id_modele != id_modele) {
            continue;
        }
        memset(&x, 0, sizeof x);
        x.id = donnees_nouvel_id(copie, ENT_LIGNE_OPERATION);
        if (x.id == ID_AUCUN) {
            rapport_ajouter(r, "Plus aucun identifiant disponible pour les lignes d'opération.");
            return RES_ERR_VALIDATION;
        }
        x.id_operation = o.id;
        x.id_categorie = lm->id_categorie;
        x.id_vehicule = lm->id_vehicule;
        x.montant_ct = lm->montant_ct;
        if (!tableau_ajouter(&copie->lignes_operations, &x)) {
            rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
            return RES_ERR_MEMOIRE;
        }
    }
    return RES_OK;
}

/*
 * Avance le curseur de x jusqu'à la date de référence incluse ; avec generer,
 * crée chaque occurrence manquante (la clé existante n'est jamais recréée).
 * x pointe dans copie->recurrences, tableau que cette fonction ne modifie pas.
 */
static Resultat avancer(Donnees *copie, Recurrence *x, Date reference, bool generer,
                        size_t *nb, RapportErreurs *r)
{
    while (!date_absente(x->date_prochaine_echeance)
           && date_comparer(x->date_prochaine_echeance, reference) <= 0) {
        if (generer && !cle_existe(copie, x->id, x->date_prochaine_echeance)) {
            Resultat res = generer_occurrence(copie, x->id, x->id_modele, x->date_prochaine_echeance, r);
            if (res != RES_OK) {
                return res;
            }
            if (nb != NULL) {
                (*nb)++;
            }
        }
        x->date_prochaine_echeance = cal_borner(
            cal_echeance_suivante(x->frequence, x->date_prochaine_echeance, x->jour_mensuel_souhaite),
            x->date_fin);
    }
    return RES_OK;
}

static Resultat debut(const Application *app, Donnees *copie, RapportErreurs *r)
{
    Resultat res = transaction_debut(app, copie);

    if (res != RES_OK) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
    }
    return res;
}

static void appliquer_calendrier(Recurrence *x, const RecurrenceSaisie *s)
{
    x->frequence = s->frequence;
    x->date_debut = s->date_debut;
    x->date_fin = s->date_fin;
    x->jour_mensuel_souhaite = jour_de(s);
}

Resultat rec_creer(Application *app, const RecurrenceSaisie *s, ChoixRetro choix, Id *id_cree,
                   size_t *nb_generees, RapportErreurs *r)
{
    ApercuEcheances ap;
    Donnees copie;
    Recurrence x;
    Recurrence *p;
    Resultat res;

    if (nb_generees != NULL) {
        *nb_generees = 0;
    }
    if (!rec_controler_saisie(&app->donnees, s, r)) {
        return RES_ERR_VALIDATION;
    }
    if (!rec_apercu(s, app->date_reference, &ap)) {
        rapport_ajouter(r, "Plus de %d échéances sont déjà dues : choisissez une date de début plus "
                           "proche.", ECHEANCES_DUES_MAX);
        return RES_ERR_VALIDATION;
    }
    if (ap.nombre > 0 && choix == RETRO_ANNULER) {
        rapport_ajouter(r, "Création annulée : aucune modification.");
        return RES_ABANDON;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    memset(&x, 0, sizeof x);
    x.id = donnees_nouvel_id(&copie, ENT_RECURRENCE);
    if (x.id == ID_AUCUN) {
        rapport_ajouter(r, "Plus aucun identifiant disponible pour les récurrences.");
        transaction_annuler(&copie);
        return RES_ERR_VALIDATION;
    }
    x.id_modele = s->id_modele;
    appliquer_calendrier(&x, s);
    x.active = true;
    x.date_prochaine_echeance = ap.curseur;
    if (!tableau_ajouter(&copie.recurrences, &x)) {
        rapport_ajouter(r, "Mémoire insuffisante : aucune modification effectuée.");
        transaction_annuler(&copie);
        return RES_ERR_MEMOIRE;
    }
    p = tableau_trouver_id(&copie.recurrences, x.id, NULL);
    if (ap.nombre > 0) {
        res = avancer(&copie, p, app->date_reference, choix == RETRO_GENERER, nb_generees, r);
        if (res != RES_OK) {
            transaction_annuler(&copie);
            return res;
        }
    }
    res = transaction_valider(app, &copie, r);
    if (res == RES_OK && id_cree != NULL) {
        *id_cree = x.id;
    }
    if (res != RES_OK && nb_generees != NULL) {
        *nb_generees = 0;
    }
    return res;
}

Resultat rec_modifier_calendrier(Application *app, Id id, const RecurrenceSaisie *s,
                                 ChoixRetro choix, size_t *nb_generees, RapportErreurs *r)
{
    const Recurrence *actuelle = regles_recurrence(&app->donnees, id);
    ApercuEcheances ap;
    Donnees copie;
    Recurrence *p;
    Resultat res;

    if (nb_generees != NULL) {
        *nb_generees = 0;
    }
    if (actuelle == NULL) {
        rapport_ajouter(r, "Récurrence %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (s->id_modele != actuelle->id_modele) {
        rapport_ajouter(r, "Le modèle d'une récurrence ne change pas après sa création.");
        return RES_ERR_VALIDATION;
    }
    if (rec_nombre_occurrences(&app->donnees, id) > 0) {
        rapport_ajouter(r, "Le calendrier est figé : la récurrence a déjà des occurrences. Seule la "
                           "clôture est possible ; pour changer le calendrier, clôturez-la et créez-en "
                           "une nouvelle.");
        return RES_ERR_VALIDATION;
    }
    if (!actuelle->active) {
        rapport_ajouter(r, "Réactivez la récurrence avant de modifier son calendrier.");
        return RES_ERR_VALIDATION;
    }
    if (!rec_controler_saisie(&app->donnees, s, r)) {
        return RES_ERR_VALIDATION;
    }
    if (!rec_apercu(s, app->date_reference, &ap)) {
        rapport_ajouter(r, "Plus de %d échéances sont déjà dues : choisissez une date de début plus "
                           "proche.", ECHEANCES_DUES_MAX);
        return RES_ERR_VALIDATION;
    }
    if (ap.nombre > 0 && choix == RETRO_ANNULER) {
        rapport_ajouter(r, "Modification annulée : aucune modification.");
        return RES_ABANDON;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    p = tableau_trouver_id(&copie.recurrences, id, NULL);
    appliquer_calendrier(p, s);
    p->date_prochaine_echeance = ap.curseur;
    if (ap.nombre > 0) {
        res = avancer(&copie, p, app->date_reference, choix == RETRO_GENERER, nb_generees, r);
        if (res != RES_OK) {
            transaction_annuler(&copie);
            return res;
        }
    }
    res = transaction_valider(app, &copie, r);
    if (res != RES_OK && nb_generees != NULL) {
        *nb_generees = 0;
    }
    return res;
}

Resultat rec_cloturer(Application *app, Id id, Date nouvelle_fin, RapportErreurs *r)
{
    const Recurrence *actuelle = regles_recurrence(&app->donnees, id);
    Donnees copie;
    Recurrence *p;
    size_t i;
    size_t conflits = 0;

    if (actuelle == NULL) {
        rapport_ajouter(r, "Récurrence %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (rec_nombre_occurrences(&app->donnees, id) == 0) {
        rapport_ajouter(r, "La récurrence n'a pas encore d'occurrence : modifiez son calendrier.");
        return RES_ERR_VALIDATION;
    }
    if (!date_valide(nouvelle_fin) || date_comparer(nouvelle_fin, actuelle->date_debut) < 0) {
        rapport_ajouter(r, "La date de fin doit être valide et postérieure ou égale à la date de début.");
        return RES_ERR_VALIDATION;
    }
    if (!date_absente(actuelle->date_fin) && date_comparer(nouvelle_fin, actuelle->date_fin) > 0) {
        rapport_ajouter(r, "La clôture ne peut que raccourcir la date de fin existante (calendrier figé).");
        return RES_ERR_VALIDATION;
    }
    for (i = 0; i < app->donnees.operations.taille; i++) {
        const Operation *o = ELEMENT(&app->donnees.operations, Operation, i);
        if (o->id_recurrence == id && date_comparer(o->date_prevue_origine, nouvelle_fin) > 0) {
            char d[DATE_TEXTE];
            date_formater(o->date_prevue_origine, d);
            rapport_ajouter(r, "Conflit : l'occurrence %d (%s) est prévue le %s, après la nouvelle "
                               "date de fin.", o->id, statut_libelle(o->statut), d);
            conflits++;
        }
    }
    if (conflits > 0) {
        return RES_ERR_VALIDATION;
    }
    if (debut(app, &copie, r) != RES_OK) {
        return RES_ERR_MEMOIRE;
    }
    p = tableau_trouver_id(&copie.recurrences, id, NULL);
    p->date_fin = nouvelle_fin;
    p->date_prochaine_echeance = cal_borner(p->date_prochaine_echeance, nouvelle_fin);
    return transaction_valider(app, &copie, r);
}

Resultat rec_desactiver(Application *app, Id id, size_t *nb_generees, RapportErreurs *r)
{
    const Recurrence *actuelle = regles_recurrence(&app->donnees, id);
    Donnees copie;
    Recurrence *p;
    Resultat res;

    if (nb_generees != NULL) {
        *nb_generees = 0;
    }
    if (actuelle == NULL) {
        rapport_ajouter(r, "Récurrence %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (!actuelle->active) {
        rapport_ajouter(r, "La récurrence est déjà en pause.");
        return RES_ERR_VALIDATION;
    }
    if (compter_dues(actuelle, app->date_reference) > ECHEANCES_DUES_MAX) {
        rapport_ajouter(r, "Plus de %d échéances sont dues : impossible de les rattraper avant la pause.",
                        ECHEANCES_DUES_MAX);
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    p = tableau_trouver_id(&copie.recurrences, id, NULL);
    /* Les échéances dues pendant la période active sont d'abord rattrapées. */
    res = avancer(&copie, p, app->date_reference, true, nb_generees, r);
    if (res != RES_OK) {
        transaction_annuler(&copie);
        return res;
    }
    p->active = false;
    res = transaction_valider(app, &copie, r);
    if (res != RES_OK && nb_generees != NULL) {
        *nb_generees = 0;
    }
    return res;
}

Resultat rec_reactiver(Application *app, Id id, RapportErreurs *r)
{
    const Recurrence *actuelle = regles_recurrence(&app->donnees, id);
    Donnees copie;
    Recurrence *p;
    Resultat res;

    if (actuelle == NULL) {
        rapport_ajouter(r, "Récurrence %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    if (actuelle->active) {
        rapport_ajouter(r, "La récurrence est déjà active.");
        return RES_ERR_VALIDATION;
    }
    res = debut(app, &copie, r);
    if (res != RES_OK) {
        return res;
    }
    p = tableau_trouver_id(&copie.recurrences, id, NULL);
    p->active = true;
    /* Aucune échéance de la période inactive n'est générée : premier terme strictement futur. */
    res = avancer(&copie, p, app->date_reference, false, NULL, r);
    if (res != RES_OK) {
        transaction_annuler(&copie);
        return res;
    }
    return transaction_valider(app, &copie, r);
}

Resultat rec_supprimer(Application *app, Id id, RapportErreurs *r)
{
    Donnees copie;
    size_t indice;
    size_t n;

    if (regles_recurrence(&app->donnees, id) == NULL) {
        rapport_ajouter(r, "Récurrence %d introuvable.", id);
        return RES_ERR_INTROUVABLE;
    }
    n = rec_nombre_occurrences(&app->donnees, id);
    if (n > 0) {
        rapport_ajouter(r, "La récurrence a %lu occurrence(s) : désactivez-la ou clôturez-la au lieu de "
                           "la supprimer.", (unsigned long)n);
        return RES_ERR_VALIDATION;
    }
    if (debut(app, &copie, r) != RES_OK) {
        return RES_ERR_MEMOIRE;
    }
    if (tableau_trouver_id(&copie.recurrences, id, &indice) != NULL) {
        tableau_retirer(&copie.recurrences, indice);
    }
    return transaction_valider(app, &copie, r);
}

Resultat rec_rattraper(Application *app, size_t *nb_creees, RapportErreurs *messages)
{
    Donnees copie;
    size_t i;
    bool a_faire = false;
    bool change = false;
    Resultat res;

    *nb_creees = 0;
    for (i = 0; i < app->donnees.recurrences.taille; i++) {
        const Recurrence *x = ELEMENT(&app->donnees.recurrences, Recurrence, i);
        if (x->active && !date_absente(x->date_prochaine_echeance)
            && date_comparer(x->date_prochaine_echeance, app->date_reference) <= 0) {
            a_faire = true;
        }
    }
    if (!a_faire) {
        return RES_OK;
    }
    res = debut(app, &copie, messages);
    if (res != RES_OK) {
        return res;
    }
    for (i = 0; i < copie.recurrences.taille; i++) {
        Recurrence *x = ELEMENT(&copie.recurrences, Recurrence, i);

        if (!x->active || date_absente(x->date_prochaine_echeance)
            || date_comparer(x->date_prochaine_echeance, app->date_reference) > 0) {
            continue;
        }
        if (compter_dues(x, app->date_reference) > ECHEANCES_DUES_MAX) {
            rapport_ajouter(messages, "Récurrence %d : plus de %d échéances dues, rattrapage ignoré "
                                      "(clôturez ou recréez cette récurrence).", x->id, ECHEANCES_DUES_MAX);
            continue;
        }
        res = avancer(&copie, x, app->date_reference, true, nb_creees, messages);
        if (res != RES_OK) {
            transaction_annuler(&copie);
            *nb_creees = 0;
            return res;
        }
        change = true;
    }
    if (!change) {
        transaction_annuler(&copie);
        return RES_OK;
    }
    res = transaction_valider(app, &copie, messages);
    if (res != RES_OK) {
        *nb_creees = 0;
    }
    return res;
}
