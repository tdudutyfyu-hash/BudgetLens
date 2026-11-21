# BudgetLens — CSV et scénarios de test

Préparé le 17 novembre 2026 pour l'application console `bin/BudgetLens.exe`.
Données fictives. Date de test : **2026-11-13**.

## Choisir le point de départ

**Base vide ou message « introuvable » : effectuer d'abord la préparation ci-dessous.**
L'import ne crée ni comptes, ni catégories, ni tiers. Un import refusé ne crée aucune opération.
Le résultat du premier import dans cette base préparée sera **4 opérations ajoutées**.

**Démonstration préchargée :** suivre « Démarrer la démonstration ». Les référentiels existent déjà.
Le résultat sera **3 opérations ajoutées** si le doublon est exclu.

## Préparer une base vide AVANT l'import

Utiliser une base de test, avec une date de référence au moins égale au **2026-11-13**.
Ne pas recréer les éléments déjà présents ; compléter seulement ceux qui manquent.

### 1. Comptes — menu 7, puis 2 (Créer)

| Nom exact du compte | Type | Solde initial à saisir en DH | Date du solde initial |
| --- | --- | --- | --- |
| Espèces | 2 — Espèces | 1000 | 2026-11-01 |
| Banque Populaire | 1 — Banque | 1000 | 2026-11-01 |
| Épargne | 1 — Banque | 0 | 2026-11-01 |

Laisser les champs facultatifs vides avec Entrée (seuil, nom de banque, référence).
Dans les menus, saisir les montants en **DH** ; dans les CSV, ils sont en **centimes**.
La date du solde initial doit précéder ou être égale aux dates des opérations importées.

### 2. Catégories — menu 8, puis 2 (Créer)

Créer chaque parent avant son enfant. Pour une racine, laisser « Catégorie parente » vide.
Pour un enfant, saisir l'identifiant du parent affiché par l'application, puis le nom de l'enfant seul.

| Nom à saisir | Parent | Type |
| --- | --- | --- |
| Alimentation | Aucun | 1 — Dépense |
| Restaurants | Alimentation | Dépense, hérité |
| Santé | Aucun | 1 — Dépense |
| Pharmacie | Santé | Dépense, hérité |
| Loisirs | Aucun | 1 — Dépense |
| Cinéma | Loisirs | Dépense, hérité |
| Bourse | Aucun | 2 — Revenu |

Ne pas créer une seule catégorie nommée « Alimentation > Restaurants » : il faut le parent
« Alimentation » et son enfant « Restaurants ». Respecter les accents.

### 3. Tiers — menu 10, puis 2 (Créer)

Créer **Pharmacie Atlas** et **Cinéma Rif**. Ils sont référencés par les CSV et sont obligatoires pour cet import.

### 4. Importer et vérifier

Menu **13 — Fichiers > 5 — Importer** : saisir le chemin du dossier contenant les deux CSV,
puis répondre `o` à « Importer maintenant ? ». Depuis la racine du dépôt, saisir le chemin relatif `demo\import_exemple`.

Sur une base initialement vide, avec exactement les soldes de test ci-dessus :

- **4 opérations ajoutées**, sans doublon au premier import.
- Dépenses de novembre : **285,00 DH** ; revenus : **500,00 DH** ; résultat net : **215,00 DH**.
- Soldes : Espèces **770,00 DH**, Banque Populaire **945,00 DH**, Épargne **500,00 DH**.

Faire **Fichiers > 1 — Enregistrer**. Les scénarios de démonstration ci-dessous utilisent un autre
jeu de départ : leurs identifiants, budgets et totaux ne s'appliquent pas à cette base minimale.

## Démarrer la démonstration

Depuis la racine du projet, lancer `lancer_demo.bat` (double-clic possible).
Ce lanceur remet `demo/travail` à zéro à chaque lancement et y copie le jeu de démonstration.
Il ne modifie pas le dossier habituel `data`. Recommencer ainsi avant chaque scénario indépendant.

Le jeu contient 3 comptes, 22 catégories, 42 opérations (dont 4 échéances à confirmer),
1 véhicule et 2 récurrences, sur septembre à novembre 2026.

## Quels fichiers utiliser ?

- `import_exemple/import_operations.csv` : 4 opérations à importer.
- `import_exemple/import_lignes.csv` : leurs 4 lignes de répartition par catégorie.
- `jeu_demo/` : export complet de démonstration, à utiliser avec **Fichiers > Restaurer une sauvegarde ou un export** sur une session de test. Conserver tous les CSV ensemble, y compris le manifeste.

L'import additif exige les deux fichiers nommés exactement `import_operations.csv` et
`import_lignes.csv` dans le même dossier. Il demande le chemin du **dossier**, pas celui d'un CSV.
Les comptes, catégories et tiers doivent déjà exister : les créer avec la procédure ci-dessus,
ou démarrer la démonstration qui les contient déjà.

Format : UTF-8, séparateur `;`, dates `AAAA-MM-JJ`, montants en centimes entiers.
Ainsi, `12000` correspond à **120,00 DH**. Ne pas remplacer les centimes par des décimales.
Le fichier des opérations utilise les noms des comptes et des tiers ; celui des lignes utilise
le chemin complet de catégorie, par exemple `Alimentation > Restaurants`.

## Scénarios indépendants

Repartir d'une démonstration neuve avant chaque ligne, sauf indication contraire.

| Test | Actions | Résultat attendu |
| --- | --- | --- |
| 1. Bilan de départ | Menu **3 — Bilan mensuel**, année 2026, mois 11. | Dépenses confirmées **2 109,00 DH**, revenus **8 500,00 DH**, résultat net **6 391,00 DH**. Courses : **745,00 DH**, plafond **700,00 DH**, dépassement **45,00 DH**. |
| 2. Import valide | Menu **13 — Fichiers**, puis **5 — Importer**. Saisir le chemin absolu du dossier `demo/import_exemple`. Choisir **2 — Exclure les opérations importées signalées**, puis `o`. | 4 opérations analysées ; la séance de cinéma du 12 novembre est un doublon. **3 opérations ajoutées** après exclusion. Dépenses de novembre : **2 284,00 DH**, revenus : **9 000,00 DH**. |
| 3. Annuler un import | Même import, puis choisir **0 — Annuler l'import** dans le choix des doublons. | Aucune opération ajoutée ; bilan initial inchangé. |
| 4. Doublons existants | Menu **6 — Doublons**, examiner le groupe avant toute suppression. | Les opérations **31 et 32**, « Médicaments », du **09/11/2026**, de **85,00 DH** chacune, sont signalées. |
| 5. Ajouter et corriger | Dans **Opérations**, ajouter une dépense « Test café » de **45 DH**, le **13/11/2026**, compte **Espèces (2)**, catégorie **Alimentation > Restaurants (3)**, sans tiers. Puis corriger cette même opération à **50 DH**, avec une ventilation totale de 50 DH. | Dépenses de novembre : **2 154,00 DH** après ajout, puis **2 159,00 DH** après correction. Une seule opération supplémentaire. |
| 6. Véhicule | Menu **5 — Véhicule**, puis **Coûts**, véhicule **1**, période du **01/11/2026 au 30/11/2026**. | Coût total attribué au véhicule : **640,00 DH**. |
| 7. Échéance | Menu **2 — Échéances à confirmer**. Confirmer uniquement le loyer **39**, en saisissant `=` pour reprendre le **01/11/2026** comme date réelle. | Loyer de **3 000,00 DH** confirmé ; dépenses de novembre **5 109,00 DH**. Les autres échéances en attente ne sont pas comptées. |
| 8. Catégorie inconnue | Copier les deux CSV d'import dans un nouveau dossier de test. Dans `import_lignes.csv`, remplacer `Alimentation > Restaurants` par `Categorie inexistante`. Importer ce dossier. | **Import refusé en entier**, erreur de catégorie avec fichier et ligne ; aucune des 4 opérations ajoutée. |
| 9. Ventilation incorrecte | Dans une autre copie des deux CSV, remplacer le montant `12000` de la ligne de référence 1 par `11000` dans `import_lignes.csv`, sans modifier le total de l'opération. | Import entièrement refusé : la ventilation ne correspond pas au total. Données inchangées. |
| 10. Date future | Dans une autre copie des deux CSV, changer la date de la première opération en **2026-11-14**, en gardant l'application au **2026-11-13**. | Import entièrement refusé pour date réelle future. Données inchangées. |
| 11. Solde insuffisant | Essayer d'ajouter une dépense de **999 999 DH**, le **13/11/2026**, compte **Espèces**, catégorie Restaurants. | Opération refusée pour solde négatif ; aucune dépense ajoutée. |
| 12. Enregistrer et rouvrir | Après le test 2, faire **Fichiers > Enregistrer**, puis quitter. Depuis la racine du projet, relancer avec la commande ci-dessous. | Les 3 opérations importées sont encore présentes. Ne pas utiliser `lancer_demo.bat` pour ce contrôle, car il réinitialise la copie. |

```powershell
.\lancer.bat --date 2026-11-13 --donnees demo\travail
```

Les résultats chiffrés supposent que chaque scénario commence avec le jeu initial,
sans confirmer ni supprimer d'autres opérations.

## Vérification réalisée

Correction vérifiée le 17 novembre 2026 : création des 3 comptes, 7 catégories et 2 tiers
dans une base vide isolée, puis import des deux CSV, enregistrement et consultation du bilan.
Sortie normale (code 0), **4 opérations ajoutées**, dépenses **285 DH**, revenus **500 DH**,
et soldes **770 / 945 / 500 DH**. Aucune donnée de la session utilisateur n'a été modifiée.

Le 17 novembre 2026, le parcours existant `tests/scenarios/demo.txt` a été exécuté
avec `bin/BudgetLens.exe`, à la date simulée du 13 novembre, sur une copie isolée.
Sortie normale (code 0) : ajout de 45 DH, bilan à 2 154 DH, dépassement Courses de 45 DH,
doublons, véhicule à 640 DH, confirmation/refus d'échéances, import de 3 opérations après
exclusion du doublon, export et enregistrement.
Les scénarios indépendants ci-dessus sont un guide de recette ; ils n'ont pas tous été
rejoués individuellement lors de cette préparation.
