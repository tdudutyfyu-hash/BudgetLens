# BudgetLens

**Gestion et analyse des dépenses personnelles avec suivi budgétaire.**

BudgetLens est un projet académique en **C23**. L’application s’utilise dans un terminal Windows et enregistre ses données en CSV. Les montants sont saisis et affichés en dirhams (DH) et stockés en centimes entiers.

## Fonctionnalités

- Gérer les comptes, les catégories hiérarchiques, les tiers et les budgets mensuels.
- Enregistrer des dépenses ventilées, des revenus et des virements entre comptes.
- Rechercher et trier les opérations, repérer les doublons et consulter les statistiques des dépenses.
- Suivre les plafonds budgétaires, les échéances récurrentes et les coûts d’un véhicule.
- Enregistrer les données, les exporter, les sauvegarder, restaurer un jeu complet ou importer des opérations supplémentaires.

## Compiler et lancer

Prérequis sur Windows : **GCC 16.1.0 avec MSYS2 UCRT64**, la version utilisée pour les vérifications. Les scripts trouvent GCC dans le PATH ou dans l’emplacement standard de MSYS2. Les tests de recette utilisent aussi Windows PowerShell 5.1.

Depuis la racine du dépôt, dans PowerShell :

```powershell
.\build.bat
.\lancer_demo.bat
```

Le lanceur de démonstration charge des données fictives et fixe la date au **13 novembre 2026**. Dans le menu, choisissez **3 — Bilan mensuel**, puis saisissez **2026** et **11**. Le jeu initial affiche **2 109,00 DH** de dépenses confirmées. La catégorie **Courses** totalise **745,00 DH** pour un plafond de **700,00 DH**, soit un dépassement de **45,00 DH**.

Consultez le [guide des scénarios de démonstration](demo/README_TESTS.md) pour tester la ventilation, les doublons, les récurrences et l’import CSV. Le dossier de travail de la démonstration est réinitialisé à chaque lancement.

Pour commencer avec vos propres données, utilisez `.\lancer.bat`. Pour vérifier le projet, lancez `.\tests.bat`, qui compile le code en C23 et exécute les tests unitaires et les scénarios de recette.

## Organisation du dépôt

| Dossier | Contenu |
| --- | --- |
| [src/](src/) | Point d’entrée, menus, règles métier, statistiques et persistance |
| [demo/](demo/) | Données fictives, export restaurable et exemples d’import CSV |
| [tests/](tests/) | Tests unitaires et scénarios de recette automatisés |
| [outils/](outils/) | Générateur du jeu de démonstration |

## Données et limites

Les CSV sont encodés en UTF-8 et utilisent le point-virgule. L’import additif attend un dossier contenant ensemble `import_operations.csv` et `import_lignes.csv`. Les comptes, catégories et tiers référencés doivent déjà exister. Pour conserver les modifications de la session, utilisez **Fichiers → Enregistrer**.

L’application est conçue pour un utilisateur local. Le stockage s’appuie sur deux emplacements d’enregistrement alternés et des sommes de contrôle pour détecter une corruption accidentelle. Une coupure de courant peut faire perdre le dernier enregistrement.
