# BudgetLens - recette automatique : scénarios console scriptés sur l'exécutable réel.
#
# Chaque scénario envoie un fichier d'entrées (tests\scenarios\*.txt) à bin\BudgetLens.exe
# avec une date simulée et un dossier de données isolé (tests\tmp\recette\...), puis contrôle
# le code de sortie et les textes présents ou absents. Les durées sont limitées (timeout).
# Les résultats attendus viennent des exigences, pas d'une sortie précédente du programme.
#
# Usage : powershell -File tests\recette.ps1   (après build.bat)
param([string]$Racine = (Split-Path -Parent $PSScriptRoot))

$ErrorActionPreference = 'Stop'
$exe = Join-Path $Racine 'bin\BudgetLens.exe'
$scenarios = Join-Path $Racine 'tests\scenarios'
$base = Join-Path $Racine 'tests\tmp\recette'
$DELAI_MS = 20000
$script:nbVerif = 0
$script:nbEchecs = 0

function Verifier([bool]$condition, [string]$message) {
    $script:nbVerif++
    if (-not $condition) {
        $script:nbEchecs++
        Write-Host "  ECHEC : $message"
    }
}

function Nouveau-Dossier([string]$nom) {
    $d = Join-Path $base $nom
    if (Test-Path $d) { Remove-Item -Recurse -Force $d }
    New-Item -ItemType Directory -Force $d | Out-Null
    return $d
}

# Lance le programme avec les octets d'entrée donnés (redirection de fichiers, exacte à l'octet) ;
# renvoie code de sortie, sortie (UTF-8) et indicateur de délai dépassé.
function Lancer([byte[]]$entree, [string]$dossier, [string]$date) {
    return Lancer-Brut $entree "--date $date --donnees `"$dossier`"" $dossier
}

# Variante où les arguments sont donnés tels quels (tests des codes de sortie de démarrage).
function Lancer-Brut([byte[]]$entree, [string]$arguments, [string]$dossier) {
    $fin = Join-Path $base 'entree.tmp'
    $fout = Join-Path $base 'sortie.tmp'
    $ferr = Join-Path $base 'erreur.tmp'
    [System.IO.File]::WriteAllBytes($fin, $entree)
    $p = Start-Process -FilePath $exe -ArgumentList $arguments -RedirectStandardInput $fin `
         -RedirectStandardOutput $fout -RedirectStandardError $ferr -NoNewWindow -PassThru `
         -WorkingDirectory $dossier
    $null = $p.Handle      # nécessaire pour lire ExitCode avec PowerShell 5.1
    $delai = $false
    if (-not $p.WaitForExit($DELAI_MS)) {
        $delai = $true
        try { $p.Kill() } catch { }
        $p.WaitForExit()
    }
    $texte = [System.IO.File]::ReadAllText($fout, [System.Text.Encoding]::UTF8) +
             [System.IO.File]::ReadAllText($ferr, [System.Text.Encoding]::UTF8)
    return [pscustomobject]@{ Code = $p.ExitCode; Sortie = $texte; Delai = $delai }
}

function Entree-Fichier([string]$nom) {
    return [System.IO.File]::ReadAllBytes((Join-Path $scenarios $nom))
}

function Controler($res, [int]$codeAttendu, [string[]]$presents, [string[]]$absents, [string]$nom) {
    Verifier (-not $res.Delai) "$nom : délai dépassé (boucle ou attente d'entrée)"
    Verifier ($res.Code -eq $codeAttendu) "$nom : code de sortie $($res.Code) au lieu de $codeAttendu"
    foreach ($t in $presents) {
        Verifier ($res.Sortie.Contains($t)) "$nom : texte attendu absent : « $t »"
    }
    foreach ($t in $absents) {
        Verifier (-not $res.Sortie.Contains($t)) "$nom : texte interdit présent : « $t »"
    }
}

if (-not (Test-Path $exe)) { Write-Host "bin\BudgetLens.exe introuvable : lancez build.bat"; exit 2 }
New-Item -ItemType Directory -Force $base | Out-Null

# ---------------------------------------------------------------------------------------
Write-Host "Scénario étape 2 : comptes, catégories, opérations, G10, seuil, B3, recherche, statistiques"
$d = Nouveau-Dossier 'etape2'
$res = Lancer (Entree-Fichier 'etape2.txt') $d '2026-11-13'
Controler $res 0 @(
    'Compte créé (identifiant 1).',
    'Compte créé (identifiant 2).',
    'Catégorie créée : [2] Alimentation > Courses',
    'Opération créée et confirmée (identifiant 1).',
    # G10 : 300 DH sur un compte de 200 DH
    "le solde de fin de journée serait négatif le 2026-11-11",
    'il manque 100,00 DH',
    # seuil : avertissement puis accord (identifiant 2), puis refus de l'utilisateur
    'sous le seuil minimal de 50,00 DH',
    'Opération créée et confirmée (identifiant 2).',
    "Aucune modification n'a été faite.",
    # B3 : date future
    'postérieure à la date de référence',
    'Opération créée et confirmée (identifiant 3).',
    'Opération créée et confirmée (identifiant 4).',
    # recherche avec catégorie : montant attribuable de l'opération ventilée 150 + 50 = 200
    'Attribuable',
    'Courses Marjane',
    "Opération 1 corrigée.",
    # statistiques de novembre : 200 + 160 DH
    'Nombre de dépenses : 2',
    'Total              : 360,00 DH',
    'Moyenne            : 180,00 DH',
    'Minimum            : 160,00 DH',
    'Maximum            : 200,00 DH',
    'Opération 2 supprimée.',
    'Données enregistrées dans'
) @(
    'Encore un achat',
    'identifiant 5'
) 'etape2'
Verifier (Test-Path (Join-Path $d 'A_manifest.csv')) "etape2 : l'enregistrement final n'a pas produit A_manifest.csv"
Verifier (-not (Test-Path (Join-Path $d 'budgetlens.verrou'))) "etape2 : le verrou n'a pas été libéré"

Write-Host "Scénario étape 2 (relecture après redémarrage) : persistance des opérations et des soldes"
$res = Lancer (Entree-Fichier 'etape2_relecture.txt') $d '2026-11-13'
Controler $res 0 @(
    'Courses Marjane',
    'Salaire novembre',
    'Retrait',
    '8 300,00 DH',      # Banque : 5 000 - 200 + 4 000 - 500
    '700,00 DH'         # Portefeuille : 200 + 500 (l'achat de 160 DH a été supprimé)
) @('Gros achat', 'modifications non enregistrées') 'etape2_relecture'

# ---------------------------------------------------------------------------------------
Write-Host "Fin d'entrée (H10) : le scénario étape 2 tronqué après chaque ligne"
$lignes = [System.IO.File]::ReadAllLines((Join-Path $scenarios 'etape2.txt'))
$n = $lignes.Length
$anomalies = 0
for ($k = 0; $k -lt $n; $k++) {
    $dossier = Nouveau-Dossier 'eof'
    $contenu = ''
    if ($k -gt 0) { $contenu = (($lignes[0..($k - 1)]) -join "`n") + "`n" }
    $octets = [System.Text.Encoding]::UTF8.GetBytes($contenu)
    $res = Lancer $octets $dossier '2026-11-13'
    $ok = (-not $res.Delai) -and ($res.Code -eq 0 -or $res.Code -eq 3) `
          -and $res.Sortie.Contains("Fin de l'entrée standard") `
          -and (-not (Test-Path (Join-Path $dossier 'A_manifest.csv'))) `
          -and (-not (Test-Path (Join-Path $dossier 'budgetlens.verrou')))
    if ($res.Code -eq 3) { $ok = $ok -and $res.Sortie.Contains('modification(s) non enregistrée(s) abandonnée(s)') }
    if (-not $ok) {
        $anomalies++
        Write-Host "  ECHEC : troncature après $k ligne(s) : code=$($res.Code) délai=$($res.Delai)"
    }
}
Verifier ($anomalies -eq 0) "fin d'entrée : $anomalies troncature(s) anormale(s) sur $n"
$script:nbVerif += ($n - 1)

# ---------------------------------------------------------------------------------------
Write-Host "Scénario étape 3 : doublons I6, bilan, véhicule, migration I3, déplacement, réaffectation 1A"
function Copier-Donnees([string]$source, [string]$nom) {
    $cible = Nouveau-Dossier $nom
    Get-ChildItem $source -File | Where-Object { $_.Name -ne 'budgetlens.verrou' } |
        ForEach-Object { Copy-Item $_.FullName $cible }
    return $cible
}
$d3 = Copier-Donnees $d 'etape3'
$res = Lancer (Entree-Fichier 'etape3.txt') $d3 '2026-11-13'
Controler $res 0 @(
    'Groupe 1/1 (3 opérations identiques)',
    'proposée à conserver',
    '2 opération(s) du groupe traitée(s).',
    # bilan de novembre : Alimentation 150 + 50 = 200 DH, revenus 4 000 DH, net 3 800 DH
    'Bilan de 2026-11',
    'Dépenses confirmées : 200,00 DH',
    'budget non défini',
    'Revenus confirmés   : 4 000,00 DH',
    'Résultat net (revenus - dépenses) : 3 800,00 DH',
    'Véhicule créé.',
    'Véhicule « Clio »',
    'Total : 60,00 DH',
    'Migration vers « Autres » (I3)',
    'Catégorie créée : [6] Alimentation > Courses > Bio',
    'Catégorie déplacée.',
    'Catégorie réaffectée.',
    'Catégorie supprimée.',
    'virement entre ces deux comptes',
    'Opération créée et confirmée (identifiant 7).',
    'Données enregistrées dans'
) @('identifiant 8') 'etape3'

Write-Host "Scénario étape 3 (relecture) : arbre des catégories après migration, déplacement et suppression"
$res = Lancer (Entree-Fichier 'etape3_relecture.txt') $d3 '2026-11-13'
Controler $res 0 @('Bio', 'Hygiène', 'Courses', 'Banque Populaire') @('Autres', 'modifications non enregistrées') 'etape3_relecture'

Write-Host "Fin d'entrée (H10) : le scénario étape 3 tronqué après chaque ligne"
$lignes3 = [System.IO.File]::ReadAllLines((Join-Path $scenarios 'etape3.txt'))
$n3 = $lignes3.Length
$anomalies3 = 0
for ($k = 0; $k -lt $n3; $k++) {
    $dossier = Copier-Donnees $d 'eof3'
    $contenu = ''
    if ($k -gt 0) { $contenu = (($lignes3[0..($k - 1)]) -join "`n") + "`n" }
    $res = Lancer ([System.Text.Encoding]::UTF8.GetBytes($contenu)) $dossier '2026-11-13'
    $ok = (-not $res.Delai) -and ($res.Code -eq 0 -or $res.Code -eq 3) `
          -and $res.Sortie.Contains("Fin de l'entrée standard") `
          -and (-not (Test-Path (Join-Path $dossier 'B_manifest.csv'))) `
          -and (-not (Test-Path (Join-Path $dossier 'budgetlens.verrou')))
    if (-not $ok) {
        $anomalies3++
        Write-Host "  ECHEC : troncature après $k ligne(s) : code=$($res.Code) délai=$($res.Delai)"
    }
}
Verifier ($anomalies3 -eq 0) "fin d'entrée (étape 3) : $anomalies3 troncature(s) anormale(s) sur $n3"
$script:nbVerif += ($n3 - 1)

# ---------------------------------------------------------------------------------------
Write-Host "Scénario étape 4 : modèle, récurrences, échéances, confirmation, lot H5, refus, annulation"
$d4 = Copier-Donnees $d3 'etape4'
$res = Lancer (Entree-Fichier 'etape4.txt') $d4 '2026-11-13'
Controler $res 0 @(
    'Modèle créé (identifiant 1).',
    '3 échéance(s) déjà due(s) (du 2026-10-26 au 2026-11-09).',
    'Récurrence créée (identifiant 1), 3 occurrence(s) générée(s).',
    'Récurrence créée (identifiant 2), 3 occurrence(s) générée(s).',
    'Récurrence mise en pause.',
    'Échéance confirmée.',
    'Opération 13 corrigée.',
    'Le montant change et la ventilation compte plusieurs lignes',
    'Lot confirmé.',
    'Échéance refusée (statut Annulée).',
    'Lot refusé.',
    'Occurrence annulée.',
    'Données enregistrées dans'
) @('Action refusée') 'etape4'

Write-Host "Scénario étape 4 (suite, deux semaines plus tard) : rattrapage au démarrage, réactivation, clôture G6"
$res = Lancer (Entree-Fichier 'etape4_suite.txt') $d4 '2026-12-14'
Controler $res 0 @(
    # curseur du 16/11 au 14/12 inclus (jour de référence dû) : 16/11, 23/11, 30/11, 7/12, 14/12
    '5 échéance(s) générée(s) au démarrage',
    'en pause',
    'Récurrence réactivée',
    '2027-01-05',
    'Conflit : l''occurrence',
    'Récurrence clôturée.',
    'terminée',
    '5 échéance(s) à confirmer',
    'Données enregistrées dans'
) @() 'etape4_suite'

Write-Host "Relecture : soldes après confirmations (8 240 - 300 - 350 DH) et absence de régénération"
$res = Lancer (Entree-Fichier 'etape2_relecture.txt') $d4 '2026-12-14'
Controler $res 0 @('7 590,00 DH') @('échéance(s) générée(s) au démarrage') 'etape4_relecture'

Write-Host "Fin d'entrée (H10) : les scénarios étape 4 tronqués après chaque ligne"
foreach ($nomScenario in @('etape4.txt', 'etape4_suite.txt')) {
    $lignes4 = [System.IO.File]::ReadAllLines((Join-Path $scenarios $nomScenario))
    $n4 = $lignes4.Length
    $anomalies4 = 0
    $dateEof = '2026-11-13'
    $source = $d3
    if ($nomScenario -eq 'etape4_suite.txt') { $dateEof = '2026-12-14'; $source = $d4 }
    for ($k = 0; $k -lt $n4; $k++) {
        $dossier = Copier-Donnees $source 'eof4'
        $contenu = ''
        if ($k -gt 0) { $contenu = (($lignes4[0..($k - 1)]) -join "`n") + "`n" }
        $res = Lancer ([System.Text.Encoding]::UTF8.GetBytes($contenu)) $dossier $dateEof
        $ok = (-not $res.Delai) -and ($res.Code -eq 0 -or $res.Code -eq 3) `
              -and $res.Sortie.Contains("Fin de l'entrée standard") `
              -and (-not (Test-Path (Join-Path $dossier 'budgetlens.verrou')))
        if (-not $ok) {
            $anomalies4++
            Write-Host "  ECHEC : $nomScenario tronqué après $k ligne(s) : code=$($res.Code) délai=$($res.Delai)"
        }
    }
    Verifier ($anomalies4 -eq 0) "fin d'entrée ($nomScenario) : $anomalies4 troncature(s) anormale(s) sur $n4"
    $script:nbVerif += ($n4 - 1)
}

# ---------------------------------------------------------------------------------------
Write-Host "Scénario étape 5 : export, sauvegarde, import refusé puis valide (doublon exclu), enregistrement"
function Copier-Arbre([string]$source, [string]$nom) {
    $cible = Nouveau-Dossier $nom
    Get-ChildItem $source | Where-Object { $_.Name -ne 'budgetlens.verrou' } |
        ForEach-Object { Copy-Item $_.FullName $cible -Recurse }
    return $cible
}
function Preparer-Etape5([string]$source, [string]$nom) {
    $cible = Copier-Arbre $source $nom
    New-Item -ItemType Directory -Force (Join-Path $cible 'export') | Out-Null
    foreach ($sous in @('import_ko', 'import_ok')) {
        $dest = Join-Path $cible $sous
        if (-not (Test-Path $dest)) { Copy-Item (Join-Path $scenarios $sous) $dest -Recurse }
    }
    return $cible
}
$d5 = Preparer-Etape5 $d4 'etape5'
$res = Lancer (Entree-Fichier 'etape5.txt') $d5 '2026-12-14'
Controler $res 0 @(
    'Export terminé dans « export »',
    'Sauvegarde créée dans',
    'Import REFUSÉ en entier',
    '3 erreur(s)',
    'import_lignes.csv, ligne 2',
    'catégorie « Alimentation > Inconnue » introuvable',
    'import_operations.csv, ligne 3 : compte source « Fantôme » introuvable',
    'import_operations.csv, ligne 4 : la date 2027-06-01 est postérieure à la date de référence',
    'Import valide : 3 opération(s)',
    'Doublons potentiels',
    'déjà présente',
    'importée',
    '2 opération(s) seront ajoutées',
    'Import terminé : 2 opération(s) ajoutée(s).',
    'Données enregistrées dans'
) @('Action refusée') 'etape5'
Verifier (Test-Path (Join-Path $d5 'export\manifest.csv')) "etape5 : export\manifest.csv absent"
Verifier (Test-Path (Join-Path $d5 'index.csv')) "etape5 : index.csv absent"

Write-Host "Scénario étape 5 (restauration d'une sauvegarde puis d'un export, quitter sans enregistrer)"
$res = Lancer (Entree-Fichier 'etape5_restauration.txt') $d5 '2026-12-14'
Controler $res 0 @(
    'Sauvegardes disponibles',
    'Source valide',
    'Restauration effectuée EN MÉMOIRE seulement',
    'ATTENTION : 1 modification(s) non enregistrée(s) seront perdue(s).',
    'Modifications abandonnées'
) @('Restauration impossible') 'etape5_restauration'

Write-Host "Relecture : la restauration non enregistrée n'a pas modifié l'état enregistré (import conservé)"
$res = Lancer (Entree-Fichier 'etape2_relecture.txt') $d5 '2026-12-14'
Controler $res 0 @('Prime import', 'Cinéma') @('modifications non enregistrées') 'etape5_relecture'

Write-Host "Fin d'entrée (H10) : les scénarios étape 5 tronqués après chaque ligne"
foreach ($nomScenario in @('etape5.txt', 'etape5_restauration.txt')) {
    $lignes5 = [System.IO.File]::ReadAllLines((Join-Path $scenarios $nomScenario))
    $n5 = $lignes5.Length
    $anomalies5 = 0
    for ($k = 0; $k -lt $n5; $k++) {
        $dossier = $null
        if ($nomScenario -eq 'etape5.txt') { $dossier = Preparer-Etape5 $d4 'eof5' } else { $dossier = Copier-Arbre $d5 'eof5' }
        $contenu = ''
        if ($k -gt 0) { $contenu = (($lignes5[0..($k - 1)]) -join "`n") + "`n" }
        $res = Lancer ([System.Text.Encoding]::UTF8.GetBytes($contenu)) $dossier '2026-12-14'
        $ok = (-not $res.Delai) -and ($res.Code -eq 0 -or $res.Code -eq 3) `
              -and $res.Sortie.Contains("Fin de l'entrée standard") `
              -and (-not (Test-Path (Join-Path $dossier 'budgetlens.verrou')))
        if (-not $ok) {
            $anomalies5++
            Write-Host "  ECHEC : $nomScenario tronqué après $k ligne(s) : code=$($res.Code) délai=$($res.Delai)"
        }
    }
    Verifier ($anomalies5 -eq 0) "fin d'entrée ($nomScenario) : $anomalies5 troncature(s) anormale(s) sur $n5"
    $script:nbVerif += ($n5 - 1)
}

# ---------------------------------------------------------------------------------------
Write-Host "Démarrage : arguments invalides, verrou, dossier absent, données abîmées, repli signalé"
$quitter = [System.Text.Encoding]::UTF8.GetBytes("0`no`n")
$res = Lancer-Brut $quitter '--date 2026-13-45' $d5
Controler $res 2 @('Date simulée invalide') @() 'demarrage_date_invalide'
$res = Lancer-Brut $quitter '--inconnu' $d5
Controler $res 2 @('Usage') @() 'demarrage_argument_inconnu'
$res = Lancer-Brut $quitter "--date 2026-12-14 --donnees `"$base\dossier_absent`"" $d5
Controler $res 7 @('Impossible de créer le verrou') @() 'demarrage_dossier_absent'

# Verrou déjà présent : refus sans contournement, aucun fichier de données modifié.
$dv = Copier-Arbre $d5 'verrou'
Set-Content -Path (Join-Path $dv 'budgetlens.verrou') -Value 'x'
$avant = (Get-FileHash (Join-Path $dv 'A_manifest.csv')).Hash + (Get-FileHash (Join-Path $dv 'B_manifest.csv')).Hash
$res = Lancer-Brut ([System.Text.Encoding]::UTF8.GetBytes("o`no`no`n")) "--date 2026-12-14 --donnees `"$dv`"" $dv
Controler $res 6 @('déjà utilisé', 'supprimez ce fichier à la main') @('Menu principal') 'demarrage_verrou'
$apres = (Get-FileHash (Join-Path $dv 'A_manifest.csv')).Hash + (Get-FileHash (Join-Path $dv 'B_manifest.csv')).Hash
Verifier ($avant -eq $apres) "verrou : les fichiers de données ont été modifiés"
Verifier (Test-Path (Join-Path $dv 'budgetlens.verrou')) "verrou : le verrou d'une autre instance a été supprimé"

# Altère un chiffre d'un fichier de données (même taille) sans toucher au manifeste.
function Alterer-Chiffre([string]$chemin) {
    $octets = [System.IO.File]::ReadAllBytes($chemin)
    $i = [Array]::IndexOf($octets, [byte]10) + 1
    while ($i -lt $octets.Length -and -not ($octets[$i] -ge 48 -and $octets[$i] -le 56)) { $i++ }
    $octets[$i] = $octets[$i] + 1
    [System.IO.File]::WriteAllBytes($chemin, $octets)
}
function Generation([string]$chemin) {
    $ligne = Get-Content $chemin | Where-Object { $_ -like 'GENERATION;*' } | Select-Object -First 1
    return [int]($ligne.Split(';')[1])
}

# Emplacement le plus récent abîmé : repli signalé sur l'emplacement précédent (code 0).
$dr = Copier-Arbre $d5 'repli'
$ga = Generation (Join-Path $dr 'A_manifest.csv')
$gb = Generation (Join-Path $dr 'B_manifest.csv')
$recent = 'A_'
if ($gb -gt $ga) { $recent = 'B_' }
Alterer-Chiffre (Join-Path $dr ($recent + 'comptes.csv'))
$res = Lancer-Brut $quitter "--date 2026-12-14 --donnees `"$dr`"" $dr
Controler $res 0 @('ATTENTION : l''enregistrement le plus récent est invalide', 'somme de contrôle incorrecte') @() 'demarrage_repli'

# Les deux emplacements abîmés : arrêt sans démarrage vide ni écrasement (code 5).
$dc = Copier-Arbre $d5 'corrompu'
Alterer-Chiffre (Join-Path $dc 'A_comptes.csv')
Alterer-Chiffre (Join-Path $dc 'B_comptes.csv')
$avantA = (Get-FileHash (Join-Path $dc 'A_comptes.csv')).Hash + (Get-FileHash (Join-Path $dc 'B_comptes.csv')).Hash
$res = Lancer-Brut ([System.Text.Encoding]::UTF8.GetBytes("1`n13`n1`n0`n")) "--date 2026-12-14 --donnees `"$dc`"" $dc
Controler $res 5 @('aucun enregistrement valide', 'BudgetLens s''arrête sans rien modifier') @('Menu principal') 'demarrage_corrompu'
$apresA = (Get-FileHash (Join-Path $dc 'A_comptes.csv')).Hash + (Get-FileHash (Join-Path $dc 'B_comptes.csv')).Hash
Verifier ($avantA -eq $apresA) "données abîmées : les fichiers ont été modifiés"
Verifier (-not (Test-Path (Join-Path $dc 'budgetlens.verrou'))) "données abîmées : le verrou n'a pas été libéré"

# ---------------------------------------------------------------------------------------
Write-Host "Démonstration : jeu fictif (date 2026-11-13) — ajout, recherche, bilan, statistiques, doublons, véhicule, lots, import, export"
$demoSource = Join-Path $Racine 'demo\donnees_demo'
if (Test-Path (Join-Path $demoSource 'A_manifest.csv')) {
    $dd = Nouveau-Dossier 'demo'
    Get-ChildItem $demoSource -File | ForEach-Object { Copy-Item $_.FullName $dd }
    Copy-Item (Join-Path $Racine 'demo\import_exemple') (Join-Path $dd 'import_exemple') -Recurse
    New-Item -ItemType Directory -Force (Join-Path $dd 'export_demo') | Out-Null
    $res = Lancer (Entree-Fichier 'demo.txt') $dd '2026-11-13'
    Controler $res 0 @(
        'Opération créée et confirmée (identifiant 43).',
        '3 opération(s).',
        # bilan de novembre : 2 109 DH + le café de 45 DH
        'Dépenses confirmées : 2 154,00 DH',
        'Restant             : 3 846,00 DH',
        'plafond 700,00 DH | DÉPASSEMENT 45,00 DH',
        'plafond 800,00 DH | reste 160,00 DH',
        'Revenus confirmés   : 8 500,00 DH',
        'Résultat net (revenus - dépenses) : 6 346,00 DH',
        'Nombre de dépenses : 10',
        'Total              : 2 154,00 DH',
        'Moyenne            : 215,40 DH',
        'Minimum            : 45,00 DH',
        'Maximum            : 420,00 DH',
        'Groupe 1/1 (2 opérations identiques)',
        '1 opération(s) du groupe traitée(s).',
        'Total : 640,00 DH',
        '4 échéance(s) à confirmer',
        'Lot confirmé.',
        'Échéance refusée (statut Annulée).',
        'Lot refusé.',
        'Import valide : 4 opération(s)',
        'Doublons potentiels',
        'Import terminé : 3 opération(s) ajoutée(s).',
        'Export terminé dans « export_demo »',
        'Données enregistrées dans'
    ) @('Action refusée', 'Import REFUSÉ') 'demo'
    Verifier (Test-Path (Join-Path $dd 'export_demo\manifest.csv')) "demo : export_demo\manifest.csv absent"
} else {
    Write-Host "  (jeu de démonstration absent : lancez generer_demo.bat)"
}

# ---------------------------------------------------------------------------------------
Write-Host "Ergonomie : menu regroupé, dernières opérations, catégorie par nom, récapitulatif d'ajout"
$dv = Nouveau-Dossier 'ergonomie_vide'
$res = Lancer (Entree-Fichier 'ergonomie_vide.txt') $dv '2026-11-13'
Controler $res 0 @(
    'Au quotidien', 'Analyses', 'Gestion',
    '   2. Échéances à confirmer (aucune)',
    '  14. Dernières opérations',
    "Aucune opération pour l'instant.",
    'Pour ajouter la première : 1. Opérations, puis 3. Ajouter une dépense'
) @('Id    Date') 'ergonomie (liste vide)'
$lignesMenu = ($res.Sortie -split "`r?`n") | Where-Object { $_ -match '^ *[0-9]+\. ' -or $_ -match '^(Au quotidien|Analyses|Gestion)$' }
Verifier (@($lignesMenu | Where-Object { $_.Length -gt 80 }).Count -eq 0) "ergonomie : ligne de menu de plus de 80 colonnes"

if (Test-Path (Join-Path $demoSource 'A_manifest.csv')) {
    $de = Nouveau-Dossier 'ergonomie'
    Get-ChildItem $demoSource -File | ForEach-Object { Copy-Item $_.FullName $de }
    $res = Lancer (Entree-Fichier 'ergonomie.txt') $de '2026-11-13'
    $res.Sortie = $res.Sortie -replace "`r`n", "`n"     # contrôles sur plusieurs lignes
    $s = $res.Sortie
    Controler $res 0 @(
        # menu : compteur = nombre d'échéances listées par le menu 2 ; date simulée conservée
        '   2. Échéances à confirmer (4 à traiter)',
        'BudgetLens | Date de référence : 2026-11-13 (SIMULÉE)',
        # dernières opérations : dix lignes, échéance marquée *, même date -> identifiant croissant
        '10 plus récente(s) sur 42 opération(s)',
        "35    2026-11-12  Dépense  Séance de cinéma",
        "42    2026-11-09* Dépense  Abonnement salle de sp… 150,00 DH      À confirmer",
        'Opération 42 : Abonnement salle de sport',
        'Récurrence    : 2 (échéance prévue le 2026-11-09)',
        'Opération 999 introuvable.',
        # catégorie : aucun résultat, nom unique, ambigu (candidats seuls), identifiant, parent refusé
        'Aucune catégorie de dépense terminale ne correspond à « zzz »',
        'Catégorie retenue : Alimentation > Courses',
        '3 catégories correspondent à « transport » :',
        "L'identifiant 99 ne fait pas partie des catégories proposées.",
        'Catégorie retenue : Transport > Transports en commun',
        'Catégorie retenue : Santé > Pharmacie',
        'Cette catégorie a des sous-catégories : choisissez une catégorie terminale.',
        'Aucune catégorie de revenu terminale ne correspond à « courses »',
        'Catégorie retenue : Salaire',
        # récapitulatifs
        "Dépense ajoutée · 85,00 DH · Alimentation > Courses`nOpération créée et confirmée (identifiant 43).`nModifications en mémoire. Pour enregistrer, ouvrez 13. Fichiers.",
        'Dépense ajoutée · 100,00 DH · ventilée sur 3 catégories (3 lignes)',
        'Opération créée et confirmée (identifiant 44).',
        'Saisie abandonnée, aucune donnée modifiée.',
        'Dépense ajoutée · 2 900,00 DH · Alimentation > Restaurants',
        'Opération créée et confirmée (identifiant 45).',
        'Revenu ajouté · 200,00 DH · Salaire',
        'Opération créée et confirmée (identifiant 46).',
        # la liste récente montre les ajouts (même date : 43, 44, 45, 46)
        '10 plus récente(s) sur 46 opération(s)',
        "43    2026-11-13  Dépense  Courses du jour         85,00 DH       Confirmée`n44    2026-11-13  Dépense  Ventilée test",
        'Données enregistrées dans'
    ) @('identifiant 47', 'Action refusée') 'ergonomie'
    # « Catégorie de dépense » : les candidats ambigus sont seulement les trois feuilles de Transport.
    $amb = $s.IndexOf('3 catégories correspondent à « transport » :')
    Verifier ($amb -ge 0 -and $s.Substring($amb).StartsWith("3 catégories correspondent à « transport » :`n  [5] Transport > Carburant`n  [7] Transport > Entretien véhicule`n  [6] Transport > Transports en commun`nIdentifiant")) "ergonomie : candidats ambigus inattendus"
    # Seuil : refus puis accord ; le récapitulatif n'apparaît qu'une fois, après l'accord.
    $refus = $s.IndexOf("Aucune modification n'a été faite.")
    $recap = $s.IndexOf('Dépense ajoutée · 2 900,00 DH')
    Verifier ($refus -ge 0 -and $recap -gt $refus -and $s.LastIndexOf('Dépense ajoutée · 2 900,00 DH') -eq $recap) "ergonomie : récapitulatif affiché avant la validation complète"
    # Après l'enregistrement, plus aucun bandeau ne signale de modification.
    $enr = $s.IndexOf('Données enregistrées dans')
    Verifier ($enr -gt 0 -and $s.Substring(0, $enr).Contains('* modifications non enregistrées') -and -not $s.Substring($enr).Contains('* modifications non enregistrées')) "ergonomie : indicateur de modifications après enregistrement"
    # Relecture après redémarrage : les ajouts sont enregistrés.
    $res = Lancer (Entree-Fichier 'ergonomie_relecture.txt') $de '2026-11-13'
    Controler $res 0 @(
        '10 plus récente(s) sur 46 opération(s)',
        '46    2026-11-13  Revenu   Prime'
    ) @('* modifications non enregistrées') 'ergonomie (relecture)'
}

# ---------------------------------------------------------------------------------------
Write-Host ""
Write-Host "Recette : $($script:nbVerif) vérification(s), $($script:nbEchecs) échec(s)"
if ($script:nbEchecs -eq 0) { exit 0 } else { exit 1 }
