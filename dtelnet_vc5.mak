# Microsoft Developer Studio Generated NMAKE File, Based on dtelnet.dsp
!IF "$(CFG)" == ""
CFG=dtelnet - Win32 Debug
!MESSAGE No configuration specified. Defaulting to dtelnet - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "dtelnet - Win32 Release" && "$(CFG)" !=\
 "dtelnet - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "dtelnet.mak" CFG="dtelnet - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "dtelnet - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "dtelnet - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "dtelnet - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\dtelnet.exe"

!ELSE 

ALL : "$(OUTDIR)\dtelnet.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\about.obj"
	-@erase "$(INTDIR)\argv.obj"
	-@erase "$(INTDIR)\buffdata.obj"
	-@erase "$(INTDIR)\connect.obj"
	-@erase "$(INTDIR)\ctab.obj"
	-@erase "$(INTDIR)\dialog.obj"
	-@erase "$(INTDIR)\dtchars.obj"
	-@erase "$(INTDIR)\dtelnet.obj"
	-@erase "$(INTDIR)\dtelnet.res"
	-@erase "$(INTDIR)\editor.obj"
	-@erase "$(INTDIR)\emul.obj"
	-@erase "$(INTDIR)\filetype.obj"
	-@erase "$(INTDIR)\font.obj"
	-@erase "$(INTDIR)\harap.obj"
	-@erase "$(INTDIR)\lines.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\printing.obj"
	-@erase "$(INTDIR)\proxy.obj"
	-@erase "$(INTDIR)\raw.obj"
	-@erase "$(INTDIR)\scroll.obj"
	-@erase "$(INTDIR)\socket.obj"
	-@erase "$(INTDIR)\status.obj"
	-@erase "$(INTDIR)\term.obj"
	-@erase "$(INTDIR)\termdef.obj"
	-@erase "$(INTDIR)\termkey.obj"
	-@erase "$(INTDIR)\termwin.obj"
	-@erase "$(INTDIR)\utils.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\dtelnet.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_Windows"\
 /Fp"$(INTDIR)\dtelnet.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o NUL /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\dtelnet.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dtelnet.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib WS2_32.lib oldnames.lib kernel32.lib user32.lib\
 gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib\
 oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /stack:0x2800\
 /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\dtelnet.pdb" /machine:IX86\
 /def:".\dtelnet.def" /out:"$(OUTDIR)\dtelnet.exe" 
DEF_FILE= \
	".\dtelnet.def"
LINK32_OBJS= \
	"$(INTDIR)\about.obj" \
	"$(INTDIR)\attrib.obj" \
	"$(INTDIR)\argv.obj" \
	"$(INTDIR)\babit.obj" \
	"$(INTDIR)\buffdata.obj" \
	"$(INTDIR)\connect.obj" \
	"$(INTDIR)\ctab.obj" \
	"$(INTDIR)\dialog.obj" \
	"$(INTDIR)\dtchars.obj" \
	"$(INTDIR)\dtelnet.obj" \
	"$(INTDIR)\dtelnet.res" \
	"$(INTDIR)\editor.obj" \
	"$(INTDIR)\emul.obj" \
	"$(INTDIR)\envvars.obj" \
	"$(INTDIR)\filetype.obj" \
	"$(INTDIR)\font.obj" \
	"$(INTDIR)\harap.obj" \
	"$(INTDIR)\lines.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\printing.obj" \
	"$(INTDIR)\proxy.obj" \
	"$(INTDIR)\raw.obj" \
	"$(INTDIR)\scroll.obj" \
	"$(INTDIR)\select.obj" \
	"$(INTDIR)\socket.obj" \
	"$(INTDIR)\status.obj" \
	"$(INTDIR)\term.obj" \
	"$(INTDIR)\termdef.obj" \
	"$(INTDIR)\termkey.obj" \
	"$(INTDIR)\termwin.obj" \
	"$(INTDIR)\utils.obj"

"$(OUTDIR)\dtelnet.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\dtelnet.exe" "$(OUTDIR)\dtelnet.bsc"

!ELSE 

ALL : "$(OUTDIR)\dtelnet.exe" "$(OUTDIR)\dtelnet.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\about.obj"
	-@erase "$(INTDIR)\about.sbr"
	-@erase "$(INTDIR)\argv.obj"
	-@erase "$(INTDIR)\argv.sbr"
	-@erase "$(INTDIR)\buffdata.obj"
	-@erase "$(INTDIR)\buffdata.sbr"
	-@erase "$(INTDIR)\connect.obj"
	-@erase "$(INTDIR)\connect.sbr"
	-@erase "$(INTDIR)\ctab.obj"
	-@erase "$(INTDIR)\ctab.sbr"
	-@erase "$(INTDIR)\dialog.obj"
	-@erase "$(INTDIR)\dialog.sbr"
	-@erase "$(INTDIR)\dtchars.obj"
	-@erase "$(INTDIR)\dtchars.sbr"
	-@erase "$(INTDIR)\dtelnet.obj"
	-@erase "$(INTDIR)\dtelnet.res"
	-@erase "$(INTDIR)\dtelnet.sbr"
	-@erase "$(INTDIR)\editor.obj"
	-@erase "$(INTDIR)\editor.sbr"
	-@erase "$(INTDIR)\emul.obj"
	-@erase "$(INTDIR)\emul.sbr"
	-@erase "$(INTDIR)\filetype.obj"
	-@erase "$(INTDIR)\filetype.sbr"
	-@erase "$(INTDIR)\font.obj"
	-@erase "$(INTDIR)\font.sbr"
	-@erase "$(INTDIR)\harap.obj"
	-@erase "$(INTDIR)\harap.sbr"
	-@erase "$(INTDIR)\lines.obj"
	-@erase "$(INTDIR)\lines.sbr"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log.sbr"
	-@erase "$(INTDIR)\printing.obj"
	-@erase "$(INTDIR)\printing.sbr"
	-@erase "$(INTDIR)\proxy.obj"
	-@erase "$(INTDIR)\proxy.sbr"
	-@erase "$(INTDIR)\raw.obj"
	-@erase "$(INTDIR)\raw.sbr"
	-@erase "$(INTDIR)\scroll.obj"
	-@erase "$(INTDIR)\scroll.sbr"
	-@erase "$(INTDIR)\socket.obj"
	-@erase "$(INTDIR)\socket.sbr"
	-@erase "$(INTDIR)\status.obj"
	-@erase "$(INTDIR)\status.sbr"
	-@erase "$(INTDIR)\term.obj"
	-@erase "$(INTDIR)\term.sbr"
	-@erase "$(INTDIR)\termdef.obj"
	-@erase "$(INTDIR)\termdef.sbr"
	-@erase "$(INTDIR)\termkey.obj"
	-@erase "$(INTDIR)\termkey.sbr"
	-@erase "$(INTDIR)\termwin.obj"
	-@erase "$(INTDIR)\termwin.sbr"
	-@erase "$(INTDIR)\utils.obj"
	-@erase "$(INTDIR)\utils.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(OUTDIR)\dtelnet.bsc"
	-@erase "$(OUTDIR)\dtelnet.exe"
	-@erase "$(OUTDIR)\dtelnet.ilk"
	-@erase "$(OUTDIR)\dtelnet.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_Windows"\
 /FR"$(INTDIR)\\" /Fp"$(INTDIR)\dtelnet.pch" /YX /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o NUL /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\dtelnet.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dtelnet.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\about.sbr" \
	"$(INTDIR)\argv.sbr" \
	"$(INTDIR)\buffdata.sbr" \
	"$(INTDIR)\connect.sbr" \
	"$(INTDIR)\ctab.sbr" \
	"$(INTDIR)\dialog.sbr" \
	"$(INTDIR)\dtchars.sbr" \
	"$(INTDIR)\dtelnet.sbr" \
	"$(INTDIR)\editor.sbr" \
	"$(INTDIR)\emul.sbr" \
	"$(INTDIR)\filetype.sbr" \
	"$(INTDIR)\font.sbr" \
	"$(INTDIR)\harap.sbr" \
	"$(INTDIR)\lines.sbr" \
	"$(INTDIR)\log.sbr" \
	"$(INTDIR)\printing.sbr" \
	"$(INTDIR)\proxy.sbr" \
	"$(INTDIR)\raw.sbr" \
	"$(INTDIR)\scroll.sbr" \
	"$(INTDIR)\socket.sbr" \
	"$(INTDIR)\status.sbr" \
	"$(INTDIR)\term.sbr" \
	"$(INTDIR)\termdef.sbr" \
	"$(INTDIR)\termkey.sbr" \
	"$(INTDIR)\termwin.sbr" \
	"$(INTDIR)\utils.sbr"

"$(OUTDIR)\dtelnet.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=wsock32.lib WS2_32.lib oldnames.lib kernel32.lib user32.lib\
 gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib\
 oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /stack:0x2800\
 /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\dtelnet.pdb" /debug\
 /machine:IX86 /def:".\dtelnet.def" /out:"$(OUTDIR)\dtelnet.exe" /pdbtype:sept 
DEF_FILE= \
	".\dtelnet.def"
LINK32_OBJS= \
	"$(INTDIR)\about.obj" \
	"$(INTDIR)\argv.obj" \
	"$(INTDIR)\buffdata.obj" \
	"$(INTDIR)\connect.obj" \
	"$(INTDIR)\ctab.obj" \
	"$(INTDIR)\dialog.obj" \
	"$(INTDIR)\dtchars.obj" \
	"$(INTDIR)\dtelnet.obj" \
	"$(INTDIR)\dtelnet.res" \
	"$(INTDIR)\editor.obj" \
	"$(INTDIR)\emul.obj" \
	"$(INTDIR)\filetype.obj" \
	"$(INTDIR)\font.obj" \
	"$(INTDIR)\harap.obj" \
	"$(INTDIR)\lines.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\printing.obj" \
	"$(INTDIR)\proxy.obj" \
	"$(INTDIR)\raw.obj" \
	"$(INTDIR)\scroll.obj" \
	"$(INTDIR)\socket.obj" \
	"$(INTDIR)\status.obj" \
	"$(INTDIR)\term.obj" \
	"$(INTDIR)\termdef.obj" \
	"$(INTDIR)\termkey.obj" \
	"$(INTDIR)\termwin.obj" \
	"$(INTDIR)\utils.obj"

"$(OUTDIR)\dtelnet.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "dtelnet - Win32 Release" || "$(CFG)" ==\
 "dtelnet - Win32 Debug"
SOURCE=.\about.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_ABOUT=\
	".\about.h"\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\filetype.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\about.obj" : $(SOURCE) $(DEP_CPP_ABOUT) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_ABOUT=\
	".\about.h"\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\filetype.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\about.obj"	"$(INTDIR)\about.sbr" : $(SOURCE) $(DEP_CPP_ABOUT)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\argv.c
DEP_CPP_ARGV_=\
	".\argv.h"\
	

!IF  "$(CFG)" == "dtelnet - Win32 Release"


"$(INTDIR)\argv.obj" : $(SOURCE) $(DEP_CPP_ARGV_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"


"$(INTDIR)\argv.obj"	"$(INTDIR)\argv.sbr" : $(SOURCE) $(DEP_CPP_ARGV_)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\buffdata.c
DEP_CPP_BUFFD=\
	".\buffdata.h"\
	

!IF  "$(CFG)" == "dtelnet - Win32 Release"


"$(INTDIR)\buffdata.obj" : $(SOURCE) $(DEP_CPP_BUFFD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"


"$(INTDIR)\buffdata.obj"	"$(INTDIR)\buffdata.sbr" : $(SOURCE) $(DEP_CPP_BUFFD)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\connect.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_CONNE=\
	".\argv.h"\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\raw.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\connect.obj" : $(SOURCE) $(DEP_CPP_CONNE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_CONNE=\
	".\argv.h"\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\raw.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\connect.obj"	"$(INTDIR)\connect.sbr" : $(SOURCE) $(DEP_CPP_CONNE)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\ctab.c
DEP_CPP_CTAB_=\
	".\ctab.h"\
	

!IF  "$(CFG)" == "dtelnet - Win32 Release"


"$(INTDIR)\ctab.obj" : $(SOURCE) $(DEP_CPP_CTAB_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"


"$(INTDIR)\ctab.obj"	"$(INTDIR)\ctab.sbr" : $(SOURCE) $(DEP_CPP_CTAB_)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\dialog.c
DEP_CPP_DIALO=\
	".\buffdata.h"\
	".\dialog.h"\
	".\preset.h"\
	".\utils.h"\
	

!IF  "$(CFG)" == "dtelnet - Win32 Release"


"$(INTDIR)\dialog.obj" : $(SOURCE) $(DEP_CPP_DIALO) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"


"$(INTDIR)\dialog.obj"	"$(INTDIR)\dialog.sbr" : $(SOURCE) $(DEP_CPP_DIALO)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\dtchars.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_DTCHA=\
	".\buffdata.h"\
	".\ctab.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\lines.h"\
	".\preset.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\dtchars.obj" : $(SOURCE) $(DEP_CPP_DTCHA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_DTCHA=\
	".\buffdata.h"\
	".\ctab.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\lines.h"\
	".\preset.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\dtchars.obj"	"$(INTDIR)\dtchars.sbr" : $(SOURCE) $(DEP_CPP_DTCHA)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\dtelnet.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_DTELN=\
	".\about.h"\
	".\argv.h"\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\editor.h"\
	".\emul.h"\
	".\filetype.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\platform.h"\
	".\preset.h"\
	".\printing.h"\
	".\proxy.h"\
	".\raw.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termdef.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\dtelnet.obj" : $(SOURCE) $(DEP_CPP_DTELN) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_DTELN=\
	".\about.h"\
	".\argv.h"\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\editor.h"\
	".\emul.h"\
	".\filetype.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\platform.h"\
	".\preset.h"\
	".\printing.h"\
	".\proxy.h"\
	".\raw.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termdef.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\dtelnet.obj"	"$(INTDIR)\dtelnet.sbr" : $(SOURCE) $(DEP_CPP_DTELN)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\dtelnet.rc
DEP_RSC_DTELNE=\
	".\RES\DTELNET.ICO"\
	".\RES\FILETYPE.ICO"\
	".\version.h"\
	

"$(INTDIR)\dtelnet.res" : $(SOURCE) $(DEP_RSC_DTELNE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\editor.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_EDITO=\
	".\buffdata.h"\
	".\ctab.h"\
	".\editor.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\editor.obj" : $(SOURCE) $(DEP_CPP_EDITO) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_EDITO=\
	".\buffdata.h"\
	".\ctab.h"\
	".\editor.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\editor.obj"	"$(INTDIR)\editor.sbr" : $(SOURCE) $(DEP_CPP_EDITO)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\emul.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_EMUL_=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\editor.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\printing.h"\
	".\raw.h"\
	".\scroll.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\emul.obj" : $(SOURCE) $(DEP_CPP_EMUL_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_EMUL_=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\editor.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\printing.h"\
	".\raw.h"\
	".\scroll.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\emul.obj"	"$(INTDIR)\emul.sbr" : $(SOURCE) $(DEP_CPP_EMUL_)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\filetype.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_FILET=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\filetype.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\raw.h"\
	".\socket.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\filetype.obj" : $(SOURCE) $(DEP_CPP_FILET) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_FILET=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtchars.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\filetype.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\raw.h"\
	".\socket.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\filetype.obj"	"$(INTDIR)\filetype.sbr" : $(SOURCE) $(DEP_CPP_FILET)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\font.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_FONT_=\
	".\buffdata.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\font.obj" : $(SOURCE) $(DEP_CPP_FONT_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_FONT_=\
	".\buffdata.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\font.obj"	"$(INTDIR)\font.sbr" : $(SOURCE) $(DEP_CPP_FONT_)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\harap.c
DEP_CPP_HARAP=\
	".\buffdata.h"\
	".\harap.h"\
	

!IF  "$(CFG)" == "dtelnet - Win32 Release"


"$(INTDIR)\harap.obj" : $(SOURCE) $(DEP_CPP_HARAP) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"


"$(INTDIR)\harap.obj"	"$(INTDIR)\harap.sbr" : $(SOURCE) $(DEP_CPP_HARAP)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\lines.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_LINES=\
	".\buffdata.h"\
	".\ctab.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\lines.obj" : $(SOURCE) $(DEP_CPP_LINES) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_LINES=\
	".\buffdata.h"\
	".\ctab.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\lines.obj"	"$(INTDIR)\lines.sbr" : $(SOURCE) $(DEP_CPP_LINES)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\log.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_LOG_C=\
	".\buffdata.h"\
	".\ctab.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\log.obj" : $(SOURCE) $(DEP_CPP_LOG_C) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_LOG_C=\
	".\buffdata.h"\
	".\ctab.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\log.obj"	"$(INTDIR)\log.sbr" : $(SOURCE) $(DEP_CPP_LOG_C)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\printing.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_PRINT=\
	".\buffdata.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\printing.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\printing.obj" : $(SOURCE) $(DEP_CPP_PRINT) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_PRINT=\
	".\buffdata.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\printing.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\printing.obj"	"$(INTDIR)\printing.sbr" : $(SOURCE) $(DEP_CPP_PRINT)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\proxy.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_PROXY=\
	".\buffdata.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\proxy.h"\
	".\socket.h"\
	".\socksdef.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\proxy.obj" : $(SOURCE) $(DEP_CPP_PROXY) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_PROXY=\
	".\buffdata.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\proxy.h"\
	".\socket.h"\
	".\socksdef.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\proxy.obj"	"$(INTDIR)\proxy.sbr" : $(SOURCE) $(DEP_CPP_PROXY)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\raw.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_RAW_C=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\protocol.h"\
	".\raw.h"\
	".\socket.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\raw.obj" : $(SOURCE) $(DEP_CPP_RAW_C) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_RAW_C=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\protocol.h"\
	".\raw.h"\
	".\socket.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\raw.obj"	"$(INTDIR)\raw.sbr" : $(SOURCE) $(DEP_CPP_RAW_C)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\scroll.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_SCROL=\
	".\buffdata.h"\
	".\font.h"\
	".\lines.h"\
	".\preset.h"\
	".\scroll.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\scroll.obj" : $(SOURCE) $(DEP_CPP_SCROL) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_SCROL=\
	".\buffdata.h"\
	".\font.h"\
	".\lines.h"\
	".\preset.h"\
	".\scroll.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\scroll.obj"	"$(INTDIR)\scroll.sbr" : $(SOURCE) $(DEP_CPP_SCROL)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\socket.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_SOCKE=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\platform.h"\
	".\preset.h"\
	".\proxy.h"\
	".\raw.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\socket.obj" : $(SOURCE) $(DEP_CPP_SOCKE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_SOCKE=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\platform.h"\
	".\preset.h"\
	".\proxy.h"\
	".\raw.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\socket.obj"	"$(INTDIR)\socket.sbr" : $(SOURCE) $(DEP_CPP_SOCKE)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\status.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_STATU=\
	".\buffdata.h"\
	".\ctab.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\status.obj" : $(SOURCE) $(DEP_CPP_STATU) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_STATU=\
	".\buffdata.h"\
	".\ctab.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\status.obj"	"$(INTDIR)\status.sbr" : $(SOURCE) $(DEP_CPP_STATU)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\term.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_TERM_=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\platform.h"\
	".\preset.h"\
	".\raw.h"\
	".\scroll.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\term.obj" : $(SOURCE) $(DEP_CPP_TERM_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_TERM_=\
	".\buffdata.h"\
	".\connect.h"\
	".\ctab.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\platform.h"\
	".\preset.h"\
	".\raw.h"\
	".\scroll.h"\
	".\socket.h"\
	".\status.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\term.obj"	"$(INTDIR)\term.sbr" : $(SOURCE) $(DEP_CPP_TERM_)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\termdef.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_TERMD=\
	".\buffdata.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\term.h"\
	".\termdef.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\termdef.obj" : $(SOURCE) $(DEP_CPP_TERMD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_TERMD=\
	".\buffdata.h"\
	".\ctab.h"\
	".\dialog.h"\
	".\dtelnet.h"\
	".\emul.h"\
	".\font.h"\
	".\lines.h"\
	".\platform.h"\
	".\preset.h"\
	".\term.h"\
	".\termdef.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\termdef.obj"	"$(INTDIR)\termdef.sbr" : $(SOURCE) $(DEP_CPP_TERMD)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\termkey.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_TERMK=\
	".\buffdata.h"\
	".\harap.h"\
	".\preset.h"\
	".\termkey.h"\
	".\utils.h"\
	

"$(INTDIR)\termkey.obj" : $(SOURCE) $(DEP_CPP_TERMK) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_TERMK=\
	".\buffdata.h"\
	".\harap.h"\
	".\preset.h"\
	".\termkey.h"\
	".\utils.h"\
	

"$(INTDIR)\termkey.obj"	"$(INTDIR)\termkey.sbr" : $(SOURCE) $(DEP_CPP_TERMK)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\termwin.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_TERMW=\
	".\buffdata.h"\
	".\font.h"\
	".\lines.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\termwin.obj" : $(SOURCE) $(DEP_CPP_TERMW) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_TERMW=\
	".\buffdata.h"\
	".\font.h"\
	".\lines.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\termwin.obj"	"$(INTDIR)\termwin.sbr" : $(SOURCE) $(DEP_CPP_TERMW)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\utils.c

!IF  "$(CFG)" == "dtelnet - Win32 Release"

DEP_CPP_UTILS=\
	".\buffdata.h"\
	".\dtelnet.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\utils.obj" : $(SOURCE) $(DEP_CPP_UTILS) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dtelnet - Win32 Debug"

DEP_CPP_UTILS=\
	".\buffdata.h"\
	".\dtelnet.h"\
	".\font.h"\
	".\lines.h"\
	".\log.h"\
	".\preset.h"\
	".\term.h"\
	".\termkey.h"\
	".\termwin.h"\
	".\utils.h"\
	

"$(INTDIR)\utils.obj"	"$(INTDIR)\utils.sbr" : $(SOURCE) $(DEP_CPP_UTILS)\
 "$(INTDIR)"


!ENDIF 


!ENDIF 

