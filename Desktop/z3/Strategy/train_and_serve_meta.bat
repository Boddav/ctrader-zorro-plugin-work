@echo off
echo ========================================
echo  META-LEARNING PIPELINE
echo ========================================
echo.
echo Step 1: Train XGBoost model from MLTrainingData.csv
echo Step 2: Start HTTP server on port 5001
echo.
echo Make sure you ran MLDATACOLLECTION.c [Train] then [Test] first!
echo.

cd /d "%~dp0"

python TENSORFLOWMODEL.py

pause
