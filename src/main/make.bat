@ECHO off
SET ROOT=..\..
SET SUBDIR=main
SET DOLIB=
SET DOEXE=epilogd
SET DEPENDS=secp256k1 wallet node ec coin global terminal interp db pow common
SET SCRIPTARGS=%ROOT%\bin\epilogd.exe
CALL ..\..\env\make.bat %*
