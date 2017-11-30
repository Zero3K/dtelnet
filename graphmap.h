/* graphmap.h
 * Copyright (c) 2017 David Cole
 */

#ifndef GRAPHMAP_H
#define GRAPHMAP_H

typedef struct GraphMap {
    char ascii;			/* unused, documentation */
    char ansi;			/* substitue character */
    char oem;			/* OEM code */
    unsigned short wide;	/* Unicode -- 16 bit*/
} GraphMap;

extern const GraphMap graphmap [6*16]; /* 0x20..0x7f */

extern const GraphMap invalid_graph;

#define GraphMapValid(xtermcode) \
    ((xtermcode)>=0x20 && (xtermcode)<=0x7f && \
     graphmap[(xtermcode)-0x20].ansi != 0)

#define GraphMapAnsi(xtermcode) \
    (GraphMapValid((xtermcode)) ? \
     graphmap[(xtermcode)-0x20].ansi: \
     invalid_graph.ansi)

#define GraphMapOem(xtermcode) \
    (GraphMapValid((xtermcode)) ? \
     graphmap[(xtermcode)-0x20].oem: \
     invalid_graph.oem)

#define GraphMapWide(xtermcode) \
    (GraphMapValid((xtermcode)) ? \
     graphmap[(xtermcode)-0x20].wide: \
     invalid_graph.wide)

#endif
