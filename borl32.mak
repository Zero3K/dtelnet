# Makefile for Borland BCC55
#
# If BCC55 installed in X:\PATH\BCC55
# edit X:\PATH\BCC55\BIN\BUILTINS.MAK
# add the following variables:
#	LD      = ILINK32
#	CFLAGS  = -IX:\PATH\BCC55\INCLUDE -LX:\PATH\BCC55\LIB
#	RFLAGS  = -iX:\PATH\BCC55\INCLUDE
#	LDFLAGS = -LX:\PATH\BCC55\LIB
#
# use:  PATH X:\PATH\BIN;%PATH%
#	make -fborl32.mak all

.AUTODEPEND

CFLAGS = $(CFLAGS) -v -w -P- -tW -DOLD_WIN32_COMPILER
LDFLAGS = $(LDFLAGS) -Tpe -aa -w -Gn -v -s -V4.0

all: dtelnet.exe

# bcc55 comes with an old 'ws2_32.lib' that doesn't have getaddrinfo

ws2_32.lib: ${WINDIR}\SYSTEM32\WS2_32.DLL
	implib -f $@ ${WINDIR}\SYSTEM32\WS2_32.DLL

ws2_32.def: ${WINDIR}\SYSTEM32\WS2_32.DLL
	impdef $@ ${WINDIR}\SYSTEM32\WS2_32.DLL

# also there is no hhctrl.lib
hhctrl.lib: ${WINDIR}\SYSTEM32\HHCTRL.OCX
	implib -f $@ ${WINDIR}\SYSTEM32\HHCTRL.OCX

hhctrl.def: ${WINDIR}\SYSTEM32\HHCTRL.OCX
	impdef $@ ${WINDIR}\SYSTEM32\HHCTRL.OCX

clean:
	erase *.obj *.o *.td2 *.tr2 *.bak *.csm *.dsw *.dbg *.~de *.obr
	erase dtelnet.res
	erase dtelnet.exe
	erase dtelnet.map
	erase dtelnet.tds

OBJS = \
   about.obj\
   attrib.obj\
   argv.obj\
   babit.obj\
   buffdata.obj\
   connect.obj\
   ctab.obj\
   dialog.obj\
   dtchars.obj\
   dtelnet.obj\
   editor.obj\
   emul.obj\
   envvars.obj\
   filetype.obj\
   font.obj\
   graphmap.obj\
   harap.obj\
   lines.obj\
   log.obj\
   printing.obj\
   proxy.obj\
   raw.obj\
   select.obj\
   scroll.obj\
   socket.obj\
   status.obj\
   term.obj\
   termdef.obj\
   termkey.obj\
   termwin.obj\
   uconv.obj\
   utils.obj

dtelnet.exe: $(OBJS) dtelnet.def dtelnet.res
	${LD} @&&|
	$(LDFLAGS) c0w32.obj $(OBJS)
	dtelnet.exe
	dtelnet.map
	import32.lib cw32.lib ws2_32.lib hhctrl.lib
	dtelnet.def
	dtelnet.res
|

.c.obj:
	$(CC) -c $(CFLAGS) -o$@ $<

.rc.res:
	$(RC) $(RFLAGS) -r -fo$@ $<

TKTOBJS= termkeyt.obj termkey.obj utils.obj harap.obj buffdata.obj

termkeyt.exe: ${TKTOBJS}
	$(CC) -v ${TKTOBJS}

ENVTOBJS= envvarst.obj envvars.obj babit.obj utils.obj

envvarst.exe: ${ENVTOBJS}
	$(CC) -v ${ENVTOBJS}
