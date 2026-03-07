@echo off
echo === ML-DRIVEN Server: LightGBM (CAGR 21.44%%, profit) ===
echo.

:: Kill old python servers
taskkill /F /IM python.exe 2>nul
timeout /t 2 /nobreak >nul

cd /d "C:\Users\Administrator\Desktop\z3\Strategy"

echo Starting LightGBM server on port 5001...
python ml_server_experiment.py serve lgbm

pause
