@echo off
REM Source ESP-IDF environment and build
C:
cd \espidf\.espressif\v6.0.2\esp-idf
call export.bat > NUL 2>&1
cd /d D:\esp32c3-supermini-prj\c3_oled
idf.py build
echo Build exit code: %ERRORLEVEL%
pause
