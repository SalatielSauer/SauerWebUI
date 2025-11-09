@ECHO OFF
setlocal EnableExtensions EnableDelayedExpansion

set MAX_CLIENTS=5

REM Base paths
set "SCRIPT_DIR=%~dp0"
set "BUNDLED_HOME=%SCRIPT_DIR%HOME"
REM SauerWebUI icon for shortcuts lives at "%SCRIPT_DIR%data\sauerwui.ico"

REM Resolve user's Documents folder
set "PERSONAL_DIR="
for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" /v Personal 2^>nul ^| findstr /I "REG_"') do (
    set "PERSONAL_DIR=%%B"
)
if not defined PERSONAL_DIR set "PERSONAL_DIR=%USERPROFILE%\Documents"

set "MYGAMES_DIR=%PERSONAL_DIR%\My Games"
if not exist "%MYGAMES_DIR%" mkdir "%MYGAMES_DIR%" >nul 2>&1

set "SAUER_HOME=%MYGAMES_DIR%\sauerwebui"
if not exist "%SAUER_HOME%" (
    echo Initializing SauerWebUI home folder at "%SAUER_HOME%"
    robocopy "%BUNDLED_HOME%" "%SAUER_HOME%" /E /NFL /NDL /NJH /NJS >nul
    if errorlevel 8 (
        echo Failed to initialize the SauerWebUI home folder. (robocopy exit code %ERRORLEVEL%)
        exit /b 1
    )
)

set "LOCK_DIR=%SAUER_HOME%"

set "SAUER_BIN=bin"
if /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" set "SAUER_BIN=bin64"
if /I "%PROCESSOR_ARCHITEW6432%"=="AMD64" set "SAUER_BIN=bin64"
set "INSTALL_BIN=%SCRIPT_DIR%%SAUER_BIN%"

set "CUSTOM_BIN=%SAUER_HOME%\bin64"
if not exist "%CUSTOM_BIN%" mkdir "%CUSTOM_BIN%" >nul 2>&1

set "INSTALL_EXE=%INSTALL_BIN%\sauerwui.exe"
set "CUSTOM_EXE=%CUSTOM_BIN%\sauerwui.exe"

set SLOT=
for /l %%N in (1,1,%MAX_CLIENTS%) do (
    if not exist "%LOCK_DIR%\Myclient%%N.lock" (
        set SLOT=%%N
        goto :foundslot
    )
)
echo All %MAX_CLIENTS% slots are in use!
exit /b

:foundslot
set "PROFILE=Myclient%SLOT%"
set "LOCK_FILE=%LOCK_DIR%\%PROFILE%.lock"
echo . > "%LOCK_FILE%"

REM Apply pending update in the custom bin64 folder
for %%F in (sauerwui_update.exe sauerwui_update) do (
    if exist "%CUSTOM_BIN%\%%F" (
        echo SauerWebUI update found under "%CUSTOM_BIN%"
        del "%CUSTOM_BIN%\sauerwui.exe" >nul 2>&1
        ren "%CUSTOM_BIN%\%%F" sauerwui.exe
        goto :after_update
    )
)
:after_update

REM Sync custom exe with installation exe if installation build is newer
if exist "%CUSTOM_EXE%" (
    powershell -NoProfile -Command "if ((Test-Path -LiteralPath '%INSTALL_EXE%') -and (Test-Path -LiteralPath '%CUSTOM_EXE%')) { $install = Get-Item -LiteralPath '%INSTALL_EXE%'; $custom = Get-Item -LiteralPath '%CUSTOM_EXE%'; if ($install.LastWriteTimeUtc -gt $custom.LastWriteTimeUtc) { Copy-Item -LiteralPath $install.FullName -Destination '%CUSTOM_EXE%' -Force; Write-Host 'Custom SauerWebUI executable refreshed from installation copy.' } }"
)
set "RUN_EXE=%INSTALL_EXE%"
if exist "%CUSTOM_EXE%" (
    set "RUN_EXE=%CUSTOM_EXE%"
    echo Launching SauerWebUI from custom home executable.
) else (
    echo Launching SauerWebUI from installation executable.
)

set "HOME_ARG=-q%SAUER_HOME%"
set "CEF_PROFILE=%PROFILE%"
set "PATH=%INSTALL_BIN%;%PATH%"
echo Starting SauerWebUI with profile: %CEF_PROFILE%
"%RUN_EXE%" "%HOME_ARG%" -glog.txt %*

del "%LOCK_FILE%"

endlocal
