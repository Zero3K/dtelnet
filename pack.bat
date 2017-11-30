@echo off

rem 2015.05.01.LZS
rem bugfix
rem 2013.07.03.LZS
rem TABs removed from echo
rem 2004.11.19.LZS
rem Using zip.exe from zip23xN.zip instead of pkzip25.exe

echo X>DTXXXSRC.ZIP
del DTXXXSRC.ZIP >nul

ECHO *** Packing Source ***
REM dont use TABs here, or else ZIP won't find the files
echo DTELNET\RES\*.*         >PACKTMP.TMP
echo DTELNET\DTELNET.HLP     >>PACKTMP.TMP
echo DTELNET\*.txt           >>PACKTMP.TMP
echo DTELNET\*.html          >>PACKTMP.TMP
echo DTELNET\dtelnet.test.sh >>PACKTMP.TMP
echo DTELNET\DTELNET.SET     >>PACKTMP.TMP
echo DTELNET\DTELNET.IN0     >>PACKTMP.TMP
echo DTELNET\*.MAK           >>PACKTMP.TMP
echo DTELNET\*.def           >>PACKTMP.TMP
echo DTELNET\*.rc            >>PACKTMP.TMP
echo DTELNET\*.c             >>PACKTMP.TMP
echo DTELNET\*.h             >>PACKTMP.TMP
echo DTELNET\*.gif           >>PACKTMP.TMP
echo DTELNET\pack.bat        >>PACKTMP.TMP
cd ..
zip DTELNET\DTXXXSRC.ZIP -@ <dtelnet\PACKTMP.TMP
cd dtelnet

ECHO ******************************************************
ECHO *** Now Build 16-bit executable - then press enter ***
ECHO ******************************************************
PAUSE

ECHO *** Packing 16-bit version ***
echo X>DTXXXB16.ZIP
del DTXXXB16.ZIP >nul
zip DTXXXB16.ZIP DTELNET.EXE DTELNET.HLP DTELNET.SET DTELNET.IN0 *.TXT *.html dtelnet.test.sh

ECHO ******************************************************
ECHO *** Now Build 32-bit executable - then press enter ***
ECHO ******************************************************
PAUSE

ECHO *** Packing 32-bit version ***
echo X>DTXXXB32.ZIP
del DTXXXB32.ZIP >nul
zip DTXXXB32.ZIP DTELNET.EXE DTELNET.CHM DTELNET.SET DTELNET.IN0 *.TXT *.html dtelnet.test.sh

ECHO ******************************************************
ECHO *** Now Build 64-bit executable - then press enter ***
ECHO ******************************************************
PAUSE

ECHO *** Packing 64-bit version ***
echo X>DTXXXB64.ZIP
del DTXXXB64.ZIP >nul
zip DTXXXB64.ZIP DTELNET.EXE DTELNET.CHM DTELNET.SET DTELNET.IN0 *.TXT *.html dtelnet.test.sh
