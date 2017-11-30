# Microsoft Visual C++ generated build script - Do not modify

PROJ = DTELNET
DEBUG = 0
PROGTYPE = 0
CALLER = 
ARGS = /C yellow-blue
DLLS = 
D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = E:\WIN32\DTELNET\
USEMFC = 0
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = 
CUSEPCHFLAG = 
CPPUSEPCHFLAG = 
FIRSTC = ABOUT.C     
FIRSTCPP =             
RC = rc
CFLAGS_D_WEXE = /nologo /G2 /W3 /Zi /AL /Od /D "_DEBUG" /FR /GA /Fd"DTELNET.PDB"
CFLAGS_R_WEXE = /nologo /W3 /AL /O1 /D "NDEBUG" /GA 
LFLAGS_D_WEXE = /NOLOGO /NOD /PACKC:61440 /STACK:10240 /ALIGN:16 /ONERROR:NOEXE /CO  
LFLAGS_R_WEXE = /NOLOGO /NOD /PACKC:61440 /STACK:10240 /ALIGN:16 /ONERROR:NOEXE  
LIBS_D_WEXE = winsock libw llibcew oldnames commdlg.lib olecli.lib olesvr.lib shell.lib 
LIBS_R_WEXE = winsock libw llibcew oldnames commdlg.lib olecli.lib olesvr.lib shell.lib 
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
DEFFILE = DTELNET.DEF
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WEXE)
LFLAGS = $(LFLAGS_D_WEXE)
LIBS = $(LIBS_D_WEXE)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_WEXE)
LFLAGS = $(LFLAGS_R_WEXE)
LIBS = $(LIBS_R_WEXE)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = ABOUT.SBR \
		ARGV.SBR \
		CONNECT.SBR \
		EMUL.SBR \
		FONT.SBR \
		LINES.SBR \
		LOG.SBR \
		RAW.SBR \
		SOCKET.SBR \
		TERM.SBR \
		UTILS.SBR \
		STATUS.SBR \
		DIALOG.SBR \
		DTELNET.SBR


ABOUT_DEP = e:\win32\dtelnet\platform.h \
	e:\win32\dtelnet\about.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\dialog.h


ARGV_DEP = e:\win32\dtelnet\argv.h


CONNECT_DEP = e:\win32\dtelnet\platform.h \
	e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\connect.h \
	e:\win32\dtelnet\emul.h \
	e:\win32\dtelnet\argv.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\socket.h \
	e:\win32\dtelnet\raw.h \
	e:\win32\dtelnet\dialog.h


EMUL_DEP = e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\termwin.h \
	e:\win32\dtelnet\lines.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\raw.h \
	e:\win32\dtelnet\emul.h \
	e:\win32\dtelnet\socket.h \
	e:\win32\dtelnet\log.h \
	e:\win32\dtelnet\status.h


FONT_DEP = e:\win32\dtelnet\platform.h \
	e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\font.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\dialog.h


LINES_DEP = e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\termwin.h \
	e:\win32\dtelnet\lines.h \
	e:\win32\dtelnet\emul.h


LOG_DEP = e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\log.h \
	e:\win32\dtelnet\emul.h


RAW_DEP = e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\protocol.h \
	e:\win32\dtelnet\emul.h \
	e:\win32\dtelnet\connect.h \
	e:\win32\dtelnet\socket.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\raw.h \
	e:\win32\dtelnet\log.h


SOCKET_DEP = e:\win32\dtelnet\platform.h \
	e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\emul.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\connect.h \
	e:\win32\dtelnet\raw.h \
	e:\win32\dtelnet\socket.h \
	e:\win32\dtelnet\status.h \
	e:\win32\dtelnet\log.h


TERM_DEP = e:\win32\dtelnet\platform.h \
	e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\termwin.h \
	e:\win32\dtelnet\lines.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\socket.h \
	e:\win32\dtelnet\raw.h \
	e:\win32\dtelnet\font.h \
	e:\win32\dtelnet\status.h \
	e:\win32\dtelnet\emul.h \
	e:\win32\dtelnet\log.h


UTILS_DEP = e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\log.h


STATUS_DEP = e:\win32\dtelnet\platform.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\emul.h \
	e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\status.h


DIALOG_DEP = e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\dialog.h


DTELNET_DEP = e:\win32\dtelnet\platform.h \
	e:\win32\dtelnet\utils.h \
	e:\win32\dtelnet\log.h \
	e:\win32\dtelnet\dtelnet.h \
	e:\win32\dtelnet\term.h \
	e:\win32\dtelnet\socket.h \
	e:\win32\dtelnet\about.h \
	e:\win32\dtelnet\connect.h \
	e:\win32\dtelnet\status.h \
	e:\win32\dtelnet\font.h \
	e:\win32\dtelnet\argv.h \
	e:\win32\dtelnet\dialog.h \
	e:\win32\dtelnet\emul.h \
	e:\win32\dtelnet\raw.h


DTELNET_RCDEP = e:\win32\dtelnet\res\dtelnet.ico


all:	$(PROJ).EXE

ABOUT.OBJ:	ABOUT.C $(ABOUT_DEP)
	$(CC) $(CFLAGS) $(CCREATEPCHFLAG) /c ABOUT.C

ARGV.OBJ:	ARGV.C $(ARGV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ARGV.C

CONNECT.OBJ:	CONNECT.C $(CONNECT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONNECT.C

EMUL.OBJ:	EMUL.C $(EMUL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c EMUL.C

FONT.OBJ:	FONT.C $(FONT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c FONT.C

LINES.OBJ:	LINES.C $(LINES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c LINES.C

LOG.OBJ:	LOG.C $(LOG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c LOG.C

RAW.OBJ:	RAW.C $(RAW_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c RAW.C

SOCKET.OBJ:	SOCKET.C $(SOCKET_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SOCKET.C

TERM.OBJ:	TERM.C $(TERM_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TERM.C

UTILS.OBJ:	UTILS.C $(UTILS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c UTILS.C

STATUS.OBJ:	STATUS.C $(STATUS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c STATUS.C

DIALOG.OBJ:	DIALOG.C $(DIALOG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c DIALOG.C

DTELNET.OBJ:	DTELNET.C $(DTELNET_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c DTELNET.C

DTELNET.RES:	DTELNET.RC $(DTELNET_RCDEP)
	$(RC) $(RCFLAGS) $(RCDEFINES) -r DTELNET.RC


$(PROJ).EXE::	DTELNET.RES

$(PROJ).EXE::	ABOUT.OBJ ARGV.OBJ CONNECT.OBJ EMUL.OBJ FONT.OBJ LINES.OBJ LOG.OBJ \
	RAW.OBJ SOCKET.OBJ TERM.OBJ UTILS.OBJ STATUS.OBJ DIALOG.OBJ DTELNET.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
ABOUT.OBJ +
ARGV.OBJ +
CONNECT.OBJ +
EMUL.OBJ +
FONT.OBJ +
LINES.OBJ +
LOG.OBJ +
RAW.OBJ +
SOCKET.OBJ +
TERM.OBJ +
UTILS.OBJ +
STATUS.OBJ +
DIALOG.OBJ +
DTELNET.OBJ +
$(OBJS_EXT)
$(PROJ).EXE
$(MAPFILE)
c:\msvc\lib\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) DTELNET.RES $@
	@copy $(PROJ).CRF MSVC.BND

$(PROJ).EXE::	DTELNET.RES
	if not exist MSVC.BND 	$(RC) $(RESFLAGS) DTELNET.RES $@

run: $(PROJ).EXE
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
