README.TXT - dtelnet

- version 1.3.9 -

* Experimental UTF8 support
  Select in 'Connect' dialog from 'Server Charset', 
  or use command-line option /C UTF-8
  An 'unicode.txt' file is added.

- version 1.3.8 -

* IPv6 support
* New help file 'dtelnet.chm' instead of 'dtelnet.hlp'
* Minor improvement regarding pseudo-graphical characters,
  (minor changes in file 'dtelnet.set')

- version 1.3.7a -

* New help file, thanks to Zvonar, JohnG

- version 1.3.7 -

* well, xterm-kbsH was a bad idea (some telnetd's convert it to 'xterm-kbsh'),
  new: xterm-kbs8
* status line now shows some keyboard-status information
* color-selection in menu Terminal

- version 1.3.6 -

* minor bugfixes
* using environment variable %USERPROFILE%, if %HOME% is not set
* new terminal types in dtelnet.set: xterm-kbs177, xterm-kbsH
  for special cases (see <a href="backspace.txt">backspace.txt</a>)

- version 1.3.5 -

* Dtelnet's file extension changed from '.ts' to '.dtelnet_ses'
  Note: the old files don't stop working, you don't have to rename them.
* From now, you can use NEW ENVIRONMENT feature (if your server supports it).
  add [EnvVars] section into your 'dtelnet.ini' or 'favourite.dtelnet_ses':
  [EnvVars]
  LC_CTYPE="hu_HU.ISO-8859-2"
  ORA_SDTZ="Europe/Budapest"
* New Connect/Reconnect menu command
* Most likely the last version for Windows16

- version 1.3.4a -

* Packing failure: too old help file, folder 'res' got lost

- version 1.3.4 -

* Help file added, thanks to Zvonar, JohnG
* Reorganized 'keypad application mode' settings in 'Connect' dialog.
  Most users should set both radio-buttons to 'Num'.
* For user without mouse: when connected, pressing and releasing both Alt's
  activates the menu
* New menu-command: Term/Mouse: you can assign function 'paste'
  to middle or right mouse-button.
* From now triple-click and quadruple-click are usable to select
  a (screen) line or a (logical) line. (The latter selects the wrapped lines,
  too.)
* Two new terminal-types for linux-users: xterm-linuxbs and rxvt-linuxbs
  Read backspace.txt for details.

- version 1.3.3 -

* Bugfix: Window-resize/maximize sometimes gave strange errors
* Bugfix: Cursor position was wrong in Midnight Commander when switched to
  alternate screen (Ctrl+O), also it wasn't 'vttest' compatible. Fixed.
* Other little fixes to make vttest happy (eg HPR = ESC[a)
  Note: after 'vttest', it's a good idea to execute 'reset' or printf '\033c'
* Another little bugfix in telnet-protocol handling
  
- version 1.3.2 -

* Bugfix: ESC[<lineno>H didn't work (VMS!EDT uses it)

* Reorganized attributes, try them with the attached 'dtelnet.test.sh' script
  with parameter 'attrs'.

  In the command line, within the /A parameter you can specify two more colors
  for bold and blink attributes (that's four colors totally):

  normalForeground/emphasizedForeground-on-normalBackground/emphasizedBackground
  
	eg: DTELNET.EXE /A White/0xfa-on-0xed/0xee
   
   In the INI-file it is 'Attrib' in section [Terminal]
   You can see the 256 colors with 'dtelnet.test.sh colors'.

* For DEC/VT-users ScrollLock and Pause now act as 'Help' and 'Do'
  (irrelevant for unix-users)

 * Also for them there is an option to map Insert/Home/PageUp/Delete/End/PageDown to
   Find/InsertHere/Remove/Select/PrevScreen/NextScreen
   (/VtKeyMap option in *.ts file)

 * If you have a 'HOME' environment variable, dtelnet will look for ini-files
   in %HOME%\dtelnet before the directory of the executable (dtelnet.exe)
   (See Help/About to check)
   
- version 1.3.1a -

* Bugfix: Shift+F<n> should work now (test in Midnight Commander),
  especially with terminal-types xterm and konsole

- version 1.3.1 -

* Bugfix in Term/Emul (Windows 16 version only)

* Support for second mouse-wheel (to tell the truth, I cannot test it),
  also support for "mouse_button_event" functionality (whatever it is)

* >>IMPORTANT<< Reorganized terminal types (dtelnet.set is changed, too); from now on,
  I'd suggest terminal-type konsole (or konsole-256color) for linux-servers,
  xterm (or xterm-256color) for AIX-servers.
  See file backspace.txt for more information.

  Also test these keys in mc/mcedit, they should work with the said terminal-types:
  Shift+Arrows, Ctrl+Up/Down, Ctrl+Home/End, Ctrl+PgUp/PgDn

- version 1.3.0 -

* In menu Term/Font you can select a 'Secondary Font' which will be used for line-graphics.
  I'd suggest you choose one from Courier New, Consolas or Lucida Console,
  and test your selection with Midnight Commander.

* The /A command line option (that sets the forward and background colours now) accepts numbers
  as well, between 0 and 255 (see 256 colors below)
  eg: /A 0xea-0xf8 -- a nice, darkgrey on whitegrey combination, easy on eyes.
  (This is 'Attrib' in the ini-file, section '[Terminal]')

* In file 'dtelnet.set', terminal type 'xterm' has been split into 'xterm-new'
  and 'xterm-old'; but both of them simply send 'xterm' to the server
  (you can change that with menu-command 'Term/Term definitions' or editing the file manually)

- version 1.2.9 -

* In Windows16, we are sending telnet.nop commands in every 65.535 secs
  to keep the connection alive;
  In Windows32/64 we use TCP_KEEPALIVE in every 10 minutes;
  (or the value from registry key:
  HKLM\SYSTEM\CurrentControlSet\Services\TCPIP\Parameters\KeepAliveTime,
  if that is shorter than 10 minutes (NB: it is in millisec))

* Supporting 256 colors (sequences ESC[38;5;<colnum> and ESC[48;5;<colnum>);
  from now, the preferred terminal type is 'rxvt-256color'
  Test with: 'dtelnet.test.sh colors'

* This is the first version coming in three variants: Windows16/32/64

- version 1.2.8 -

* From now on, you can map menu-commands to keys, too.
  Currently available: NewWindow and PrintScreen.
  Example:

  ; Map Ctrl+W to function 'NewWindow', Ctrl+P to function 'PrintScreen'
  ; Notice the lack of quotes.
  ;
  ; Ctrl,0x57 = NewWindow
  ; Ctrl,0x50 = PrintScreen

* Mouse wheel works again (but only in the 32-bit version) to scroll vertically;
  in Midnight Commander test the wheel in itself, and together with Shift

* There were some problems with selection, attributes and anti-aliasing 
  (especially ClearType) -- hopefully it's better now.

* The handling of colors has been changed: now 'blink' means intense background
  color, like 'bold' means intense foreground color.

* In the Font-selection menu you can choose bold font now -- it might or might not
  work for you (check with Midnight Commander).

* More function keys (26, 24 editable in the GUI) -- for special uses.
  Remember: F11=Shift+F1, F22=Shift+F12

* A test script (dtelnet.test.sh), to test colors, alternate buffer, scrolling etc.

* Underline support, it may or may not work depending on font/size, 
  test from unix: tput smul; echo underlined; tput rmul

* Option -I<inifile> works again, but you have to use quotes if there are
  backslashes in the pathname: -I'C:\HOME\USERNAME\DTELNET.INI'

- version 1.2.7 -

New feature: keymapping.
No user interface yet, the definitions are to be entered into dtelnet.ini
or *.ts files. Documentation can be found in file keymap.txt.

Quick example:
[Keymap]
; map Alt+M to "mc\r", Alt+LeftShift+M to "mcedit ",
; Alt+RightShift+M to "mcview ", Alt+BothShifts+M to "man mc\r":
Alt,Shift-,0x4d = "mc\r"
Alt,LShift+,RShift-,0x4d = "mcedit "
Alt,LShift-,RShift+,0x4d = "mcview "
Alt,LShift+,RShift+,0x4d = "man mc\r"

Help-file dropped, it's been unmaintaned since the past millenium.

- version 1.2.6 -

Now Shift-key undoes 'mouse reporting', so in mc[edit] you can select text parts
with Shift+mouse.

Command-line attribute /A has changed: now 'color' means dark-color, 'Color' means ligh-color (compare '/A black-white' and '/A black-White').
(Also a bug has been fixed in handling sequence esc[49m.)

Maximize now doesn't actually maximize (we don't want unused areas on the bottom/right);
from now when you open the program tries to keep the window within the screen.

You may assign longer strings to function keys (for special usage only).

- version 1.2.5 -

Better support for terminal-type 'rxvt' -- 
you should consider using it as default terminal type
if your servers support it.
(File dtelnet.set is changed as well).

If you are using Midnight Commander/Editor,
try these key-sequences (in 'rxvt' mode):

Ctrl+O: hide mc, show previous content
Shift+Arraws: select text
Ctrl+Home: goto the beginning
Ctrl+End: goto the end

Keypad mode: for special applications only,
normally it is safe to disable.

Local Execute: enables a special escape-sequence,
to execute programs on client side -- 
for special applications only,
normally it is highly recommended to disable.

Note: If you are interested in Escape-sequences,
you might be interested in these files 
(from dtelnet source package):

    rxvt.txt:   rxvt-specific sequences 
    keypad.txt: keypad-specific sequences
    shkeys.c:   testprogram for unix
