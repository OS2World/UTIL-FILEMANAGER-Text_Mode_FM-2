
FMT.EXE version 1.00                           			02 Apr 2003
====================

FMT is the FM/2 Tiny Textmode file manager, for use on those occasions when 
you have to boot to a text-only OS/2 command prompt.  Its footprint (size and 
memory usage) is very small, so it can easily be run from diskette.  
Multi-file operations are multithreaded, so you don't have to wait on the 
program while it's doing work that may take considerable time.  FMT is page 
tuned, which means it uses even less memory and loads even faster than just 
looking at the size of the program might lead you to believe.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


Installation
============

To install FMT, place:

 FMT.EXE
 FMT.CFG

in the directory of your choice.  If you intend to run FMT from the command 
line, this directory should be in the PATH.  You can create a program object 
to start FMT if you wish.


Using FMT
=========

Just enter 'FMT' from an OS/2 command prompt or create a program object on 
your desktop. The program will start a window displaying the files and 
directories in the current directory.  The function key menus displays on the 
bottom line.  Online help is available via the F1 key.

To see the command line switches, start FMT as:

  FMT /?

FMT is keyboard driven.  There is no mouse support.


Settings
========

FMT saves it's settings in the text file FMT.CFG.



Known problems/shortcomings
=========================== 

 - Colors can not be customized

 - Some features could be better documented.

 - Configuration file could be better documented.


About FMT
=========

FMT was originally written by:

  Mark Kimes
  <hectorplasmic@worldnet.att.net>

He has kindly allowed me to take over maintenance and support of FMT and to 
release the program under the GNU GPL license.  I'm sure he would appreciate 
a Thank You note for his generosity.


Support
=======

Please address support questions and enhancement requests to:

  Steven H. Levine
  steve53@earthlink.net

I also monitor the comp.os.os2.apps newsgroup and others in the 
comp.os.os2.* hierarchy.

Thanks and enjoy.

$TLIB$: $ &(#) %n - Ver %v, %f $
TLIB: $ $
