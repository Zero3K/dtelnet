#
# makefile for gcc (mingwin and cygwin)
# (by massimo morara)
#
# use: make -f gcc.mak
#

DUMALIB=dumalib

CC       = gcc
LD       = gcc
WR       = windres
CC_FLAGS = -g -c -Wall -Wextra -W -Wno-unused-parameter -Wmissing-prototypes \
	   -I${DUMALIB}	# -mno-cygwin
LD_FLAGS = -g -mwindows # -s # -mno-cygwin
WR_FLAGS = -i
CC__OBJS = about.o argv.o buffdata.o ctab.o connect.o 			\
	   dialog.o dtchars.o dtelnet.o					\
           editor.o emul.o filetype.o font.o harap.o			\
	   lines.o log.o printing.o					\
           proxy.o raw.o scroll.o socket.o status.o			\
	   term.o termdef.o termkey.o					\
	   termwin.o utils.o
RES_FILE = dtelnet.rc
RES_OBJS = resource.o
TARGET   = dtelnet.exe
LIBS     = -L${DUMALIB} -lduma -lws2_32 -lwinspool


all: $(TARGET)

clean:
	-rm *.obj *.o *.td2 *.tr2 *.bak *.csm *.dsw *.dbg *.~de *.obr *.res *.exe *.map *.tds 2>/dev/null

do/%.o: %.c
	$(CC) -o $@ $(CC_FLAGS) $*.c
        
$(RES_OBJS): $(RES_FILE)
	$(WR) -o $@ $(WR_FLAGS) $(RES_FILE)

$(TARGET): $(foreach L,$(CC__OBJS),do/$L) $(RES_OBJS)
	$(LD) -o $@ $(LD_FLAGS) $^ $(LIBS)

IMPORTANT = *.c *.h *.txt *.TXT *.set *.in0 *.sh *.mak *.bat

save:
	rsync -avu ${IMPORTANT} saveloc

load:
	cd saveloc; rsync -avu ${IMPORTANT} ..
