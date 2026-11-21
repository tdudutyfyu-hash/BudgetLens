@echo off
rem BudgetLens - (re)generation du jeu de demonstration fictif (date de reference 2026-11-13)
rem Produit demo\donnees_demo (donnees prêtes a l'emploi) et demo\jeu_demo (export restaurable).
setlocal
cd /d "%~dp0"
call "%~dp0build.bat" || exit /b 1
if exist demo\donnees_demo rmdir /s /q demo\donnees_demo
if exist demo\jeu_demo rmdir /s /q demo\jeu_demo
mkdir demo\donnees_demo
mkdir demo\jeu_demo
chcp 65001 >nul
bin\generer_demo.exe demo\donnees_demo demo\jeu_demo
exit /b %ERRORLEVEL%
