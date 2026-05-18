@echo off
echo =======================================================
echo          Smart Medicine Dispenser System Startup
echo =======================================================
echo.

echo 1. Starting AI Camera Detection System...
start "Camera Detection System" cmd /k "python detect.py"
timeout /t 2 /nobreak >nul

echo 2. Starting Patient Chatbot AI...
start "Patient Telegram Bot" cmd /k "python patient_bot.py"

echo.
echo Both systems are now running in separate windows!
echo DO NOT close the new black windows if you want the system to keep running.
echo You can use your computer normally while they run in the background.
echo.
pause
