@echo off
rem BudgetLens - compilation (GCC MSYS2 UCRT64, ISO C23 uniquement)
setlocal
cd /d "%~dp0"
if exist C:\msys64\ucrt64\bin\gcc.exe set "PATH=C:\msys64\ucrt64\bin;%PATH%"
set CFLAGS=-std=c23 -O2 -Wall -Wextra -Wpedantic
set MODULES=src\analyses.c src\app.c src\calendrier.c src\csv.c src\import.c src\index_id.c src\modeles.c src\operations.c src\persistance.c src\recurrences.c src\referentiel.c src\regles.c src\sauvegardes.c src\soldes.c src\somme.c src\tableau.c src\texte.c src\valeurs.c
if not exist bin mkdir bin
echo [1/3] Compilation C23 de BudgetLens...
gcc %CFLAGS% -o bin\BudgetLens.exe src\*.c || goto :echec
echo [2/3] Compilation des tests unitaires...
gcc %CFLAGS% -Isrc -o bin\tests_unitaires.exe tests\*.c %MODULES% || goto :echec
echo [3/3] Compilation du generateur de la demonstration...
gcc %CFLAGS% -Isrc -o bin\generer_demo.exe outils\generer_demo.c %MODULES% || goto :echec
echo Compilation reussie : bin\BudgetLens.exe, bin\tests_unitaires.exe et bin\generer_demo.exe
exit /b 0
:echec
echo ECHEC de la compilation.
exit /b 1
