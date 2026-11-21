@echo off
rem BudgetLens - compilation, tests unitaires puis recette (scenarios console scriptes)
setlocal
cd /d "%~dp0"
call "%~dp0build.bat" || exit /b 1
if not exist tests\tmp mkdir tests\tmp
chcp 65001 >nul
echo.
echo === Tests unitaires ===
bin\tests_unitaires.exe tests\tmp || exit /b 1
echo.
echo === Recette (scenarios sur l'executable) ===
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tests\recette.ps1" || exit /b 1
exit /b 0
