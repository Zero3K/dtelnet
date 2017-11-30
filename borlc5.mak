#
# Borland C++ IDE generated makefile
# Generated 6/25/2002 at 13:01:57 
#
.AUTODEPEND


#
# Borland C++ tools
#
IMPLIB  = Implib
BCC32   = Bcc32 +BccW32.cfg 
BCC32I  = Bcc32i +BccW32.cfg 
TLINK32 = TLink32
ILINK32 = Ilink32
TLIB    = TLib
BRC32   = Brc32
TASM32  = Tasm32
#
# IDE macros
#


#
# Options
#
IDE_LinkFLAGS32 =  -LC:\BC5\LIB
IDE_ResFLAGS32 = 
LinkerLocalOptsAtW32_dtelnetdexe =  -Tpe -aa -V4.0 -c
ResLocalOptsAtW32_dtelnetdexe = 
BLocalOptsAtW32_dtelnetdexe = 
CompInheritOptsAt_dtelnetdexe = -IC:\BC5\INCLUDE 
LinkerInheritOptsAt_dtelnetdexe = -x
LinkerOptsAt_dtelnetdexe = $(LinkerLocalOptsAtW32_dtelnetdexe)
ResOptsAt_dtelnetdexe = $(ResLocalOptsAtW32_dtelnetdexe)
BOptsAt_dtelnetdexe = $(BLocalOptsAtW32_dtelnetdexe)

#
# Dependency List
#
Dep_borlc5 = \
   dtelnet.exe

borlc5 : BccW32.cfg $(Dep_borlc5)
  echo MakeNode

Dep_dtelnetdexe = \
   proxy.obj\
   ctab.obj\
   dtchars.obj\
   dtelnet.res\
   filetype.obj\
   termdef.obj\
   editor.obj\
   printing.obj\
   emul.obj\
   argv.obj\
   connect.obj\
   dialog.obj\
   dtelnet.obj\
   font.obj\
   lines.obj\
   log.obj\
   raw.obj\
   socket.obj\
   status.obj\
   term.obj\
   utils.obj\
   dtelnet.def\
   about.obj

dtelnet.exe : $(Dep_dtelnetdexe)
  $(ILINK32) @&&|
 /v $(IDE_LinkFLAGS32) $(LinkerOptsAt_dtelnetdexe) $(LinkerInheritOptsAt_dtelnetdexe) +
C:\BC5\LIB\c0w32.obj+
proxy.obj+
ctab.obj+
dtchars.obj+
filetype.obj+
termdef.obj+
editor.obj+
printing.obj+
emul.obj+
argv.obj+
connect.obj+
dialog.obj+
dtelnet.obj+
font.obj+
lines.obj+
log.obj+
raw.obj+
socket.obj+
status.obj+
term.obj+
utils.obj+
about.obj
$<,$*
C:\BC5\LIB\import32.lib+
C:\BC5\LIB\cw32.lib
dtelnet.def
dtelnet.res

|
proxy.obj :  proxy.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ proxy.c
|

ctab.obj :  ctab.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ ctab.c
|

dtchars.obj :  dtchars.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ dtchars.c
|

dtelnet.res :  dtelnet.rc
  $(BRC32) -R @&&|
 $(IDE_ResFLAGS32) $(ROptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe)  -FO$@ dtelnet.rc
|
filetype.obj :  filetype.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ filetype.c
|

termdef.obj :  termdef.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ termdef.c
|

editor.obj :  editor.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ editor.c
|

printing.obj :  printing.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ printing.c
|

emul.obj :  emul.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ emul.c
|

argv.obj :  argv.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ argv.c
|

connect.obj :  connect.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ connect.c
|

dialog.obj :  dialog.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ dialog.c
|

dtelnet.obj :  dtelnet.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ dtelnet.c
|

font.obj :  font.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ font.c
|

lines.obj :  lines.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ lines.c
|

log.obj :  log.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ log.c
|

raw.obj :  raw.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ raw.c
|

socket.obj :  socket.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ socket.c
|

status.obj :  status.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ status.c
|

term.obj :  term.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ term.c
|

utils.obj :  utils.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ utils.c
|

about.obj :  about.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_dtelnetdexe) $(CompInheritOptsAt_dtelnetdexe) -o$@ about.c
|

# Compiler configuration file
BccW32.cfg : 
   Copy &&|
-w
-R
-v
-WM-
-vi
-H
-H=borl32.csm
-H"OWL\PCH.H"
-WE
-v-
-R-
-k
-pr
-Og
-Oi
-Ov
-He-
-x-
-xd-
-RT-
-O-d
-d
-N
-a
-Z
-O
-Oe
-Ol
-Ob
-O-a
-Om
-O-p
-w-cln
-wdef
-wnod
-wamb
-w-par
-wstv
-wamp
-w-obs
-wmsg
-waus
-wuse
-w-sig
-w-ucp
-H-
| $@


