@ECHO OFF

setlocal enabledelayedexpansion

set MAX_CLIENTS=5

set SLOT=
for /l %%N in (1,1,%MAX_CLIENTS%) do (
    if not exist "Myclient%%N.lock" (
        set SLOT=%%N
        goto :found
    )
)
echo All %MAX_CLIENTS% slots are in use!
exit /b

:found
set PROFILE=Myclient%SLOT%

echo . > "%PROFILE%.lock"

set SAUER_BIN=bin
IF /I "%PROCESSOR_ARCHITECTURE%" == "amd64" (
    set SAUER_BIN=bin64
)
IF /I "%PROCESSOR_ARCHITEW6432%" == "amd64" (
    set SAUER_BIN=bin64
)

set CEF_PROFILE=%PROFILE%
echo Starting client with profile: %CEF_PROFILE%
"%SAUER_BIN%\sauerwui.exe" "-qHOME" -glog.txt %*

del "%PROFILE%.lock"

endlocal