#
# makefile for gcc (mingwin and cygwin)
# (by massimo morara)
#
# use: make -f gcc.mak
#

CC       = gcc
LD       = gcc
WR       = windres
CC_FLAGS = -g -c -Wall -Wextra -W -Wno-unused-parameter -Wmissing-prototypes # -mno-cygwin
LD_FLAGS = -g -mwindows # -s # -mno-cygwin
WR_FLAGS = -i

MODULES := \
	about     ctab     editor  harap    proxy  termdef termwin	\
	utils     argv     dialog  emul     lines  raw     socket  	\
	termkey   buffdata dtchars filetype log    scroll  status 	\
	connect   dtelnet  font    printing term   resource select

OBJS    := $(foreach M,$(MODULES),$M.o)
CC__OBJS:= $(foreach O,$(OBJS),$O)

RES_FILE = dtelnet.rc
TARGET   = dtelnet.exe
LIBS     = -lws2_32 -lwinspool

all: $(TARGET)

clean:
	-rm *.obj *.o *.td2 *.tr2 *.bak *.csm *.dsw *.dbg *.~de *.obr *.res *.exe *.map *.tds 2>/dev/null

%.o: %.c
	$(CC) -o $@ $(CC_FLAGS) $*.c
        
resource.o: $(RES_FILE)
	$(WR) -o $@ $(WR_FLAGS) $(RES_FILE)
        
$(TARGET): $(CC__OBJS) $(RES_OBJS)
	$(LD) -o $@ $(LD_FLAGS) $(CC__OBJS) $(RES_OBJS) $(LIBS)


IMPORTANT = *.c *.h *.txt *.TXT *.set *.in0 *.sh *.mak *.bat

save:
	rsync -avu ${IMPORTANT} saveloc

load:
	cd saveloc; rsync -avu ${IMPORTANT} ..

