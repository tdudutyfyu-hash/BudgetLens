@echo off
rem BudgetLens - lancement avec console UTF-8
rem Arguments transmis : [--date AAAA-MM-JJ] [--donnees DOSSIER]
setlocal
cd /d "%~dp0"
chcp 65001 >nul
if not exist data mkdir data
if not exist backups mkdir backups
if not exist bin\BudgetLens.exe call "%~dp0build.bat" || exit /b 1
bin\BudgetLens.exe %*
set CODE=%ERRORLEVEL%
echo.
echo BudgetLens termine (code %CODE%).
exit /b %CODE%
