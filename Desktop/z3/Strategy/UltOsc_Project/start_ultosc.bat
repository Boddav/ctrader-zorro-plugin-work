@echo off
echo ========================================
echo  UltOsc ML Pipeline
echo ========================================
echo.

echo [1/3] Killing old Python server on port 5002...
for /f "tokens=5" %%a in ('netstat -aon ^| findstr :5002 ^| findstr LISTENING') do (
    taskkill /F /PID %%a 2>nul
)
timeout /t 2 /nobreak >nul

echo [2/3] Training models from CSV...
"C:\Users\Administrator\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0UltOsc_Server.py" train

echo [3/3] Starting UltOsc server on port 5002...
start "" /b "C:\Users\Administrator\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0UltOsc_Server.py" serve
timeout /t 3 /nobreak >nul

echo.
echo Done! UltOsc server running on localhost:5002
echo   POST /predict  -> 5 optimal params
echo   POST /filter   -> GO/SKIP
echo   GET  /health   -> status
echo.
pause
