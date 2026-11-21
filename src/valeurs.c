/*
 * BudgetLens — valeurs.c
 * Dates (validation, conversion en nombre de jours), montants en centimes
 * convertis sans passer par un nombre flottant, codes des énumérations.
 */
#include "valeurs.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Dates                                                               */
/* ------------------------------------------------------------------ */

Date date_aucune(void)
{
    Date d = {0, 0, 0};
    return d;
}

Date date_creer(int annee, int mois, int jour)
{
    Date d;
    d.annee = annee;
    d.mois = mois;
    d.jour = jour;
    return d;
}

bool date_absente(Date d)
{
    return d.annee == 0;
}

bool annee_bissextile(int annee)
{
    return (annee % 4 == 0 && annee % 100 != 0) || annee % 400 == 0;
}

int jours_dans_mois(int annee, int mois)
{
    static const int jours[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (mois < 1 || mois > 12) {
        return 0;
    }
    if (mois == 2 && annee_bissextile(annee)) {
        return 29;
    }
    return jours[mois - 1];
}

bool date_valide(Date d)
{
    return d.annee >= ANNEE_MIN && d.annee <= ANNEE_MAX
        && d.mois >= 1 && d.mois <= 12
        && d.jour >= 1 && d.jour <= jours_dans_mois(d.annee, d.mois);
}

int date_comparer(Date a, Date b)
{
    if (a.annee != b.annee) {
        return a.annee < b.annee ? -1 : 1;
    }
    if (a.mois != b.mois) {
        return a.mois < b.mois ? -1 : 1;
    }
    if (a.jour != b.jour) {
        return a.jour < b.jour ? -1 : 1;
    }
    return 0;
}

static int chiffres_vers_entier(const char *s, size_t n)
{
    int v = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

bool date_lire(const char *texte, Date *d)
{
    size_t i;
    Date r;

    if (strlen(texte) != 10) {
        return false;
    }
    for (i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (texte[i] != '-') {
                return false;
            }
        } else if (!isdigit((unsigned char)texte[i])) {
            return false;
        }
    }
    r.annee = chiffres_vers_entier(texte, 4);
    r.mois = chiffres_vers_entier(texte + 5, 2);
    r.jour = chiffres_vers_entier(texte + 8, 2);
    if (!date_valide(r)) {
        return false;
    }
    *d = r;
    return true;
}

void date_formater(Date d, char sortie[DATE_TEXTE])
{
    if (date_absente(d)) {
        sortie[0] = '\0';
        return;
    }
    if (snprintf(sortie, DATE_TEXTE, "%04d-%02d-%02d", d.annee, d.mois, d.jour)
        >= DATE_TEXTE) {
        sortie[0] = '\0';
    }
}

/*
 * Conversion date <-> nombre de jours depuis le 1970-01-01, d'après
 * l'algorithme « days_from_civil » de Howard Hinnant (calendrier grégorien
 * proleptique, arithmétique entière uniquement).
 */
long date_vers_jours(Date d)
{
    long y = d.annee;
    long m = d.mois;
    long j = d.jour;
    long era;
    long yoe;
    long doy;
    long doe;

    y -= (m <= 2) ? 1 : 0;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + j - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

Date jours_vers_date(long jours)
{
    long z = jours + 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = yoe + era * 400;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    long j = doy - (153 * mp + 2) / 5 + 1;
    long m = (mp < 10) ? mp + 3 : mp - 9;

    if (m <= 2) {
        y++;
    }
    return date_creer((int)y, (int)m, (int)j);
}

Date date_ajouter_jours(Date d, long jours)
{
    return jours_vers_date(date_vers_jours(d) + jours);
}

Date date_aujourdhui(void)
{
    time_t maintenant = time(NULL);
    struct tm *local;

    if (maintenant == (time_t)-1) {
        return date_aucune();
    }
    local = localtime(&maintenant);
    if (local == NULL) {
        return date_aucune();
    }
    /* Copie immédiate : le tampon de localtime n'est pas conservé. */
    return date_creer(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
}

/* ------------------------------------------------------------------ */
/* Montants                                                            */
/* ------------------------------------------------------------------ */

bool montant_lire(const char *texte, long long *ct)
{
    const char *p = texte;
    long long entier = 0;
    long long centimes = 0;
    long long total;

    if (!isdigit((unsigned char)*p)) {
        return false;
    }
    while (isdigit((unsigned char)*p)) {
        if (entier > MONTANT_MAX_CT / 100) {
            return false;
        }
        entier = entier * 10 + (*p - '0');
        p++;
    }
    if (*p == ',' || *p == '.') {
        int decimales = 0;
        p++;
        while (isdigit((unsigned char)*p)) {
            if (decimales == 2) {
                return false;
            }
            centimes = centimes * 10 + (*p - '0');
            decimales++;
            p++;
        }
        if (decimales == 0) {
            return false;
        }
        if (decimales == 1) {
            centimes *= 10;
        }
    }
    if (*p != '\0' || entier > MONTANT_MAX_CT / 100) {
        return false;
    }
    total = entier * 100 + centimes;
    if (total > MONTANT_MAX_CT) {
        return false;
    }
    *ct = total;
    return true;
}

void montant_formater(long long ct, char *sortie, size_t taille)
{
    char chiffres[32];
    char groupes[48];
    unsigned long long valeur;
    unsigned long long entier;
    unsigned long long centimes;
    size_t longueur;
    size_t i;
    size_t k = 0;
    bool negatif = ct < 0;

    valeur = negatif ? (unsigned long long)(-(ct + 1)) + 1u : (unsigned long long)ct;
    entier = valeur / 100u;
    centimes = valeur % 100u;
    if (snprintf(chiffres, sizeof chiffres, "%llu", entier) >= (int)sizeof chiffres) {
        chiffres[0] = '\0';
    }
    longueur = strlen(chiffres);
    for (i = 0; i < longueur; i++) {
        if (i > 0 && (longueur - i) % 3 == 0) {
            groupes[k++] = ' ';
        }
        groupes[k++] = chiffres[i];
    }
    groupes[k] = '\0';
    if (snprintf(sortie, taille, "%s%s,%02llu DH", negatif ? "-" : "", groupes, centimes)
        >= (int)taille && taille > 0) {
        sortie[taille - 1] = '\0';
    }
}

bool liste_ids_lire(const char *texte, Id *ids, size_t max, size_t *n)
{
    const char *p = texte;
    size_t k = 0;

    *n = 0;
    for (;;) {
        long long v = 0;
        size_t i;
        bool chiffre = false;

        while (*p == ' ') {
            p++;
        }
        while (isdigit((unsigned char)*p)) {
            v = v * 10 + (*p - '0');
            if (v > ID_MAX) {
                return false;
            }
            chiffre = true;
            p++;
        }
        if (!chiffre || v <= 0 || k >= max) {
            return false;
        }
        for (i = 0; i < k; i++) {
            if (ids[i] == (Id)v) {
                return false;
            }
        }
        ids[k++] = (Id)v;
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (*p != ',') {
            return false;
        }
        p++;
    }
    *n = k;
    return true;
}

bool montant_additionner(long long a, long long b, long long *resultat)
{
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) {
        return false;
    }
    *resultat = a + b;
    return true;
}

/* ------------------------------------------------------------------ */
/* Énumérations                                                        */
/* ------------------------------------------------------------------ */

static bool lire_code(const char *code, const char *const codes[], int nombre, int *valeur)
{
    int i;
    for (i = 0; i < nombre; i++) {
        if (strcmp(code, codes[i]) == 0) {
            *valeur = i;
            return true;
        }
    }
    return false;
}

static const char *const CODES_TYPE_COMPTE[] = {"BANQUE", "ESPECES"};
static const char *const LIBELLES_TYPE_COMPTE[] = {"Banque", "Espèces"};
static const char *const CODES_TYPE_CATEGORIE[] = {"DEPENSE", "REVENU"};
static const char *const LIBELLES_TYPE_CATEGORIE[] = {"Dépense", "Revenu"};
static const char *const CODES_NATURE[] = {"DEPENSE", "REVENU", "VIREMENT"};
static const char *const LIBELLES_NATURE[] = {"Dépense", "Revenu", "Virement"};
static const char *const CODES_STATUT[] = {"A_CONFIRMER", "CONFIRMEE", "ANNULEE"};
static const char *const LIBELLES_STATUT[] = {"À confirmer", "Confirmée", "Annulée"};
static const char *const CODES_FREQUENCE[] = {"HEBDOMADAIRE", "MENSUELLE"};
static const char *const LIBELLES_FREQUENCE[] = {"Hebdomadaire", "Mensuelle"};

const char *type_compte_code(TypeCompte t) { return CODES_TYPE_COMPTE[t == COMPTE_ESPECES]; }
const char *type_compte_libelle(TypeCompte t) { return LIBELLES_TYPE_COMPTE[t == COMPTE_ESPECES]; }
bool type_compte_lire(const char *code, TypeCompte *t)
{
    int v;
    if (!lire_code(code, CODES_TYPE_COMPTE, 2, &v)) {
        return false;
    }
    *t = (v == 0) ? COMPTE_BANQUE : COMPTE_ESPECES;
    return true;
}

const char *type_categorie_code(TypeCategorie t) { return CODES_TYPE_CATEGORIE[t == CAT_REVENU]; }
const char *type_categorie_libelle(TypeCategorie t) { return LIBELLES_TYPE_CATEGORIE[t == CAT_REVENU]; }
bool type_categorie_lire(const char *code, TypeCategorie *t)
{
    int v;
    if (!lire_code(code, CODES_TYPE_CATEGORIE, 2, &v)) {
        return false;
    }
    *t = (v == 0) ? CAT_DEPENSE : CAT_REVENU;
    return true;
}

static int indice_nature(Nature n) { return n == NATURE_REVENU ? 1 : (n == NATURE_VIREMENT ? 2 : 0); }
const char *nature_code(Nature n) { return CODES_NATURE[indice_nature(n)]; }
const char *nature_libelle(Nature n) { return LIBELLES_NATURE[indice_nature(n)]; }
bool nature_lire(const char *code, Nature *n)
{
    int v;
    if (!lire_code(code, CODES_NATURE, 3, &v)) {
        return false;
    }
    *n = (v == 0) ? NATURE_DEPENSE : (v == 1 ? NATURE_REVENU : NATURE_VIREMENT);
    return true;
}

static int indice_statut(Statut s) { return s == STATUT_CONFIRMEE ? 1 : (s == STATUT_ANNULEE ? 2 : 0); }
const char *statut_code(Statut s) { return CODES_STATUT[indice_statut(s)]; }
const char *statut_libelle(Statut s) { return LIBELLES_STATUT[indice_statut(s)]; }
bool statut_lire(const char *code, Statut *s)
{
    int v;
    if (!lire_code(code, CODES_STATUT, 3, &v)) {
        return false;
    }
    *s = (v == 0) ? STATUT_A_CONFIRMER : (v == 1 ? STATUT_CONFIRMEE : STATUT_ANNULEE);
    return true;
}

const char *frequence_code(Frequence f) { return CODES_FREQUENCE[f == FREQ_MENSUELLE]; }
const char *frequence_libelle(Frequence f) { return LIBELLES_FREQUENCE[f == FREQ_MENSUELLE]; }
bool frequence_lire(const char *code, Frequence *f)
{
    int v;
    if (!lire_code(code, CODES_FREQUENCE, 2, &v)) {
        return false;
    }
    *f = (v == 0) ? FREQ_HEBDOMADAIRE : FREQ_MENSUELLE;
    return true;
}
