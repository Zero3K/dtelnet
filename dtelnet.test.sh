#!/bin/sh

if [ $# -eq 0 ]; then
    printf 'usage: %s {colors | altbuff | chars | keys |\n' "$0"
    printf '\tscroll | scroll-fs | scroll-top | scroll-bottom |\n'
    printf '\thistory | attrs}\n'
    exit
fi

if [ x"$1" = x"colors" ];
then
    bgcolor_line () {
	local i

	for i in 0 1 2 3 4 5 6 7; do
	    printf '\033[25;4%dm  %x%x ' "$i" "$i" "$1"
	    printf '\033[5;4%dm  %x%x ' "$i" "$(expr $i + 8)" "$1"
	done
	printf '\033[0;25;39;49m\n'
    }

    color_tab () {
	local i

	for i in 0 1 2 3 4 5 6 7; do
	    printf '\033[22;3%dm' "$i"
	    bgcolor_line "$i"
	    printf '\033[3%d;1m' "$i"
	    bgcolor_line "$(expr $i + 8)"
	done
	printf '\n'
    }

    echo '16x16 color'
    color_tab

# 256 colors -- background only

# $1=start $2=limit (eg: 240 256 => 240..255 )
    colorbg_linepart () {
	JMAX="$(expr $2 - 1)"
	for j in $(seq $1 $JMAX); do
	    printf '\033[48;5;%dm %02x ' "$j" "$j"
	done
    }

# $1= start of the first part
# $2= number of elements in a part
# $3= (start of the second part) - (start of the first part)
# $4= number of parts
    colorbg_parts_line () {
	PMAX="$(expr $4 - 1)"
	for p in $(seq 0 $PMAX); do
	    RMIN="$(expr $1 + $3 \* $p)"
	    RLIM="$(expr $RMIN + $2)"
# printf 'Debug: colorbg_linepart jön p=%d RMIN=d RLIM=%d $3=%d\n' $p $RMIN $RLIM $3
	    colorbg_linepart "$RMIN" "$RLIM"
	    if [ $p -eq $PMAX ]; then
		printf '\033[0;25;39;49m\n'
	    else
		printf '\033[0;25;39;49m   '
	    fi
	done
    }

# $1=start $2=step $3=limit (eg: 0 16 32 => two lines: 0..15, 16..31)
    colorbg_block () {
	IMAX="$(expr $3 - $2)"
	for i in $(seq $1 $2 $IMAX); do
	    JLIM="$(expr $i + $2)"
	    colorbg_linepart "$i" "$JLIM"
	    printf '\033[0;25;39;49m\n'
	done
    }

# $1= start of the l1-p1
# $2= number of elements in a part
# $3= (start of l1-p2) - (start l1-p1)
# $4= number of parts in a line
# $5= (start of l2-p1) - (start l1-p1)
# $6= number of lines

    colorbg_parts_block () {
	LMAX="$(expr $6 - 1)"
	for l in $(seq 0 $LMAX); do
	    NMIN="$(expr $1 + $l \* $5)"
	    colorbg_parts_line "$NMIN" 6 36 3
	done
    }

# $1= start of the l1-p1
# $2= number of elements in a part
# $3= (start of l1-p2) - (start l1-p1)
# $4= number of parts in a line
# $5= (start of l2-p1) - (start l1-p1)
# $6= number of lines
# $7= (start of b2-l1-p1) - (start of b1-l1-p1)
# $8= number of blocks

    colorbg_parts_cluster () {
	CMAX="$(expr $8 - 1)"
	for b in $(seq 0 $CMAX); do
	    BMIN="$(expr $1 + $b \* $7)"
	    colorbg_parts_block "$BMIN" 6 36 3 6 6
	done
    }

    printf '256 color (part#1: 0x00-0x0f)\n'
    colorbg_block 0 8 16
    printf '\n'

    printf '256 color (part#2: 0x10-0xe7)\n'
    colorbg_parts_cluster 16 6 36 3 6 6 108 2
    printf '\n'

    printf '256 color (part#3: 0xe8-0xff)\n'
    colorbg_block 232 8 256

    exit

elif [ x"$1" = x"altbuff" ];
then

    scr1=$(printf '\033[?47l')
    scr2=$(printf '\033[?47h')

    color1=$(printf '\033[42m\033[30m')
    color2=$(printf '\033[44m\033[37m')

    home=$(printf '\033[H')
    clear="$home$(printf '\033[2J')"

    printf "$scr1$color1$clear"
    printf 'Screen #1 Press ENTER to switch\n'
    printf 'Second line on the first screen\n'
    read X

    printf "$scr2$color2$clear"
    printf 'Screen #2 Press ENTER to switch back to screen #1\n'
    printf 'This is how mc/less/etc does the trick'
    read X

    while true; do
	printf "$scr1$color1"'Screen #1 again -- press Enter (Ctrl+C to stop)'
	read X
	if [ "$X" = "H" -o "$X" = "h" ]; then
	    printf '\033[H';
	fi
	printf "$scr2$color2"'Screen #2 again -- Ctrl+C to stop, Enter to go on'
	read X
	if [ "$X" = "H" -o "$X" = "h" ]; then
	    printf '\033[H';
	fi
    done

elif [ x"$1" = x"chars" ];
then
    smacs=$(printf '\016');
    rmacs=$(printf '\017');

    echo "    0 1 2 3 4 5 6 7 8 9 a b c d e f      0 1 2 3 4 5 6 7 8 9 a b c d e f"
    echo "00  . . . . . . . . . . . . . . . .  00  ${smacs}. . . . . . . . . . . . . . . . ${rmacs} 00"
    echo "10  . . . . . . . . . . . . . . . .  10  ${smacs}. . . . . . . . . . . . . . . . ${rmacs} 10"
    echo "20    ! " # \$ % & ' ( ) * + , - . /  20 ${smacs}  ! " # \$ % & ' ( ) * + , - . / ${rmacs} 20"
    echo "30  0 1 2 3 4 5 6 7 8 9 : ; < = > ?  30  ${smacs}0 1 2 3 4 5 6 7 8 9 : ; < = > ? ${rmacs} 30"
    echo "40  @ A B C D E F G H I J K L M N O  40  ${smacs}@ A B C D E F G H I J K L M N O ${rmacs} 40"
    echo "50  P Q R S T U V W X Y Z [ \ ] ^ _  50  ${smacs}P Q R S T U V W X Y Z [ \ ] ^ _ ${rmacs} 50"
    echo "60  \` a b c d e f g h i j k l m n o  60  ${smacs}\` a b c d e f g h i j k l m n o ${rmacs} 60"
    echo "70  p q r s t u v w x y z { | } ~ .  70  ${smacs}p q r s t u v w x y z { | } ~ . ${rmacs} 70"
    echo "80  . . . . . . . . . . . . . . . .  80  ${smacs}. . . . . . . . . . . . . . . . ${rmacs} 80"
    echo "90  . . . . . . . . . . . . . . . .  90  ${smacs}. . . . . . . . . . . . . . . . ${rmacs} 90"
    echo "a0    ¡ ¢ £ ¤ ¥ ¦ § ¨ © ª « ¬ ­ ® ¯  a0  ${smacs}  ¡ ¢ £ ¤ ¥ ¦ § ¨ © ª « ¬ ­ ® ¯ ${rmacs} a0"
    echo "b0  ° ± ² ³ ´ µ ¶ · ¸ ¹ º » ¼ ½ ¾ ¿  b0  ${smacs}° ± ² ³ ´ µ ¶ · ¸ ¹ º » ¼ ½ ¾ ¿ ${rmacs} b0"
    echo "c0  À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï  c0  ${smacs}À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï ${rmacs} c0"
    echo "d0  Ð Ñ Ò Ó Ô Õ Ö × Ø Ù Ú Û Ü Ý Þ ß  d0  ${smacs}Ð Ñ Ò Ó Ô Õ Ö × Ø Ù Ú Û Ü Ý Þ ß ${rmacs} d0"
    echo "e0  à á â ã ä å æ ç è é ê ë ì í î ï  e0  ${smacs}à á â ã ä å æ ç è é ê ë ì í î ï ${rmacs} e0"
    echo "f0  ð ñ ò ó ô õ ö ÷ ø ù ú û ü ý þ ÿ  f0  ${smacs}ð ñ ò ó ô õ ö ÷ ø ù ú û ü ý þ ÿ ${rmacs} f0"
    echo "    0 1 2 3 4 5 6 7 8 9 a b c d e f      0 1 2 3 4 5 6 7 8 9 a b c d e f"

elif [ x"$1" = x"scroll" -o		\
       x"$1" = x"scroll-fs" -o 		\
       x"$1" = x"scroll-top" -o		\
       x"$1" = x"scroll-bottom" ];
then
    lines=$(tput lines)
    ltopn=$(expr $lines / 2)
    lbot1=$(expr $ltopn + 1)
    cols=$(tput cols)

    esc=$(printf '\033')
    ctrl_d=$(printf '\004')
    ctrl_m=$(printf '\015')
    alt_d=$(printf '\033d')
    alt_e=$(printf '\033e')
    alt_m=$(printf '\033m')
    alt_D=$(printf '\033D')
    alt_E=$(printf '\033E')
    alt_M=$(printf '\033M')

    home=$(printf '\033[H')
    clear=$(printf '\033[J')
    reset=$(printf '\033[r\033[0m\033[?7h')
    color1=$(printf '\033[37;40m')
    color2=$(printf '\033[37;42m')
    color3=$(printf '\033[30;43m')
    scroll=$(printf '\033[4;6r\033[?6l')
    noscroll=$(printf '\033[r\033[?6l')

    set -- "prepare-$1"
    . "$0" "$1"

    printf "$home$color3"

    savestty=$(stty -g)
    stty -istrip -icanon -isig -ixon -echo -igncr -icrnl -inlcr min 1
    while C=$(dd if=/dev/tty bs=8 count=1 2>/dev/null);  test x"$C" != x"$esc"; do
	case "$C" in
	[ABCDELMST]) printf '\033[%c' "$C" ;;
	$alt_d|$alt_D) printf '\033D' ;;
	$alt_e|$alt_E) printf '\033E' ;;
	$alt_m|$alt_M) printf '\033M' ;;
	$ctrl_m) printf '\012' ;;	# it should be: $ctrl_j) printf '\012'
	U) printf '\033[4m' ;;
	V) printf '\033[24m' ;;
	R) printf '\033[4h' ;;
	J) printf '\033[0J' ;;
	[1-9]) printf '\033[%d;1H' "$C" ;;
	I) printf '\033[7m' ;;
	*) printf '%s' "$C" ;;
	esac
    done
    printf "${noscroll}"
    stty $savestty
    exit

elif [ x"$1" = x"prepare-scroll" ];
then
    printf "$reset$home$color1$clear"
    printf 'a1 Scroll-test (scroll region: lines 4-6), use ESC to exit\n'
    printf 'a2 use ABCD (uppercase) or 1-9 to move the cursor\n'
    printf 'a3 try to overwrite line-ends, and see how the cursor moves\n'

    printf "$color2$clear"
    printf 'b4 +-----------------------------\n'
    printf 'b5 | This is the scrolling region\n'
    printf 'b6 +-----------------------------\n'

    printf "$color1$clear"
    printf 'a7 This is the first line below the scrolling region\n'
    printf 'a8 Note: auto wrap is on (ESC[?7h)\n'
    printf 'a9       origin mode is full screen (ESC[?6l)\n'
    printf 'a10 Other keys: L=InsertLine,M=DeleteLine,Alt+D=Index,Alt+M=ReverseIndex\n'
    printf 'a11 U/V=undeline on/off; Enter=NewLine(sigh); R=Insert Mode\n'
    printf 'a12 E=CNL J=clear(forward) S=PanDown T=PanUp\n'
    printf 'a13 Results so far:\n'
    printf 'a14 * You can enter the scroll region with cursor UP(A)/DOWN(B)\n'
    printf 'a15   but cannot leave it\n'
    printf 'a16 * You cannot leave a line with LEFT(C)/RIGHT(D)\n'
    printf 'a17 * Writing at the right margin doesn'"'"'t start a new line\n'
    printf 'a18   only the next write does.\n'

    printf '\033[%d;%dr' 4 6

elif [ x"$1" = x"prepare-scroll-fs" ];
then
    printf "$reset$home$color2$clear"
    printf 'a1 In this version,\n'
    printf 'a2 the whole screen is the\n'
    printf 'a3 scrolling region\n'
    seq 4 $lines | while read l; do printf '\033[%d;1H%d' "$l" "$l"; done

    printf '\033[r'

elif [ x"$1" = x"prepare-scroll-top" ];
then
    printf "$reset$home$color2$clear"
    printf 'a1 In this version,\n'
    printf 'a2 the upper part of the screen is the\n'
    printf 'a3 scrolling region\n'
    seq 4 $ltopn | while read l; do printf '\033[%d;1H%d' "$l" "$l"; done

    printf "\033[%d;0H$color1$clear" "$lbot1"
    seq $lbot1 $lines | while read l; do printf '\033[%d;1H%d' "$l" "$l"; done

    printf '\033[%d;%dr' 1 "$ltopn"

elif [ x"$1" = x"prepare-scroll-bottom" ];
then
    printf "$reset$home$color1$clear"
    printf 'a1 In this version,\n'
    printf 'a2 the lower part of the screen is the\n'
    printf 'a3 scrolling region\n'
    seq 4 $ltopn | while read l; do printf '\033[%d;1H%d' "$l" "$l"; done

    printf "\033[%d;0H$color2$clear" "$lbot1"
    seq $lbot1 $lines | while read l; do printf '\033[%d;1H%d' "$l" "$l"; done

    printf '\033[%d;%dr' "$lbot1" "$lines"

elif [ x"$1" = x"keys" ];
then
    esc=$(printf '\033')
    XD=$(which xd) || XD='od -tx1 -tc | head -n-1'
    savestty=$(stty -g)
    stty -istrip -icanon -isig -ixon -echo -igncr -icrnl -inlcr min 1
    printf 'Press ESC to exit (or Ctrl+[)\n'
    while C="$(dd if=/dev/tty bs=8 count=1 2>/dev/null)";  test x"$C" != x"$esc"; do
	printf '%s' "$C" | eval "$XD"
    done
    stty $savestty

elif [ x"$1" = x"history" ];
then
    seq 1 1980
    echo 'Now you might unselect Term/BottomOnOutput, scroll up'
    echo 'and watch scrolling. Also watch selection scolling upwards.'
    seq 1 100 | while read N
    do
	sleep 2
	printf -- '%d more line\n' "$N"
    done
elif [ x"$1" = x"attrs" ];
then
    printf '\n\tTesting Bold/Blink/Inverse/Underline\n\n'

    printf '\033[m Normal  '
    printf '\033[1m Bold \033[21m Norm '
    printf '\033[5m Blink \033[25m Norm '
    printf '\033[1;5m BdBl \033[21;25m Norm '
    printf '\033[m\n\n'

    printf '\033[0;4m Underln '
    printf '\033[1m Bold \033[21m Norm '
    printf '\033[5m Blink \033[25m Norm '
    printf '\033[1;5m BdBl \033[21;25m Norm '
    printf '\033[m\n\n'

    printf '\033[0;7m Invers  '
    printf '\033[1m Bold \033[21m Norm '
    printf '\033[5m Blink \033[25m Norm '
    printf '\033[1;5m BdBl \033[21;25m Norm '
    printf '\n\n'

    printf '\033[0;4;7m InvUndl '
    printf '\033[1m Bold \033[21m Norm '
    printf '\033[5m Blink \033[25m Norm '
    printf '\033[1;5m BdBl \033[21;25m Norm '
    printf '\n\033[m\n'

    printf 'Press Enter to inverse the screen'; read X
    printf '\033[?5h'
    printf 'Press Enter to restore the screen'; read X
    printf '\033[?5l'

else
    printf '%s is unimplemented yet\n' "$1"
fi
