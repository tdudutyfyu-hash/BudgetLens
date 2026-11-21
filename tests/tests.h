/*
 * BudgetLens — tests.h
 * Assertions et compteurs partagés par les fichiers de tests unitaires.
 */
#ifndef BUDGETLENS_TESTS_H
#define BUDGETLENS_TESTS_H

#include <stdio.h>

extern int nb_tests;
extern int nb_echecs;
extern const char *dossier_test;     /* dossier temporaire existant, passé en argument */

#define VERIFIER(cond)                                                                  \
    do {                                                                                \
        nb_tests++;                                                                     \
        if (!(cond)) {                                                                  \
            nb_echecs++;                                                                \
            printf("  ÉCHEC %s:%d : %s\n", __FILE__, __LINE__, #cond);                  \
        }                                                                               \
    } while (0)

/* tests_operations.c */
void test_validateur_operations(void);
void test_soldes_et_seuils(void);
void test_operations_manuelles(void);
void test_analyses(void);
void test_utilitaires_etape3(void);
void test_doublons(void);
void test_bilan_vehicule(void);
void test_categories_avancees(void);
void test_reaffectations(void);
void test_calendrier(void);
void test_validateur_recurrences(void);
void test_modeles(void);
void test_recurrences(void);
void test_echeances_et_lots(void);
void test_export_sauvegarde_restauration(void);
void test_import_additif(void);
void test_ergonomie(void);           /* catégorie par nom, dernières opérations */

#endif
