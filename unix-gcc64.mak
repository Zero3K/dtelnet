# unix-Makefile
WINCC	   := win64-gcc			# create a symlink to your crosscompiler
WINCPP	   := win64-cpp
WINRC	   := win64-windres
WINCFLAGS  := ${CFLAGS}  -W -Wall -Wextra -Wno-unused-parameter -mwindows
WINCPPFLAGS:= ${CPPFLAGS}
WINLDFLAGS := ${LDFLAGS} -mwindows -Wl,--enable-stdcall-fixup
WINLIBS    := -lwinspool -lcomdlg32 -lgdi32 -lkernel32 -lws2_32

WIN32CC	   := win32-gcc			# create a symlink to your crosscompiler
WIN32RC	   := win32-windres

CC	   := gcc
CPP	   := cpp
CFLAGS	   := -W -Wall -Wextra -Wno-unused-parameter -m64 -g
CPPFLAGS   := -I.			# windows.h
LDFLAGS	   := -m64 -g

MODULES := \
	about   attrib   argv    babit   buffdata connect  ctab	    \
	dialog  dtchars  dtelnet editor  emul     envvars  filetype \
	font    graphmap harap	 lines	 log      printing proxy    \
	raw	res     scroll	 select  socket   status   term     \
	termdef termkey termwin  uconv   utils

all: ${OBJS} dt64.exe dt64_debug.exe

OBJDIR := obj-win64 obj-win32 obj-unix64

clean:
	-rm -f *.map *.tds *.tr2 *.td2 *.o *.obj *.res res.rc obj-win64/* obj-unix64/* 2>/dev/null
	-rm -f $(foreach d,${OBJDIR},$d/*) 2>/dev/null

dt64.exe: $(foreach M,$(MODULES),obj-win64/$M.o)
	ln -sf hhctrl64.ocx hhctrl.ocx
	${WINCC} -o $@ ${WINLDFLAGS} $^ ${WINLIBS} hhctrl.ocx

dt32.exe: $(foreach M,$(MODULES),obj-win32/$M.o)
	ln -sf hhctrl32.ocx hhctrl.ocx
	${WIN32CC} -o $@ ${WINLDFLAGS} $^ ${WINLIBS} hhctrl.ocx

dt64_debug.exe: $(foreach M,$(MODULES),obj-win64/$M.o) obj-win64/debuglog.o
	${WINCC} -o $@ ${WINLDFLAGS} $^ ${WINLIBS}

dt32_debug.exe: $(foreach M,$(MODULES),obj-win32/$M.o) obj-win32/debuglog.o
	ln -sf hhctrl32.ocx hhctrl.ocx
	${WIN32CC} -o $@ ${WINLDFLAGS} $^ ${WINLIBS} hhctrl.ocx

TERMKEYT_OBJS := termkeyt termkey utils buffdata harap
termkeyt.exe: $(foreach m,${TERMKEYT_OBJS},obj-win64/$m.o)
	${WINCC} -o $@ $^

ENVVARST_OBJS := envvarst envvars utils buffdata babit harap
envvarst.exe: $(foreach m,${ENVVARST_OBJS},obj-win32/$m.o)
	${WIN32CC} -o $@ $^

envvarst:  $(foreach m,${ENVVARST_OBJS},obj-unix64/$m.o)
	${CC} ${LDFLAGS} -o $@ $^

obj-win64/res.o: res.rc
	$(WINRC) $< $@
obj-win32/res.o: res.rc
	$(WIN32RC) $< $@

res.rc:	dtelnet.rc
	echo 's|RES\\\\\\\\DTELNET.ICO|res/dtelnet.ico|' >res.tmp
	echo 's|RES\\\\\\\\FILETYPE.ICO|res/filetype.ico|' >>res.tmp
	sed -fres.tmp "$<" >"$@"

IMPORTANT = \
	$(wildcard *.c)   $(wildcard *.h)   $(wildcard *.txt) $(wildcard *.TXT) \
	$(wildcard *.set) $(wildcard *.in0) $(wildcard *.sh)  $(wildcard *.mak) \
	$(wildcard *.bat) $(wildcard *.html)

save:
	rsync -avu ${IMPORTANT} saveloc

load:
	cd saveloc; rsync -avu ${IMPORTANT} ${PWD}

rmcr:	
	rmcr ${IMPORTANT}

winsave:
	rsync -avu ${IMPORTANT} winloc
	cd winloc; adcr ${IMPORTANT}

winload:
	cd winloc; rsync -avu ${IMPORTANT} ${PWD}
	rmcr ${IMPORTANT}

obj-win32/%.o:	%.c
	${WIN32CC} ${WINCFLAGS} ${WINCPPFLAGS} -o $@ -c $<
obj-win64/%.o:	%.c
	${WINCC} ${WINCFLAGS} ${WINCPPFLAGS} -o $@ -c $<
obj-win64/%.E: %.c
	${WINCPP} ${WINCPPFLAGS} -dD $*.c $@

obj-unix64/%.o:	%.c
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ -c $<
obj-unix64/%.E:	%.c
	${CPP} ${CFLAGS} ${CPPFLAGS} -dD $*.c $@

OBJDIRS := obj-win32 obj-win64 obj-unix32 obj-unix64

$(foreach d,${OBJDIRS},$d/select.o):  select.c termwin.h select.h
$(foreach d,${OBJDIRS},$d/scroll.o):  scroll.c scroll.h termwin.h select.h
$(foreach d,${OBJDIRS},$d/term.o):    term.c term.h termwin.h select.h
$(foreach d,${OBJDIRS},$d/termwin.o): termwin.c termwin.h
$(foreach d,${OBJDIRS},$d/emul.o):    emul.c term.h termwin.h select.h emul.h platform.h preset.h
$(foreach d,${OBJDIRS},$d/babit.o):   babit.h babit.c
$(foreach d,${OBJDIRS},$d/buffdata.o):buffdata.c buffdata.h
$(foreach d,${OBJDIRS},$d/envvars.o): envvars.c envvars.h buffdata.h babit.h harap.h
$(foreach d,${OBJDIRS},$d/envvarst.o):envvarst.c envvars.h utils.h
