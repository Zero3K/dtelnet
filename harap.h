/* harap.h */

#ifndef HARAP_H
#define HARAP_H

#include "buffdata.h"

extern int xharap (const ConstBuffData *text, BuffData *word,
		   BuffData *rest, int sep, int dir);

#define XHARAP_RETMOREINFO 4

#define harap(t,w,r,s) xharap ((t), (w), (r), (s), 1)
#define jharap(t,w,r,s) xharap ((t), (w), (r), (s), -1)

#define harapr(t,w,r,s) xharap ((t), (w), (r), (s), XHARAP_RETMOREINFO+1)
#define jharapr(t,w,r,s) xharap ((t), (w), (r), (s), XHARAP_RETMOREINFO-1)

/*
   2008.09.15. LZS:
    Mostantól a tabulátort is szóközként kezeljük...
    ezt eddig elfelejtettem...

   2006.05.09. LZS:

   RETMOREINFO: a visszaadott értéket finomabban állítja.
    nélküle:
	-1: az input üres (szóközöket leszámítva)
	 0: talált szeparátort, visszaadja a szeparátor elõtti részt
	    (lehet üres is),
	    vagy nem talált szeparátort, visszaadja a teljes input-ot
	    (amely nem üres)
    opcióval:
	-1: az input üres (szóközöket leszámítva)
	 0: talált szeparátort, visszaadja a szeparátor elõtti részt
	    (lehet üres is)
	 1: nem talált szeparátort, visszaadja a teljes input-ot
	    (amely nem üres)

   2002.08.29. LZS:
   lett egy jharap, aki a string jobboldalarol vag le egy tokent

   Harap: a lexikalis elemzes ujabb eszkoze; levalasztja az inputbol
   az elso reszt, amelyet a megadott separator hatarol.
   A separator karaktert es a szokozoket lenyeli, lasd a peldakat.
   Hasznos lehetoseg, hogy a szokoz is lehet separator, lasd a peldakat.

   Input: az elemzendo szoveg (text) es a separator (sep)
   Output: a megtalalt szo (word), es a maradek (rest)
	   (mindketto opcionalis)
   Return: 0 ha talalt elemet, EOF ha nem (lasd a peldakat)

   Megjegyzes: a 'BuffData' strukturat a 'buffdata.h' -ban talaljuk.

   Hivasi peldak:

   "ABC:DEF",	sep=':'  ===>  "ABC"+"DEF",return=0
   " ABC : DEF",sep=':'  ===>  "ABC"+"DEF",return=0
   "  :  "	sep=':'  ===>  ""   +"",   return=0
   "     "	sep=':'  ===>  ""   +"",   return=EOF
   ":DEF"	sep=':'  ===>  ""   +"DEF",return=0
   "ABCDEF"	sep=':'  ===>  "ABCDEF"+"",return=0 (harapr: return=1)

   "ABC:DEF",	sep=' '  ===>  "ABC:DEF"+"", return=0 (harapr: return=1)
   " ABC : DEF",sep=' '  ===>  "ABC"+": DEF",return=0
   "  :  "	sep=' '  ===>  ":"+"",	     return=0
   "     "	sep=' '  ===>  ""+"",	     return=EOF
   ":DEF"	sep=' '  ===>  ":DEF"+"",    return=0 (harapr: return=1)
   "ABCDEF"	sep=' '  ===>  "ABCDEF"+"",  return=0 (harapr: return=1)
*/

#endif
