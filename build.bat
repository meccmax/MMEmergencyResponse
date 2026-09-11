@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM  MM Emergency Response - PBO build
REM
REM  Safe to double-click: the window stays open and tells you what happened.
REM
REM  If auto-detection fails, set the path by hand on the next line (point it at
REM  the Bin folder, no trailing backslash) and remove the word REM:
REM
REM set DAYZ_TOOLS_BIN=E:\SteamLibrary\steamapps\common\DayZ Tools\Bin
REM ============================================================================

set MODNAME=MMEmergencyResponse
set SRC=%~dp0
set SRC=%SRC:~0,-1%
set OUTROOT=%~dp0..\@%MODNAME%
set OUTDIR=%OUTROOT%\addons

echo.
echo  ===============================================
echo   Building %MODNAME%
echo  ===============================================
echo.
echo   Source: %SRC%
echo   Output: %OUTDIR%
echo.

REM --------------------------------------------------------------------------
REM  Locate AddonBuilder
REM --------------------------------------------------------------------------

set AB=

if defined DAYZ_TOOLS_BIN (
	if exist "%DAYZ_TOOLS_BIN%\AddonBuilder\AddonBuilder.exe" (
		set "AB=%DAYZ_TOOLS_BIN%\AddonBuilder\AddonBuilder.exe"
	)
)

if not defined AB (
	for %%D in (C D E F G H) do (
		for %%P in (
			"%%D:\SteamLibrary\steamapps\common\DayZ Tools\Bin\AddonBuilder\AddonBuilder.exe"
			"%%D:\Steam\steamapps\common\DayZ Tools\Bin\AddonBuilder\AddonBuilder.exe"
			"%%D:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\AddonBuilder\AddonBuilder.exe"
			"%%D:\Games\Steam\steamapps\common\DayZ Tools\Bin\AddonBuilder\AddonBuilder.exe"
		) do (
			if not defined AB (
				if exist %%P set "AB=%%~P"
			)
		)
	)
)

if not defined AB (
	echo   AddonBuilder.exe was not found.
	echo.
	echo   Open build.bat in a text editor, find the DAYZ_TOOLS_BIN line near
	echo   the top, remove the leading REM, and set it to your Bin folder:
	echo.
	echo       set DAYZ_TOOLS_BIN=E:\SteamLibrary\steamapps\common\DayZ Tools\Bin
	echo.
	goto :fail
)

echo   Builder: %AB%
echo.

REM --------------------------------------------------------------------------
REM  Prefix file - AddonBuilder and every other packer reads this, so the mod
REM  folder is self-describing rather than depending on a command-line flag.
REM --------------------------------------------------------------------------

if not exist "%SRC%\$PBOPREFIX$" (
	echo %MODNAME%> "%SRC%\$PBOPREFIX$"
	echo   Wrote $PBOPREFIX$
)

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM --------------------------------------------------------------------------
REM  Pack. -packonly skips binarisation, which is what a script-only mod wants.
REM --------------------------------------------------------------------------

echo   Packing...
echo.

REM -include tells AddonBuilder which non-binarised file types to copy into the
REM PBO. Without it .xml (and therefore Scripts/Data/Inputs.xml) is silently
REM dropped, which is exactly what happened - the keybind simply did not exist.
"%AB%" "%SRC%" "%OUTDIR%" -clear -packonly -prefix=%MODNAME% -include="%SRC%\include.txt"
set ABERR=%ERRORLEVEL%

echo.

if not exist "%OUTDIR%\%MODNAME%.pbo" (
	echo   BUILD FAILED - no PBO was produced. AddonBuilder exit code: %ABERR%
	echo.
	echo   Scroll up for the compiler output. The usual causes are a syntax
	echo   error in a .c file, or AddonBuilder not having write access to the
	echo   output folder.
	echo.
	echo   If nothing useful printed, run the AddonBuilder GUI instead:
	echo     "%AB%"
	echo   Source:      %SRC%
	echo   Destination: %OUTDIR%
	echo   Tick "Do not binarize", set the prefix to %MODNAME%.
	echo.
	goto :fail
)

for %%A in ("%OUTDIR%\%MODNAME%.pbo") do set PBOSIZE=%%~zA

echo   ===============================================
echo    BUILD OK
echo   ===============================================
echo.
echo    %OUTDIR%\%MODNAME%.pbo  (%PBOSIZE% bytes)
echo.
echo    Next:
echo      1. Copy the @%MODNAME% folder to your server root.
echo      2. Add it LAST on the -mod= line.
echo      3. Copy the same folder into your client mod list.
echo      4. Start the server once to generate
echo         $profile\MMEmergency\config.json, then stop it.
echo.

pause
endlocal
exit /b 0

:fail
echo.
pause
endlocal
exit /b 1
