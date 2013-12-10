ECHO OFF

REM Copyright (c) 2012 The Chromium Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

REM Copies an installer and symbols from a build directory on a network share
REM into the directory \[out|build]\[Debug|Release] on the current drive.
REM
REM Usage:
REM   \\build.share\<path_to_checkout>\src\tools\win\copy-installer.bat
REM
REM By default, the script will copy the Debug build in the tree, falling back
REM to the Release build if one is not found.  Similarly, the ninja output
REM directory is preferred over the devenv output directory.  Specify
REM "out|build" and/or "Debug|Release" (case matters) on the command line in
REM any order to influence selection.  The defaults for location and build type
REM can also be overridden in a given build tree by creating a
REM "copy-installer.cfg" file alongside the .gclient file that sets the OUTPUT
REM and/or BUILDTYPE variables.
REM
REM Install Robocopy for superior performance on Windows XP if desired (it is
REM present by default on Vista+).

SETLOCAL

REM Get the path to the build tree's src directory.
CALL :_canonicalize "%~dp0..\.."
SET FROM=%RET%

REM Read local configuration (set OUTPUT and BUILDTYPE there).
IF EXIST "%FROM%\..\copy-installer.cfg" CALL "%FROM%\..\copy-installer.cfg"

REM Read OUTPUT and/or BUILDTYPE from command line.
FOR %%a IN (%1 %2) do (
IF "%%a"=="out" SET OUTPUT=out
IF "%%a"=="build" SET OUTPUT=build
IF "%%a"=="Debug" SET BUILDTYPE=Debug
IF "%%a"=="Release" SET BUILDTYPE=Release
)

CALL :_find_build
IF "%OUTPUT%%BUILDTYPE%"=="" (
ECHO No build found to copy.
EXIT 1
)

SET FROM=%FROM%\%OUTPUT%\%BUILDTYPE%
SET TO=\%OUTPUT%\%BUILDTYPE%

REM Figure out what files to copy based on the component type (shared/static).
IF EXIST "%FROM%\base.dll" (
SET TOCOPY=setup.exe setup.exe.manifest chrome.7z *.dll
SET ARCHIVETODELETE=chrome.packed.7z
SET INSTALLER=setup.exe
) ELSE (
SET TOCOPY=mini_installer.exe mini_installer.exe.pdb
SET INSTALLER=mini_installer.exe
)

SET TOCOPY=%TOCOPY% *.dll.pdb chrome.exe.pdb setup.exe.pdb^
           chrome_frame_helper.exe.pdb chrome_launcher.exe.pdb^
           delegate_execute.exe.pdb

CALL :_copyfiles

REM incremental_chrome_dll=1 puts chrome_dll.pdb into the "initial" dir.
IF EXIST "%FROM%\initial" (
SET FROM=%FROM%\initial
SET TOCOPY=*.pdb
CALL :_copyfiles
)

REM Keeping the old chrome.packed.7z around could cause the new setup.exe to
REM use it instead of the new chrome.7z, delete it to save developers from
REM debugging nightmares!
IF NOT "%ARCHIVETODELETE%"=="" (
IF EXIST "%TO%\%ARCHIVETODELETE%" (
ECHO Deleting old/deprecated %ARCHIVETODELETE%
del /Q "%TO%\%ARCHIVETODELETE%"
)
)

ECHO Ready to run/debug %TO%\%INSTALLER%.
GOTO :EOF

REM All labels henceforth are subroutines intended to be invoked by CALL.

REM Canonicalize the first argument, returning it in RET.
:_canonicalize
SET RET=%~f1
GOTO :EOF

REM Search for a mini_installer.exe in the candidate build outputs.
:_find_build
IF "%OUTPUT%"=="" (
SET OUTPUTS=out build
) ELSE (
SET OUTPUTS=%OUTPUT%
SET OUTPUT=
)

IF "%BUILDTYPE%"=="" (
SET BUILDTYPES=Debug Release
) ELSE (
SET BUILDTYPES=%BUILDTYPE%
SET BUILDTYPE=
)

FOR %%o IN (%OUTPUTS%) DO (
FOR %%f IN (%BUILDTYPES%) DO (
IF EXIST "%FROM%\%%o\%%f\mini_installer.exe" (
SET OUTPUT=%%o
SET BUILDTYPE=%%f
GOTO :EOF
)
)
)
GOTO :EOF

REM Branch to handle copying via robocopy (fast) or xcopy (slow).
:_copyfiles
robocopy /? 1> nul 2> nul
IF NOT "%ERRORLEVEL%"=="9009" (
robocopy "%FROM%" "%TO%" %TOCOPY% /MT /XX
) ELSE (
IF NOT EXIST "%TO%" mkdir "%TO%"
call :_xcopy_hack %TOCOPY%
)
GOTO :EOF

REM We can't use a for..in..do loop since we have wildcards, so we make a call
REM to this with the files to copy.
:_xcopy_hack
SHIFT
IF "%0"=="" GOTO :EOF
xcopy "%FROM%\%0" "%TO%" /d /y
GOTO _xcopy_hack
