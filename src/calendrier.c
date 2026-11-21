/*
 * BudgetLens — calendrier.c
 */
#include "calendrier.h"

#include "valeurs.h"

static int jour_effectif(int annee, int mois, int jour_souhaite)
{
    int dans_mois = jours_dans_mois(annee, mois);

    return jour_souhaite < dans_mois ? jour_souhaite : dans_mois;
}

Date cal_premiere_echeance(Frequence f, Date date_debut, int jour_souhaite)
{
    Date d;

    if (!date_valide(date_debut)) {
        return date_aucune();
    }
    if (f == FREQ_HEBDOMADAIRE) {
        return date_debut;
    }
    d = date_creer(date_debut.annee, date_debut.mois,
                   jour_effectif(date_debut.annee, date_debut.mois, jour_souhaite));
    if (date_comparer(d, date_debut) < 0) {
        /* Le jour effectif du mois de début précède le début : mois suivant. */
        return cal_echeance_suivante(f, d, jour_souhaite);
    }
    return d;
}

Date cal_echeance_suivante(Frequence f, Date courante, int jour_souhaite)
{
    int annee = courante.annee;
    int mois = courante.mois + 1;
    Date d;

    if (!date_valide(courante)) {
        return date_aucune();
    }
    if (f == FREQ_HEBDOMADAIRE) {
        d = date_ajouter_jours(courante, 7);
        return date_valide(d) ? d : date_aucune();
    }
    if (mois > 12) {
        mois = 1;
        annee++;
    }
    if (annee > ANNEE_MAX) {
        return date_aucune();
    }
    /* Le calcul part du mois et du jour souhaité, jamais du jour déjà ajusté. */
    return date_creer(annee, mois, jour_effectif(annee, mois, jour_souhaite));
}

Date cal_borner(Date d, Date date_fin)
{
    if (date_absente(d)) {
        return date_aucune();
    }
    if (!date_absente(date_fin) && date_comparer(d, date_fin) > 0) {
        return date_aucune();
    }
    return d;
}

bool cal_sur_calendrier(const Recurrence *r, Date d)
{
    if (!date_valide(d) || !date_valide(r->date_debut) || date_comparer(d, r->date_debut) < 0) {
        return false;
    }
    if (!date_absente(r->date_fin) && date_comparer(d, r->date_fin) > 0) {
        return false;
    }
    if (r->frequence == FREQ_HEBDOMADAIRE) {
        return (date_vers_jours(d) - date_vers_jours(r->date_debut)) % 7 == 0;
    }
    return r->jour_mensuel_souhaite >= 1 && r->jour_mensuel_souhaite <= 31
        && d.jour == jour_effectif(d.annee, d.mois, r->jour_mensuel_souhaite);
}
