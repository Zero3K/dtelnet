/* graphmap.c
 * Copyright (c) 2017 David Cole
 */

#include "graphmap.h"

/*
#1    char ascii	-- unused, documentation only: 0x20 .. 0x7f
#2    char ansi		-- ANSI code: Windows-1250 (or 1252, etc  -- that's why we move to Unicode)
#3    char oem		-- OEM code:  CP437 (or CP850, CP852, etc -- that's why we move to Unicode)
#4    uint16_t wide	-- Unicode (for now, BMP only (BMP = Basic Multilingual Plane))
*/

const GraphMap graphmap [6*16] = {
/* 20   */ {' ',  0 ,    0,      0},
/* 21   */ {'!',  0 ,    0,      0},
/* 22   */ {'"',  0 ,    0,      0},
/* 23   */ {'#',  0 ,    0,      0},
/* 24   */ {'$',  0 ,    0,      0},
/* 25   */ {'%',  0 ,    0,      0},
/* 26   */ {'&',  0 ,    0,      0},
/* 27   */ {'\'', 0 ,    0,      0},
/* 28   */ {'(',  0 ,    0,      0},
/* 29   */ {')',  0 ,    0,      0},
/* 2a   */ {'*',  0 ,    0,      0},
/* 2b → */ {'+', '>', 0x10, 0x2192},	/* ACS_RARROW	*/
/* 2c ← */ {',', '<', 0x11, 0x2190},	/* ACS_LARROW	*/
/* 2d ↑ */ {'-', '^', 0x18, 0x2191},	/* ACS_UARROW	*/
/* 2e ↓ */ {'.', 'v', 0x19, 0x2193},	/* ACS_DARROW	*/
/* 2f   */ {'/',  0 ,    0,      0},

/* 30 █ */ {'0', '#', 0xDB, 0x2588},	/* ACS_BLOCK	*/
/* 31   */ {'1',  0 ,    0,      0},
/* 32   */ {'2',  0 ,    0,      0},
/* 33   */ {'3',  0 ,    0,      0},
/* 34   */ {'4',  0 ,    0,      0},
/* 35   */ {'5',  0 ,    0,      0},
/* 36   */ {'6',  0 ,    0,      0},
/* 37   */ {'7',  0 ,    0,      0},
/* 38   */ {'8',  0 ,    0,      0},
/* 39   */ {'9',  0 ,    0,      0},
/* 3a   */ {':',  0 ,    0,      0},
/* 3b   */ {';',  0 ,    0,      0},
/* 3c   */ {'<',  0 ,    0,      0},
/* 3d   */ {'=',  0 ,    0,      0},
/* 3e   */ {'>',  0 ,    0,      0},
/* 3f   */ { 0 ,  0 ,    0,      0},

/* 40   */ {'@',  0 ,    0,      0},
/* 41   */ {'A',  0 ,    0,      0},
/* 42   */ {'B',  0 ,    0,      0},
/* 43   */ {'C',  0 ,    0,      0},
/* 44   */ {'D',  0 ,    0,      0},
/* 45   */ {'E',  0 ,    0,      0},
/* 46   */ {'F',  0 ,    0,      0},
/* 47   */ {'G',  0 ,    0,      0},
/* 48   */ {'H',  0 ,    0,      0},
/* 49   */ {'I',  0 ,    0,      0},
/* 4a   */ {'J',  0 ,    0,      0},
/* 4b   */ {'K',  0 ,    0,      0},
/* 4c   */ {'L',  0 ,    0,      0},
/* 4d   */ {'M',  0 ,    0,      0},
/* 4e   */ {'N',  0 ,    0,      0},
/* 4f   */ {'O',  0 ,    0,      0},

/* 50   */ {'P',  0 ,    0,      0},
/* 51   */ {'Q',  0 ,    0,      0},
/* 52   */ {'R',  0 ,    0,      0},
/* 53   */ {'S',  0 ,    0,      0},
/* 54   */ {'T',  0 ,    0,      0},
/* 55   */ {'U',  0 ,    0,      0},
/* 56   */ {'V',  0 ,    0,      0},
/* 57   */ {'W',  0 ,    0,      0},
/* 58   */ {'X',  0 ,    0,      0},
/* 59   */ {'Y',  0 ,    0,      0},
/* 5a   */ {'Z',  0 ,    0,      0},
/* 5b   */ {'[',  0 ,    0,      0},
/* 5c   */ {'\\', 0 ,    0,      0},
/* 5d   */ {']',  0 ,    0,      0},
/* 5e   */ {'^',  0 ,    0,      0},
/* 5f   */ {'_',  0 ,    0,      0},

/* 60 ♦ */ {'`', '+', 0x04, 0x2666},	/* ACS_DIAMOND */
/* 61 ░ */ {'a', ':', 0xB0, 0x2591},	/* ACS_CKBOARD */
/* 62   */ {'b',  0 ,    0,      0},
/* 63   */ {'c',  0 ,    0,      0},
/* 64   */ {'d',  0 ,    0,      0},
/* 65   */ {'e',  0 ,    0,      0},
/* 66 ° */ {'f','\'', 0xF8, 0x00B0},	/* ACS_DEGREE	*/
/* 67 ± */ {'g', '#', 0xF1, 0x00B1},	/* ACS_PLMINUS	*/
/* 68 ▒ */ {'h', '#', 0xB1, 0x2592},	/* ACS_BOARD	*/
/* 69 § */ {'i', '#', 0x15, 0x00A7},	/* ACS_LANTERN	*/
/* 6a ┘ */ {'j', '+', 0xD9, 0x2518},	/* ACS_LRCORNER */
/* 6b ┐ */ {'k', '+', 0xBF, 0x2510},	/* ACS_URCORNER */
/* 6c ┌ */ {'l', '+', 0xDA, 0x250C},	/* ACS_ULCORNER */
/* 6d └ */ {'m', '+', 0xC0, 0x2514},	/* ACS_LLCORNER	*/
/* 6e ┼ */ {'n', '+', 0xC5, 0x253C},	/* ACS_PLUS	*/
/* 6f ⎺ */ {'o', '~', 0x7E, 0x23BA},	/* ACS_S1	e2 8e ba */

/* 70 ⎻ */ {'p', '-', 0xC4, 0x23BB},	/* ACS_S3	e2 8e bb */
/* 71 ─ */ {'q', '-', 0xC4, 0x2500},	/* ACS_HLINE	e2 94 80 */
/* 72 ⎼ */ {'r', '-', 0xC4, 0x23BC},	/* ACS_S7	e2 8e bc */
/* 73 ⎽ */ {'s', '_', 0x5F, 0x23BD},	/* ACS_S9	e2 8e bd */
/* 74 ├ */ {'t', '+', 0xC3, 0x251C},	/* ACS_LTEE	*/
/* 75 ┤ */ {'u', '+', 0xB4, 0x2524},	/* ACS_RTEE	*/
/* 76 ┴ */ {'v', '+', 0xC1, 0x2534},	/* ACS_BTEE	*/
/* 77 ┬ */ {'w', '+', 0xC2, 0x252C},	/* ACS_TTEE	*/
/* 78 │ */ {'x', '|', 0xB3, 0x2502},	/* ACS_VLINE	*/
/* 79 ≤ */ {'y', '<', 0xF3, 0x2264},	/* ACS_LEQUAL	*/
/* 7a ≥ */ {'z', '>', 0xF2, 0x2265},	/* ACS_GEQUAL	*/
/* 7b π */ {'{', '*', 0xE3, 0x03C0},	/* ACS_PI	*/
/* 7c ≠ */ {'|', '!', 0xD8, 0x2260},	/* ACS_NEQUAL	*/
/* 7d £ */ {'}', 'f', 0x9C, 0x00A3},	/* ACS_STERLING */
/* 7e ∙ */ {'~', 'o', 0xF9, 0x2219},	/* ACS_BULLET	*/
/* 7f    */ {127,  0 ,    0,      0}
};

const GraphMap invalid_graph =
    {' ', '?', 0xB1, 0x2592};
