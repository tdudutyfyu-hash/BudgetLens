@echo off
rem BudgetLens - lancement sur le jeu de demonstration (date simulee 2026-11-13)
rem Les donnees de demo\donnees_demo sont copiees dans demo\travail : le jeu d'origine n'est jamais modifie.
setlocal
cd /d "%~dp0"
chcp 65001 >nul
if not exist bin\BudgetLens.exe call "%~dp0build.bat" || exit /b 1
if not exist demo\donnees_demo\A_manifest.csv call "%~dp0generer_demo.bat" || exit /b 1
if exist demo\travail rmdir /s /q demo\travail
mkdir demo\travail
xcopy /q /y demo\donnees_demo\*.csv demo\travail\ >nul
xcopy /q /y demo\import_exemple demo\travail\import_exemple\ /e >nul
bin\BudgetLens.exe --date 2026-11-13 --donnees demo\travail
set CODE=%ERRORLEVEL%
echo.
echo BudgetLens termine (code %CODE%).
exit /b %CODE%
