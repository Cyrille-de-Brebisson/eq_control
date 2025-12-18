net session >nul 2>&1
if %errorLevel% == 0 (
    echo Success: Administrative permissions confirmed.
) else (
    echo Requesting administrative permissions...
    powershell -Command "Start-Process -FilePath '%0' -Verb RunAs"
    exit /b
)

copy /Y "%~dp0\ASCOM.BrebissonV1.exe" "C:\Program Files (x86)\Common Files\ASCOM"
copy /Y "%~dp0\iss.wav" "C:\Program Files (x86)\Common Files\ASCOM"
copy /Y "%~dp0\issposdll.dll" "C:\Program Files (x86)\Common Files\ASCOM"
copy /Y "%~dp0\power.wav" "C:\Program Files (x86)\Common Files\ASCOM"
cd /d "C:\Program Files (x86)\Common Files\ASCOM"
ASCOM.BrebissonV1.exe /register