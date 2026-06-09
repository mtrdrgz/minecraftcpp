@echo off
REM Quick profiling helper script
REM Usage: run_profile.bat [analyze|visualize|both|view]

setlocal enabledelayedexpansion

if "%1"=="" (
    echo.
    echo Minecraft C++ Profiling Helper
    echo ==============================
    echo.
    echo Usage: run_profile.bat [command]
    echo.
    echo Commands:
    echo   run              - Run the game with profiling enabled (default)
    echo   analyze          - Analyze latest profile
    echo   visualize        - Generate visualizations for latest profile
    echo   both             - Analyze and visualize
    echo   view             - Open latest profile CSV in notepad
    echo   list             - List all available profiles
    echo.
    cd mcpp
    call build\mcpp.exe --quickPlaySingleplayer
    goto end
)

if /i "%1"=="run" (
    cd mcpp
    call build\mcpp.exe --quickPlaySingleplayer
    goto end
)

if /i "%1"=="list" (
    echo Available profiles:
    dir /b profiling\profiles\profile_*.csv 2>nul || echo No profiles found
    goto end
)

REM Find latest profile
for /f "tokens=*" %%A in ('dir /b /o-d profiling\profiles\profile_*.csv 2^>nul') do (
    set LATEST=%%A
    goto found
)
echo Error: No profile files found in profiling/profiles/
goto end

:found
set PROFILE=profiling\profiles\!LATEST!

if /i "%1"=="view" (
    start notepad !PROFILE!
    goto end
)

if /i "%1"=="analyze" (
    echo Analyzing: !LATEST!
    python profiling\tools\analyze_profile.py !PROFILE!
    goto end
)

if /i "%1"=="visualize" (
    echo Generating visualizations for: !LATEST!
    python profiling\tools\visualize_profile.py !PROFILE!
    if !errorlevel! equ 0 (
        echo.
        echo Visualizations saved to: profiling\profiles\viz\
        start profiling\profiles\viz
    )
    goto end
)

if /i "%1"=="both" (
    echo Analyzing: !LATEST!
    python profiling\tools\analyze_profile.py !PROFILE!
    echo.
    echo Generating visualizations...
    python profiling\tools\visualize_profile.py !PROFILE!
    if !errorlevel! equ 0 (
        echo.
        echo Visualizations saved to: profiling\profiles\viz\
        start profiling\profiles\viz
    )
    goto end
)

echo Unknown command: %1
echo Run "run_profile.bat" for help

:end
endlocal
