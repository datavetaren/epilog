@echo off

REM -- Change settings here -------------------------------------------

REM To set a specific VC: SET CHOOSEVC=2017, 2019, ...
SET CHOOSEVC=2022

REM Set 32-bit or 64-bit version
SET BIT=64

REM To set debug mode, set DEBUGMODE=1
SET DEBUGMODE=1

SET CCFLAGS=/nologo /c /EHsc /D_WIN32_WINNT=0x0501 %CC_EXTRA%
SET LINKFLAGS=/nologo

SET CCFLAGS_DEBUG=/nologo /DDEBUG /c /EHsc /Zi /D_WIN32_WINNT=0x0501 %CC_EXTRA%
SET LINKFLAGS_DEBUG=/nologo /debug

REM -------------------------------------------------------------------

REM
REM It's hopeless to use gmake and fix gmake so it works under Windows.
REM Normally, users would use Visual Studio and not the command line.
REM However, this provides a simple way of building without relying
REM on external tools.
REM

REM
REM Check if cl.exe is present on path
REM
WHERE /q cl.exe
IF NOT ERRORLEVEL 1 (
GOTO :MAIN
)

SETLOCAL ENABLEDELAYEDEXPANSION
set VCVARSDIR=
call :SELECTVC "%CHOOSEVC%" VCVARSDIR VCNAME
ECHO !VCVARSDIR! >%TEMP%\vcvarsdir.txt
ECHO !VCNAME! >%TEMP%\vcname.txt
ENDLOCAL

FOR /f "delims=" %%A IN (%TEMP%\vcvarsdir.txt) DO SET VCVARSDIR=%%A
FOR /f "delims=" %%A IN (%TEMP%\vcname.txt) DO SET VCNAME=%%A
CALL :trim VCVARSDIR %VCVARSDIR%
CALL :trim VCNAME %VCNAME%

IF %BIT%==32 (
    CALL "%VCVARSDIR%\vcvars32.bat" >NUL
)
IF %BIT%==64 (
    ECHO The dir chosen is %VCVARSDIR%
    CALL "%VCVARSDIR%\vcvarsx86_amd64.bat"
REM CALL "%VCVARSDIR%\vcvarsx86_amd64.bat" >NUL
)

GOTO:MAIN

:trim
SETLOCAL ENABLEDELAYEDEXPANSION
SET params=%*
FOR /f "tokens=1*" %%a IN ("!params!") DO ENDLOCAL & SET %1=%%b
GOTO :EOF

:CHECKVC
SET DIRCHK=%1
SET NAME=%2
SET PATTERN=%3
IF "%PATTERN%"=="" SET PATTERN=xyzzyxyzzy
SET PATTERN=%PATTERN:"=%
set FOUND="notfound"
set XNAME="notfound"
IF EXIST "!DIRCHK!" (
   IF NOT "!DIRCHK:%PATTERN%=!"=="!DIRCHK!" (
       SET FOUND="!DIRCHK!"
       SET XNAME=!NAME!
   )
)
set FOUND=%FOUND:"=%
set %4=!FOUND!
set %5=!XNAME!
GOTO:EOF

:SELECTVC
set CHOOSEVC=%1
set VCDIR=notfound
set VCNAME=notfound
IF "!VCDIR!"=="notfound" call :CHECKVC "%PROGRAMFILES%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\" VS2022 "%CHOOSEVC%" VCDIR VCNAME
IF "!VCDIR!"=="notfound" call :CHECKVC "%PROGRAMFILES%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\" VS2022 "%CHOOSEVC%" VCDIR VCNAME
IF "!VCDIR!"=="notfound" call :CHECKVC "%PROGRAMFILES(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\" VS2019 "%CHOOSEVC%" VCDIR VCNAME
IF "!VCDIR!"=="notfound" call :CHECKVC "%PROGRAMFILES(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\" VS2019 "%CHOOSEVC%" VCDIR VCNAME
IF "!VCDIR!"=="notfound" call :CHECKVC "%PROGRAMFILES(x86)%\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\" VS2017 "%CHOOSEVC%" VCDIR VCNAME
IF "!VCDIR!"=="notfound" call :CHECKVC "%PROGRAMFILES(x86)%\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\" VS2017 "%CHOOSEVC%" VCDIR VCNAME
)
set %2=!VCDIR!
set %3=!VCNAME!

GOTO :EOF

:MAIN

REM ----------------------------------------------------
REM  MAIN
REM ----------------------------------------------------

IF "%1"=="help" (
GOTO :HELP
)

SETLOCAL ENABLEEXTENSIONS
SETLOCAL ENABLEDELAYEDEXPANSION

REM
REM Retrocheck version of Visual Studio from the CL command
REM

SET VCVER=
CALL :GETVCVER VCVER
IF "!VCVER!"=="" (
   ECHO Couldn't determine version of Visual C++ from CL command
   ECHO Where cl.exe is
   WHERE cl.exe
   GOTO :EOF
)

SET BOOST=
CALL :CHKBST !VCVER! BOOST
IF "!BOOST!"=="" (
   GOTO :EOF
)

IF "%DEBUGMODE%"=="1" (
   SET CCFLAGS=!CCFLAGS_DEBUG!
   SET LINKFLAGS=!LINKFLAGS_DEBUG!
)

IF NOT "%DOLIB%"=="" (
   SET LINKFLAGS=!LINKFLAGS:/debug=!
)
IF NOT "%DOEXE%"=="" (
   SET LINKFLAGS=!LINKFLAGS! /SUBSYSTEM:CONSOLE
)

SET CCFLAGS=!CCFLAGS! /I"!BOOST!"

IF NOT "!CINCLUDE1!"=="" (
   SET CCFLAGS=!CCFLAGS! /I"!CINCLUDE1!"
)
IF NOT "!CINCLUDE2!"=="" (
   SET CCFLAGS=!CCFLAGS! /I"!CINCLUDE2!"
)

rem echo !CCFLAGS!

SET LINKFLAGS=!LINKFLAGS! /LIBPATH:"!BOOST!\lib!BIT!-msvc-!VCVER!"
set ABSROOT=%~dp0

set ENV=%ROOT%\env
set SRC=%ROOT%\src
set OUT=%ROOT%\out
set BIN=%ROOT%\bin

REM
REM Is this a LIB target? Update goal appropriately.
REM
IF NOT "%DOLIB%"=="" (
   SET GOAL=%BIN%\%DOLIB%.lib
   SET LINKCMD=lib.exe
)
IF NOT "%DOEXE%"=="" (
   SET GOAL=%BIN%\%DOEXE%.exe
   SET LINKCMD=link.exe
)

IF "%1"=="clean" (
GOTO :CLEAN
)

IF "%1"=="cleanall" (
GOTO :CLEANALL
)

IF "%1"=="testclean" (
GOTO :TESTCLEAN
)

IF "%1"=="vcxproj" (
GOTO :VCXPROJ
)

IF "%1"=="vssln" (
GOTO :VSSLN
)

set DBGPRNT=0
set WORKDONE=0
GOTO START
:DBG
IF !DBGPRNT!==1 GOTO :EOF
IF "%DEBUGMODE%"=="1" (
   set DBGPRNT=1
   echo [DEBUG MODE]
)
GOTO :EOF

:START

REM
REM Get all .cpp files
REM
set CPP_FILES=
FOR %%S IN (%SRC%\%SUBDIR%\*.cpp) DO (
    set CPP_FILES=!CPP_FILES! %%S
)

REM
REM Ensure that out directory is present
REM
IF NOT EXIST %OUT% (
mkdir %OUT%
)
IF NOT EXIST %OUT%\%SUBDIR% (
mkdir %OUT%\%SUBDIR%
)

REM
REM Compute all dependent .lib files
REM
IF NOT "%DOLIB%"=="" (
    SET LIB_FILES=!BIN!\!DOLIB!.lib
)
for %%i in (!DEPENDS!) DO (
    set LIBFILE=!BIN!\%%i.lib
    REM
    REM check if dependent LIBFILE is never than BIN file
    REM
    set R=0
    call :FCMP !LIBFILE! !GOAL! !R!
    IF "!R!"=="1" (
        REM Force relinking
        set WORKDONE=1
    )    
    set LIB_FILES=!LIB_FILES! "!LIBFILE!"
)

REM
REM Compile all .cpp files into .obj
REM
set OBJ_FILES=
for %%i in (!CPP_FILES!) DO (
    set CPPFILE=%%i
    set OBJFILE=!CPPFILE:.cpp=.obj!
    set OBJFILE=!OBJFILE:%SRC%=%OUT%!

REM 
REM Check if CPP file is newer than OBJ file.
REM Delete OBJ file if it is.
REM

    IF EXIST !OBJFILE! (
        set R=0
        call :FCMP !CPPFILE! !OBJFILE! !R!
        IF "!R!"=="1" (
            del /Q /F !OBJFILE!
        )
    )

    IF NOT EXIST !OBJFILE! (
        set PDBFILE=
	IF "%1"=="check" (
	    EXIT /b 1
	    GOTO :EOF
	)
	CALL :DBG
        IF "!DEBUGMODE!"=="1" set PDBFILE=/Fd:!OBJFILE:.obj=.pdb!
rem        echo cl.exe !CCFLAGS! /Fo:!OBJFILE! !PDBFILE! !CPPFILE!
        cl.exe !CCFLAGS! /Fo:!OBJFILE! !PDBFILE! !CPPFILE!
	set WORKDONE=1
        IF ERRORLEVEL 1 GOTO :EOF
    )
    set OBJ_FILES=!OBJ_FILES! "!OBJFILE!"
)


REM
REM Ensure that bin directory is present
REM
IF NOT EXIST %BIN% (
mkdir %BIN%
)

REM
REM Dispatch to test if requested
REM
IF "%1"=="test" (
GOTO :TEST
)

REM
REM Dispatch to script if requested
REM
IF "%1"=="script" (
GOTO :SCRIPT
)

IF !WORKDONE!==0 GOTO :EOF

REM IF EXIST !GOAL! (
REM echo !GOAL! already exists. Run make clean to first to recompile.
REM GOTO :EOF
REM )

REM
REM Link final goal
REM
ECHO !GOAL!

set PDBFILE=
IF "!DEBUGMODE!"=="1" IF "!DOLIB!"=="" set PDBFILE=/pdb:"!GOAL:.exe=.pdb!"
IF NOT "%DOEXE%"=="" (
    SET DEPEND_LIBS=!LIB_FILES!
    %LINKCMD% %LINKFLAGS% /out:"!GOAL!" !PDBFILE! !DEPEND_LIBS! !OBJ_FILES!
    
)
IF NOT "%DOLIB%"=="" (
    %LINKCMD% %LINKFLAGS% /out:"!GOAL!" !PDBFILE! !DEPEND_LIBS! !OBJ_FILES!
)

GOTO :EOF

REM ----------------------------------------------------
REM  TEST
REM ----------------------------------------------------

:TEST

REM
REM Ensure that out\test and bin\test directories are present
REM
IF NOT EXIST %OUT%\%SUBDIR%\test (
mkdir %OUT%\%SUBDIR%\test
)
IF NOT EXIST %BIN%\test (
mkdir %BIN%\test
)
IF NOT EXIST %BIN%\test\!SUBDIR!\ (
mkdir %BIN%\test\!SUBDIR!
)

SET MOREOBJ=
IF NOT "%DOEXE%"=="" (
    for %%S in (%SRC%\%SUBDIR%\*.cpp) DO (
        set CPPFILE=%%S
        IF NOT "!CPPFILE!"=="%SRC%\%SUBDIR%\main.cpp" (
	    set OBJFILE=!CPPFILE:.cpp=.obj!
	    set OBJFILE=!OBJFILE:%SRC%=%OUT%!
	    set MOREOBJ=!MOREOBJ! !OBJFILE!
	)
    )
)

REM
REM Compile and run tests
REM
for %%S in (%SRC%\%SUBDIR%\test\*.cpp) DO (
    set CPPFILE=%%S
    set OBJFILE=!CPPFILE:.cpp=.obj!
    set OBJFILE=!OBJFILE:%SRC%=%OUT%!
    set BINOKFILE=!OBJFILE:%OUT%=%BIN%!
    set BINEXEFILE=!OBJFILE:%OUT%=%BIN%!

REM 
REM Check if CPP file is newer than OBJ file.
REM Delete OBJ file if it is.
REM

    for %%i in (!CPPFILE!) do set BINOKFILE=%BIN%\test\!SUBDIR!\%%~ni.ok
    for %%i in (!CPPFILE!) do set BINEXEFILE=%BIN%\test\!SUBDIR!\%%~ni.exe
    for %%i in (!CPPFILE!) do set BINLOGFILE=%BIN%\test\!SUBDIR!\%%~ni.log


    IF EXIST !BINOKFILE! (
        set R=0
        call :FCMP !CPPFILE! !BINOKFILE! !R!
        IF "!R!"=="1" (
            del /Q /F !OBJFILE!
            del /Q /F !BINOKFILE!
            del /Q /F !BINEXEFILE!
        )
    )

    IF NOT EXIST !BINOKFILE! (
	set PDBFILE=
        IF "!DEBUGMODE!"=="1" set PDBFILE=/Fd:!OBJFILE:.obj=.pdb!
        cl.exe !CCFLAGS! /I%SRC% /Fo:!OBJFILE! !PDBFILE! !CPPFILE!
        IF ERRORLEVEL 1 GOTO :EOF

	set PDBFILE=
	IF "!DEBUGMODE!"=="1" set PDBFILE=/debug /pdb:"!BINEXEFILE:.exe=.pdb!"
	link !LINKFLAGS! /out:!BINEXEFILE! !PDBFILE! !LIB_FILES! !OBJFILE! !MOREOBJ!
        IF ERRORLEVEL 1 GOTO :EOF
	ECHO Running !BINEXEFILE!
	ECHO   [output to !BINLOGFILE!]
	!BINEXEFILE! %2 %3 %4 %5 %6 %7 %8 %9 1>!BINLOGFILE! 2>&1
        IF ERRORLEVEL 1 GOTO :EOF
	for %%i in (!BINLOGFILE!) do if %%~zi==0 (
	    echo Error while running !BINEXEFILE!
	    GOTO :EOF
	)
	copy /Y NUL !BINOKFILE! >NUL
    )
)

GOTO :EOF

REM ----------------------------------------------------
REM  SCRIPT
REM ----------------------------------------------------

:SCRIPT

REM
REM Ensure that out\script and bin\script directories are present
REM
IF NOT EXIST %OUT%\%SUBDIR%\script (
mkdir %OUT%\%SUBDIR%\script
)
IF NOT EXIST %BIN%\script (
mkdir %BIN%\script
)
IF NOT EXIST %BIN%\script\!SUBDIR!\ (
mkdir %BIN%\script\!SUBDIR!
)

REM
REM Compile and run script
REM
for %%S in (%SRC%\%SUBDIR%\script\*.cpp) DO (
    set CPPFILE=%%S
    set OBJFILE=!CPPFILE:.cpp=.obj!
    set OBJFILE=!OBJFILE:%SRC%=%OUT%!
    set BINOKFILE=!OBJFILE:%OUT%=%BIN%!
    set BINEXEFILE=!OBJFILE:%OUT%=%BIN%!

REM 
REM Check if CPP file is newer than OBJ file.
REM Delete OBJ file if it is.
REM

    for %%i in (!CPPFILE!) do set BINOKFILE=%BIN%\script\!SUBDIR!\%%~ni.ok
    for %%i in (!CPPFILE!) do set BINEXEFILE=%BIN%\script\!SUBDIR!\%%~ni.exe
    for %%i in (!CPPFILE!) do set BINLOGFILE=%BIN%\script\!SUBDIR!\%%~ni.log


    IF EXIST !BINOKFILE! (
        set R=0
        call :FCMP !CPPFILE! !BINOKFILE! !R!
        IF "!R!"=="1" (
            del /Q /F !OBJFILE!
            del /Q /F !BINOKFILE!
            del /Q /F !BINEXEFILE!
        )
    )

    IF NOT EXIST !BINOKFILE! (
	set PDBFILE=
        IF "!DEBUGMODE!"=="1" set PDBFILE=/Fd:!OBJFILE:.obj=.pdb!
        cl.exe !CCFLAGS! /I%SRC% /Fo:!OBJFILE! !PDBFILE! !CPPFILE!
        IF ERRORLEVEL 1 GOTO :EOF

	set PDBFILE=
	IF "!DEBUGMODE!"=="1" set PDBFILE=/debug /pdb:"!BINEXEFILE:.exe=.pdb!"
	link !LINKFLAGS! /out:!BINEXEFILE! !PDBFILE! !LIB_FILES! !OBJFILE!
        IF ERRORLEVEL 1 GOTO :EOF
	ECHO Running !BINEXEFILE!
	ECHO   [output to !BINLOGFILE!]
	!BINEXEFILE! %BIN% %2 %3 %4 %5 %6 %7 %8 %9 1>!BINLOGFILE! 2>&1
        IF ERRORLEVEL 1 GOTO :EOF
	for %%i in (!BINLOGFILE!) do if %%~zi==0 (
	    echo Error while running !BINEXEFILE!
	    GOTO :EOF
	)
	copy /Y NUL !BINOKFILE! >NUL
    )
)
GOTO :EOF


REM ----------------------------------------------------
REM  CLEAN
REM ----------------------------------------------------

:CLEAN
del /S /F /Q %OUT%\!SUBDIR!
del /S /F /Q %BIN%\!SUBDIR!.*
del /S /F /Q %BIN%\test\!SUBDIR!.*
del /S /F /Q %BIN%\test\!SUBDIR!\*
del /S /F /Q %BIN%\script\!SUBDIR!.*
del /S /F /Q %BIN%\script\!SUBDIR!\*
rmdir /S /Q %OUT%\!SUBDIR!
rmdir /S /Q %BIN%\test\!SUBDIR!
rmdir /S /Q %BIN%\script\!SUBDIR!

GOTO :EOF

REM ----------------------------------------------------
REM  CLEANALL
REM ----------------------------------------------------

:CLEANALL
del /S /F /Q %OUT%
del /S /F /Q %BIN%
rmdir /S /Q %OUT%
rmdir /S /Q %BIN%

GOTO :EOF

REM ----------------------------------------------------
REM  TESTCLEAN
REM ----------------------------------------------------

:TESTCLEAN
del /S /F /Q %OUT%\!SUBDIR!\test
del /S /F /Q %BIN%\test\!SUBDIR!
rmdir /S /Q %OUT%\!SUBDIR!\test
rmdir /S /Q %BIN%\test\!SUBDIR!

GOTO :EOF

REM ----------------------------------------------------
REM  FCMP (file time compare)
REM ----------------------------------------------------

:FCMP
set r=0
for /f "delims=" %%k in ('cmd ^2^>NUL /c xcopy /D /Y /f /e "%1" "%2"') do (
    if "%%k" EQU "1 File(s) copied" (
        set r=1
    )
)
set %3=!r!

GOTO :EOF

REM ----------------------------------------------------
REM  GETVCVER
REM ----------------------------------------------------

:GETVCVER
SET VCVER=
WHERE cl.exe >%TEMP%\cl.txt
SET /P CLTXT=<%TEMP%\cl.txt

IF "!VCVER!"=="" IF NOT "!CLTXT:14.2=!"=="!CLTXT!" (
   SET VCVER=14.2
)
IF "!VCVER!"=="" IF NOT "!CLTXT:14.1=!"=="!CLTXT!" (
   SET VCVER=14.1
)
IF "!VCVER!"=="" IF NOT "!CLTXT:14.0=!"=="!CLTXT!" (
   SET VCVER=14.0
)
SET %1=!VCVER!

GOTO :EOF

REM ----------------------------------------------------
REM  BOOST
REM ----------------------------------------------------

:CHKBST

REM
REM Determine version of BOOST from version of Visual C++
REM

SET VCVER=%1

SET BOOSTVER=boost_1_74_0

SET BOOST=

IF EXIST "C:\local\!BOOSTVER!" (
    SET BOOST=C:\local\!BOOSTVER!
) ELSE IF EXIST "%PROGRAMFILES%\boost\!BOOSTVER!" (
    SET BOOST=%PROGRAMFILES%\boost\!BOOSTVER!
)

IF "!BOOST!"=="" (
   ECHO This package requires BOOST !BOOSTVER! and couldn't find it.
   ECHO Tried the following locations:
   ECHO     %PROGRAMFILES%\boost\!BOOSTVER!
   GOTO :EOF
)

SET %2=!BOOST!

GOTO :EOF

REM ----------------------------------------------------
REM  VCXPROJ
REM ----------------------------------------------------

:VCXPROJ

REM
REM Check if csc.exe is present on path
REM
WHERE /q csc.exe
IF ERRORLEVEL 1 (
echo Cannot find csc.exe
GOTO :EOF
)

REM
REM Ensure that bin directory is present
REM
IF NOT EXIST %BIN% (
mkdir %BIN%
)

echo Compiling make_vcxproj.cs
csc.exe /nologo /reference:Microsoft.Build.dll /reference:Microsoft.Build.Framework.dll /out:%BIN%\make_vcxproj.exe %ENV%\make_vcxproj.cs
IF ERRORLEVEL 1 GOTO :EOF
%BIN%\make_vcxproj.exe env=%VCNAME% bit=%BIT% root=%ROOT% src=%SRC% out=%OUT% bin=%BIN% boost="!BOOST!"

GOTO :EOF

REM ----------------------------------------------------
REM  VSSLN
REM ----------------------------------------------------

:VSSLN

REM
REM Check if csc.exe is present on path
REM
WHERE /q csc.exe
IF ERRORLEVEL 1 (
echo Cannot find csc.exe
GOTO :EOF
)

REM
REM Ensure that bin directory is present
REM
IF NOT EXIST %BIN% (
mkdir %BIN%
)

echo Compiling make_vssln.cs
csc.exe /nologo /reference:Microsoft.Build.dll /reference:Microsoft.Build.Framework.dll /out:%BIN%\make_vssln.exe %ENV%\make_vssln.cs
IF ERRORLEVEL 1 GOTO :EOF
%BIN%\make_vssln.exe bin=%BIN% src=%SRC%

GOTO :EOF

:HELP
echo --------------------------------------------------------
echo  make help
echo --------------------------------------------------------
echo  make          To build the application
echo  make clean    To clean everything
echo  make test     To build and run tests
echo  make vcxproj  To build vcxproj file for Visual Studio
echo  make vssln    To build sln file for Visual Studio
echo --------------------------------------------------------

GOTO :EOF
