@echo off
setlocal EnableExtensions

set "RESULTS_LOG=%~dp0Results.log"

echo TapeStopper build is running. Detailed output will appear when it finishes.
call :BUILD_INNER > "%RESULTS_LOG%" 2>&1
set "BUILD_EXIT=%ERRORLEVEL%"

type "%RESULTS_LOG%"

echo.
pause
exit /b %BUILD_EXIT%

:BUILD_INNER
setlocal EnableExtensions EnableDelayedExpansion

set "STAGE=Stage 6.1"
set "PROJECT_ROOT=%~dp0"
set "SOURCE_DIR=%PROJECT_ROOT%source"
set "BUILD_DIR=%PROJECT_ROOT%build"
set "DIST_DIR=%PROJECT_ROOT%dist"
set "FINAL_OUTPUT=%DIST_DIR%\TapeStopper.vst3"
set "CMAKE_EXE=%PROJECT_ROOT%..\_Tools\cmake\_4.4.2\bin\cmake.exe"
set "JUCE_CMAKE=%PROJECT_ROOT%..\_Tools\JUCE\_8.0.15\CMakeLists.txt"

echo TapeStopper %STAGE%
echo Project Root: %PROJECT_ROOT%
echo.

echo Closing PolyHostInterface.exe if it is running...
taskkill /IM PolyHostInterface.exe /F >nul 2>&1
if errorlevel 1 (
    echo - INFO: PolyHostInterface.exe was not running.
) else (
    echo - PASS: PolyHostInterface.exe was closed.
)

call :CHECK_FILE "%CMAKE_EXE%" "CMake 4.4.2 executable"
if errorlevel 1 exit /b 1
call :CHECK_FILE "%JUCE_CMAKE%" "JUCE 8.0.15 CMakeLists.txt"
if errorlevel 1 exit /b 1
call :CHECK_FILE "%SOURCE_DIR%\CMakeLists.txt" "project CMakeLists.txt"
if errorlevel 1 exit /b 1
call :CHECK_FILE "%SOURCE_DIR%\PluginProcessor.h" "PluginProcessor.h"
if errorlevel 1 exit /b 1
call :CHECK_FILE "%SOURCE_DIR%\PluginProcessor.cpp" "PluginProcessor.cpp"
if errorlevel 1 exit /b 1
call :CHECK_FILE "%SOURCE_DIR%\PluginEditor.h" "PluginEditor.h"
if errorlevel 1 exit /b 1
call :CHECK_FILE "%SOURCE_DIR%\PluginEditor.cpp" "PluginEditor.cpp"
if errorlevel 1 exit /b 1
call :CHECK_FILE "%SOURCE_DIR%\PortableSettings.h" "PortableSettings.h"
if errorlevel 1 exit /b 1
call :CHECK_FILE "%SOURCE_DIR%\PortableSettings.cpp" "PortableSettings.cpp"
if errorlevel 1 exit /b 1

echo.
echo Configuring x64 Release project...
"%CMAKE_EXE%" -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64
if errorlevel 1 (
    call :FAIL "CMake configuration failed." "A Visual Studio 18 2026 x64 project in the build folder."
    exit /b 1
)
echo - PASS: CMake configuration completed.

echo.
echo Building TapeStopper_VST3 Release with a clean rebuild...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --target TapeStopper_VST3 --clean-first --parallel
if errorlevel 1 (
    call :FAIL "The Release VST3 build failed." "The TapeStopper_VST3 target to build successfully."
    exit /b 1
)
echo - PASS: Release VST3 target built.

set "BUILT_BINARY=%BUILD_DIR%\TapeStopper_artefacts\Release\VST3\TapeStopper.vst3\Contents\x86_64-win\TapeStopper.vst3"
if not exist "%BUILT_BINARY%" (
    call :FAIL "The compiled VST3 binary was not found." "%BUILT_BINARY%"
    exit /b 1
)

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
if errorlevel 1 (
    call :FAIL "The dist folder could not be created." "%DIST_DIR%"
    exit /b 1
)

if exist "%FINAL_OUTPUT%\NUL" (
    call :FAIL "A directory exists where the portable VST3 file must be written." "%FINAL_OUTPUT% as a single file."
    exit /b 1
)

copy /Y "%BUILT_BINARY%" "%FINAL_OUTPUT%" >nul
if errorlevel 1 (
    call :FAIL "The portable VST3 file could not be copied." "%FINAL_OUTPUT%"
    exit /b 1
)

set "UNEXPECTED_DIST_ITEMS="
for /F "delims=" %%F in ('dir /B /A "%DIST_DIR%" 2^>nul') do (
    if /I not "%%F"=="TapeStopper.vst3" if /I not "%%F"=="Data" set "UNEXPECTED_DIST_ITEMS=!UNEXPECTED_DIST_ITEMS! %%F"
)
if defined UNEXPECTED_DIST_ITEMS (
    call :FAIL "Unexpected item(s) were found in dist:!UNEXPECTED_DIST_ITEMS!" "Only TapeStopper.vst3 and an optional Data folder."
    exit /b 1
)

if exist "%DIST_DIR%\Data" if not exist "%DIST_DIR%\Data\" (
    call :FAIL "dist\Data exists but is not a folder." "An optional writable Data folder."
    exit /b 1
)

set "TAPESTOPPER_OUTPUT=%FINAL_OUTPUT%"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$p=$env:TAPESTOPPER_OUTPUT; $b=[IO.File]::ReadAllBytes($p); if ($b.Length -lt 64 -or $b[0] -ne 77 -or $b[1] -ne 90) { exit 1 }; $pe=[BitConverter]::ToInt32($b,60); if ($pe -lt 0 -or $pe + 6 -gt $b.Length) { exit 1 }; $machine=[BitConverter]::ToUInt16($b,$pe+4); if ($machine -ne 0x8664) { exit 1 }; exit 0"
if errorlevel 1 (
    call :FAIL "The final output is not a valid Windows x64 PE binary." "%FINAL_OUTPUT% to be a Windows x64 VST3 module."
    exit /b 1
)

echo.
echo BUILD SUMMARY
echo - PASS: Required tools and source files found.
echo - PASS: Visual Studio 18 2026 x64 configuration completed.
echo - PASS: TapeStopper_VST3 Release target built cleanly.
echo - PASS: Portable single-file VST3 created.
echo - PASS: Final file validated as Windows x64 PE.
echo - PASS: dist contains only allowed items.
echo - OUTPUT: %FINAL_OUTPUT%
echo - NOTE: Stage 6.1 restricts timing-marker clicks and drags to the slider track.
exit /b 0

:CHECK_FILE
if exist "%~1" (
    echo - PASS: Found %~2.
    exit /b 0
)
call :FAIL "Missing %~2." "%~1"
exit /b 1

:FAIL
echo.
echo BUILD SUMMARY
echo - FAIL: %~1
echo - EXPECTED: %~2
echo - LOG: Provide Results.log from the TapeStopper Project Root.
exit /b 0
