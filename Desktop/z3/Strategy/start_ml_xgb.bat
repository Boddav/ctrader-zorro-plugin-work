@echo off
echo === ML-DRIVEN Server: XGBoost (PF 1.59, stabil) ===
echo.

:: Kill old python servers
taskkill /F /IM python.exe 2>nul
timeout /t 2 /nobreak >nul

cd /d "C:\Users\Administrator\Desktop\z3\Strategy"

echo Starting XGBoost server on port 5001...
python TENSORFLOWMODEL.py serve

pause
