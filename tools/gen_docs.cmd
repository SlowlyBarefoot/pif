@echo off
rem SPDX-License-Identifier: BSD-3-Clause
rem
rem Builds the PIF manual with Doxygen on Windows (cmd.exe or PowerShell).
rem
rem Usage: tools\gen_docs.cmd [--open]
rem
rem   --open   Open docs\build\html\index.html when the build is done.
rem
rem Doxygen is taken from %DOXYGEN%, then from the PATH, then from the default
rem install folder. The output goes to docs\build\html, the warnings to
rem docs\build\warnings.log.

setlocal EnableExtensions

cd /d "%~dp0.." || exit /b 2

if not defined DOXYGEN (
	where doxygen >nul 2>&1 && set "DOXYGEN=doxygen"
)
if not defined DOXYGEN (
	if exist "%ProgramFiles%\doxygen\bin\doxygen.exe" set "DOXYGEN=%ProgramFiles%\doxygen\bin\doxygen.exe"
)
if not defined DOXYGEN (
	echo ERROR: doxygen not found. Install it from https://www.doxygen.nl or set DOXYGEN. 1>&2
	exit /b 2
)

set "PIF_DOC_VERSION="
for /f "delims=" %%v in ('git describe --tags --always --dirty 2^>nul') do set "PIF_DOC_VERSION=%%v"

if exist docs\build rmdir /s /q docs\build
mkdir docs\build
"%DOXYGEN%" docs\Doxyfile || exit /b 1

set "WARNINGS=0"
for /f %%n in ('find /c "warning:" ^< docs\build\warnings.log 2^>nul') do set "WARNINGS=%%n"
echo Manual: docs\build\html\index.html (%WARNINGS% warning(s), see docs\build\warnings.log)

if /i "%~1"=="--open" start "" "docs\build\html\index.html"

exit /b 0
